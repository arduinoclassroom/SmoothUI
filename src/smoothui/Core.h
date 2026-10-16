// SmoothUI core: the UI object, pages, transitions, overlays and timers.
// Copyright (c) 2026 Oshan Dissanayaka. MIT License.
#pragma once
#include <Arduino.h>
#include "Display.h"
#include "Theme.h"
#include "Input.h"

class SmoothUI;

// A screen. Menus, lists, pickers and progress screens are pages; open one
// with ui.show(&page) and it slides in, ui.back() slides it out.
class SmoothPage {
 public:
  explicit SmoothPage(const char* title = "") : _title(title) {}
  virtual ~SmoothPage() {}
  const char* title() const { return _title; }
  void setTitle(const char* t) { _title = t; }

  virtual void enter(SmoothUI&) {}                       // opened with show()
  virtual void resume(SmoothUI&) {}                      // shown again after back()
  virtual bool tick(SmoothUI&, uint32_t dtMs) = 0;       // animate; true while moving
  virtual void draw(SmoothUI&, int16_t ox) = 0;          // draw at x offset ox
  virtual void event(SmoothUI&, SmoothEvent e) = 0;

 protected:
  const char* _title;
};

// A countdown (sleep timers, auto-off...). Register it with ui.add() or call
// poll() yourself; `done` runs once when it reaches zero.
class SmoothCountdown {
 public:
  void start(uint32_t ms, void (*done)() = nullptr) {
    _end = millis() + ms;
    _total = ms;
    _running = ms > 0;
    if (done) _done = done;
  }
  void startMinutes(uint32_t minutes, void (*done)() = nullptr) { start(minutes * 60000UL, done); }
  void stop() { _running = false; }
  bool running() const { return _running; }
  uint32_t remainingMs() const {
    if (!_running) return 0;
    int32_t left = (int32_t)(_end - millis());
    return left > 0 ? (uint32_t)left : 0;
  }
  uint32_t remainingMinutes() const { return (remainingMs() + 59999UL) / 60000UL; }   // rounded up
  float progress() const { return _total ? 1.0f - (float)remainingMs() / _total : 0; }
  // "1:05:09" / "4:59"
  void format(char* out, size_t n) const {
    uint32_t s = (remainingMs() + 999) / 1000;
    if (s >= 3600) snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    else snprintf(out, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  }
  void poll() {
    if (_running && (int32_t)(millis() - _end) >= 0) {
      _running = false;
      if (_done) _done();
    }
  }

 private:
  uint32_t _end = 0, _total = 0;
  bool _running = false;
  void (*_done)() = nullptr;
};

struct SmoothMenuItem;
class SmoothNumberPage;
class SmoothChoicePage;
class SmoothTimePage;

class SmoothUI {
 public:
  SmoothDisplay display;
  SmoothTheme theme;

  SmoothUI() {}
  // SMOOTHUI_CUSTOM_DISPLAY: your own Adafruit_GFX display (see examples).
  SmoothUI(Adafruit_GFX* gfx, bool mono, uint8_t* monoBuffer, SmoothDisplay::PushFn push) {
    display.useCustom(gfx, mono, monoBuffer, push);
  }

  // Starts the display. `home` is the first page; `input` an encoder or
  // buttons (or feed events yourself with handle()). False if the display
  // did not answer or there is not enough RAM.
  bool begin(SmoothPage* home = nullptr, SmoothInput* input = nullptr) {
    if (!display.begin()) return false;
    _w = display.width();
    _h = display.height();
    _canvas = new SmoothCanvas(_w, _h);
    if (!_canvas) return false;
    if (display.mono()) {
      if (!display.monoBuffer()) return false;
      _canvas->attachMono(display.monoBuffer(), display.monoFlipped());
      theme.text = theme.textDim = theme.accent = theme.highlight = theme.muted = 1;
      theme.background = theme.track = theme.panel = 0;
      theme.accentText = 0;
      theme.danger = 1;
    } else {
      int16_t rows = (int16_t)smoothui::lo<long>(_h, smoothui::hi<long>(1, (long)SMOOTHUI_BUFFER_BYTES / (_w * 2)));
      while (rows > 0 && !(_strip = (uint16_t*)malloc((size_t)_w * rows * 2))) rows /= 2;
      if (!_strip) return false;
      _canvas->attachColor(_strip, rows);
#if defined(SMOOTHUI_FONT)
      if (!theme.font) theme.font = SMOOTHUI_FONT;
#endif
    }
    layout();
    _input = input;
    _last = millis();
    if (home) show(home);
    return true;
  }

