// SmoothUI widgets: menus, lists, value editors, time picker, progress.
// Copyright (c) 2026 Oshan Dissanayaka. MIT License.
#pragma once
#include "Core.h"

// =============================================================================
// SmoothListPage - the animated list every menu and list is built on:
// rows unfold from the title when it opens, the selection bar glides and
// stretches to the row's text, the list scrolls smoothly, long text scrolls.
// =============================================================================
class SmoothListPage : public SmoothPage {
 public:
  using SmoothPage::SmoothPage;
  int selected() const { return _sel; }
  void setSelected(int i) { _sel = i; }

  void enter(SmoothUI& ui) override {
    _backRow = SMOOTHUI_BACK_ROW && ui.depth() > 1;
    clampSelection();
    _unfold = 0;
    _top = _topTarget = 0;
    keepVisible(ui);
    _top = _topTarget;
    _selRow = (float)_sel;
    _selW = 0;
    _thumb = 0;
  }
  void resume(SmoothUI& ui) override {
    clampSelection();
    keepVisible(ui);
  }

  bool tick(SmoothUI& ui, uint32_t dt) override {
    clampSelection();
    keepVisible(ui);
    bool moving = false;
    moving |= SmoothAnim::approach(_unfold, 1.0f, ui.theme.listSpeed, dt);
    moving |= SmoothAnim::approach(_top, _topTarget, ui.theme.selectSpeed, dt);
    moving |= SmoothAnim::approach(_selRow, (float)_sel, ui.theme.selectSpeed, dt);
    char buf[48];
    const char* label = labelFor(_sel, buf, sizeof(buf));
    int16_t avail = labelSpace(ui, _sel);
    int16_t want = smoothui::lo<int16_t>(ui.textWidth(label), avail) + 8;
    moving |= SmoothAnim::approach(_selW, want, ui.theme.selectSpeed, dt);
    int total = rows(), visible = visibleRows(ui);
    if (total > visible) {
      float target = (float)(ui.height() - ui.titleHeight()) * _topTarget / total;
      moving |= SmoothAnim::approach(_thumb, target, ui.theme.barSpeed, dt);
    }
    if (ui.marqueeNeeded(avail, label)) moving = true;   // keeps the long title scrolling
    moving |= tickItems(ui, dt);
    return moving;
  }

  void draw(SmoothUI& ui, int16_t ox) override {
    SmoothCanvas& g = ui.gfx();
    ui.titleBar(ox, _title);
    const int16_t top = ui.titleHeight(), rowH = ui.rowHeight(), w = ui.width(), h = ui.height();
    const int total = rows();
    const int16_t textY = (rowH - ui.lineHeight()) / 2;
    // Whole rows only: no sliver of the next row under the last one.
    const int16_t listH = smoothui::lo<int16_t>(h - top, visibleRows(ui) * rowH);
    g.setClip(ox, top, w, listH);
    // Selection bar.
    float selY = top + (_selRow - _top) * rowH * _unfold;
    if (!ui.mono()) ui.box(ox + 2, (int16_t)selY, (int16_t)_selW, rowH - 1, ui.theme.accent);
    // Rows (only those that can be on screen).
    int first = smoothui::hi(0, (int)_top - 1), last = smoothui::lo(total - 1, (int)_top + visibleRows(ui) + 1);
    for (int i = first; i <= last; ++i) {
      int16_t y = (int16_t)(top + (i - _top) * rowH * _unfold + 0.5f);
      if (y >= h || y + rowH <= top) continue;
      bool sel = i == _sel;
      bool onBar = !ui.mono() && fabsf(i - _selRow) < 0.5f;
      char buf[48];
      const char* label = labelFor(i, buf, sizeof(buf));
      int16_t avail = labelSpace(ui, i);
      uint16_t color = onBar ? ui.theme.accentText : (isBackRow(i) ? ui.theme.textDim : ui.theme.text);
      ui.marquee(ox + 6, y + textY, avail, label, color, sel);
      if (!isBackRow(i)) drawRight(ui, i, ox + w - rightEdgeInset(ui), y, rowH, sel);
    }
    // Mono: the bar inverts what is under it, like a hardware cursor.
    if (ui.mono()) invert(g, ox + 2, (int16_t)selY, (int16_t)_selW, rowH - 1);
    g.clearClip();
    // Scrollbar.
    if (total > visibleRows(ui)) {
      int16_t x = ox + w - 3, areaH = h - top;
      for (int16_t yy = top; yy < h; yy += 2) g.drawPixel(x, yy, ui.theme.muted);
      int16_t thumbH = smoothui::hi<int16_t>(4, areaH * visibleRows(ui) / total);
      g.fillRect(x - 1, top + (int16_t)_thumb, 3, thumbH, ui.theme.highlight);
    }
  }

