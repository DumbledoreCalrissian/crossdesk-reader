#pragma once

#include <TodoModel.h>

#include <cstddef>
#include <cstdint>

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// The next open tasks, as shown by the Desk and Coverflow Todo widgets.
// Reads a Todo snapshot once; Todo owns edits and persistence.
struct HomeTodoPreview {
  static constexpr size_t ROWS = 3;
  static constexpr int ROW_MIN_HEIGHT = 24;
  static constexpr int GAP = 4;

  char texts[ROWS][TodoModel::MAX_TEXT_BYTES + 4]{};
  char dueTexts[ROWS][40]{};
  uint8_t priorities[ROWS]{};
  size_t count = 0;
  size_t open = 0;         // All incomplete tasks, shown or not
  bool available = false;  // Snapshot loaded; false shows "unavailable"

  // width: the row width the texts are truncated to.
  void load(const GfxRenderer& renderer, int width);
  static int rowHeight(const GfxRenderer& renderer);
  // Rows from top, or the empty/unavailable line when there are none.
  void drawRows(const GfxRenderer& renderer, int x, int top, int width) const;
};
