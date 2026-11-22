#include "TodoOrder.h"

namespace {
bool before(const TodoModel::Task& a, const TodoModel::Task& b, TodoModel::Sort sort) {
  switch (sort) {
    case TodoModel::Sort::AddedOrder:
      return false;
    case TodoModel::Sort::Priority:
      return a.priority > b.priority;
    case TodoModel::Sort::DueDate:
      return a.due && (!b.due || a.due < b.due);
    case TodoModel::Sort::Created:
      return a.created > b.created;
  }
  return false;
}
}  // namespace

void TodoOrder::reset(const TodoModel& model) {
  count = 0;
  for (size_t i = 0; i < model.size(); ++i) {
    const auto& task = *model.at(i);
    size_t position = count;
    while (position && before(task, *model.find(ids[position - 1]), model.sort())) {
      ids[position] = ids[position - 1];
      --position;
    }
    ids[position] = task.id;
    ++count;
  }
}

int TodoOrder::modelIndex(const TodoModel& model, int row) const {
  if (row < 0) return -1;
  for (size_t rank = 0; rank < count; ++rank) {
    for (size_t i = 0; i < model.size(); ++i) {
      const auto& task = *model.at(i);
      if (task.id == ids[rank] && !task.completed && row-- == 0) return static_cast<int>(i);
    }
  }
  for (size_t i = 0; i < model.size(); ++i) {
    const auto& task = *model.at(i);
    if (task.completed) continue;
    bool known = false;
    for (size_t rank = 0; rank < count; ++rank) known |= ids[rank] == task.id;
    if (!known && row-- == 0) return static_cast<int>(i);
  }
  for (size_t i = 0; i < model.size(); ++i)
    if (model.at(i)->completed && row-- == 0) return static_cast<int>(i);
  return -1;
}

int TodoOrder::rowOf(const TodoModel& model, size_t index) const {
  for (size_t row = 0; row < model.size(); ++row)
    if (modelIndex(model, row) == static_cast<int>(index)) return static_cast<int>(row);
  return -1;
}
