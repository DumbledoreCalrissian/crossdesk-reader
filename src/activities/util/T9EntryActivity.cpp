#include "T9EntryActivity.h"

#include <I18n.h>

#include <algorithm>
#include <cstring>

#include "HapticFeedback.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

constexpr fui::ActionId ACTION_KEY = 1;
// Key values. Digit keys use DIGIT_BASE + digit.
constexpr int16_t DIGIT_BASE = 100;
constexpr int16_t KEY_PAGE = -10;
constexpr int16_t KEY_NEXT = -11;
constexpr int16_t KEY_DELETE = -12;
constexpr int16_t KEY_CASE = -13;
constexpr int16_t KEY_OK = -14;

constexpr int GRID_COLS = 3;
constexpr int GRID_ROWS = 4;
constexpr int SIDE_ROWS = 3;
constexpr int KEY_GAP = 8;
constexpr int PANEL_PADDING = 6;
// Keypad height as a share of the screen: tall keys for a fingertip.
constexpr int KEYPAD_HEIGHT_PERCENT = 52;
// Characters shown on a key; key 1 has more than fit.
constexpr size_t MAX_LABEL_CHARS = 5;
// No hold actions on this keypad: a slow tap must still type.
constexpr uint16_t NO_HOLD_MS = UINT16_MAX;
// Space between the text and the password show/hide label; also widens the
// label's touch area.
constexpr int TOGGLE_GAP = 8;
// Length limit when the caller sets none. The text never grows past the
// capacity reserved in onEnter(), so render() on the render task never reads
// a buffer that loop() has just freed.
constexpr size_t DEFAULT_MAX_LENGTH = 255;

// Key labels stay untranslated: they show the Latin characters the keys type.
const char* pageKeyLabel(const T9MultiTap::Page page) {
  switch (page) {
    case T9MultiTap::Page::Letters:
      return "123";
    case T9MultiTap::Page::Digits:
      return "#+=";
    case T9MultiTap::Page::Symbols:
      break;
  }
  return "abc";
}

const char* caseKeyLabel(const T9MultiTap::Case letterCase) {
  switch (letterCase) {
    case T9MultiTap::Case::Capital:
      return "Abc";
    case T9MultiTap::Case::Lower:
      return "abc";
    case T9MultiTap::Case::Upper:
      break;
  }
  return "ABC";
}

size_t utf8Prev(const std::string& s, size_t pos) {
  if (pos == 0) return 0;
  --pos;
  while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) --pos;
  return pos;
}

}  // namespace

void T9EntryActivity::onEnter() {
  Activity::onEnter();
  // Credentials and addresses rarely start with a capital letter.
  t9.setPage(T9MultiTap::Page::Letters);
  t9.setCase(inputType == InputType::Text ? T9MultiTap::Case::Capital : T9MultiTap::Case::Lower);
  passwordVisible = false;
  if (maxLength == 0) maxLength = DEFAULT_MAX_LENGTH;
  text.reserve(std::max(maxLength, text.size()) + 1);
  cursorPos = text.size();
  touchRouter.reset();
  touchRouter.holdMs = NO_HOLD_MS;
  touchRouter.overrideHoldMs = NO_HOLD_MS;
  interactionsReady = false;
  requestUpdate();
}

void T9EntryActivity::typeDigitKey(const uint8_t key) {
  const T9MultiTap::Output out = t9.tap(key, millis());
  if (!out.text) return;
  if (out.replacesPending) {
    // The pending character sits just before the cursor.
    const size_t start = utf8Prev(text, cursorPos);
    text.erase(start, cursorPos - start);
    cursorPos = start;
  } else if (text.size() >= maxLength) {
    t9.cancel();  // full: the new character is not added
    return;
  }
  text.insert(cursorPos, out.text);
  cursorPos += strlen(out.text);
}

void T9EntryActivity::deleteBeforeCursor() {
  t9.cancel();  // a pending character is the one before the cursor
  const size_t start = utf8Prev(text, cursorPos);
  text.erase(start, cursorPos - start);
  cursorPos = start;
}

void T9EntryActivity::handleKey(const int16_t value) {
  if (value >= DIGIT_BASE && value < DIGIT_BASE + T9MultiTap::KEY_COUNT) {
    typeDigitKey(static_cast<uint8_t>(value - DIGIT_BASE));
    return;
  }
  switch (value) {
    case KEY_PAGE:
      t9.nextPage();
      break;
    case KEY_NEXT:
      t9.commit();
      break;
    case KEY_DELETE:
      deleteBeforeCursor();
      break;
    case KEY_CASE:
      t9.cycleCase();
      break;
    case KEY_OK:
      finishWith(false);
      break;
    default:
      break;
  }
}

