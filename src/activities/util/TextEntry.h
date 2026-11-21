#pragma once

#include <memory>
#include <string>

#include "KeyboardEntryActivity.h"

// Opens the keyboard chosen in Settings: the full keyboard, or the T9 keypad
// on touch devices. Returns nullptr when memory runs out.
std::unique_ptr<Activity> makeTextEntry(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title,
                                        std::string initialText = "", size_t maxLength = 0,
                                        InputType inputType = InputType::Text);
