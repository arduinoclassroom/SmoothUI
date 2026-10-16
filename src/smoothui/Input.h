// SmoothUI input: a rotary encoder with push button, or plain buttons.
// Both turn into SmoothEvents: Next / Prev (turn or up/down), Select (short
// press) and Back (long press, or a back button).
// Copyright (c) 2026 Oshan Dissanayaka. MIT License.
#pragma once
#include <Arduino.h>

enum class SmoothEvent : uint8_t { None, Next, Prev, Select, Back };

// Interrupt handlers stay in flash: an inline (header) function can't be put
// in ESP32 IRAM, and attachInterrupt() handlers don't need to be.
#define SMOOTHUI_ISR

class SmoothInput {
 public:
  virtual ~SmoothInput() {}
  virtual SmoothEvent read() = 0;   // one event per call, None when idle
};

// Rotary encoder (A, B) with an optional push switch. Turning is decoded on
// every edge of both channels by interrupt, so no step is lost while the
// screen is busy drawing. One encoder per sketch.
class SmoothEncoder : public SmoothInput {
 public:
  // stepsPerDetent: 4 for most encoders (one click = 4 transitions), 2 or 1
  // for some. Set reverse if turning clockwise goes up the list.
  SmoothEncoder(int8_t pinA, int8_t pinB, int8_t pinButton = -1, uint8_t stepsPerDetent = 4, bool reverse = false)
      : _a(pinA), _b(pinB), _sw(pinButton), _steps(stepsPerDetent ? stepsPerDetent : 4), _reverse(reverse) {}

  void begin() {
    pinMode(_a, INPUT_PULLUP);
    pinMode(_b, INPUT_PULLUP);
    if (_sw >= 0) pinMode(_sw, INPUT_PULLUP);
    instance() = this;
    _state = (digitalRead(_a) << 1) | digitalRead(_b);
    attachInterrupt(digitalPinToInterrupt(_a), isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_b), isr, CHANGE);
  }

  SmoothEvent read() override {
    // Turning
    noInterrupts();
    int32_t sub = _sub;
    interrupts();
    // Floor division, so turning back past zero is as quick as forward.
    int32_t detents = sub >= 0 ? sub / _steps : -((-sub + _steps - 1) / _steps);
    if (detents != _reported) {
      SmoothEvent e = (detents > _reported) != _reverse ? SmoothEvent::Next : SmoothEvent::Prev;
      _reported += detents > _reported ? 1 : -1;
      return e;
    }
    return button();
  }

  bool pressed() const { return _sw >= 0 && digitalRead(_sw) == LOW; }

 private:
  int8_t _a, _b, _sw;
  uint8_t _steps;
  bool _reverse;
  volatile int32_t _sub = 0;
  volatile uint8_t _state = 0;
  int32_t _reported = 0;
  bool _down = false, _longSent = false;
  uint32_t _changedAt = 0, _pressedAt = 0;

  static SmoothEncoder*& instance() { static SmoothEncoder* p = nullptr; return p; }

  static void SMOOTHUI_ISR isr() {
    SmoothEncoder* e = instance();
    if (!e) return;
    // Gray-code transition table: +1 / -1 for valid steps, 0 for bounce.
    static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    uint8_t now = (digitalRead(e->_a) << 1) | digitalRead(e->_b);
    e->_sub += table[(e->_state << 2) | now];
    e->_state = now;
  }

  SmoothEvent button() {
    if (_sw < 0) return SmoothEvent::None;
    bool down = digitalRead(_sw) == LOW;
    uint32_t ms = millis();
    if (down != _down && ms - _changedAt > 30) {   // debounced edge
      _changedAt = ms;
      _down = down;
      if (down) {
        _pressedAt = ms;
        _longSent = false;
      } else if (!_longSent) {
        return SmoothEvent::Select;
      }
    }
    if (_down && !_longSent && ms - _pressedAt > SMOOTHUI_LONG_PRESS_MS) {
      _longSent = true;
      return SmoothEvent::Back;
    }
    return SmoothEvent::None;
  }
};

// Up / Down / OK / (Back) push buttons to ground. Holding Up or Down repeats;
// holding OK means Back when there is no Back button.
class SmoothButtons : public SmoothInput {
 public:
  SmoothButtons(int8_t up, int8_t down, int8_t ok, int8_t back = -1) : _pins{up, down, ok, back} {}

  void begin() {
    for (int8_t p : _pins)
      if (p >= 0) pinMode(p, INPUT_PULLUP);
  }

  SmoothEvent read() override {
    uint32_t ms = millis();
    for (uint8_t i = 0; i < 4; ++i) {
      if (_pins[i] < 0) continue;
      bool down = digitalRead(_pins[i]) == LOW;
      if (down != _down[i] && ms - _changed[i] > 30) {
        _changed[i] = ms;
        _down[i] = down;
        if (down) {
          _pressedAt[i] = ms;
          _repeatAt[i] = ms + 400;
          _longSent[i] = false;
          if (i == 0) return SmoothEvent::Prev;
          if (i == 1) return SmoothEvent::Next;
          if (i == 3) return SmoothEvent::Back;
        } else if (i == 2 && !_longSent[i]) {
          return SmoothEvent::Select;
        }
      }
      if (_down[i] && (i == 0 || i == 1) && ms >= _repeatAt[i]) {   // auto-repeat
        _repeatAt[i] = ms + 90;
        return i == 0 ? SmoothEvent::Prev : SmoothEvent::Next;
      }
      if (_down[i] && i == 2 && _pins[3] < 0 && !_longSent[i] && ms - _pressedAt[i] > SMOOTHUI_LONG_PRESS_MS) {
        _longSent[i] = true;
        return SmoothEvent::Back;
      }
    }
    return SmoothEvent::None;
  }

 private:
  int8_t _pins[4];
  bool _down[4] = {false, false, false, false};
  bool _longSent[4] = {false, false, false, false};
  uint32_t _changed[4] = {0, 0, 0, 0}, _pressedAt[4] = {0, 0, 0, 0}, _repeatAt[4] = {0, 0, 0, 0};
};
