/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Bob Miller
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "UniWS.h"

// POSIX headers
#include <string.h>

// Arduino/Teensy headers
#include <Arduino.h>
#include <DMAChannel.h>
#include <IntervalTimer.h>

// The plan is, we use the timer-counter TPM1 (aka FTM1) to drive the pin.
// The timer overflows once per bit transmitted, and uses PWM to output
// long pulses (1) and short pulses (0).  A DMA channel is used to reload
// the pulse width after every pulse.  To create the 50 usec zero signal
// that resets the pulse train, we send a series of zero-length pulses.
// The DMA channel reads from a circular buffer.  A separate PIT timer
// fires twice in the time it takes the DMA to drain the buffer.  The
// timer's ISR refills the buffer by unpacking pixels from the
// pixel back buffer.

// Constants assume F_TIMER == 24 MHz, LED freq == 800 KHz.
// XXX fixme
#define BIT_TICKS  (30 + 10) // XXX slow down to 600 KHz.
#define ONE_TICKS  20
#define ZERO_TICKS 10

#define LED_BITS   24
#define RESET_BITS 40           // hold pin low this many bit-times.

// The DMA buffer is hardwired to 256 bytes.  That allows the PIT
// timer ISR 80 usec of latency.  (That's probably overgenerous.)
#define DMABUF_COUNT 128
#define DMABUF_BYTES (DMABUF_COUNT * sizeof (uint16_t))

enum State {
    S_IDLE,
    S_RUNNING,
    S_RESETTING,
    S_DRAINING,
};

// Here we drop the pretense of a UniWS object.  Most of the state has
// file-static scope so the ISRs can find it.

static DMAChannel dma;
static IntervalTimer itimer;
static uint16_t DMABuf[DMABUF_COUNT]__attribute__((__aligned__(DMABUF_BYTES)));

static volatile State state;
const uint8_t *volatile packedBits;
volatile uint32_t packedByteCount;
volatile uint16_t bitCursor;
volatile uint8_t resetBitCount;

static void unpackBits(uint16_t i0, uint16_t i1)
{
    uint16_t avail = i1 >= i0 ? i1 - i0 : i1 + DMABUF_COUNT - i0;
    while (state == S_RUNNING && avail >= 8) {
        if (packedByteCount) {
            uint8_t byte = *packedBits++;
            for (uint8_t mask = 0x80; mask; mask >>= 1) {
                DMABuf[bitCursor] = (byte & mask) ? ONE_TICKS : ZERO_TICKS;
                bitCursor = (bitCursor + 1) % DMABUF_COUNT;
            }
            --packedByteCount;
            avail -= 8;
        } else {
            state = S_RESETTING;
            resetBitCount = RESET_BITS;
        }            
    }
    while (state == S_RESETTING && avail) {
        if (resetBitCount) {
            DMABuf[bitCursor] = 0;
            bitCursor = (bitCursor + 1) % DMABUF_COUNT;
            --avail;
            --resetBitCount;
        } else
            state = S_DRAINING;
    }
    if (state == S_DRAINING)
        itimer.end();
}

static void itimer_ISR(void)
{
    volatile const void *sar = dma.CFG->SAR;
    uint16_t i0 = bitCursor;
    uint16_t i1 = (uint16_t *)sar - DMABuf;
    unpackBits(i0, i1);
}

static void DMA_ISR(void)
{
    dma.clearInterrupt();
    FTM1_C1SC &= ~FTM_CSC_DMA;
    state = S_IDLE;
}

UniWS::UniWS(uint32_t LEDCount, uint8_t config)
    : LEDCount(LEDCount),
      byteCount(3 * LEDCount),
      config(config)
{
    frontPixels = (uint8_t *)malloc(byteCount);
    backPixels = (uint8_t *)malloc(byteCount);
}

UniWS::UniWS(uint32_t LEDCount,
          uint8_t *frontPixels,
          uint8_t *backPixels,
          uint8_t config)
    : LEDCount(LEDCount),
      byteCount(3 * LEDCount),
      frontPixels(frontPixels),
      backPixels(backPixels),
      config(config)
{}

void UniWS::begin()
{
    // Init pixels and state.
    clear();
    state = S_IDLE;

    // Init DMA.
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_FTM1_CH1);
    dma.attachInterrupt(DMA_ISR);
    dma.interruptAtCompletion();
    dma.disableOnCompletion();
    dma.destination(*(volatile uint16_t *)&FTM1_C1V);

    // Init interval timer.
    // (nothing to do)
    
    // Init timer-counter.
    FTM1_SC = 0;
    FTM1_MOD = BIT_TICKS - 1;
    FTM1_C1V = 0;
    FTM1_C1SC = FTM_CSC_CHF | FTM_CSC_MSB | FTM_CSC_ELSB;
    FTM1_SC = FTM_SC_CLKS(0b01) | FTM_SC_PS(0b001) | FTM_SC_TOF;

    // Init pin.
    CORE_PIN17_CONFIG = PORT_PCR_MUX(3) | PORT_PCR_DSE | PORT_PCR_SRE;
}

void UniWS::show()
{
    if (LEDCount == 0)
        return;
    
    while (state == S_RUNNING)
        continue;

    // Copy pixels while LEDs are resetting (50 usec).
    if (backPixels) {
        memcpy(backPixels, frontPixels, byteCount);
        packedBits = backPixels;
    } else
        packedBits = frontPixels;
    
    while (state != S_IDLE)
        continue;

    state = S_RUNNING;
    packedByteCount = byteCount;
    bitCursor = 0;
    unpackBits(0, DMABUF_COUNT);

    // Start DMA.
    dma.sourceCircular(DMABuf, sizeof DMABuf);
    unsigned int transferCount = LEDCount * LED_BITS + RESET_BITS;
#if 0
    // Bug in DMAChannel: transferCount shifts right to convert words to bytes.
    // It should shift left.
    transferCount <<= 2;  // shift left twice to compensate.
    dma.transferCount(transferCount);
#else
    // We'll just set the register directly.
    dma.CFG->DSR_BCR = transferCount * sizeof (uint16_t);
#endif
    dma.enable();

    // Start interval timer.
    // The timer-counter overflows every 64K bus clock ticks.
    // We want to wake up after buf_size/2 overflows.
    // Multiply by 1000000 for microseconds.
    uint32_t interval = BIT_TICKS / (F_BUS / 1000000) * DMABUF_COUNT / 2;
    itimer.begin(itimer_ISR, interval);

    // Start timer-counter.
    FTM1_C1SC |= FTM_CSC_CHF;
    FTM1_C1SC |= FTM_CSC_DMA;
}

bool UniWS::busy() const
{
    return state == S_RUNNING;
}

void UniWS::clear()
{
    memset(frontPixels, 0, byteCount);
}

int UniWS::getPixel(uint32_t index) const
{
    const uint8_t *pixel = frontPixels + 3 * index;
    return (pixel[config >> 6 & 3] << 16 |
            pixel[config >> 3 & 3] <<  8 |
            pixel[config >> 0 & 3] <<  0);
}

void UniWS::setPixel(uint32_t index, int color)
{
    uint8_t *pixel = frontPixels + 3 * index;
    pixel[config >> 6 & 3] = color >> 16;
    pixel[config >> 3 & 3] = color >>  8;
    pixel[config >> 0 & 3] = color >>  0;
}
