#include <UniWS.h>

#define NUM_LEDS 19

UniWS strip(NUM_LEDS);

void setup()
{
  strip.begin();
  strip.show();               // turn LEDs off
}

void loop()
{
  uint8_t wait = 50;
  uint8_t high = 255;
  colorWipe(strip.color(high, 0, 0), wait); // Red
  colorWipe(strip.color(0, 0, 0), wait);    // Black
  colorWipe(strip.color(0, high, 0), wait); // Green
  colorWipe(strip.color(0, 0, 0), wait);    // Black
  colorWipe(strip.color(0, 0, high), wait); // Blue
  colorWipe(strip.color(0, 0, 0), wait);    // Black
  delay(500);
};

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint16_t wait)
{
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixel(i, c);
    strip.show();
    delay(wait);
  }
}