  void event(SmoothUI& ui, SmoothEvent e) override {
    int total = rows();
    switch (e) {
      case SmoothEvent::Next:
        if (_sel < total - 1) { ++_sel; ui.restartMarquee(); }
        break;
      case SmoothEvent::Prev:
        if (_sel > 0) { --_sel; ui.restartMarquee(); }
        break;
      case SmoothEvent::Select:
        if (isBackRow(_sel)) ui.back();
        else if (_sel < total) activate(ui, _sel);
        break;
      case SmoothEvent::Back:
        if (!isBackRow(_sel) && hold(ui, _sel)) break;
        ui.back();
        break;
      default: break;
    }
  }

 protected:
  virtual int itemCount() = 0;
  virtual const char* itemLabel(int i, char* buf, size_t n) = 0;
  virtual void activate(SmoothUI& ui, int i) = 0;
  // Long press on a row. Return true if handled; false goes back.
  virtual bool hold(SmoothUI&, int) { return false; }
  // Width kept free on the right for row i's value / switch / chevron.
  virtual int16_t rightWidth(SmoothUI&, int) { return 0; }
  // Draws row i's right-hand part ending at x = right.
  virtual void drawRight(SmoothUI&, int, int16_t, int16_t, int16_t, bool) {}
  virtual bool tickItems(SmoothUI&, uint32_t) { return false; }

  bool isBackRow(int i) { return _backRow && i == itemCount(); }
  int rows() { return itemCount() + (_backRow ? 1 : 0); }

 private:
  int _sel = 0;
  float _top = 0, _topTarget = 0, _selRow = 0, _selW = 0, _unfold = 1, _thumb = 0;
  bool _backRow = false;

  const char* labelFor(int i, char* buf, size_t n) {
    if (isBackRow(i)) return "Back";
    if (i < 0 || i >= itemCount()) return "";
    return itemLabel(i, buf, n);
  }
  int16_t rightEdgeInset(SmoothUI& ui) { return rows() > visibleRows(ui) ? 8 : 5; }
  int16_t labelSpace(SmoothUI& ui, int i) {
    int16_t r = (i >= 0 && i < itemCount()) ? rightWidth(ui, i) : 0;
    return ui.width() - 6 - rightEdgeInset(ui) - (r ? r + 6 : 0);
  }
  int visibleRows(SmoothUI& ui) {
    return smoothui::hi(1, (ui.height() - ui.titleHeight()) / smoothui::hi<int16_t>(1, ui.rowHeight()));
  }
  void clampSelection() {
    int total = rows();
    if (_sel >= total) _sel = total - 1;
    if (_sel < 0) _sel = 0;
  }
  void keepVisible(SmoothUI& ui) {
    int visible = visibleRows(ui), total = rows();
    if (_sel < _topTarget) _topTarget = _sel;
    if (_sel >= _topTarget + visible) _topTarget = _sel - visible + 1;
    _topTarget = constrain(_topTarget, 0.0f, (float)smoothui::hi(0, total - visible));
  }
  static void invert(SmoothCanvas& g, int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t yy = y; yy < y + h; ++yy)
      for (int16_t xx = x; xx < x + w; ++xx) g.invertPixel(xx, yy);
  }
};

// =============================================================================
// SmoothMenu - a list of items: actions, sub-menus, switches, numbers,
// choices, times and read-only info. Values live in your variables.
// =============================================================================
enum class SmoothItemKind : uint8_t { Action, Page, Toggle, Number, Choice, Time, Info };

