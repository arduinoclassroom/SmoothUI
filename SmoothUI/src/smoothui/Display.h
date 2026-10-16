// SmoothUI display layer: drivers for the presets in SmoothUI_Config.h and
// the canvas every widget draws on.
//
// The canvas gives OLEDs and TFTs the same Adafruit_GFX surface:
//  * mono OLEDs: pixels go straight into the driver's own frame buffer,
//    which is sent with display() - one flicker-free frame at a time;
//  * colour TFTs: the frame is drawn into a RAM strip and sent in bands, so
//    even a 320x240 screen animates without tearing in ~32 KB of RAM.
// It also clips (scrolling text, wheels), which plain Adafruit_GFX does not.
//
// Copyright (c) 2026 Oshan Dissanayaka. MIT License.
#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// ---- presets -----------------------------------------------------------------
#if defined(SMOOTHUI_SSD1306_128X64) || defined(SMOOTHUI_SSD1306_128X32)
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#define SMOOTHUI_MONO 1
#define SMOOTHUI_W 128
#if defined(SMOOTHUI_SSD1306_128X64)
#define SMOOTHUI_H 64
#else
#define SMOOTHUI_H 32
#endif
#elif defined(SMOOTHUI_SH1106_128X64)
#include <Wire.h>
#include <Adafruit_SH110X.h>
#define SMOOTHUI_MONO 1
#define SMOOTHUI_W 128
#define SMOOTHUI_H 64
#elif defined(SMOOTHUI_ST7735_160X80) || defined(SMOOTHUI_ST7735_160X128)
#include <SPI.h>
#include <Adafruit_ST7735.h>
#define SMOOTHUI_MONO 0
#define SMOOTHUI_W 160
#if defined(SMOOTHUI_ST7735_160X80)
#define SMOOTHUI_H 80
#ifndef SMOOTHUI_ST7735_TAB
#define SMOOTHUI_ST7735_TAB INITR_MINI160x80_PLUGIN   // IPS modules (inverted, offset)
#endif
#else
#define SMOOTHUI_H 128
#ifndef SMOOTHUI_ST7735_TAB
#define SMOOTHUI_ST7735_TAB INITR_BLACKTAB
#endif
#endif
#elif defined(SMOOTHUI_ST7789_240X135) || defined(SMOOTHUI_ST7789_240X240) || defined(SMOOTHUI_ST7789_320X240)
#include <SPI.h>
#include <Adafruit_ST7789.h>
#define SMOOTHUI_MONO 0
#if defined(SMOOTHUI_ST7789_240X135)
#define SMOOTHUI_W 240
#define SMOOTHUI_H 135
#elif defined(SMOOTHUI_ST7789_240X240)
#define SMOOTHUI_W 240
#define SMOOTHUI_H 240
#else
#define SMOOTHUI_W 320
#define SMOOTHUI_H 240
#endif
#elif defined(SMOOTHUI_ILI9341_320X240)
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#define SMOOTHUI_MONO 0
#define SMOOTHUI_W 320
#define SMOOTHUI_H 240
#elif defined(SMOOTHUI_CUSTOM_DISPLAY)
#ifndef SMOOTHUI_MONO
#define SMOOTHUI_MONO 0
#endif
#endif

// min/max that work on every core (AVR has them as macros, others as std::).
namespace smoothui {
template <typename T>
inline T lo(T a, T b) { return a < b ? a : b; }
template <typename T>
inline T hi(T a, T b) { return a > b ? a : b; }
}  // namespace smoothui

// ---- colours (RGB565) --------------------------------------------------------
namespace SmoothColor {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// Blend a toward b by t (0..255).
inline uint16_t mix(uint16_t a, uint16_t b, uint8_t t) {
  int r1 = a >> 11, g1 = (a >> 5) & 63, b1 = a & 31;
  int r2 = b >> 11, g2 = (b >> 5) & 63, b2 = b & 31;
  return (uint16_t)(((r1 + ((r2 - r1) * t >> 8)) << 11) | ((g1 + ((g2 - g1) * t >> 8)) << 5) |
                    (b1 + ((b2 - b1) * t >> 8)));
}
}  // namespace SmoothColor

