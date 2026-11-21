#pragma once

#include <cstdint>

// Multi-tap entry for a 3x4 phone keypad. Pure logic, no display or input
// dependencies, so the commit rules can be host-tested.
//
// Taps are counted when they arrive, not when the screen shows them: on e-ink
// several taps can land before one repaint. A pending character commits when
// another key is tapped, when commit() is called, or when expire() sees the
// timeout pass.
class T9MultiTap {
 public:
  // Which characters the digit keys type. Digits type at once; the other
  // pages cycle through the characters on the tapped key.
  enum class Page : uint8_t { Letters, Digits, Symbols };
  // Letter case. Capital makes the next letter upper case, then turns Lower.
  enum class Case : uint8_t { Capital, Lower, Upper };

  // What the caller must do with its text after a tap.
  struct Output {
    // Text to insert, or nullptr when the tap produced nothing.
    const char* text = nullptr;
    // True when `text` replaces the pending character instead of adding one.
    bool replacesPending = false;
  };

  static constexpr uint8_t KEY_COUNT = 10;  // digit keys 0-9
  // Longer than a phone's 0.5-0.7 s: the user may wait for an e-ink repaint
  // before tapping the same key again.
  static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;

  explicit T9MultiTap(uint32_t timeoutMs = DEFAULT_TIMEOUT_MS) : timeoutMs(timeoutMs) {}

  Output tap(uint8_t key, uint32_t nowMs);
  // Commits a pending character. Returns true when one was pending.
  bool commit();
  // Drops a pending character; the caller removes it from its text.
  // Returns true when one was pending.
  bool cancel();
  // Commits once the timeout has passed. Returns true when that happened.
  bool expire(uint32_t nowMs);
  bool hasPending() const { return pendingKey >= 0; }

  Page page() const { return currentPage; }
  void setPage(Page page);
  // Letters -> Digits -> Symbols -> Letters.
  void nextPage();
  Case letterCase() const { return currentCase; }
  void setCase(Case letterCase);
  // Capital -> Lower -> Upper -> Capital.
  void cycleCase();

  // Characters a key types on a page, for drawing them on the keypad. All are
  // single-byte ASCII.
  static const char* keyChars(Page page, uint8_t key);

 private:
  uint32_t timeoutMs;
  uint32_t lastTapMs = 0;
  int8_t pendingKey = -1;
  uint8_t tapCount = 0;
  bool pendingUpper = false;
  Page currentPage = Page::Letters;
  Case currentCase = Case::Capital;
  char out[2] = {};

  void finishPending();
};