struct SmoothMenuItem {
  const char* label = "";
  SmoothItemKind kind = SmoothItemKind::Action;
  void (*onAction)() = nullptr;
  void (*onBool)(bool) = nullptr;
  void (*onInt)(int) = nullptr;
  void (*onTime)(int minutes, bool enabled) = nullptr;
  SmoothPage* page = nullptr;
  bool* flag = nullptr;
  int* value = nullptr;
  int minValue = 0, maxValue = 100, step = 1;
  const char* unit = "";
  const char* const* options = nullptr;
  uint8_t optionCount = 0;
  const char* info = nullptr;
  const char* (*infoFn)() = nullptr;
  float anim = 0;   // switch knob position
};

// "7:05 AM" / "07:05"
inline void smoothFormatTime(int minutes, char* out, size_t n, bool h24 = SMOOTHUI_24H) {
  int h = (minutes / 60) % 24, m = minutes % 60;
  if (h24) snprintf(out, n, "%02d:%02d", h, m);
  else snprintf(out, n, "%d:%02d %s", h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
}

class SmoothMenu : public SmoothListPage {
 public:
  explicit SmoothMenu(const char* title = "Menu") : SmoothListPage(title) {}

  // Runs fn when chosen.
  int addAction(const char* label, void (*fn)()) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Action);
    if (it) it->onAction = fn;
    return _count - 1;
  }
  // Opens another page (a sub-menu, list, progress screen...).
  int addMenu(const char* label, SmoothPage* page) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Page);
    if (it) it->page = page;
    return _count - 1;
  }
  // An on/off switch bound to *value.
  int addToggle(const char* label, bool* value, void (*changed)(bool) = nullptr) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Toggle);
    if (it) { it->flag = value; it->onBool = changed; it->anim = *value ? 1 : 0; }
    return _count - 1;
  }
  // A number from min to max (opens a slider page). Called live while turning.
  int addNumber(const char* label, int* value, int minValue, int maxValue, int step = 1, const char* unit = "",
                void (*changed)(int) = nullptr) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Number);
    if (it) {
      it->value = value; it->minValue = minValue; it->maxValue = maxValue;
      it->step = step ? step : 1; it->unit = unit ? unit : ""; it->onInt = changed;
    }
    return _count - 1;
  }
  // One of `count` options (opens a list with a tick). *index = the choice.
  int addChoice(const char* label, const char* const* options, uint8_t count, int* index, void (*changed)(int) = nullptr) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Choice);
    if (it) { it->options = options; it->optionCount = count; it->value = index; it->onInt = changed; }
    return _count - 1;
  }
  // A time of day in minutes after midnight (opens the wheel picker). With
  // `enabled`, the picker has an on/off switch (alarm style: changing the
  // time switches it off, switching it on saves); without, a Save button.
  int addTime(const char* label, int* minutes, bool* enabled = nullptr, void (*saved)(int minutes, bool enabled) = nullptr) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Time);
    if (it) { it->value = minutes; it->flag = enabled; it->onTime = saved; }
    return _count - 1;
  }
  // Read-only text on the right (fixed, or from a function each frame).
  int addInfo(const char* label, const char* value) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Info);
    if (it) it->info = value;
    return _count - 1;
  }
  int addInfo(const char* label, const char* (*value)()) {
    SmoothMenuItem* it = add(label, SmoothItemKind::Info);
    if (it) it->infoFn = value;
    return _count - 1;
  }

  SmoothMenuItem& item(int i) { return _items[constrain(i, 0, SMOOTHUI_MAX_ITEMS - 1)]; }
  void setLabel(int i, const char* label) { item(i).label = label; }
  int count() const { return _count; }
  void clear() { _count = 0; }

 protected:
  int itemCount() override { return _count; }
  const char* itemLabel(int i, char*, size_t) override { return _items[i].label; }

  void activate(SmoothUI& ui, int i) override {
    SmoothMenuItem& it = _items[i];
    switch (it.kind) {
      case SmoothItemKind::Action: if (it.onAction) it.onAction(); break;
      case SmoothItemKind::Page: if (it.page) ui.show(it.page); break;
      case SmoothItemKind::Toggle:
        if (it.flag) {
          *it.flag = !*it.flag;
          if (it.onBool) it.onBool(*it.flag);
        }
        break;
      case SmoothItemKind::Number: ui.editNumber(&it); break;
      case SmoothItemKind::Choice: ui.editChoice(&it); break;
      case SmoothItemKind::Time: ui.editTime(&it); break;
      default: break;
    }
  }

  int16_t rightWidth(SmoothUI& ui, int i) override {
    SmoothMenuItem& it = _items[i];
    if (it.kind == SmoothItemKind::Toggle) return switchW(ui);
    if (it.kind == SmoothItemKind::Page) return ui.lineHeight() / 2 + 2;
    if (it.kind == SmoothItemKind::Action) return 0;
    char buf[24];
    return ui.textWidth(valueText(it, buf, sizeof(buf)));
  }

  void drawRight(SmoothUI& ui, int i, int16_t right, int16_t y, int16_t rowH, bool) override {
    SmoothMenuItem& it = _items[i];
    int16_t lh = ui.lineHeight(), ty = y + (rowH - lh) / 2;
    switch (it.kind) {
      case SmoothItemKind::Toggle: {
        int16_t sw = switchW(ui), sh = smoothui::lo<int16_t>(rowH - 3, lh + 3);
        ui.toggleSwitch(right - sw, y + (rowH - sh) / 2, sw, sh, it.anim);
        break;
      }
      case SmoothItemKind::Page: {
        int16_t s = lh / 2;
        ui.chevron(right - s / 2 - 1, ty + (lh - s) / 2, s, ui.theme.textDim);
        break;
      }
      case SmoothItemKind::Action: break;
      default: {
        char buf[24];
        ui.textRight(right, ty, valueText(it, buf, sizeof(buf)), ui.theme.textDim);
      }
    }
  }

  bool tickItems(SmoothUI& ui, uint32_t dt) override {
    bool moving = false;
    for (int i = 0; i < _count; ++i)
      if (_items[i].kind == SmoothItemKind::Toggle && _items[i].flag)
        moving |= SmoothAnim::approach(_items[i].anim, *_items[i].flag ? 1.0f : 0.0f, ui.theme.barSpeed, dt);
    return moving;
  }

 private:
  SmoothMenuItem _items[SMOOTHUI_MAX_ITEMS];
  int _count = 0;

  SmoothMenuItem* add(const char* label, SmoothItemKind kind) {
    if (_count >= SMOOTHUI_MAX_ITEMS) return nullptr;
    SmoothMenuItem& it = _items[_count++];
    it = SmoothMenuItem();
    it.label = label;
    it.kind = kind;
    return &it;
  }
  static int16_t switchW(SmoothUI& ui) { return (ui.lineHeight() + 3) * 9 / 5; }
  static const char* valueText(SmoothMenuItem& it, char* buf, size_t n) {
    switch (it.kind) {
      case SmoothItemKind::Number: snprintf(buf, n, "%d%s", it.value ? *it.value : 0, it.unit); return buf;
      case SmoothItemKind::Choice:
        if (it.value && *it.value >= 0 && *it.value < it.optionCount) return it.options[*it.value];
        return "";
      case SmoothItemKind::Time:
        if (it.flag && !*it.flag) return "Off";
        smoothFormatTime(it.value ? *it.value : 0, buf, n);
        return buf;
      case SmoothItemKind::Info: return it.infoFn ? it.infoFn() : (it.info ? it.info : "");
      default: return "";
    }
  }
};