// ---- canvas ------------------------------------------------------------------
class SmoothCanvas : public Adafruit_GFX {
 public:
  SmoothCanvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) { clearClip(); }

  // Colour: an RGB565 strip `rows` lines tall; setBand() picks which lines.
  void attachColor(uint16_t* strip, int16_t rows) {
    _strip = strip;
    _rows = rows;
    _mono = nullptr;
  }
  void setBand(int16_t y0) { _band0 = y0; }
  int16_t bandRows() const { return _rows; }
  // Mono: the driver's page buffer (byte = 8 vertical pixels). `flip` turns
  // the picture 180 degrees (rotation 2).
  void attachMono(uint8_t* pages, bool flip) {
    _mono = pages;
    _flip = flip;
    _strip = nullptr;
    _band0 = 0;
    _rows = HEIGHT;
  }
  bool isMono() const { return _mono != nullptr; }

  void setClip(int16_t x, int16_t y, int16_t w, int16_t h) {
    _cx0 = smoothui::hi<int16_t>(0, x);
    _cy0 = smoothui::hi<int16_t>(0, y);
    _cx1 = smoothui::lo<int16_t>(WIDTH, x + w);
    _cy1 = smoothui::lo<int16_t>(HEIGHT, y + h);
  }
  void clearClip() { _cx0 = 0; _cy0 = 0; _cx1 = WIDTH; _cy1 = HEIGHT; }
  // Saving / narrowing the clip (nested clipping).
  struct Clip { int16_t x0, y0, x1, y1; };
  Clip clip() const { return {_cx0, _cy0, _cx1, _cy1}; }
  void restoreClip(const Clip& c) { _cx0 = c.x0; _cy0 = c.y0; _cx1 = c.x1; _cy1 = c.y1; }
  void narrowClip(int16_t x, int16_t y, int16_t w, int16_t h) {
    _cx0 = smoothui::hi<int16_t>(_cx0, x);
    _cy0 = smoothui::hi<int16_t>(_cy0, y);
    _cx1 = smoothui::lo<int16_t>(_cx1, x + w);
    _cy1 = smoothui::lo<int16_t>(_cy1, y + h);
  }
  // Mono: flip a pixel (the inverted selection bar).
  void invertPixel(int16_t x, int16_t y) {
    if (!_mono || x < _cx0 || x >= _cx1 || y < _cy0 || y >= _cy1) return;
    if (_flip) { x = WIDTH - 1 - x; y = HEIGHT - 1 - y; }
    _mono[x + (y >> 3) * WIDTH] ^= (1 << (y & 7));
  }

  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < _cx0 || x >= _cx1 || y < _cy0 || y >= _cy1) return;
    if (_mono) {
      if (_flip) { x = WIDTH - 1 - x; y = HEIGHT - 1 - y; }
      uint8_t& b = _mono[x + (y >> 3) * WIDTH];
      if (c) b |= (1 << (y & 7)); else b &= ~(1 << (y & 7));
      return;
    }
    y -= _band0;
    if (y < 0 || y >= _rows) return;
    _strip[y * WIDTH + x] = c;
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    int16_t x0 = smoothui::hi(x, _cx0), x1 = smoothui::lo<int16_t>(x + w, _cx1);
    int16_t y0 = smoothui::hi(y, _cy0), y1 = smoothui::lo<int16_t>(y + h, _cy1);
    if (_strip) {
      y0 = smoothui::hi<int16_t>(y0, _band0);
      y1 = smoothui::lo<int16_t>(y1, _band0 + _rows);
    }
    if (x0 >= x1 || y0 >= y1) return;
    if (_mono) {
      for (int16_t yy = y0; yy < y1; ++yy)
        for (int16_t xx = x0; xx < x1; ++xx) drawPixel(xx, yy, c);
      return;
    }
    for (int16_t yy = y0; yy < y1; ++yy) {
      uint16_t* p = _strip + (yy - _band0) * WIDTH + x0;
      for (int16_t n = x1 - x0; n > 0; --n) *p++ = c;
    }
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fillRect(x, y, 1, h, c); }
  void fillScreen(uint16_t c) override {
    int16_t x0 = _cx0, y0 = _cy0, x1 = _cx1, y1 = _cy1;
    clearClip();
    fillRect(0, 0, WIDTH, HEIGHT, c);
    _cx0 = x0; _cy0 = y0; _cx1 = x1; _cy1 = y1;
  }

 private:
  uint16_t* _strip = nullptr;
  uint8_t* _mono = nullptr;
  bool _flip = false;
  int16_t _rows = 0, _band0 = 0;
  int16_t _cx0, _cy0, _cx1, _cy1;
};

