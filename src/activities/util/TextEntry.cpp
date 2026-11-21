#include "TextEntry.h"

#include <Memory.h>

#include "CrossPointSettings.h"
#include "KeyboardLayoutSet.h"
#include "MappedInputManager.h"
#include "T9EntryActivity.h"

namespace {
// The T9 keypad types Latin letters only. Plain text in a non-Latin layout
// keeps the full keyboard; credentials and URLs are Latin either way.
bool useT9(const MappedInputManager& mappedInput, const InputType inputType) {
  if (SETTINGS.keyboardType != CrossPointSettings::KEYBOARD_T9 || !mappedInput.hasTouch()) return false;
  if (inputType != InputType::Text) return true;
  for (uint8_t i = 0; i < keyboard_layouts::COUNT; ++i) {
    if (keyboard_layouts::ALL[i].id == keyboard_layouts::startingLayout()) {
      return (keyboard_layouts::LATIN_BITS & keyboard_layouts::bitAt(i)) != 0;
    }
  }
  return false;
}
}  // namespace

std::unique_ptr<Activity> makeTextEntry(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title,
                                        std::string initialText, const size_t maxLength, const InputType inputType) {
  if (useT9(mappedInput, inputType)) {
    return makeUniqueNoThrow<T9EntryActivity>(renderer, mappedInput, std::move(title), std::move(initialText),
                                              maxLength, inputType);
  }
  return makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, std::move(title), std::move(initialText),
                                                  maxLength, inputType);
}
