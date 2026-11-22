#pragma once

#include "TodoModel.h"

// Freeze open-task order until reset; completion groups and saved-order done
// tasks remain live. New or reopened tasks absent from the snapshot follow it.
class TodoOrder {
 public:
  void reset(const TodoModel& model);
  int modelIndex(const TodoModel& model, int row) const;
  int rowOf(const TodoModel& model, size_t index) const;

 private:
  std::array<uint32_t, TodoModel::MAX_TASKS> ids{};
  size_t count = 0;
};
