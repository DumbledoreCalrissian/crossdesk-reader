#include "CoverflowHomeUi.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

// Cache slots are indexed by recent-book position.
static_assert(RecentBooksStore::MAX_RECENT_BOOKS <= HomeCoverCache::MAX_COVERS);

namespace {
// Portrait design size; other sizes scale to the content area.
constexpr int COVER_W = 372;
constexpr int COVER_H = 520;
// Neighbours stand on the centre cover's floor line, one gap away from it.
constexpr int SIDE_H = 450;
constexpr int SIDE_GAP = 12;
// Floor shadow below each cover: a solid contact line, then a dithered band
// that fades and widens downward.
constexpr int SHADOW_H = 34;
constexpr int SIDE_SHADOW_H = 22;
constexpr int CONTACT_H = 2;
constexpr int SHADOW_START_DENSITY = 10;  // of 16, at the contact line
// 4x4 ordered-dither thresholds.
constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
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
  centreArtWidth.fill(0);
  sideArtWidth.fill(0);
  // One centre snapshot per recent book (about 24 KB each in portrait) lets a
  // revisited cover paste back without reading the SD card.
  const Rect centre = centreSlot();
  const size_t perCover = renderer.getRegionByteSize(centre.x, centre.y, centre.width, centre.height) +
                          static_cast<size_t>(std::max(0, centre.height));
  coverCache.begin(perCover * RecentBooksStore::MAX_RECENT_BOOKS);
}

bool CoverflowHomeUi::takeHalfRefreshDue() {
  constexpr uint8_t REDRAWS_PER_HALF_REFRESH = 8;
  if (++redrawsSinceHalfRefresh < REDRAWS_PER_HALF_REFRESH) return false;
  redrawsSinceHalfRefresh = 0;
  return true;
}

int CoverflowHomeUi::bookCount() const { return books ? static_cast<int>(books->size()) : 0; }

