// SmoothUI theme (colours, animation speeds, layout) and the animation curve.
// Copyright (c) 2026 Oshan Dissanayaka. MIT License.
#pragma once
#include <Arduino.h>
#include <math.h>
#include "Display.h"

struct SmoothTheme {
  // Colours (RGB565). On mono OLEDs anything non-zero is "on" and the
  // selection is drawn inverted, so these only matter on colour screens.
  uint16_t background = SmoothColor::rgb(8, 8, 12);
  uint16_t text = 0xFFFF;
  uint16_t textDim = SmoothColor::rgb(160, 176, 248);
  uint16_t accent = SmoothColor::rgb(32, 152, 248);       // selection, bars, switches
  uint16_t accentText = 0xFFFF;                           // text on the accent
  uint16_t highlight = SmoothColor::rgb(0, 252, 248);     // ticks, scrollbar, focus
  uint16_t track = SmoothColor::rgb(16, 20, 16);          // empty bars, switch off
  uint16_t panel = SmoothColor::rgb(24, 28, 24);          // popup boxes
  uint16_t danger = SmoothColor::rgb(248, 52, 52);        // destructive buttons
  uint16_t muted = SmoothColor::rgb(72, 76, 72);          // outlines, off knobs

  // Animation speeds, 0..99 (higher = faster, 99 = instant). The curve is
  //   value += (target - value) / (100 - speed)   per 60 Hz frame.
  uint8_t listSpeed = 88;     // rows unfolding when a list opens
  uint8_t selectSpeed = 91;   // the selection bar gliding to a row
  uint8_t slideSpeed = 95;    // pages sliding in and out
  uint8_t barSpeed = 95;      // scrollbar, progress bars, switches

  // Layout (0 = worked out from the screen size and font).
  int16_t rowHeight = 0;
  int16_t titleHeight = 0;
  const GFXfont* font = nullptr;   // nullptr = built-in 6x8 font
  uint8_t textSize = 1;            // scale for the built-in font
  bool showTitle = true;           // title bar on list pages
};

namespace SmoothAnim {
// Moves `value` toward `target` along SmoothUI's ease-out curve, frame-rate
// independent (`dtMs` since the last frame). Returns true while moving.
inline bool approach(float& value, float target, uint8_t speed, uint32_t dtMs) {
  float d = target - value;
  if (fabsf(d) < 0.02f) {
    bool moved = value != target;
    value = target;
    return moved;
  }
  if (speed >= 99) { value = target; return true; }
  float k = 1.0f / (100 - speed);
  float frames = dtMs / 16.667f;
  float f = 1.0f - powf(1.0f - k, frames);
  value += d * f;
  return true;
}
}  // namespace SmoothAnim
