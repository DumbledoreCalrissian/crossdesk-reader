#pragma once

#include <Bitmap.h>
#include <HalStorage.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "CoverflowSheet.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/HomeCoverCache.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;
struct BookReadingSummary;

// Coverflow theme Home: recent covers, optional progress and an apps sheet.
class CoverflowHomeUi final {
 public:
  // What a pass of touch input asked Home to do.
  enum class Request : uint8_t { None, Redraw, OpenBook, OpenLibrary, OpenMenuItem };

  CoverflowHomeUi(GfxRenderer& renderer, MappedInputManager& input)
      : renderer(renderer), input(input), coverCache(renderer), sheet(renderer, input) {}
  // Home owns the books and matching summaries for the visit, most recent first.
  // A null summaries pointer disables the progress line.
  // slot: Plugins, OPDS or NONE, for the apps sheet.
  void begin(const std::vector<RecentBook>* books, HomeMenuItem slot, const BookReadingSummary* summaries);
  void draw();
  // Thumbs are generated at the drawn heights; rescaling dithered art aliases.
  int centreThumbHeight() const { return centreSlot().height; }
  int sideThumbHeight() const { return sideSlotHeight(); }
  // Reads each thumb's width once, after the thumbs exist, so frames hug the art.
  void measureCovers();
  Request handleTouch();
  // The apps sheet; Back closes it, and Up/Down do nothing while it is open.
  bool sheetOpen() const { return sheet.isOpen(); }
  void closeSheet() { sheet.close(); }
  HomeMenuItem chosenMenuItem() const { return sheet.chosen(); }
  // Moves the centre by one book; false at either end (no wrap-around).
  bool move(bool next);
  // Index into books of the centre cover.
  int current() const { return currentBook.load(); }
  // Fast refresh per move leaves ghosts; every Nth redraw is a half refresh.
  bool takeHalfRefreshDue();

 private:
  int bookCount() const;
  Rect contentArea() const;
  // Largest centre frame; covers without art and the cache budget use it.
  Rect centreSlot(const BookReadingSummary* summary = nullptr) const;
  int progressHeight(const BookReadingSummary& summary) const;
  int sideSlotHeight() const;
  // Frames narrowed to each book's art width.
  Rect centreCoverRect(int book) const;
  // Full rect of a neighbour cover; only the part on screen is drawn.
  Rect sideCoverRect(Rect centre, bool right, int centreBook) const;
  int thumbWidth(const std::string& path);
  Rect libraryButtonRect() const;
  void drawHome();
  void drawEmpty() const;
  Rect arrowRect() const;
  void drawArrow() const;
  void drawTitleCard(Rect rect, const RecentBook& book) const;
  void drawProgress(Rect cover, const RecentBook& book, const BookReadingSummary& summary) const;
  void drawSideCover(Rect centre, bool right, int centreBook);
  void drawHalo(Rect cover) const;
  bool drawSideArt(Rect rect, int left, int width, const std::string& path);
  bool tapped(Rect rect) const;

  GfxRenderer& renderer;
  MappedInputManager& input;
  HomeCoverCache coverCache;
  CoverflowSheet sheet;
  HomeMenuItem slot = HomeMenuItem::NONE;
  // Neighbour edges are drawn straight from their thumbs; only their visible strip is painted.
  HalFile sideFile;
  Bitmap sideBitmap{sideFile};
  const std::vector<RecentBook>* books = nullptr;
  const BookReadingSummary* summaries = nullptr;
  // Written by input (main task), read by render; draw() reads it once.
  std::atomic<int> currentBook{0};
  // Only touched on the render task, so no lock.
  uint8_t redrawsSinceHalfRefresh = 0;
  // Art width per book at each thumb height; 0 uses the full frame width.
  std::array<int16_t, RecentBooksStore::MAX_RECENT_BOOKS> centreArtWidth{};
  std::array<int16_t, RecentBooksStore::MAX_RECENT_BOOKS> sideArtWidth{};
};
