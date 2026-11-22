#include "HomeTodoPreview.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <TodoOrder.h>
#include <TodoStore.h>

#include <algorithm>
#include <cstdio>

#include "components/TodoPresentation.h"
#include "fontIds.h"

namespace {
constexpr int TEXT_INSET = 16;

int markerWidth(const GfxRenderer& renderer) {
  return std::max(TEXT_INSET, renderer.getTextWidth(SMALL_FONT_ID, "!!!") + HomeTodoPreview::GAP);
}
}  // namespace

int HomeTodoPreview::rowHeight(const GfxRenderer& renderer) {
  return std::max(renderer.getLineHeight(SMALL_FONT_ID), ROW_MIN_HEIGHT);
}

void HomeTodoPreview::load(const GfxRenderer& renderer, const int width) {
  count = 0;
  open = 0;
  available = false;
  auto store = makeUniqueNoThrow<TodoStore>();
  if (!store) {
    LOG_ERR("HOME", "OOM: Todo snapshot (%u bytes)", static_cast<unsigned>(sizeof(TodoStore)));
    return;
  }
  if (store->load() != TodoStore::Status::Ok) {
    LOG_ERR("HOME", "Todo snapshot load failed");
    return;
  }
  available = true;
  const auto& model = store->model();
  for (size_t i = 0; i < model.size(); ++i) {
    if (!model.at(i)->completed) ++open;
  }
  const int textWidth = width - markerWidth(renderer);
  TodoOrder order;
  order.reset(model);
  const int32_t today = todoToday();
  for (size_t row = 0; row < model.size() && count < ROWS; ++row) {
    const int index = order.modelIndex(model, row);
    if (index < 0) break;
    const auto& task = *model.at(index);
    if (task.completed) break;
    priorities[count] = task.priority;
    formatTodoShortDate(task.due, dueTexts[count], sizeof(dueTexts[count]));
    const int dateWidth = renderer.getTextWidth(SMALL_FONT_ID, dueTexts[count]);
    const int room = textWidth - (dateWidth ? dateWidth + GAP : 0);
    const char* prefix = todoOverdue(task, today) && room >= 100 ? "* " : "";
    const auto text = renderer.truncatedText(SMALL_FONT_ID, task.text,
                                             std::max(0, room - renderer.getTextWidth(SMALL_FONT_ID, prefix)));
    snprintf(texts[count], sizeof(texts[count]), "%s%s", prefix, text.c_str());
    ++count;
  }
}

void HomeTodoPreview::drawRows(const GfxRenderer& renderer, const int x, const int top, const int width) const {
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = rowHeight(renderer);
  const int marker = markerWidth(renderer);
  for (size_t i = 0; i < count; ++i) {
    const int y = top + static_cast<int>(i) * rowH;
    const int textY = y + (rowH - lineH) / 2;
    if (priorities[i]) {
      renderer.drawText(SMALL_FONT_ID, x, textY, todoPriority(priorities[i]), true);
    } else {
      renderer.fillRoundedRect(x + 3, y + (rowH - 5) / 2, 5, 5, 2, Color::Black);
    }
    renderer.drawText(SMALL_FONT_ID, x + marker, textY, texts[i], true);
    const int dateWidth = renderer.getTextWidth(SMALL_FONT_ID, dueTexts[i]);
    if (dateWidth) renderer.drawText(SMALL_FONT_ID, x + width - dateWidth, textY, dueTexts[i], true);
  }
  if (!count) {
    const char* text = available ? tr(STR_TODO_NONE) : tr(STR_TODO_UNAVAILABLE);
    renderer.drawText(SMALL_FONT_ID, x, top + (rowH - lineH) / 2, text, true);
  }
}
