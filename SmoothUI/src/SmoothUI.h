// SmoothUI - smooth, animated menus for OLED and TFT displays on Arduino.
//
//   #define SMOOTHUI_SSD1306_128X64      // your display (or edit SmoothUI_Config.h)
//   #include <SmoothUI.h>
//
//   SmoothUI ui;
//   SmoothEncoder knob(32, 33, 25);      // A, B, push button
//   SmoothMenu menu("Main");
//
//   void setup() { menu.addToggle("Light", &light); knob.begin(); ui.begin(&menu, &knob); }
//   void loop()  { ui.update(); }
//
// Version 1.0.0
// Copyright (c) 2026 Oshan Dissanayaka. MIT License (see LICENSE).
// Animation style inspired by Astra UI (github.com/AstraThreshold/oled-ui-astra);
// no code from it is used.
#pragma once

#define SMOOTHUI_VERSION "1.0.0"

#include "SmoothUI_Config.h"
#include "smoothui/Display.h"
#include "smoothui/Theme.h"
#include "smoothui/Input.h"
#include "smoothui/Core.h"
#include "smoothui/Widgets.h"