  void setInput(SmoothInput* input) { _input = input; }

  // ---- pages ----
  void show(SmoothPage* page) {
    if (!page || _depth >= SMOOTHUI_MAX_DEPTH) return;
    _leaving = _depth ? _stack[_depth - 1] : nullptr;
    _stack[_depth++] = page;
    page->enter(*this);
    startSlide(_leaving ? 1 : 0);
  }
  void back() {
    if (_depth <= 1) return;
    _leaving = _stack[--_depth];
    _stack[_depth - 1]->resume(*this);
    startSlide(-1);
  }
  void home() {
    while (_depth > 2) --_depth;
    back();
  }
  SmoothPage* current() const { return _depth ? _stack[_depth - 1] : nullptr; }
  uint8_t depth() const { return _depth; }

  // ---- overlays ----
  // A message box; closes after `ms` (0 = until any input) or on any input.
  void alert(const char* title, const char* message = "", uint32_t ms = 2000) {
    copy(_ovTitle, title);
    copy(_ovText, message);
    _ovKind = OvAlert;
    _ovUntil = ms ? millis() + ms : 0;
    _ovAnim = 0;
    redraw();
  }
  // A question with two answers; `done(true)` for yes. The safe answer is
  // highlighted first. danger = the yes button turns red when chosen.
  void confirm(const char* title, const char* message, void (*done)(bool yes), const char* yes = "Yes",
               const char* no = "No", bool danger = false) {
    copy(_ovTitle, title);
    copy(_ovText, message);
    _ovYes = yes;
    _ovNo = no;
    _ovDone = done;
    _ovDanger = danger;
    _ovKind = OvConfirm;
    _ovChoice = false;
    _ovChoiceAnim = 0;
    _ovAnim = 0;
    redraw();
  }
  bool overlayOpen() const { return _ovKind != OvNone; }

  // ---- timers ----
  bool add(SmoothCountdown* t) {
    if (_timerCount >= SMOOTHUI_MAX_TIMERS) return false;
    _timers[_timerCount++] = t;
    return true;
  }

  // ---- loop ----
  void handle(SmoothEvent e) {
    if (e == SmoothEvent::None) return;
    redraw();
    if (_ovKind != OvNone) { overlayEvent(e); return; }
    if (_slide < 1.0f) _slide = 1.0f;   // a key during a slide finishes it
    if (SmoothPage* p = current()) p->event(*this, e);
  }
  // Call from loop(): reads the input, runs timers, draws when needed.
  void update() {
    if (_input) {
      for (uint8_t i = 0; i < 8; ++i) {
        SmoothEvent e = _input->read();
        if (e == SmoothEvent::None) break;
        handle(e);
      }
    }
    for (uint8_t i = 0; i < _timerCount; ++i) _timers[i]->poll();
    if (_ovKind == OvAlert && _ovUntil && (int32_t)(millis() - _ovUntil) >= 0) closeOverlay();
    uint32_t now = millis();
    uint32_t dt = now - _last;
    if (dt < 1000 / SMOOTHUI_FPS) return;
    // Still screens are redrawn twice a second (clocks, countdowns).
    if (!_moving && !_dirty && dt < 500) return;
    _last = now;
    _frameMs = now;
    if (dt > 100) dt = 100;
    _moving = tick(dt);
    _dirty = false;
    render();
  }
  void redraw() { _dirty = true; }
  // Call after changing theme.font / textSize / rowHeight once begin() ran.
  void refreshLayout() {
    if (_canvas) layout();
    redraw();
  }

