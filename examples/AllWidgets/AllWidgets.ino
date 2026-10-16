// SmoothUI - AllWidgets
// Every widget in one sketch: sub-menus, switches, sliders, choice lists,
// a time picker, read-only info, a long dynamic list, alerts, a yes/no
// question and a progress screen.
//
// Shown on a 0.96" 160x80 ST7735 TFT; change the #define for your display.
// Wiring (ESP32, change to your pins):
//   TFT: SCL -> 18, SDA -> 23, CS -> 5, DC -> 16, RES -> 17, BLK -> 4
//   Encoder: A -> 32, B -> 33, push -> 25

#define SMOOTHUI_ST7735_160X80
#define SMOOTHUI_PIN_SCLK 18
#define SMOOTHUI_PIN_MOSI 23
#define SMOOTHUI_PIN_CS 5
#define SMOOTHUI_PIN_DC 16
#define SMOOTHUI_PIN_RST 17
#define SMOOTHUI_PIN_BL 4
#include <SmoothUI.h>

SmoothUI ui;
SmoothEncoder knob(32, 33, 25);

// ---- values the menus edit ----
bool wifiOn = true, sound = true, darkMode = false;
int volume = 12, contrast = 80;
int playMode = 0;
const char* const playModes[] = {"In order", "Repeat one", "Repeat all", "Shuffle"};
int language = 0;
const char* const languages[] = {"English", "Sinhala", "Tamil"};
int alarmMinutes = 7 * 60 + 30;   // 7:30 AM
bool alarmOn = false;
int reminderMinutes = 18 * 60;    // 6:00 PM, no switch: picker has Save

// ---- a long list served by functions (no RAM per entry) ----
int songCount() { return 40; }
const char* songName(int i, char* buf, size_t n) {
  snprintf(buf, n, "Song number %d with a long title that scrolls", i + 1);
  return buf;
}
const char* songLength(int i, char* buf, size_t n) {
  snprintf(buf, n, "%d:%02d", 2 + i % 4, (i * 17) % 60);
  return buf;
}
int playing = -1;
bool isPlaying(int i) { return i == playing; }
void playSong(int i) { playing = i; ui.redraw(); }
int toDelete = -1;
void deleteAnswer(bool yes) { if (yes) ui.alert("Deleted", "(pretend)"); }
void askDelete(int i) {
  toDelete = i;
  ui.confirm("Delete song?", "This can't be undone", deleteAnswer, "Delete", "Cancel", true);
}
SmoothList songs("Songs", songCount, songName, playSong, askDelete);

// ---- progress screen driven from loop() ----
SmoothProgressPage download("Downloading");
bool downloading = false;
uint32_t downloadStart;
void cancelDownload() { downloading = false; ui.back(); ui.alert("Cancelled"); }
void startDownload() {
  downloading = true;
  downloadStart = millis();
  download.set(0, "episode-42.mp3");
  ui.show(&download);
}

// ---- menus ----
SmoothMenu mainMenu("SmoothUI demo");
SmoothMenu settings("Settings");
SmoothMenu displayMenu("Display");
SmoothMenu alarms("Alarms");

const char* uptime() {
  static char buf[16];
  snprintf(buf, sizeof(buf), "%lus", millis() / 1000);
  return buf;
}
void powerOffAnswer(bool yes) { if (yes) ui.alert("Bye!", "(pretend power off)"); }
void askPowerOff() { ui.confirm("Power off?", "Hold the knob to turn on", powerOffAnswer, "Power off", "Cancel", true); }

void setup() {
  displayMenu.addNumber("Contrast", &contrast, 0, 100, 5, "%");
  displayMenu.addToggle("Dark mode", &darkMode);
  displayMenu.addChoice("Language", languages, 3, &language);

  alarms.addTime("Wake-up", &alarmMinutes, &alarmOn);
  alarms.addTime("Reminder", &reminderMinutes);

  settings.addToggle("Wi-Fi", &wifiOn);
  settings.addToggle("Sound", &sound);
  settings.addNumber("Volume", &volume, 0, 21);
  settings.addChoice("Play mode", playModes, 4, &playMode);
  settings.addMenu("Display", &displayMenu);
  settings.addInfo("Uptime", uptime);

  songs.setRightText(songLength);
  songs.setChecked(isPlaying);
  download.onCancel(cancelDownload);

  mainMenu.addMenu("Songs", &songs);
  mainMenu.addMenu("Alarms", &alarms);
  mainMenu.addMenu("Settings", &settings);
  mainMenu.addAction("Download", startDownload);
  mainMenu.addAction("Power off", askPowerOff);

  knob.begin();
  ui.begin(&mainMenu, &knob);
}

void loop() {
  if (downloading) {
    float f = (millis() - downloadStart) / 6000.0f;
    download.set(f);
    if (f >= 1) {
      downloading = false;
      ui.back();
      ui.alert("Download done", "episode-42.mp3");
    }
  }
  ui.update();
}
