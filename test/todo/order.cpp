#include <TodoOrder.h>

#include <cassert>
#include <initializer_list>
#include <iostream>
#include <memory>

static void expect(const TodoOrder& order, const TodoModel& model, std::initializer_list<uint32_t> ids) {
  int row = 0;
  for (const auto id : ids) {
    const int index = order.modelIndex(model, row);
    assert(index >= 0 && model.at(index)->id == id);
    assert(order.rowOf(model, index) == row++);
  }
  assert(order.modelIndex(model, row) == -1 && order.modelIndex(model, -1) == -1);
}

int main() {
  auto model = std::make_unique<TodoModel>();
  TodoOrder order;
  using S = TodoModel::Sort;
  assert(model->appendLoaded(1, false, "unknown", 0));
  assert(model->appendLoaded(2, false, "first high", 3, 20, 25));
  assert(model->appendLoaded(3, true, "done high", 3, 30, 1));
  assert(model->appendLoaded(4, false, "earliest", 1, 20, 10));
  assert(model->appendLoaded(5, false, "second high", 3, 10, 25));
  assert(model->appendLoaded(6, true, "done low", 0, 0, 0));
  model->setSort(S::AddedOrder);
  order.reset(*model);
  expect(order, *model, {1, 2, 4, 5, 3, 6});
  model->setSort(S::Priority);
  order.reset(*model);
  expect(order, *model, {2, 5, 4, 1, 3, 6});
  model->setPriority(1, 3);
  model->setDue(1, 1);
  expect(order, *model, {2, 5, 4, 1, 3, 6});  // frozen during edits
  order.reset(*model);
  expect(order, *model, {1, 2, 5, 4, 3, 6});
  model->setDue(1, 0);
  model->setSort(S::DueDate);
  order.reset(*model);
  expect(order, *model, {4, 2, 5, 1, 3, 6});
  model->setSort(S::Created);
  order.reset(*model);
  expect(order, *model, {2, 4, 5, 1, 3, 6});
  model->toggle(2);
  expect(order, *model, {4, 5, 1, 2, 3, 6});  // done always in saved order
  model->toggle(3);
  expect(order, *model, {3, 4, 5, 1, 2, 6});  // reopened task uses its frozen sort position
  model->erase(4);
  expect(order, *model, {3, 5, 1, 2, 6});  // deletion cannot corrupt saved-index mapping
  model->add("new", 50, 1);
  expect(order, *model, {3, 5, 1, 7, 2, 6});
  order.reset(*model);
  expect(order, *model, {7, 3, 5, 1, 2, 6});
  for (size_t i = 0; i < model->size(); ++i) assert(model->at(i)->id == (i < 3 ? i + 1 : i + 2));
  model->clear();
  order.reset(*model);
  expect(order, *model, {});
  for (size_t i = 0; i < 32; ++i) {
    model->add("task", i + 1, 32 - i);
    model->setPriority(i + 1, i % 4);
  }
  for (auto sort : {S::AddedOrder, S::Priority, S::DueDate, S::Created}) {
    model->setSort(sort);
    order.reset(*model);
    uint32_t seen = 0;
    for (int row = 0; row < 32; ++row) {
      const int index = order.modelIndex(*model, row);
      assert(index >= 0 && index < 32 && !(seen & (uint32_t{1} << index)));
      seen |= uint32_t{1} << index;
      assert(order.rowOf(*model, index) == row);
    }
    assert(seen == UINT32_MAX);
  }
  std::cout << "Todo display order checks passed; order bytes=" << sizeof(order) << '\n';
}
