// SmoothUI - CustomDisplay
// Use any display that has an Adafruit_GFX driver, with your own setup code.
// Here: an ST7789 240x320 started by hand, and events fed from Serial
// (type n = next, p = previous, s = select, b = back) - handy for testing
// without an encoder.

#define SMOOTHUI_CUSTOM_DISPLAY
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans9pt7b.h>
#include <SmoothUI.h>

Adafruit_ST7789 tft(5, 16, 17);   // CS, DC, RST

// SmoothUI draws the screen in bands; this sends one band to the display.
void push(Adafruit_GFX*, const uint16_t* pixels, int16_t y, int16_t w, int16_t h) {
  tft.drawRGBBitmap(0, y, pixels, w, h);
}

SmoothUI ui(&tft, false, nullptr, push);
SmoothMenu menu("Custom display");
bool option = true;
int level = 3;

void setup() {
  Serial.begin(115200);
  tft.init(240, 320);
  tft.setRotation(1);
  tft.setSPISpeed(40000000);

  ui.theme.font = &FreeSans9pt7b;           // a nicer font on big screens
  ui.theme.accent = SmoothColor::rgb(255, 120, 0);
  ui.theme.highlight = SmoothColor::rgb(255, 200, 80);

  menu.addToggle("Option", &option);
  menu.addNumber("Level", &level, 1, 10);
  ui.begin(&menu);
}

void loop() {
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'n': ui.handle(SmoothEvent::Next); break;
      case 'p': ui.handle(SmoothEvent::Prev); break;
      case 's': ui.handle(SmoothEvent::Select); break;
      case 'b': ui.handle(SmoothEvent::Back); break;
    }
  }
  ui.update();
}