void T9EntryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finishWith(true);
    return;
  }

  int tx = 0;
  int ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty) && handleFieldTap(tx, ty)) {
    haptic_feedback::touchAction();
    touchRouter.reset();
    requestUpdate();
    return;
  }

  if (interactionsReady) {
    const bool pressedDown = mappedInput.wasScreenTouchDown(tx, ty);
    int tapX = 0;
    int tapY = 0;
    const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
    int hx = 0;
    int hy = 0;
    const bool inContact = mappedInput.isScreenTouchHeld(hx, hy);
    const fui::TouchHoldRouter::Result result =
        touchRouter.update(interactions, pressedDown, static_cast<int16_t>(tx), static_cast<int16_t>(ty), tapped,
                           static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), inContact, millis());
    // No repaint for the touch-down highlight: on e-ink it would delay the
    // repaint that shows the typed character during quick repeated taps.
    if (result.event) {
      haptic_feedback::touchAction();
      handleKey(result.event.value);
      requestUpdate();
      return;
    }
  }

  // Repaint once a pending character commits, to remove its underline.
  if (t9.expire(millis())) requestUpdate();
}

bool T9EntryActivity::textHidden() const { return inputType == InputType::Password && !passwordVisible; }

std::string T9EntryActivity::displayText() const {
  if (!textHidden()) return text;
  // Masked, except the pending character so the user can see the cycle. The
  // cursor stays at the end while the text is hidden.
  std::string masked;
  const size_t keepFrom = t9.hasPending() ? utf8Prev(text, text.size()) : text.size();
  for (size_t i = 0; i < keepFrom; ++i) {
    if ((static_cast<uint8_t>(text[i]) & 0xC0) != 0x80) masked += '*';
  }
  masked.append(text, keepFrom, std::string::npos);
  return masked;
}

T9EntryActivity::FieldLayout T9EntryActivity::fieldLayout() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect content = UITheme::getContentArea(renderer);
  FieldLayout f;
  f.lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  f.top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  f.keypadTop = renderer.getScreenHeight() - renderer.getScreenHeight() * KEYPAD_HEIGHT_PERCENT / 100;
  f.textLeft = content.x + metrics.contentSidePadding;
  int right = content.x + content.width - metrics.contentSidePadding;
  if (inputType == InputType::Password) {
    // Show/hide label at the right end of the first line.
    const int labelW =
        std::max(renderer.getTextWidth(UI_12_FONT_ID, "[abc]"), renderer.getTextWidth(UI_12_FONT_ID, "[***]"));
    f.toggleLeft = right - labelW;
    right = f.toggleLeft - TOGGLE_GAP;
  }
  f.maxWidth = right - f.textLeft;
  // Room below the text for the field line.
  f.maxLines = std::max(1, (f.keypadTop - f.top - 2 * f.lineHeight) / f.lineHeight);
  return f;
}

template <typename Visit>
int T9EntryActivity::walkText(const std::string& s, const FieldLayout& f, Visit&& visit) const {
  char glyph[8] = {};
  int line = 0;
  int x = f.textLeft;
  for (size_t i = 0; i < s.size();) {
    size_t next = i + 1;
    while (next < s.size() && (static_cast<uint8_t>(s[next]) & 0xC0) == 0x80) ++next;
    const size_t n = std::min(next - i, sizeof(glyph) - 1);
    memcpy(glyph, s.data() + i, n);
    glyph[n] = '\0';
    const int w = renderer.getTextAdvanceX(UI_12_FONT_ID, glyph, EpdFontFamily::REGULAR);
    if (x + w > f.textLeft + f.maxWidth && x > f.textLeft) {
      x = f.textLeft;
      ++line;
    }
    visit(i, glyph, x, line, w);
    x += w;
    i = next;
  }
  return line + 1;
}

int T9EntryActivity::firstVisibleLine(const std::string& s, const FieldLayout& f, const size_t anchor,
                                      int& lines) const {
  // The last lines that fit, moved up when the cursor is above them.
  int cursorLine = -1;
  lines = walkText(s, f, [&](const size_t i, const char*, int, const int line, int) {
    if (i == anchor) cursorLine = line;
  });
  if (cursorLine < 0) cursorLine = lines - 1;  // anchor at the end
  return std::max(0, std::min(lines - f.maxLines, cursorLine));
}