// =============================================================================
// SmoothList - a list filled by your functions, for any number of entries
// (files, songs, Wi-Fi networks...) without storing them in the menu.
// =============================================================================
class SmoothList : public SmoothListPage {
 public:
  typedef int (*CountFn)();
  typedef const char* (*LabelFn)(int i, char* buf, size_t n);
  typedef void (*IndexFn)(int i);

  // onHold (long press on an entry, e.g. "delete?") is optional; without it a
  // long press goes back.
  SmoothList(const char* title, CountFn count, LabelFn label, IndexFn onSelect, IndexFn onHold = nullptr)
      : SmoothListPage(title), _count(count), _label(label), _select(onSelect), _hold(onHold) {}
  // Optional text on the right of each entry (size, duration...).
  void setRightText(LabelFn right) { _right = right; }
  // Optional tick on entries (the playing song, the chosen network...).
  void setChecked(bool (*checked)(int i)) { _checked = checked; }

 protected:
  int itemCount() override { return _count ? _count() : 0; }
  const char* itemLabel(int i, char* buf, size_t n) override { return _label ? _label(i, buf, n) : ""; }
  void activate(SmoothUI&, int i) override { if (_select) _select(i); }
  bool hold(SmoothUI&, int i) override {
    if (!_hold) return false;
    _hold(i);
    return true;
  }
  int16_t rightWidth(SmoothUI& ui, int i) override {
    int16_t w = 0;
    if (_checked && _checked(i)) w = ui.lineHeight() + 2;
    if (_right) {
      char buf[24];
      w += ui.textWidth(_right(i, buf, sizeof(buf))) + (w ? 4 : 0);
    }
    return w;
  }
  void drawRight(SmoothUI& ui, int i, int16_t right, int16_t y, int16_t rowH, bool) override {
    int16_t lh = ui.lineHeight(), ty = y + (rowH - lh) / 2;
    if (_checked && _checked(i)) {
      ui.checkMark(right - lh, ty + 1, lh - 2, ui.theme.highlight);
      right -= lh + 4;
    }
    if (_right) {
      char buf[24];
      ui.textRight(right, ty, _right(i, buf, sizeof(buf)), ui.theme.textDim);
    }
  }