// ---- display -----------------------------------------------------------------
// Owns the driver chosen in SmoothUI_Config.h, or wraps one you pass in.
class SmoothDisplay {
 public:
#if defined(SMOOTHUI_SSD1306_128X64) || defined(SMOOTHUI_SSD1306_128X32)
  Adafruit_SSD1306 driver{SMOOTHUI_W, SMOOTHUI_H, &Wire, SMOOTHUI_PIN_RST};
#elif defined(SMOOTHUI_SH1106_128X64)
  Adafruit_SH1106G driver{SMOOTHUI_W, SMOOTHUI_H, &Wire, SMOOTHUI_PIN_RST};
#elif defined(SMOOTHUI_ST7735_160X80) || defined(SMOOTHUI_ST7735_160X128)
  Adafruit_ST7735 driver{SMOOTHUI_PIN_CS, SMOOTHUI_PIN_DC, SMOOTHUI_PIN_RST};
#elif defined(SMOOTHUI_ST7789_240X135) || defined(SMOOTHUI_ST7789_240X240) || defined(SMOOTHUI_ST7789_320X240)
  Adafruit_ST7789 driver{SMOOTHUI_PIN_CS, SMOOTHUI_PIN_DC, SMOOTHUI_PIN_RST};
#elif defined(SMOOTHUI_ILI9341_320X240)
  Adafruit_ILI9341 driver{SMOOTHUI_PIN_CS, SMOOTHUI_PIN_DC, SMOOTHUI_PIN_RST};
#endif

  // A display you created yourself (SMOOTHUI_CUSTOM_DISPLAY). `push` sends a
  // band of RGB565 pixels (colour) or is called once per frame with nullptr
  // (mono: draw from `monoBuffer`).
  typedef void (*PushFn)(Adafruit_GFX* gfx, const uint16_t* pixels, int16_t y, int16_t w, int16_t h);
  void useCustom(Adafruit_GFX* gfx, bool mono, uint8_t* monoBuffer, PushFn push) {
    _custom = gfx;
    _customMono = mono;
    _customBuffer = monoBuffer;
    _customPush = push;
  }

  bool begin() {
    if (SMOOTHUI_PIN_BL >= 0) {
      pinMode(SMOOTHUI_PIN_BL, OUTPUT);
      digitalWrite(SMOOTHUI_PIN_BL, HIGH);
    }
#if defined(SMOOTHUI_SSD1306_128X64) || defined(SMOOTHUI_SSD1306_128X32) || defined(SMOOTHUI_SH1106_128X64)
#if defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
    if (SMOOTHUI_PIN_SDA >= 0 && SMOOTHUI_PIN_SCL >= 0) {
#if defined(ESP32)
      Wire.begin(SMOOTHUI_PIN_SDA, SMOOTHUI_PIN_SCL);
#else
      Wire.setSDA(SMOOTHUI_PIN_SDA);
      Wire.setSCL(SMOOTHUI_PIN_SCL);
#endif
    }
#endif
#if defined(SMOOTHUI_SH1106_128X64)
    if (!driver.begin(SMOOTHUI_I2C_ADDRESS, true)) return false;
#else
    if (!driver.begin(SSD1306_SWITCHCAPVCC, SMOOTHUI_I2C_ADDRESS)) return false;
#endif
    Wire.setClock(400000);   // the full frame goes out every animation frame
#ifdef SMOOTHUI_INVERT_COLORS
    driver.invertDisplay(true);
#endif
    driver.clearDisplay();
    driver.display();
    return true;
#elif defined(SMOOTHUI_ST7735_160X80) || defined(SMOOTHUI_ST7735_160X128) || \
    defined(SMOOTHUI_ST7789_240X135) || defined(SMOOTHUI_ST7789_240X240) ||   \
    defined(SMOOTHUI_ST7789_320X240) || defined(SMOOTHUI_ILI9341_320X240)
#if defined(ESP32)
    if (SMOOTHUI_PIN_SCLK >= 0 && SMOOTHUI_PIN_MOSI >= 0) SPI.begin(SMOOTHUI_PIN_SCLK, -1, SMOOTHUI_PIN_MOSI, SMOOTHUI_PIN_CS);
#elif defined(ARDUINO_ARCH_RP2040)
    if (SMOOTHUI_PIN_SCLK >= 0 && SMOOTHUI_PIN_MOSI >= 0) { SPI.setSCK(SMOOTHUI_PIN_SCLK); SPI.setTX(SMOOTHUI_PIN_MOSI); }
#endif
#if defined(SMOOTHUI_ST7735_160X80) || defined(SMOOTHUI_ST7735_160X128)
    driver.initR(SMOOTHUI_ST7735_TAB);
    driver.setSPISpeed(SMOOTHUI_SPI_HZ);
#ifndef SMOOTHUI_ROTATION
#define SMOOTHUI_ROTATION 1
#endif
#elif defined(SMOOTHUI_ST7789_240X135)
    driver.init(135, 240);
    driver.setSPISpeed(SMOOTHUI_SPI_HZ);
#ifndef SMOOTHUI_ROTATION
#define SMOOTHUI_ROTATION 1
#endif
#elif defined(SMOOTHUI_ST7789_240X240)
    driver.init(240, 240);
    driver.setSPISpeed(SMOOTHUI_SPI_HZ);
#ifndef SMOOTHUI_ROTATION
#define SMOOTHUI_ROTATION 0
#endif
#elif defined(SMOOTHUI_ST7789_320X240)
    driver.init(240, 320);
    driver.setSPISpeed(SMOOTHUI_SPI_HZ);
#ifndef SMOOTHUI_ROTATION
#define SMOOTHUI_ROTATION 1
#endif
#else
    driver.begin(SMOOTHUI_SPI_HZ);
#ifndef SMOOTHUI_ROTATION
#define SMOOTHUI_ROTATION 1
#endif
#endif
    driver.setRotation(SMOOTHUI_ROTATION);
#ifdef SMOOTHUI_INVERT_COLORS
    driver.invertDisplay(true);
#endif
    driver.fillScreen(0);
    return true;
#else
    return _custom != nullptr;
#endif
  }

