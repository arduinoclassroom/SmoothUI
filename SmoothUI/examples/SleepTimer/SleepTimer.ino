// SmoothUI - SleepTimer
// Timers: a sleep timer chosen from a list (30 min ... 2 h) that counts down
// in the menu and shows an alert at the end, plus a wake-up time picker.
// Uses push buttons instead of an encoder: Up, Down, OK (hold OK = back).
//
// Display: 1.3" SH1106 OLED (I2C). Buttons to GND on pins 26, 27, 14.

#define SMOOTHUI_SH1106_128X64
#include <SmoothUI.h>

SmoothUI ui;
SmoothButtons buttons(26, 27, 14);   // up, down, ok
SmoothCountdown sleepTimer;
SmoothMenu menu("Sleep timer");

const char* const choices[] = {"Off", "30 min", "1 h", "1 h 30 min", "2 h"};
const uint16_t minutes[] = {0, 30, 60, 90, 120};
int choice = 0;
int wakeAt = 6 * 60 + 45;
bool wakeOn = false;

void sleepNow() {
  choice = 0;
  ui.alert("Good night", "Timer finished", 3000);
  // Stop the music, switch things off, go to deep sleep...
}

void onChoice(int i) {
  if (minutes[i]) sleepTimer.startMinutes(minutes[i], sleepNow);
  else sleepTimer.stop();
}

const char* timeLeft() {
  static char buf[12];
  if (!sleepTimer.running()) return "Off";
  sleepTimer.format(buf, sizeof(buf));
  return buf;
}

void onWake(int minutesOfDay, bool on) {
  // Save to EEPROM / Preferences, set an RTC alarm...
  (void)minutesOfDay;
  (void)on;
}

void setup() {
  menu.addChoice("Sleep after", choices, 5, &choice, onChoice);
  menu.addInfo("Time left", timeLeft);   // redrawn twice a second
  menu.addTime("Wake-up", &wakeAt, &wakeOn, onWake);

  buttons.begin();
  ui.add(&sleepTimer);   // ui.update() checks it
  ui.begin(&menu, &buttons);
}

void loop() {
  ui.update();
}
