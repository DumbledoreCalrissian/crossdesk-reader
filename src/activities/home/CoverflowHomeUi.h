#pragma once

#include <cstdint>
#include <vector>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Coverflow theme Home: the recent books' covers and nothing else.
class CoverflowHomeUi final {
 public:
  // What a pass of touch input asked Home to do.
  enum class Request : uint8_t { None, Redraw, OpenBook, OpenLibrary };

  CoverflowHomeUi(GfxRenderer& renderer, MappedInputManager& input) : renderer(renderer), input(input) {}
  // books stays owned by Home for the visit, most recent first.
  void begin(const std::vector<RecentBook>* books);
  void draw() const;
  Request handleTouch();
  // Moves the centre by one book; false at either end (no wrap-around).
  bool move(bool next);
  // Index into books of the centre cover.
  int current() const { return currentBook; }

 private:
  int bookCount() const;
  Rect contentArea() const;
  Rect centreCoverRect() const;
  // Full rect of a neighbour cover; only the part on screen is drawn.
  Rect sideCoverRect(bool right) const;
  Rect libraryButtonRect() const;
  void drawEmpty() const;
  void drawTitleCard(Rect rect, const RecentBook& book) const;
  void drawSideCover(bool right) const;
  bool tapped(Rect rect) const;

  GfxRenderer& renderer;
  MappedInputManager& input;
  const std::vector<RecentBook>* books = nullptr;
  int currentBook = 0;
};
