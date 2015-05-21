#ifndef UniWS_h
#define UniWS_h

#include <stdint.h>

// UniWS drives a single strand of WS28xx LEDs.  It runs on the Teensy
// LC microcontroller board from PJRC.com.
//
// The output pin is hardcoded to 17, because the Teensy LC has a 5
// volt output on that pin, and WS28xx LEDs need 5V data..
//
// You can use the driver either in double- or single-buffer mode.
//
// In double buffer mode, setPixel() writes to the front buffer, and
// show() copies the pixels to the back buffer.  That means you can start
// updating pixel data immediately.
//
// In single buffer mode, there is no back buffer.  After calling
// show(), you can't call setPixel() until busy() returns false.  (You
// can; you'll just make pixels momentarily show incorrect values.)
//
// This driver outputs on pin 17.  It uses one DMA channel, one
// programmable interval timer (PIT), and the TPM1 timer-counter, so
// those are not available to your program.  Note that TPM1 is used to
// control pins 16 and 17 in PWM, so you can't use PWM on pin 16 either.

#define WS2811_RGB    0012
#define WS2811_RBG    0021
#define WS2811_GRB    0102  // Most LED strips are wired this way
#define WS2811_GBR    0201

#define WS2811_800kHz   00  // Nearly all WS2811 are 800 kHz
#define WS2811_400kHz   04  // Adafruit's Flora Pixels
// XXX 400 KHz not implemented


class UniWS {

public:
    enum { LED_PIN = 17 };

    UniWS(uint32_t LEDCount, uint8_t config = WS2811_GRB);
    UniWS(uint32_t LEDCount,
          uint8_t *frontPixels,
          uint8_t *backPixels,
          uint8_t config = WS2811_GRB);
    void begin();

    int  getPixel(uint32_t index) const;
    void clear();
    void setPixel(uint32_t index, int color);
    void setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
    {
        setPixel(index, color(red, green, blue));
    }

    void show();
    bool busy() const;

    uint32_t numPixels() const
    {
        return LEDCount;
    }
    
    static int color(uint8_t red, uint8_t green, uint8_t blue)
    {
        return red << 16 | green << 8 | blue << 0;
    }

private:
    uint32_t LEDCount;
    uint32_t byteCount;
    uint8_t *frontPixels;
    uint8_t *backPixels;
    uint8_t config;

    UniWS(const UniWS&);        // No copy semantics
    void operator = (const UniWS&);
};

#endif  /* !UniWS_h */