 private:
  CountFn _count;
  LabelFn _label, _right = nullptr;
  IndexFn _select, _hold;
  bool (*_checked)(int) = nullptr;
};

// =============================================================================
// Editors opened by menu items (one of each, shared).
// =============================================================================
class SmoothNumberPage : public SmoothPage {
 public:
  SmoothMenuItem* item = nullptr;
  void enter(SmoothUI&) override {
    _title = item ? item->label : "";
    _fill = fraction();
  }
  bool tick(SmoothUI& ui, uint32_t dt) override { return SmoothAnim::approach(_fill, fraction(), ui.theme.barSpeed, dt); }
  void draw(SmoothUI& ui, int16_t ox) override {
    if (!item || !item->value) return;
    ui.titleBar(ox, _title);
    int16_t top = ui.titleHeight(), h = ui.height() - top, w = ui.width();
    uint8_t scale = ui.bigScale(h);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d%s", *item->value, item->unit);
    int16_t barH = smoothui::hi<int16_t>(6, ui.lineHeight() / 2 + 3);
    int16_t blockH = ui.lineHeight() * scale + 6 + barH;
    int16_t y = top + (h - blockH) / 2;
    ui.textCentered(ox + w / 2, y, buf, ui.theme.text, scale);
    ui.bar(ox + 8, y + ui.lineHeight() * scale + 6, w - 16, barH, _fill);
  }
  void event(SmoothUI& ui, SmoothEvent e) override {
    if (!item || !item->value) { ui.back(); return; }
    int v = *item->value;
    if (e == SmoothEvent::Next) v = smoothui::lo(item->maxValue, v + item->step);
    else if (e == SmoothEvent::Prev) v = smoothui::hi(item->minValue, v - item->step);
    else { ui.back(); return; }
    if (v != *item->value) {
      *item->value = v;
      if (item->onInt) item->onInt(v);
    }
  }

 private:
  float _fill = 0;
  float fraction() {
    if (!item || !item->value || item->maxValue == item->minValue) return 0;
    return (float)(*item->value - item->minValue) / (item->maxValue - item->minValue);
  }
};

class SmoothChoicePage : public SmoothListPage {
 public:
  SmoothMenuItem* item = nullptr;
  SmoothChoicePage() : SmoothListPage("") {}
  void enter(SmoothUI& ui) override {
    _title = item ? item->label : "";
    if (item && item->value) setSelected(*item->value);
    SmoothListPage::enter(ui);
  }

 protected:
  int itemCount() override { return item ? item->optionCount : 0; }
  const char* itemLabel(int i, char*, size_t) override { return item->options[i]; }
  void activate(SmoothUI& ui, int i) override {
    if (!item || !item->value) return;
    *item->value = i;
    if (item->onInt) item->onInt(i);
    ui.redraw();
  }
  int16_t rightWidth(SmoothUI& ui, int i) override { return item && item->value && *item->value == i ? ui.lineHeight() : 0; }
  void drawRight(SmoothUI& ui, int i, int16_t right, int16_t y, int16_t rowH, bool) override {
    if (!item || !item->value || *item->value != i) return;
    int16_t lh = ui.lineHeight();
    ui.checkMark(right - lh + 1, y + (rowH - lh) / 2 + 1, lh - 2, ui.theme.highlight);
  }
};