  // ---- drawing helpers for pages ----
  SmoothCanvas& gfx() { return *_canvas; }
  int16_t width() const { return _w; }
  int16_t height() const { return _h; }
  bool mono() const { return display.mono(); }
  int16_t lineHeight() const { return _lineH; }
  int16_t rowHeight() const { return _rowH; }
  int16_t titleHeight() const { return theme.showTitle ? _titleH : 0; }
  uint32_t frameMillis() const { return _frameMs; }
  // Scale for big numbers (sliders, time wheels) in an area `areaH` tall.
  uint8_t bigScale(int16_t areaH) const {
    if (theme.font) return _h >= 200 && areaH >= _lineH * 4 ? 2 : 1;
    return areaH >= _lineH * 4 ? 2 : 1;
  }

  int16_t textWidth(const char* s) {
    int16_t x1, y1;
    uint16_t w, h;
    applyFont(1);
    _canvas->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
  }
  int16_t textWidth(const char* s, uint8_t scale) {
    int16_t x1, y1;
    uint16_t w, h;
    applyFont(scale);
    _canvas->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
  }
  // Text with its top-left corner at (x, y). scale: 1 normal, 2 large.
  void text(int16_t x, int16_t y, const char* s, uint16_t color, uint8_t scale = 1) {
    applyFont(scale);
    _canvas->setTextColor(color);
    _canvas->setTextWrap(false);
    _canvas->setCursor(x, y + (theme.font ? _ascent * scale : 0));
    _canvas->print(s);
  }
  void textCentered(int16_t cx, int16_t y, const char* s, uint16_t color, uint8_t scale = 1) {
    text(cx - textWidth(s, scale) / 2, y, s, color, scale);
  }
  void textRight(int16_t right, int16_t y, const char* s, uint16_t color, uint8_t scale = 1) {
    text(right - textWidth(s, scale), y, s, color, scale);
  }
  // Text clipped to `w` pixels; scrolls when longer (used for the selected row).
  void marquee(int16_t x, int16_t y, int16_t w, const char* s, uint16_t color, bool scroll) {
    int16_t tw = textWidth(s);
    SmoothCanvas::Clip saved = _canvas->clip();
    _canvas->narrowClip(x, 0, w, _h);
    if (tw <= w || !scroll) {
      text(x, y, s, color);
    } else {
      const uint32_t hold = 1200, stepMs = 30;
      int16_t span = tw + 24;
      uint32_t t = (_frameMs - _marqueeSince) % (hold + (uint32_t)span * stepMs);
      int16_t off = t < hold ? 0 : (int16_t)((t - hold) / stepMs);
      text(x - off, y, s, color);
      text(x - off + span, y, s, color);
    }
    _canvas->restoreClip(saved);
  }
  bool marqueeNeeded(int16_t w, const char* s) { return textWidth(s) > w; }
  void restartMarquee() { _marqueeSince = millis(); }

