#include "CoverflowHomeUi.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Portrait design size; other sizes scale to the content area.
constexpr int COVER_W = 372;
constexpr int COVER_H = 520;
// Neighbours stand on the centre cover's floor line, one gap away from it.
constexpr int SIDE_H = 450;
constexpr int SIDE_GAP = 12;
// Taps at y < 44 open the frontlight panel (ActivityManager).
constexpr int TOP_RESERVE = 44;
constexpr int BUTTON_W = 200;
constexpr int BUTTON_H = 56;
constexpr int BORDER = 2;
constexpr int SPINE = 5;
constexpr int TEXT_INSET = 32;
constexpr int GAP = 16;
}  // namespace

void CoverflowHomeUi::begin(const std::vector<RecentBook>* recent) {
  books = recent;
  currentBook = 0;
}

int CoverflowHomeUi::bookCount() const { return books ? static_cast<int>(books->size()) : 0; }

bool CoverflowHomeUi::move(const bool next) {
  const int target = currentBook + (next ? 1 : -1);
  if (target < 0 || target >= bookCount()) return false;
  currentBook = target;
  return true;
}

Rect CoverflowHomeUi::contentArea() const {
  Rect area = UITheme::getContentArea(renderer);
  const int top = std::max(area.y, TOP_RESERVE);
  area.height -= top - area.y;
  area.y = top;
  return area;
}

Rect CoverflowHomeUi::centreCoverRect() const {
  const Rect area = contentArea();
  // Keep 2:3-ish proportions when the screen is smaller than the design size.
  const int height = std::min(COVER_H, area.height);
  const int width = std::min(area.width, height * COVER_W / COVER_H);
  const int scaledHeight = width * COVER_H / COVER_W;
  return Rect{area.x + (area.width - width) / 2, area.y + (area.height - scaledHeight) / 2, width, scaledHeight};
}

Rect CoverflowHomeUi::sideCoverRect(const bool right) const {
  const Rect centre = centreCoverRect();
  const int height = centre.height * SIDE_H / COVER_H;
  const int width = height * COVER_W / COVER_H;
  const int y = centre.y + centre.height - height;
  const int x = right ? centre.x + centre.width + SIDE_GAP : centre.x - SIDE_GAP - width;
  return Rect{x, y, width, height};
}

Rect CoverflowHomeUi::libraryButtonRect() const {
  const Rect area = contentArea();
  return Rect{area.x + (area.width - BUTTON_W) / 2, area.y + area.height / 2 + GAP, BUTTON_W, BUTTON_H};
}

void CoverflowHomeUi::draw() const {
  if (!books || books->empty()) {
    drawEmpty();
    return;
  }
  if (currentBook > 0) drawSideCover(false);
  if (currentBook + 1 < bookCount()) drawSideCover(true);
  drawTitleCard(centreCoverRect(), (*books)[currentBook]);
}

// Only the edge of a neighbour shows: outline plus a light tone.
void CoverflowHomeUi::drawSideCover(const bool right) const {
  const Rect rect = sideCoverRect(right);
  const int left = std::max(0, rect.x);
  const int width = std::min(renderer.getScreenWidth(), rect.x + rect.width) - left;
  if (width <= 0) return;
  renderer.fillRectDither(left, rect.y, width, rect.height, Color::LightGray);
  renderer.fillRect(left, rect.y, width, BORDER, true);
  renderer.fillRect(left, rect.y + rect.height - BORDER, width, BORDER, true);
  const int edgeX = right ? rect.x : rect.x + rect.width - BORDER;
  renderer.fillRect(edgeX, rect.y, BORDER, rect.height, true);
}

void CoverflowHomeUi::drawEmpty() const {
  const Rect area = contentArea();
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int textH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect hint{area.x + TEXT_INSET, area.y + area.height / 2 - GAP - 2 * textH, area.width - 2 * TEXT_INSET,
                  2 * textH};
  UITheme::drawCenteredText(renderer, area, UI_12_FONT_ID, hint.y - GAP / 2 - titleH, tr(STR_COVERFLOW_EMPTY), true,
                            EpdFontFamily::BOLD);
  UITheme::drawCenteredWrappedText(renderer, hint, UI_10_FONT_ID, tr(STR_COVERFLOW_EMPTY_HINT), 2);
  const Rect button = libraryButtonRect();
  renderer.drawRect(button.x, button.y, button.width, button.height, BORDER, true);
  UITheme::drawCenteredText(renderer, button, UI_10_FONT_ID, button.y + (button.height - textH) / 2,
                            tr(STR_COVERFLOW_OPEN_LIBRARY), true, EpdFontFamily::BOLD);
}

// Title and author on a plain card, for books shown without cover art.
void CoverflowHomeUi::drawTitleCard(const Rect rect, const RecentBook& book) const {
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height, BORDER, true);
  renderer.fillRect(rect.x + BORDER, rect.y + BORDER, SPINE, rect.height - 2 * BORDER, true);
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int authorH = renderer.getLineHeight(UI_10_FONT_ID);
  const Rect title{rect.x + TEXT_INSET, rect.y + rect.height / 3, rect.width - 2 * TEXT_INSET, 3 * titleH};
  const int ruleX = rect.x + TEXT_INSET;
  const int ruleW = rect.width - 2 * TEXT_INSET;
  renderer.fillRect(ruleX, title.y - GAP, ruleW, BORDER, true);
  UITheme::drawCenteredWrappedText(renderer, title, UI_12_FONT_ID, book.title.c_str(), 3, true, EpdFontFamily::BOLD);
  const int authorY = title.y + title.height + GAP / 2;
  if (!book.author.empty()) {
    const auto author = renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), ruleW);
    UITheme::drawCenteredText(renderer, rect, UI_10_FONT_ID, authorY, author.c_str());
  }
  renderer.fillRect(ruleX, authorY + authorH + GAP, ruleW, BORDER, true);
}

bool CoverflowHomeUi::tapped(const Rect rect) const {
  int row = -1;
  return input.rowTouch(row, rect.y, rect.height, 1, rect.x, rect.x + rect.width, rect.height) ==
         MappedInputManager::RowTouch::Tap;
}

CoverflowHomeUi::Request CoverflowHomeUi::handleTouch() {
  if (bookCount() == 0) return tapped(libraryButtonRect()) ? Request::OpenLibrary : Request::None;
  const auto swipe = input.wasSwipe();
  // A right swipe from the left edge is also Back; Back does nothing on this Home.
  if (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right) {
    return move(swipe == MappedInputManager::SwipeDir::Left) ? Request::Redraw : Request::None;
  }
  const Rect centre = centreCoverRect();
  if (tapped(centre)) return Request::OpenBook;
  // The whole strip beside the centre cover is a neighbour's target, not just its visible edge.
  const int screenWidth = renderer.getScreenWidth();
  if (tapped(Rect{0, centre.y, centre.x, centre.height})) return move(false) ? Request::Redraw : Request::None;
  const int rightX = centre.x + centre.width;
  if (tapped(Rect{rightX, centre.y, screenWidth - rightX, centre.height})) {
    return move(true) ? Request::Redraw : Request::None;
  }
  return Request::None;
}