// Hour : minute (AM/PM) wheels, plus an on/off switch or a Save button.
class SmoothTimePage : public SmoothPage {
 public:
  SmoothMenuItem* item = nullptr;
  void enter(SmoothUI&) override {
    _title = item ? item->label : "";
    _minutes = item && item->value ? constrain(*item->value, 0, 1439) : 0;
    _focus = 0;
    _editing = false;
    for (float& a : _anim) a = 0;
  }
  bool tick(SmoothUI& ui, uint32_t dt) override {
    bool moving = false;
    for (float& a : _anim) moving |= SmoothAnim::approach(a, 0.0f, ui.theme.selectSpeed, dt);
    moving |= SmoothAnim::approach(_switchPos, isOn() ? 1.0f : 0.0f, ui.theme.barSpeed, dt);
    return moving;
  }

  void draw(SmoothUI& ui, int16_t ox) override {
    SmoothCanvas& g = ui.gfx();
    ui.titleBar(ox, _title);
    const int16_t w = ui.width(), lh = ui.lineHeight();
    const int16_t top = ui.titleHeight(), hintH = ui.height() >= 64 ? lh + 3 : 0;
    const int16_t areaH = ui.height() - top - hintH;
    const uint8_t big = ui.bigScale(areaH);
    const int16_t bandH = lh * big + 4, mid = top + areaH / 2, step = lh + 4;
    const bool neighbours = areaH >= bandH + 2 * step - 4;
    const int parts = wheelCount() + 1;   // + switch / Save
    // Column centres: wheels share the left 3/4, the control the rest.
    int16_t controlW = item && item->flag ? (lh + 3) * 9 / 5 : ui.textWidth("Save") + 10;
    int16_t wheelsW = w - controlW - 12;
    int16_t colW = wheelsW / wheelCount();
    // Band behind the wheels.
    if (!ui.mono()) ui.box(ox + 3, mid - bandH / 2, wheelsW, bandH, ui.theme.track);
    for (int c = 0; c < wheelCount(); ++c) {
      int16_t cx = ox + 3 + colW * c + colW / 2;
      bool focused = _focus == c, editing = focused && _editing;
      int16_t boxW = colW - 4, bx = cx - boxW / 2, by = mid - bandH / 2;
      if (editing) ui.box(bx, by, boxW, bandH, ui.theme.accent);
      else if (focused) g.drawRoundRect(bx, by, boxW, bandH, ui.mono() ? 2 : 5, ui.theme.highlight);
      g.setClip(ox, top, w, areaH);
      for (int k = -1; k <= 1; ++k) {
        if (k && !neighbours) continue;
        float pos = k + _anim[c];
        bool centre = fabsf(pos) < 0.5f;
        char t[4];
        if (!wheelText(c, k, t)) continue;
        int16_t y = (int16_t)(mid + pos * step - (centre ? lh * big / 2 : lh / 2));
        uint16_t col = centre ? (editing ? ui.theme.accentText : ui.theme.text) : ui.theme.muted;
        if (ui.mono() && centre && editing) col = 0;
        ui.textCentered(cx, y, t, col, centre ? big : 1);
      }
      g.clearClip();
      if (c < wheelCount() - 1 && c == 0) ui.textCentered(ox + 3 + colW, mid - lh * big / 2, ":", ui.theme.text, big);
    }
    // Control: switch or Save.
    int16_t cx = ox + w - 6 - controlW;
    bool focused = _focus == parts - 1;
    if (item && item->flag) {
      int16_t sh = lh + 3;
      ui.toggleSwitch(cx, mid - sh / 2, controlW, sh, _switchPos);
      if (focused) g.drawRoundRect(cx - 2, mid - sh / 2 - 2, controlW + 4, sh + 4, sh / 2 + 2, ui.theme.highlight);
    } else {
      int16_t bh = lh + 6;
      ui.box(cx, mid - bh / 2, controlW, bh, focused ? ui.theme.accent : ui.theme.track);
      if (ui.mono() && !focused) g.drawRoundRect(cx, mid - bh / 2, controlW, bh, 2, 1);
      ui.textCentered(cx + controlW / 2, mid - lh / 2, "Save", focused ? ui.theme.accentText : ui.theme.text);
    }
    if (hintH) ui.textCentered(ox + w / 2, ui.height() - lh - 1, hint(), ui.theme.textDim);
  }

