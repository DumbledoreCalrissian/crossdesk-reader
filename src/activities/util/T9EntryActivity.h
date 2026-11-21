#pragma once
#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>

#include <atomic>
#include <string>
#include <utility>

#include "KeyboardEntryActivity.h"  // InputType
#include "activities/Activity.h"
#include "util/T9MultiTap.h"

// Touch-only phone keypad: multi-tap letters, a digits page and a symbols
// page. Tapping visible text moves the cursor.
// Returns KeyboardResult like KeyboardEntryActivity.
class T9EntryActivity : public Activity {
 public:
  T9EntryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title, std::string initialText,
                  size_t maxLength, InputType inputType)
      : Activity("T9Entry", renderer, mappedInput),
        title(std::move(title)),
        text(std::move(initialText)),
        maxLength(maxLength),
        inputType(inputType) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  std::string title;
  std::string text;
  size_t maxLength;
  InputType inputType;
  bool passwordVisible = false;
  size_t cursorPos = 0;  // byte offset into text, on a character boundary
  T9MultiTap t9;

  // 12 keypad keys and 3 side keys. Double-buffered like KeyboardEntryActivity:
  // render() builds one table while loop() routes taps against the other.
  freeink::ui::InteractionBuffer<16> interactions;
  freeink::ui::TouchHoldRouter touchRouter;
  std::atomic<bool> interactionsReady{false};
  // Key labels for the current page and case; rebuilt in render().
  char labels[T9MultiTap::KEY_COUNT][8] = {};

  void handleKey(int16_t value);
  void typeDigitKey(uint8_t key);
  void deleteBeforeCursor();
  // Handles a tap on the text field: the password show/hide label, or cursor
  // placement on visible text. Returns true when the tap was used.
  bool handleFieldTap(int x, int y);
  bool textHidden() const;
  std::string displayText() const;

  // Text field geometry, shared by render() and tap handling.
  struct FieldLayout {
    int top = 0;
    int keypadTop = 0;
    int lineHeight = 1;
    int textLeft = 0;
    int maxWidth = 0;
    int maxLines = 1;
    int toggleLeft = -1;  // password show/hide label; -1 when absent
  };
  FieldLayout fieldLayout() const;
  // Wraps `s` into lines and calls visit(byteIndex, glyph, x, line, width)
  // per character. Returns the number of lines.
  template <typename Visit>
  int walkText(const std::string& s, const FieldLayout& f, Visit&& visit) const;
  // First line to draw: the last lines that fit, keeping the line of byte
  // `anchor` (the pending character, else the cursor) visible. Sets `lines` to the total line count.
  int firstVisibleLine(const std::string& s, const FieldLayout& f, size_t anchor, int& lines) const;
  void finishWith(bool cancelled);
};