bool T9EntryActivity::handleFieldTap(const int x, const int y) {
  const FieldLayout f = fieldLayout();
  if (y < f.top || y >= f.keypadTop) return false;
  if (f.toggleLeft >= 0 && x >= f.toggleLeft - TOGGLE_GAP && y < f.top + f.lineHeight + TOGGLE_GAP) {
    t9.commit();
    passwordVisible = !passwordVisible;
    if (!passwordVisible) cursorPos = text.size();
    return true;
  }
  if (textHidden()) return false;

  // Place the cursor before the character nearest the tap. The window is
  // taken before commit(), matching the one render() last drew.
  const size_t anchor = t9.hasPending() ? utf8Prev(text, cursorPos) : cursorPos;
  t9.commit();
  // Visible text only (hidden passwords returned above), so `text` wraps
  // exactly like the string render() draws.
  int lines = 0;
  const int firstLine = firstVisibleLine(text, f, anchor, lines);
  const int tapLine = std::min(lines - 1, firstLine + std::max(0, (y - f.top) / f.lineHeight));
  size_t position = text.size();
  bool found = false;
  walkText(text, f, [&](const size_t i, const char*, const int gx, const int line, const int w) {
    if (found) return;
    if (line > tapLine || (line == tapLine && x < gx + w / 2)) {
      position = i;
      found = true;
    }
  });
  cursorPos = position;
  return true;
}