  Adafruit_GFX* gfx() {
#if defined(SMOOTHUI_CUSTOM_DISPLAY)
    return _custom;
#else
    return &driver;
#endif
  }
  int16_t width() { return gfx() ? gfx()->width() : 0; }
  int16_t height() { return gfx() ? gfx()->height() : 0; }
  bool mono() const {
#if defined(SMOOTHUI_CUSTOM_DISPLAY)
    return _customMono;
#else
    return SMOOTHUI_MONO;
#endif
  }

  // Mono: the page buffer the canvas draws into.
  uint8_t* monoBuffer() {
#if defined(SMOOTHUI_CUSTOM_DISPLAY)
    return _customBuffer;
#elif SMOOTHUI_MONO
    return driver.getBuffer();
#else
    return nullptr;
#endif
  }
  bool monoFlipped() const {
#if defined(SMOOTHUI_ROTATION)
    return SMOOTHUI_ROTATION == 2;
#else
    return false;
#endif
  }

  // Colour: send `h` rows starting at `y`.
  void pushBand(const uint16_t* pixels, int16_t y, int16_t w, int16_t h) {
#if defined(SMOOTHUI_CUSTOM_DISPLAY)
    if (_customPush) _customPush(_custom, pixels, y, w, h);
#elif !SMOOTHUI_MONO
    driver.drawRGBBitmap(0, y, pixels, w, h);
#else
    (void)pixels; (void)y; (void)w; (void)h;
#endif
  }
  // Mono: send the finished frame.
  void flush() {
#if defined(SMOOTHUI_CUSTOM_DISPLAY)
    if (_customPush) _customPush(_custom, nullptr, 0, 0, 0);
#elif SMOOTHUI_MONO
    driver.display();
#endif
  }

  // 0..255. TFT backlight (PWM on SMOOTHUI_PIN_BL) or OLED contrast.
  void setBrightness(uint8_t level) {
#if defined(SMOOTHUI_SSD1306_128X64) || defined(SMOOTHUI_SSD1306_128X32)
    driver.ssd1306_command(SSD1306_SETCONTRAST);
    driver.ssd1306_command(level);
#elif defined(SMOOTHUI_SH1106_128X64)
    driver.setContrast(level);
#else
    if (SMOOTHUI_PIN_BL >= 0) analogWrite(SMOOTHUI_PIN_BL, level);
#endif
  }

 private:
  Adafruit_GFX* _custom = nullptr;
  bool _customMono = false;
  uint8_t* _customBuffer = nullptr;
  PushFn _customPush = nullptr;
};