  void event(SmoothUI& ui, SmoothEvent e) override {
    const int parts = wheelCount() + 1;
    if (e == SmoothEvent::Back) {
      if (_editing) { _editing = false; _minutes = item && item->value ? *item->value : _minutes; }
      else ui.back();
      return;
    }
    if (e == SmoothEvent::Next || e == SmoothEvent::Prev) {
      int dir = e == SmoothEvent::Next ? 1 : -1;
      if (!_editing) { _focus = constrain(_focus + dir, 0, parts - 1); return; }
      // Alarm style: changing the time of a timer that is on switches it off.
      if (item && item->flag && *item->flag) {
        *item->flag = false;
        if (item->onTime) item->onTime(item->value ? *item->value : _minutes, false);
      }
      int h = _minutes / 60, m = _minutes % 60;
      if (_focus == 0) h = SMOOTHUI_24H ? (h + dir + 24) % 24 : ((h % 12 + dir + 12) % 12) + (h >= 12 ? 12 : 0);
      else if (_focus == 1) m = (m + dir + 60) % 60;
      else h = (h + 12) % 24;   // AM <-> PM
      _minutes = h * 60 + m;
      _anim[_focus] = (float)dir;
      return;
    }
    if (e != SmoothEvent::Select) return;
    if (_focus < parts - 1) { _editing = !_editing; return; }
    // The control: switch on = save (switch off = off), or Save.
    if (item && item->flag) {
      bool on = !*item->flag;
      if (on && item->value) *item->value = _minutes;
      *item->flag = on;
      if (item->onTime) item->onTime(item->value ? *item->value : _minutes, on);
      if (on) {
        char t[12], msg[24];
        smoothFormatTime(_minutes, t, sizeof(t));
        snprintf(msg, sizeof(msg), "%s every day", t);
        ui.alert(_title, msg, 1800);
      }
    } else {
      if (item && item->value) *item->value = _minutes;
      if (item && item->onTime) item->onTime(_minutes, true);
      ui.back();
    }
  }

 private:
  int _minutes = 0, _focus = 0;
  bool _editing = false;
  float _anim[3] = {0, 0, 0};
  float _switchPos = 0;

  bool isOn() { return item && item->flag && *item->flag; }
  int wheelCount() const { return SMOOTHUI_24H ? 2 : 3; }
  bool changed() { return item && item->value && *item->value != _minutes; }
  const char* hint() {
    if (_editing) return "Turn, then press";
    if (_focus == wheelCount()) {
      if (!(item && item->flag)) return "Press to save";
      return isOn() ? "Press: turn off" : "Press: on to save";
    }
    if (changed() && item && item->flag) return "Switch on to save";
    return "Press to edit";
  }
  bool wheelText(int c, int k, char* t) {
    int h = _minutes / 60, m = _minutes % 60;
    if (c == 0) {
      if (SMOOTHUI_24H) snprintf(t, 4, "%02d", (h + k + 24) % 24);
      else { int h12 = (h % 12 + k + 12) % 12; snprintf(t, 4, "%d", h12 == 0 ? 12 : h12); }
    } else if (c == 1) {
      snprintf(t, 4, "%02d", (m + k + 60) % 60);
    } else {
      int idx = (h >= 12 ? 1 : 0) + k;   // AM/PM: two rows, no wrap
      if (idx < 0 || idx > 1) return false;
      strcpy(t, idx ? "PM" : "AM");
    }
    return true;
  }
};