bool CoverflowHomeUi::move(const bool next) {
  const int target = currentBook.load() + (next ? 1 : -1);
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

Rect CoverflowHomeUi::centreSlot() const {
  const Rect area = contentArea();
  // Keep 2:3-ish proportions when the screen is smaller than the design size.
  const int height = std::min(COVER_H, area.height);
  const int width = std::min(area.width, height * COVER_W / COVER_H);
  const int scaledHeight = width * COVER_H / COVER_W;
  // The cover and its shadow are centred together.
  const int top = area.y + std::max(0, (area.height - scaledHeight - SHADOW_H) / 2);
  return Rect{area.x + (area.width - width) / 2, top, width, scaledHeight};
}

int CoverflowHomeUi::sideSlotHeight() const { return centreSlot().height * SIDE_H / COVER_H; }

// Narrows a frame around art of artWidth, keeping it centred; art wider than the frame is cropped.
static Rect fitToArt(const Rect slot, const int artWidth) {
  if (artWidth <= 0 || artWidth >= slot.width) return slot;
  return Rect{slot.x + (slot.width - artWidth) / 2, slot.y, artWidth, slot.height};
}

Rect CoverflowHomeUi::centreCoverRect(const int book) const {
  return fitToArt(centreSlot(), book >= 0 && book < bookCount() ? centreArtWidth[book] : 0);
}

int CoverflowHomeUi::thumbWidth(const std::string& path) {
  if (!Storage.openFileForRead("HOME", path, sideFile)) return 0;
  const int width = sideBitmap.parseHeaders() == BmpReaderError::Ok ? sideBitmap.getWidth() : 0;
  sideFile.close();
  return width;
}

void CoverflowHomeUi::measureCovers() {
  const int count = std::min(bookCount(), static_cast<int>(centreArtWidth.size()));
  for (int i = 0; i < count; ++i) {
    const std::string& path = (*books)[i].coverBmpPath;
    if (path.empty()) continue;
    centreArtWidth[i] = static_cast<int16_t>(thumbWidth(UITheme::getCoverThumbPath(path, centreThumbHeight())));
    sideArtWidth[i] = static_cast<int16_t>(thumbWidth(UITheme::getCoverThumbPath(path, sideThumbHeight())));
  }
}

Rect CoverflowHomeUi::sideCoverRect(const bool right, const int centreBook) const {
  const Rect centre = centreCoverRect(centreBook);
  const int height = sideSlotHeight();
  const int fullWidth = height * COVER_W / COVER_H;
  const int book = centreBook + (right ? 1 : -1);
  const int art = book >= 0 && book < bookCount() ? sideArtWidth[book] : 0;
  const int width = art > 0 ? std::min(art, fullWidth) : fullWidth;
  const int y = centre.y + centre.height - height;
  const int x = right ? centre.x + centre.width + SIDE_GAP : centre.x - SIDE_GAP - width;
  return Rect{x, y, width, height};
}

bool CoverflowHomeUi::drawSideArt(const Rect rect, const int left, const int width, const std::string& path) {
  if (!Storage.openFileForRead("HOME", path, sideFile)) return false;
  bool drawn = false;
  if (sideBitmap.parseHeaders() == BmpReaderError::Ok && sideBitmap.getWidth() > 0 && sideBitmap.getHeight() > 0) {
    const auto clip = renderer.getClipRect();
    renderer.setClipRect(left, rect.y, width, rect.height);
    drawn = renderer.drawBitmap(sideBitmap, rect.x + (rect.width - sideBitmap.getWidth()) / 2,
                                rect.y + (rect.height - sideBitmap.getHeight()) / 2, sideBitmap.getWidth(),
                                sideBitmap.getHeight());
    renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
  }
  sideFile.close();
  if (!drawn) LOG_DBG("HOME", "Side cover not drawn: %s", path.c_str());
  return drawn;
}

Rect CoverflowHomeUi::libraryButtonRect() const {
  const Rect area = contentArea();
  return Rect{area.x + (area.width - BUTTON_W) / 2, area.y + area.height / 2 + GAP, BUTTON_W, BUTTON_H};
}

void CoverflowHomeUi::draw() {
  if (!books || books->empty()) {
    drawEmpty();
    return;
  }
  coverCache.prepare();
  // One read: a swipe during this draw must not mix two books' layouts.
  const int cur = currentBook.load();
  if (cur > 0) drawSideCover(false, cur);
  if (cur + 1 < bookCount()) drawSideCover(true, cur);
  const Rect shown = centreCoverRect(cur);
  // The centre's shadow sits in front of the neighbours' shadows.
  const int spread = SHADOW_H * 3 / 5;
  renderer.fillRect(shown.x - spread, shown.y + shown.height, shown.width + 2 * spread, SHADOW_H, false);
  drawFloorShadow(shown, SHADOW_H);
  const RecentBook& book = (*books)[cur];
  // No usable art: an empty path, or a thumb that could not be read when measured.
  if (book.coverBmpPath.empty() || centreArtWidth[cur] <= 0) {
    drawTitleCard(centreSlot(), book);
    return;
  }
  const Rect centre = shown;
  coverCache.paint(freeink::ui::Rect{static_cast<int16_t>(centre.x), static_cast<int16_t>(centre.y),
                                     static_cast<int16_t>(centre.width), static_cast<int16_t>(centre.height)},
                   static_cast<size_t>(cur), UITheme::getCoverThumbPath(book.coverBmpPath, centreThumbHeight()),
                   /*artShift=*/0);
}

// Only the edge of a neighbour shows: its art (or a light tone) and outline.
void CoverflowHomeUi::drawSideCover(const bool right, const int centreBook) {
  const Rect rect = sideCoverRect(right, centreBook);
  const int left = std::max(0, rect.x);
  const int width = std::min(renderer.getScreenWidth(), rect.x + rect.width) - left;
  if (width <= 0) return;
  const RecentBook& book = (*books)[centreBook + (right ? 1 : -1)];
  const bool art = !book.coverBmpPath.empty() &&
                   drawSideArt(rect, left, width, UITheme::getCoverThumbPath(book.coverBmpPath, rect.height));
  if (!art) renderer.fillRectDither(left, rect.y, width, rect.height, Color::LightGray);
  renderer.fillRect(left, rect.y, width, BORDER, true);
  renderer.fillRect(left, rect.y + rect.height - BORDER, width, BORDER, true);
  const int edgeX = right ? rect.x : rect.x + rect.width - BORDER;
  renderer.fillRect(edgeX, rect.y, BORDER, rect.height, true);
  drawFloorShadow(rect, SIDE_SHADOW_H);
}

// Widens by 3 px every 5 rows as it fades; pixels off screen are clipped.
void CoverflowHomeUi::drawFloorShadow(const Rect cover, const int depth) const {
  const int base = cover.y + cover.height;
  renderer.fillRect(cover.x, base, cover.width, CONTACT_H, true);
  const int fadeRows = depth - CONTACT_H;
  for (int row = 0; row < fadeRows; ++row) {
    const int y = base + CONTACT_H + row;
    const int density = SHADOW_START_DENSITY * (fadeRows - row) / fadeRows;
    const int spread = (CONTACT_H + row) * 3 / 5;
    const int left = std::max(0, cover.x - spread);
    const int right = std::min(renderer.getScreenWidth(), cover.x + cover.width + spread);
    for (int x = left; x < right; ++x) {
      if (BAYER[y & 3][x & 3] < density) renderer.drawPixel(x, y, true);
    }
  }
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
  const Rect centre = centreCoverRect(currentBook.load());
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
