// SmoothUI - BasicMenu
// A main menu with a sub-menu, a switch, a slider and an action.
//
// Wiring (change the numbers to your pins):
//   OLED SSD1306 128x64 (I2C): SDA/SCL to the board's I2C pins
//   Rotary encoder: A -> 32, B -> 33, push button -> 25 (other side to GND)
// Turn = move, press = choose, hold = back.

#define SMOOTHUI_SSD1306_128X64   // pick your display (see SmoothUI_Config.h)
#include <SmoothUI.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // the blue LED on most ESP32 boards
#endif

SmoothUI ui;
SmoothEncoder knob(32, 33, 25);

SmoothMenu mainMenu("Main menu");
SmoothMenu settings("Settings");

bool ledOn = false;
int brightness = 70;

void onLed(bool on) { digitalWrite(LED_BUILTIN, on ? HIGH : LOW); }
void onBrightness(int value) { ui.display.setBrightness(map(value, 0, 100, 1, 255)); }
void sayHello() { ui.alert("Hello!", "SmoothUI is running"); }

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  settings.addToggle("LED", &ledOn, onLed);
  settings.addNumber("Brightness", &brightness, 0, 100, 5, "%", onBrightness);
  settings.addInfo("Version", SMOOTHUI_VERSION);

  mainMenu.addAction("Say hello", sayHello);
  mainMenu.addMenu("Settings", &settings);

  knob.begin();
  ui.begin(&mainMenu, &knob);
}

void loop() {
  ui.update();
}
