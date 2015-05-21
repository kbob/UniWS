# UniWS
Low latency WS28xx LED driver for Teensy LC

UniWS drives a single strand of WS28xx LEDs.  It runs on the [Teensy LC
microcontroller board](https://www.pjrc.com/teensy/teensyLC.html).

The output pin is hardcoded to 17, because the Teensy LC has a 5
volt output on that pin, and WS28xx LEDs need 5V data.

# Use



## Buffering

You can use the driver either in single- or double-buffered mode.

In double buffered mode, setPixel() writes to the front buffer, and
show() copies the pixels to the back buffer.  That means you can start
updating pixel data immediately after calling show().

In single buffered mode, there is no back buffer.  After calling
show(), you can't call setPixel() until busy() returns false.  (You
can; you'll just make pixels show incorrect values.)

By default, the UniWS allocates double buffers for itself.

## Resources

This driver outputs on pin 17.  It allocates one DMA channel, one
programmable interval timer (PIT), and the TPM1 timer-counter, so
those are not available to your program.  Note that TPM1 is used to
control pins 16 and 17 in PWM, so you can't use PWM on pin 16 either.
You can use pin 16 for non-PWM digital or analog I/O.

## Limitations

RAM
Refresh Speed
Power