void T9EntryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const Rect content = UITheme::getContentArea(renderer);
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  // Text field: wrapped lines, the cursor, and the pending character
  // underlined. Only the last lines that fit above the keypad are shown.
  const std::string shown = displayText();
  const FieldLayout f = fieldLayout();
  const int keypadTop = f.keypadTop;
  const size_t cursor = textHidden() ? shown.size() : std::min(cursorPos, shown.size());
  const size_t pendingStart = t9.hasPending() ? utf8Prev(shown, cursor) : std::string::npos;
  int lineCount = 0;
  const int firstLine = firstVisibleLine(shown, f, t9.hasPending() ? utf8Prev(shown, cursor) : cursor, lineCount);

  int lastY = f.top;
  int endX = f.textLeft;
  int cursorX = -1;
  int cursorY = f.top;
  walkText(shown, f, [&](const size_t i, const char* glyph, const int x, const int line, const int w) {
    if (line < firstLine) return;
    const int y = f.top + (line - firstLine) * f.lineHeight;
    if (i == cursor) {
      cursorX = x;
      cursorY = y;
    }
    if (i == pendingStart) renderer.drawLine(x, y + f.lineHeight, x + w, y + f.lineHeight, 2, true);
    renderer.drawText(UI_12_FONT_ID, x, y, glyph);
    lastY = y;
    endX = x + w;
  });
  if (cursorX < 0) {
    cursorX = endX;
    cursorY = lastY;
  }
  if (pendingStart == std::string::npos) renderer.fillRect(cursorX, cursorY, 2, f.lineHeight, true);
  const int margin = metrics.contentSidePadding;
  GUI.drawTextField(renderer, Rect{content.x, f.top, content.width, lastY - f.top}, content.width - 2 * margin, false,
                    margin, content.width - 2 * margin);

  if (f.toggleLeft >= 0) {
    renderer.drawText(UI_12_FONT_ID, f.toggleLeft, f.top, passwordVisible ? "[***]" : "[abc]");
  }

  // Keypad: a 3x4 digit grid with a side column of Delete, Case and OK.
  const fui::Rect panel{static_cast<int16_t>(content.x), static_cast<int16_t>(keypadTop),
                        static_cast<int16_t>(content.width), static_cast<int16_t>(pageHeight - keypadTop)};
  const int innerW = panel.width - 2 * PANEL_PADDING;
  const int innerH = panel.height - 2 * PANEL_PADDING;
  const int sideW = (innerW - KEY_GAP) / 4;
  const fui::Rect gridRect{static_cast<int16_t>(panel.x + PANEL_PADDING), static_cast<int16_t>(panel.y + PANEL_PADDING),
                           static_cast<int16_t>(innerW - sideW - KEY_GAP), static_cast<int16_t>(innerH)};
  const fui::Rect sideRect{static_cast<int16_t>(gridRect.right() + KEY_GAP), gridRect.y, static_cast<int16_t>(sideW),
                           static_cast<int16_t>(innerH)};

  const T9MultiTap::Page page = t9.page();
  const bool upper = page == T9MultiTap::Page::Letters && t9.letterCase() != T9MultiTap::Case::Lower;
  for (uint8_t key = 0; key < T9MultiTap::KEY_COUNT; ++key) {
    const char* chars = T9MultiTap::keyChars(page, key);
    size_t len = strlen(chars);
    if (page == T9MultiTap::Page::Letters && len > 1) --len;  // the digit is the secondary label
    len = std::min(len, MAX_LABEL_CHARS);
    for (size_t i = 0; i < len; ++i) {
      const char c = chars[i];
      labels[key][i] = upper && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }
    labels[key][len] = '\0';
  }

  static constexpr char DIGIT_LABELS[T9MultiTap::KEY_COUNT][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  const bool lettersPage = page == T9MultiTap::Page::Letters;
  const bool spaceKey = page != T9MultiTap::Page::Digits;
  auto digitKey = [&](const uint8_t key) {
    fui::KeyGridKey k;
    k.label = labels[key];
    k.secondaryLabel = lettersPage ? DIGIT_LABELS[key] : nullptr;
    if (key == 0 && spaceKey) k.label = nullptr;  // drawn as a space symbol below
    k.value = static_cast<int16_t>(DIGIT_BASE + key);
    return k;
  };
  auto controlKey = [](const char* label, const fui::KeyKind kind, const int16_t value, const bool enabled = true) {
    fui::KeyGridKey k;
    k.label = label;
    k.kind = kind;
    k.value = value;
    k.enabled = enabled;
    return k;
  };

  const fui::KeyGridKey gridKeys[GRID_COLS * GRID_ROWS] = {
      digitKey(1), digitKey(2),
      digitKey(3), digitKey(4),
      digitKey(5), digitKey(6),
      digitKey(7), digitKey(8),
      digitKey(9), controlKey(pageKeyLabel(page), fui::KeyKind::Mode, KEY_PAGE),
      digitKey(0), controlKey(">", fui::KeyKind::Normal, KEY_NEXT, t9.hasPending()),
  };
  const fui::KeyGridKey sideKeys[SIDE_ROWS] = {
      controlKey(nullptr, fui::KeyKind::Delete, KEY_DELETE, !text.empty()),
      controlKey(caseKeyLabel(t9.letterCase()), fui::KeyKind::Shift, KEY_CASE, lettersPage),
      controlKey(tr(STR_OK_BUTTON), fui::KeyKind::Ok, KEY_OK),
  };

  interactions.beginPublishCycle();
  fui::GfxRendererTarget target(renderer);
  target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  fui::Frame<16> frame(target, device, noInput, interactions);
  frame.target().fill(panel, fui::Paint::dither(fui::Color::LightGray));

  fui::KeyGridProps props;
  props.action = ACTION_KEY;
  props.inputMask = fui::InputTouch;
  props.labelText.font = fui::GfxRendererTarget::FONT_BODY;
  props.secondaryText.font = fui::GfxRendererTarget::FONT_SMALL;
  props.gap = KEY_GAP;
  props.radius = 3;
  props.keys = gridKeys;
  props.rows = GRID_ROWS;
  props.cols = GRID_COLS;
  fui::keyGrid(frame, gridRect, props);
  if (spaceKey) {
    // Space symbol: a line with raised ends, centered on key 0 (row 3, column 1).
    // Uses the same cell math as fui::keyGrid(); update both together.
    const int cellW = (gridRect.width - (GRID_COLS - 1) * KEY_GAP) / GRID_COLS;
    const int cellH = (gridRect.height - (GRID_ROWS - 1) * KEY_GAP) / GRID_ROWS;
    const int cx = gridRect.x + cellW + KEY_GAP + cellW / 2;
    const int cy = gridRect.y + 3 * (cellH + KEY_GAP) + cellH / 2;
    const int half = cellW / 6;
    const int rise = std::max(3, cellH / 12);
    const int base = cy + rise / 2;
    renderer.drawLine(cx - half, base, cx + half, base, 3, true);
    renderer.drawLine(cx - half, base - rise, cx - half, base, 3, true);
    renderer.drawLine(cx + half, base - rise, cx + half, base, 3, true);
  }
  props.keys = sideKeys;
  props.rows = SIDE_ROWS;
  props.cols = 1;
  fui::keyGrid(frame, sideRect, props);
  interactions.publish();
  interactionsReady = true;

  renderer.displayBuffer();
}

void T9EntryActivity::finishWith(const bool cancelled) {
  if (cancelled) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
  } else {
    t9.commit();
    setResult(KeyboardResult{text});
  }
  finish();
}
