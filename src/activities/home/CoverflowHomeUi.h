#pragma once

#include <Bitmap.h>
#include <HalStorage.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/HomeCoverCache.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Coverflow theme Home: the recent books' covers and nothing else.
class CoverflowHomeUi final {
 public:
  // What a pass of touch input asked Home to do.
  enum class Request : uint8_t { None, Redraw, OpenBook, OpenLibrary };

  CoverflowHomeUi(GfxRenderer& renderer, MappedInputManager& input)
      : renderer(renderer), input(input), coverCache(renderer) {}
  // books stays owned by Home for the visit, most recent first.
  void begin(const std::vector<RecentBook>* books);
  void draw();
  // Thumbs are generated at the drawn heights; rescaling dithered art aliases.
  int centreThumbHeight() const { return centreSlot().height; }
  int sideThumbHeight() const { return sideSlotHeight(); }
  // Reads each thumb's width once, after the thumbs exist, so frames hug the art.
  void measureCovers();
  Request handleTouch();
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
  Rect centreSlot() const;
  int sideSlotHeight() const;
  // Frames narrowed to each book's art width.
  Rect centreCoverRect(int book) const;
  // Full rect of a neighbour cover; only the part on screen is drawn.
  Rect sideCoverRect(bool right, int centreBook) const;
  int thumbWidth(const std::string& path);
  Rect libraryButtonRect() const;
  void drawEmpty() const;
  void drawTitleCard(Rect rect, const RecentBook& book) const;
  void drawSideCover(bool right, int centreBook);
  void drawFloorShadow(Rect cover, int depth) const;
  bool drawSideArt(Rect rect, int left, int width, const std::string& path);
  bool tapped(Rect rect) const;

  GfxRenderer& renderer;
  MappedInputManager& input;
  HomeCoverCache coverCache;
  // Neighbour edges are drawn straight from their thumbs; only their visible strip is painted.
  HalFile sideFile;
  Bitmap sideBitmap{sideFile};
  const std::vector<RecentBook>* books = nullptr;
  // Written by input (main task), read by render; draw() reads it once.
  std::atomic<int> currentBook{0};
  // Only touched on the render task, so no lock.
  uint8_t redrawsSinceHalfRefresh = 0;
  // Art width per book at each thumb height; 0 uses the full frame width.
  std::array<int16_t, RecentBooksStore::MAX_RECENT_BOOKS> centreArtWidth{};
  std::array<int16_t, RecentBooksStore::MAX_RECENT_BOOKS> sideArtWidth{};
};