  // A rounded box (filled or outlined).
  void box(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled = true) {
    int16_t r = smoothui::lo<int16_t>(mono() ? 2 : 5, h / 2);
    if (filled) _canvas->fillRoundRect(x, y, w, h, r, color);
    else _canvas->drawRoundRect(x, y, w, h, r, color);
  }
  // A progress bar; `fill` 0..1. Colour screens get a gradient and a knob.
  void bar(int16_t x, int16_t y, int16_t w, int16_t h, float fill, bool knob = true) {
    fill = constrain(fill, 0.0f, 1.0f);
    int16_t inner = w - 4, px = (int16_t)(inner * fill + 0.5f);
    if (mono()) {
      _canvas->drawRoundRect(x, y, w, h, smoothui::lo<int16_t>(2, h / 2), 1);
      if (px > 0) _canvas->fillRect(x + 2, y + 2, px, h - 4, 1);
      return;
    }
    box(x, y, w, h, theme.track);
    _canvas->drawRoundRect(x, y, w, h, smoothui::lo<int16_t>(5, h / 2), theme.accent);
    for (int16_t i = 0; i < px; ++i)
      _canvas->drawFastVLine(x + 2 + i, y + 2, h - 4, SmoothColor::mix(theme.accent, theme.highlight, (uint8_t)(i * 255 / smoothui::hi<int16_t>(1, inner))));
    if (knob && px > 0) {
      int16_t kx = x + 2 + px, ky = y + h / 2, r = h / 2 + 1;
      _canvas->fillCircle(kx, ky, r, theme.accent);
      _canvas->fillCircle(kx, ky, smoothui::hi<int16_t>(1, r - 2), theme.highlight);
    }
  }
  // An on/off switch; `pos` 0 (off) .. 1 (on), animated by the caller.
  void toggleSwitch(int16_t x, int16_t y, int16_t w, int16_t h, float pos) {
    int16_t r = h / 2;
    bool on = pos > 0.5f;
    if (mono()) {
      _canvas->drawRoundRect(x, y, w, h, r, 1);
      if (on) _canvas->fillRoundRect(x, y, w, h, r, 1);
      int16_t kx = x + r + (int16_t)((w - 2 * r) * pos);
      _canvas->fillCircle(kx, y + r, smoothui::hi<int16_t>(1, r - 2), on ? 0 : 1);
      return;
    }
    _canvas->fillRoundRect(x, y, w, h, r, SmoothColor::mix(theme.track, theme.accent, (uint8_t)(pos * 255)));
    int16_t kx = x + r + (int16_t)((w - 2 * r) * pos);
    _canvas->fillCircle(kx, y + r, smoothui::hi<int16_t>(1, r - 2), on ? 0xFFFF : theme.muted);
  }
  // A tick mark (selected option).
  void checkMark(int16_t x, int16_t y, int16_t size, uint16_t color) {
    _canvas->drawLine(x, y + size / 2, x + size / 3, y + size, color);
    _canvas->drawLine(x + size / 3, y + size, x + size, y, color);
    _canvas->drawLine(x, y + size / 2 + 1, x + size / 3, y + size + 1, color);
    _canvas->drawLine(x + size / 3, y + size + 1, x + size, y + 1, color);
  }
  // A ">" chevron.
  void chevron(int16_t x, int16_t y, int16_t size, uint16_t color) {
    _canvas->drawLine(x, y, x + size / 2, y + size / 2, color);
    _canvas->drawLine(x + size / 2, y + size / 2, x, y + size, color);
  }
  // Page title with a rule under it.
  void titleBar(int16_t ox, const char* title) {
    if (!theme.showTitle) return;
    text(ox + 3, 1, title, theme.text);
    _canvas->drawFastHLine(ox + 1, _titleH - 2, _w - 2, theme.accent);
  }

  // Opens a value editor for a menu item (used by SmoothMenu).
  void editNumber(SmoothMenuItem* item);
  void editChoice(SmoothMenuItem* item);
  void editTime(SmoothMenuItem* item);

