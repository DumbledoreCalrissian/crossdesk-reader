#include <TodoModel.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
int main() {
  auto m = std::make_unique<TodoModel>();
  using R = TodoModel::Result;
  assert(m->size() == 0 && !m->at(0));
  assert(m->add("") == R::InvalidText);
  assert(m->add(" \t\n") == R::InvalidText);
  assert(m->add(std::string(193, 'a')) == R::InvalidText);
  assert(m->add(std::string(192, 'a')) == R::Ok);
  assert(m->edit(1, "  Café 日本語 😀 & \"quoted\" ") == R::Ok);
  assert(std::string(m->at(0)->text) == "Café 日本語 😀 & \"quoted\"");
  assert(m->edit(1, m->at(0)->text) == R::Unchanged);
  assert(m->edit(1, "") == R::InvalidText);
  assert(m->toggle(1) == R::Ok && m->find(1)->completed);
  assert(m->toggle(1) == R::Ok && !m->find(1)->completed);
  assert(m->add(std::string("a\0b", 3)) == R::InvalidText);
  assert(m->add("\xc0\x80") == R::InvalidText);
  assert(m->add("\xed\xa0\x80") == R::InvalidText);
  assert(m->add("\xf4\x90\x80\x80") == R::InvalidText);
  assert(m->add("\xe3\x81") == R::InvalidText);
  for (int i = 1; i < 32; ++i) assert(m->add("task") == R::Ok);
  assert(m->add("one too many") == R::Full);
  assert(m->erase(1) == R::Ok && !m->find(1));
  assert(m->add("new") == R::Ok && m->at(31)->id == 33);
  assert(m->erase(999) == R::NotFound);
  assert(m->edit(999, "test") == R::NotFound);
  assert(m->toggle(999) == R::NotFound);
  assert(!m->appendLoaded(2, false, "duplicate"));
  assert(!m->setNextId(1));
  m->clear();
  assert(m->sort() == TodoModel::Sort::AddedOrder);
  assert(m->add("dated", 20779, 20785) == R::Ok);
  const auto checksum = m->checksum(17);
  assert(m->setPriority(1, 3) == R::Ok);
  assert(m->setPriority(1, 3) == R::Unchanged);
  assert(m->setPriority(1, 4) == R::InvalidData);
  assert(m->setPriority(99, 1) == R::NotFound);
  assert(m->setDue(1, -1) == R::InvalidData);
  assert(m->setDue(1, TodoModel::MAX_DAY + 1) == R::InvalidData);
  assert(m->setDue(1, TodoModel::MAX_DAY) == R::Ok);
  assert(m->setDue(1, 0) == R::Ok);
  assert(m->setDue(1, 0) == R::Unchanged);
  assert(m->setDue(99, 1) == R::NotFound);
  assert(m->setSort(TodoModel::Sort::Priority) == R::Ok);
  assert(m->setSort(TodoModel::Sort::Priority) == R::Unchanged);
  assert(m->setSort(static_cast<TodoModel::Sort>(4)) == R::InvalidData);
  assert(m->checksum(17) == checksum);
  assert(m->edit(1, "renamed") == R::Ok);
  assert(m->find(1)->created == 20779 && m->find(1)->priority == 3);
  assert(m->add("invalid", -1) == R::InvalidData && m->size() == 1);
  assert(m->add("invalid", 0, TodoModel::MAX_DAY + 1) == R::InvalidData && m->size() == 1);
  m->clear();
  assert(m->sort() == TodoModel::Sort::AddedOrder);
  assert(!m->appendLoaded(1, false, "bad", 4));
  assert(!m->appendLoaded(1, false, "bad", 0, -1));
  assert(!m->appendLoaded(1, false, "bad", 0, 0, TodoModel::MAX_DAY + 1));
  assert(m->appendLoaded(1, true, "loaded", 2, 20779, 20785));
  assert(m->find(1)->priority == 2 && m->find(1)->created == 20779 && m->find(1)->due == 20785);
  assert(sizeof(TodoModel::Task) == 208);
  assert(TodoModel::shiftDue(0, 0, 1) == 0);
  assert(TodoModel::shiftDue(0, 20000, 1) == 20001);
  assert(TodoModel::shiftDue(0, 20000, -7) == 19993);
  assert(TodoModel::shiftDue(20000, 0, -1) == 19999);
  assert(TodoModel::shiftDue(1, 0, -1) == 1);
  assert(TodoModel::shiftDue(7, 0, -7) == 7);
  assert(TodoModel::shiftDue(8, 0, -7) == 1);
  assert(TodoModel::shiftDue(TodoModel::MAX_DAY, 0, 1) == TodoModel::MAX_DAY);
  assert(TodoModel::shiftDue(TodoModel::MAX_DAY - 7, 0, 7) == TodoModel::MAX_DAY);
  assert(TodoModel::shiftDue(1, 0, INT32_MAX) == 1);
  assert(TodoModel::shiftDue(TodoModel::MAX_DAY, 0, INT32_MIN) == TodoModel::MAX_DAY);
  assert(TodoModel::shiftDue(0, -1, 1) == 0);
  std::cout << "Todo model checks passed; model bytes=" << sizeof(TodoModel) << '\n';
}