// A progress screen: a bar (or a spinner when the amount is unknown), a title
// and a line of text. Update it with set(); close it with ui.back().
class SmoothProgressPage : public SmoothPage {
 public:
  explicit SmoothProgressPage(const char* title = "Working") : SmoothPage(title) {}
  // fraction 0..1, or < 0 for "unknown" (spinner).
  void set(float fraction, const char* message = nullptr) {
    _target = fraction;
    if (message) _message = message;
  }
  void setMessage(const char* message) { _message = message; }
  // Long press runs this (e.g. cancel the job); without it, long press is ignored.
  void onCancel(void (*cancel)()) { _cancel = cancel; }
  float value() const { return _target; }

  void enter(SmoothUI&) override { _shown = _target < 0 ? 0 : _target; }
  bool tick(SmoothUI& ui, uint32_t dt) override {
    if (_target < 0) return true;   // spinner keeps turning
    return SmoothAnim::approach(_shown, _target, ui.theme.barSpeed, dt);
  }
  void draw(SmoothUI& ui, int16_t ox) override {
    SmoothCanvas& g = ui.gfx();
    ui.titleBar(ox, _title);
    const int16_t w = ui.width(), lh = ui.lineHeight(), top = ui.titleHeight();
    const int16_t areaH = ui.height() - top;
    int16_t barH = smoothui::hi<int16_t>(6, lh / 2 + 3);
    // Percentage, bar and message; on short screens the extras drop out.
    bool showMsg = _message && _message[0], showPct = true;
    if (lh + 4 + barH + (showMsg ? lh + 4 : 0) > areaH) showMsg = false;
    if (lh + 4 + barH > areaH) showPct = false;
    int16_t spinR = smoothui::lo<int16_t>(18, (areaH - (showMsg ? lh + 4 : 0) - 8) / 2);
    if (_target < 0 && spinR < 6 && showMsg) { showMsg = false; spinR = smoothui::lo<int16_t>(18, (areaH - 8) / 2); }
    int16_t blockH = _target < 0 ? 2 * spinR + 6 : (showPct ? lh + 4 : 0) + barH + 4;
    blockH += showMsg ? lh : 0;
    int16_t y = top + (areaH - blockH) / 2;
    if (_target < 0) {
      // Spinner: dots around a circle, one bright head.
      int16_t r = smoothui::hi<int16_t>(3, spinR), cx = ox + w / 2, cy = y + r;
      float a0 = (ui.frameMillis() % 1000) / 1000.0f * 6.2832f;
      for (int i = 0; i < 8; ++i) {
        float a = a0 - i * 0.785f;
        uint16_t col = ui.mono() ? 1 : SmoothColor::mix(ui.theme.highlight, ui.theme.track, (uint8_t)(i * 30));
        g.fillCircle(cx + (int16_t)(cosf(a) * r), cy + (int16_t)(sinf(a) * r), i == 0 ? 3 : 2, col);
        if (ui.mono() && i > 3) break;
      }
      y += 2 * r + 6;
    } else {
      char pct[8];
      snprintf(pct, sizeof(pct), "%d%%", (int)(_shown * 100 + 0.5f));
      if (showPct) {
        ui.textCentered(ox + w / 2, y, pct, ui.theme.text);
        y += lh + 4;
      }
      ui.bar(ox + 8, y, w - 16, barH, _shown);
      y += barH + 4;
    }
    if (showMsg) ui.marquee(ox + 6, y, w - 12, _message, ui.theme.textDim, true);
  }
  void event(SmoothUI&, SmoothEvent e) override {
    if (e == SmoothEvent::Back && _cancel) _cancel();
  }

 private:
  float _target = 0, _shown = 0;
  const char* _message = "";
  void (*_cancel)() = nullptr;
};

// ---- SmoothUI editor hooks ---------------------------------------------------
inline void SmoothUI::editNumber(SmoothMenuItem* item) {
  if (!_numberPage) _numberPage = new SmoothNumberPage();
  _numberPage->item = item;
  show(_numberPage);
}
inline void SmoothUI::editChoice(SmoothMenuItem* item) {
  if (!_choicePage) _choicePage = new SmoothChoicePage();
  _choicePage->item = item;
  show(_choicePage);
}
inline void SmoothUI::editTime(SmoothMenuItem* item) {
  if (!_timePage) _timePage = new SmoothTimePage();
  _timePage->item = item;
  show(_timePage);
}