 private:
  enum OvKind : uint8_t { OvNone, OvAlert, OvConfirm };
  SmoothCanvas* _canvas = nullptr;
  uint16_t* _strip = nullptr;
  SmoothInput* _input = nullptr;
  int16_t _w = 0, _h = 0, _lineH = 8, _rowH = 13, _titleH = 12, _ascent = 0;
  SmoothPage* _stack[SMOOTHUI_MAX_DEPTH];
  uint8_t _depth = 0;
  SmoothPage* _leaving = nullptr;
  float _slide = 1.0f;
  int8_t _slideDir = 0;
  uint32_t _last = 0, _frameMs = 0, _marqueeSince = 0;
  bool _moving = true, _dirty = true;
  SmoothCountdown* _timers[SMOOTHUI_MAX_TIMERS];
  uint8_t _timerCount = 0;
  // overlay
  OvKind _ovKind = OvNone;
  char _ovTitle[32] = "", _ovText[48] = "";
  const char *_ovYes = "Yes", *_ovNo = "No";
  void (*_ovDone)(bool) = nullptr;
  bool _ovDanger = false, _ovChoice = false;
  float _ovAnim = 0, _ovChoiceAnim = 0;
  uint32_t _ovUntil = 0;
  SmoothNumberPage* _numberPage = nullptr;
  SmoothChoicePage* _choicePage = nullptr;
  SmoothTimePage* _timePage = nullptr;

  template <size_t N>
  static void copy(char (&dst)[N], const char* src) {
    strncpy(dst, src ? src : "", N - 1);
    dst[N - 1] = '\0';
  }

  void applyFont(uint8_t scale) {
    _canvas->setFont(theme.font);
    _canvas->setTextSize(theme.font ? scale : theme.textSize * scale);
  }

  void layout() {
    if (theme.font) {
      int16_t x1, y1;
      uint16_t w, h;
      _canvas->setFont(theme.font);
      _canvas->setTextSize(1);
      _canvas->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
      _ascent = -y1;
      _lineH = (int16_t)h;
    } else {
      _ascent = 0;
      _lineH = 8 * theme.textSize;
    }
    _rowH = theme.rowHeight ? theme.rowHeight : _lineH + smoothui::hi<int16_t>(5, _lineH / 2);
    _titleH = theme.titleHeight ? theme.titleHeight : _lineH + 4;
    // Fit whole rows under the title on short OLEDs (128x64: 12 + 4 x 13).
    if (!theme.rowHeight && _h <= 64) _rowH = (_h - _titleH) / smoothui::hi<int16_t>(1, (_h - _titleH) / _rowH);
  }

  void startSlide(int8_t dir) {
    _slideDir = dir;
    _slide = dir ? 0.0f : 1.0f;
    _marqueeSince = millis();
    redraw();
  }

  bool tick(uint32_t dt) {
    bool moving = false;
    if (_slide < 1.0f) {
      SmoothAnim::approach(_slide, 1.0f, theme.slideSpeed, dt);
      if (_slide > 0.995f) _slide = 1.0f;
      moving = true;
      if (_leaving) _leaving->tick(*this, dt);
    }
    if (SmoothPage* p = current()) moving |= p->tick(*this, dt);
    if (_ovKind != OvNone) {
      moving |= SmoothAnim::approach(_ovAnim, 1.0f, theme.slideSpeed, dt);
      moving |= SmoothAnim::approach(_ovChoiceAnim, _ovChoice ? 1.0f : 0.0f, theme.selectSpeed, dt);
    }
    return moving;
  }

  void scene() {
    SmoothPage* top = current();
    if (_slide < 1.0f && _leaving && top) {
      // Opening slides the new page in from the right; closing from the left.
      int16_t dx = (int16_t)((1.0f - _slide) * _w);
      if (_slideDir > 0) {
        _leaving->draw(*this, dx - _w);
        top->draw(*this, dx);
      } else {
        _leaving->draw(*this, _w - dx);
        top->draw(*this, -dx);
      }
    } else if (top) {
      top->draw(*this, 0);
    }
    if (_ovKind != OvNone) drawOverlay();
  }

  void render() {
    if (!_canvas) return;
    if (display.mono()) {
      _canvas->fillScreen(theme.background);
      scene();
      display.flush();
      return;
    }
    int16_t rows = _canvas->bandRows();
    for (int16_t y = 0; y < _h; y += rows) {
      int16_t n = smoothui::lo<int16_t>(rows, _h - y);
      _canvas->setBand(y);
      _canvas->fillScreen(theme.background);
      scene();
      display.pushBand(_strip, y, _w, n);
    }
  }

