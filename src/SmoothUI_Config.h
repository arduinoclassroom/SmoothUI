// SmoothUI configuration.
//
// Pick your display in ONE of two ways:
//   a) uncomment one line in section 1 below, or
//   b) #define it in your sketch BEFORE #include <SmoothUI.h>, e.g.
//        #define SMOOTHUI_ST7735_160X80
//        #include <SmoothUI.h>
// Anything else in this file can be overridden the same way.
//
// SmoothUI - smooth animated menus for OLED and TFT displays.
// Copyright (c) 2026 Oshan Dissanayaka. MIT License (see LICENSE).
#pragma once

// =============================================================================
// 1. Display - uncomment ONE
// =============================================================================
// Monochrome OLED (I2C)
// #define SMOOTHUI_SSD1306_128X64     // 0.96" / 1.3" SSD1306 OLED, 128x64
// #define SMOOTHUI_SSD1306_128X32     // 0.91" SSD1306 OLED, 128x32
// #define SMOOTHUI_SH1106_128X64      // 1.3" SH1106 OLED, 128x64
//
// Colour TFT (SPI)
// #define SMOOTHUI_ST7735_160X80      // 0.96" IPS ST7735S, 160x80
// #define SMOOTHUI_ST7735_160X128     // 1.8" ST7735, 160x128
// #define SMOOTHUI_ST7789_240X135     // 1.14" IPS ST7789, 240x135 (TTGO T-Display)
// #define SMOOTHUI_ST7789_240X240     // 1.3" / 1.54" IPS ST7789, 240x240
// #define SMOOTHUI_ST7789_320X240     // 2.0" IPS ST7789, 320x240
// #define SMOOTHUI_ILI9341_320X240    // 2.4" / 2.8" ILI9341, 320x240
//
// Anything else: #define SMOOTHUI_CUSTOM_DISPLAY and pass your own
// Adafruit_GFX display to SmoothUI (see examples/CustomDisplay).

#if !defined(SMOOTHUI_SSD1306_128X64) && !defined(SMOOTHUI_SSD1306_128X32) && \
    !defined(SMOOTHUI_SH1106_128X64) && !defined(SMOOTHUI_ST7735_160X80) &&   \
    !defined(SMOOTHUI_ST7735_160X128) && !defined(SMOOTHUI_ST7789_240X135) && \
    !defined(SMOOTHUI_ST7789_240X240) && !defined(SMOOTHUI_ST7789_320X240) && \
    !defined(SMOOTHUI_ILI9341_320X240) && !defined(SMOOTHUI_CUSTOM_DISPLAY)
#define SMOOTHUI_SSD1306_128X64   // nothing chosen: the most common OLED
#endif

// =============================================================================
// 2. Pins
// =============================================================================
// I2C OLED: SDA/SCL are the board's default Wire pins unless set here
// (ESP32 / RP2040 can use any pins).
#ifndef SMOOTHUI_I2C_ADDRESS
#define SMOOTHUI_I2C_ADDRESS 0x3C   // some modules use 0x3D
#endif
#ifndef SMOOTHUI_PIN_SDA
#define SMOOTHUI_PIN_SDA -1
#endif
#ifndef SMOOTHUI_PIN_SCL
#define SMOOTHUI_PIN_SCL -1
#endif

// SPI TFT: CS and DC are required. SCLK/MOSI -1 = the board's default SPI pins
// (on ESP32 any pins can be given here).
#ifndef SMOOTHUI_PIN_CS
#define SMOOTHUI_PIN_CS 5
#endif
#ifndef SMOOTHUI_PIN_DC
#define SMOOTHUI_PIN_DC 16
#endif
#ifndef SMOOTHUI_PIN_RST
#define SMOOTHUI_PIN_RST -1        // -1 if RES is tied to the board's reset
#endif
#ifndef SMOOTHUI_PIN_SCLK
#define SMOOTHUI_PIN_SCLK -1
#endif
#ifndef SMOOTHUI_PIN_MOSI
#define SMOOTHUI_PIN_MOSI -1
#endif
#ifndef SMOOTHUI_PIN_BL
#define SMOOTHUI_PIN_BL -1         // TFT backlight (BLK/LED); -1 if wired to 3.3 V
#endif

// =============================================================================
// 3. Display options
// =============================================================================
#ifndef SMOOTHUI_SPI_HZ
#define SMOOTHUI_SPI_HZ 40000000   // lower (e.g. 20000000) for long wires
#endif
// 0..3. Default: landscape for TFTs, normal for OLEDs. OLEDs accept 0 or 2.
// #define SMOOTHUI_ROTATION 1
// Swap if colours look inverted (black background shows white).
// #define SMOOTHUI_INVERT_COLORS 1
// ST7735 variant for the Adafruit driver (see Adafruit_ST7735.h), e.g.
// INITR_MINI160x80 for non-inverted 160x80 modules, INITR_GREENTAB...
// #define SMOOTHUI_ST7735_TAB INITR_MINI160x80_PLUGIN

// =============================================================================
// 4. Behaviour
// =============================================================================
#ifndef SMOOTHUI_MAX_ITEMS
#define SMOOTHUI_MAX_ITEMS 20      // items per SmoothMenu
#endif
#ifndef SMOOTHUI_MAX_DEPTH
#define SMOOTHUI_MAX_DEPTH 8       // pages open at once (menu -> submenu -> ...)
#endif
#ifndef SMOOTHUI_MAX_TIMERS
#define SMOOTHUI_MAX_TIMERS 4      // SmoothCountdowns registered with ui.add()
#endif
#ifndef SMOOTHUI_FPS
#define SMOOTHUI_FPS 60            // frame rate while something moves
#endif
#ifndef SMOOTHUI_BUFFER_BYTES
// Colour TFTs draw through a strip of this many bytes (the frame is sent in
// bands). More = fewer bands = faster; at least one display line.
#if defined(ESP32) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
#define SMOOTHUI_BUFFER_BYTES 32768
#else
#define SMOOTHUI_BUFFER_BYTES 2048
#endif
#endif
#ifndef SMOOTHUI_LONG_PRESS_MS
#define SMOOTHUI_LONG_PRESS_MS 600 // hold time that means "back"
#endif
// 1 = show "Back" as the last row of every list (long press always goes back).
#ifndef SMOOTHUI_BACK_ROW
#define SMOOTHUI_BACK_ROW 1
#endif
// 1 = 24-hour time pickers, 0 = 12-hour with AM/PM.
#ifndef SMOOTHUI_24H
#define SMOOTHUI_24H 0
#endif
// Text uses the crisp built-in 6x8 font. Colour screens of 120 px or taller
// look nicer with a proportional Adafruit GFX font, e.g. in your sketch:
//   #include <Fonts/FreeSans9pt7b.h>
//   #define SMOOTHUI_FONT &FreeSans9pt7b
//   #include <SmoothUI.h>
// (or set ui.theme.font before ui.begin()).
