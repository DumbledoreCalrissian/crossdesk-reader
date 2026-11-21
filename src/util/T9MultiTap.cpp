#include "T9MultiTap.h"

#include <cstring>

namespace {
// Each letter key ends with its digit, as on phones. Key 1 holds the
// punctuation that user names, addresses and URLs need; key 0 is space.
constexpr const char* LETTER_CHARS[T9MultiTap::KEY_COUNT] = {
    " 0", ".,?!'\"-_@/:1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9",
};
// The rest of printable ASCII, so any password can be typed.
constexpr const char* SYMBOL_CHARS[T9MultiTap::KEY_COUNT] = {
    " ", "#$%", "&*+", "=;^", "()", "[]", "{}", "<>", "\\|", "`~",
};
constexpr const char* DIGIT_CHARS[T9MultiTap::KEY_COUNT] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

const char* const* tableFor(const T9MultiTap::Page page) {
  switch (page) {
    case T9MultiTap::Page::Digits:
      return DIGIT_CHARS;
    case T9MultiTap::Page::Symbols:
      return SYMBOL_CHARS;
    case T9MultiTap::Page::Letters:
      break;
  }
  return LETTER_CHARS;
}

bool isLower(const char c) { return c >= 'a' && c <= 'z'; }
}  // namespace

const char* T9MultiTap::keyChars(const Page page, const uint8_t key) {
  return key < KEY_COUNT ? tableFor(page)[key] : "";
}

T9MultiTap::Output T9MultiTap::tap(const uint8_t key, const uint32_t nowMs) {
  Output result;
  if (key >= KEY_COUNT) return result;

  const char* chars = keyChars(currentPage, key);
  if (currentPage == Page::Digits) {
    commit();
    out[0] = chars[0];
    result.text = out;
    return result;
  }

  if (pendingKey == key && static_cast<uint32_t>(nowMs - lastTapMs) < timeoutMs) {
    tapCount = static_cast<uint8_t>((tapCount + 1) % std::strlen(chars));
    result.replacesPending = true;
  } else {
    commit();
    pendingKey = static_cast<int8_t>(key);
    tapCount = 0;
    pendingUpper = currentCase != Case::Lower;
  }
  lastTapMs = nowMs;

  const char c = chars[tapCount];
  out[0] = pendingUpper && isLower(c) ? static_cast<char>(c - 'a' + 'A') : c;
  result.text = out;
  return result;
}

bool T9MultiTap::commit() {
  if (!hasPending()) return false;
  finishPending();
  return true;
}

bool T9MultiTap::cancel() {
  if (!hasPending()) return false;
  pendingKey = -1;
  tapCount = 0;
  return true;
}

bool T9MultiTap::expire(const uint32_t nowMs) {
  if (!hasPending() || static_cast<uint32_t>(nowMs - lastTapMs) < timeoutMs) return false;
  finishPending();
  return true;
}

void T9MultiTap::finishPending() {
  // A capital applies to one letter; space and punctuation do not use it up.
  if (currentCase == Case::Capital && isLower(keyChars(currentPage, pendingKey)[tapCount])) {
    currentCase = Case::Lower;
  }
  pendingKey = -1;
  tapCount = 0;
}

void T9MultiTap::setPage(const Page page) {
  commit();
  currentPage = page;
}

void T9MultiTap::nextPage() {
  switch (currentPage) {
    case Page::Letters:
      setPage(Page::Digits);
      break;
    case Page::Digits:
      setPage(Page::Symbols);
      break;
    case Page::Symbols:
      setPage(Page::Letters);
      break;
  }
}

void T9MultiTap::setCase(const Case letterCase) {
  commit();
  currentCase = letterCase;
}

void T9MultiTap::cycleCase() {
  switch (currentCase) {
    case Case::Capital:
      setCase(Case::Lower);
      break;
    case Case::Lower:
      setCase(Case::Upper);
      break;
    case Case::Upper:
      setCase(Case::Capital);
      break;
  }
}