  void closeOverlay() {
    _ovKind = OvNone;
    redraw();
  }

  void overlayEvent(SmoothEvent e) {
    if (_ovKind == OvAlert) { closeOverlay(); return; }
    switch (e) {
      case SmoothEvent::Next: _ovChoice = true; break;
      case SmoothEvent::Prev: _ovChoice = false; break;
      case SmoothEvent::Select:
      case SmoothEvent::Back: {
        bool yes = e == SmoothEvent::Select && _ovChoice;
        void (*done)(bool) = _ovDone;
        closeOverlay();
        if (done) done(yes);
        break;
      }
      default: break;
    }
  }

  void drawOverlay() {
    SmoothCanvas& g = *_canvas;
    int16_t bw = _w - (_w >= 200 ? _w / 5 : 8);
    // Title, message, buttons; the message is left out when it doesn't fit.
    const int16_t btnH = _lineH + 6;
    bool showText = _ovText[0] != 0;
    int16_t bh = 5 + _lineH + 3 + (showText ? _lineH + 3 : 0) + (_ovKind == OvConfirm ? btnH + 4 : 0) + 3;
    if (bh > _h - 2 && showText) {
      showText = _ovKind != OvConfirm;
      bh = _ovKind == OvConfirm ? 5 + _lineH + 3 + btnH + 4 + 3 : bh;
    }
    if (bh > _h - 2) bh = _h - 2;
    // Grows from the middle.
    int16_t gh = smoothui::hi<int16_t>(4, (int16_t)(bh * _ovAnim));
    int16_t bx = (_w - bw) / 2, by = (_h - gh) / 2;
    if (mono()) {
      g.fillRect(bx, by, bw, gh, 0);
      g.drawRoundRect(bx, by, bw, gh, 3, 1);
    } else {
      box(bx, by, bw, gh, theme.panel);
      g.drawRoundRect(bx, by, bw, gh, 7, _ovDanger ? theme.danger : theme.highlight);
    }
    if (_ovAnim < 0.85f) return;
    int16_t y = by + (_h <= 40 ? 3 : 5);
    textCentered(_w / 2, y, _ovTitle, theme.text);
    y += _lineH + 3;
    if (showText) marquee(bx + 6, y, bw - 12, _ovText, theme.textDim, true);
    if (_ovKind != OvConfirm) return;
    // Two buttons sized to their labels; the highlight glides between them.
    int16_t pad = mono() ? 6 : 10, gap = 8;
    int16_t nw = smoothui::hi<int16_t>(textWidth(_ovNo) + 2 * pad, bw / 3), yw = smoothui::hi<int16_t>(textWidth(_ovYes) + 2 * pad, bw / 3);
    int16_t btnY = by + bh - btnH - 4;
    int16_t nx = (_w - nw - gap - yw) / 2, yx = nx + nw + gap;
    int16_t hx = nx + (int16_t)((yx - nx) * _ovChoiceAnim), hw = nw + (int16_t)((yw - nw) * _ovChoiceAnim);
    box(hx, btnY, hw, btnH, _ovChoice && _ovDanger && !mono() ? theme.danger : theme.accent);
    if (!mono()) {
      g.drawRoundRect(nx, btnY, nw, btnH, 5, theme.muted);
      g.drawRoundRect(yx, btnY, yw, btnH, 5, theme.muted);
    } else {
      g.drawRoundRect(_ovChoice ? nx : yx, btnY, _ovChoice ? nw : yw, btnH, 2, 1);
    }
    bool onNo = _ovChoiceAnim < 0.5f;
    textCentered(nx + nw / 2, btnY + 3, _ovNo, onNo ? theme.accentText : theme.text);
    textCentered(yx + yw / 2, btnY + 3, _ovYes, !onNo ? theme.accentText : theme.text);
  }
};
