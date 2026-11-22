#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class TodoModel {
 public:
  static constexpr size_t MAX_TASKS = 32;
  static constexpr size_t MAX_TEXT_BYTES = 192;
  static constexpr int32_t MAX_DAY = 2932896;  // civilDay(9999, 12, 31); zero means no date.
  enum class Sort : uint8_t { AddedOrder, Priority, DueDate, Created };
  struct Task {
    uint32_t id = 0;
    bool completed = false;
    uint8_t priority = 0;
    char text[MAX_TEXT_BYTES + 1]{};
    int32_t created = 0;
    int32_t due = 0;
  };
  enum class Result { Ok, Unchanged, InvalidText, InvalidData, Full, NotFound, IdExhausted };

  void clear() {
    count = 0;
    next = 1;
    order = Sort::AddedOrder;
    for (auto& task : tasks) task = {};
  }
  size_t size() const { return count; }
  const Task* at(size_t index) const { return index < count ? &tasks[index] : nullptr; }
  const Task* find(uint32_t id) const;
  uint32_t nextId() const { return next; }
  Sort sort() const { return order; }
  Result setSort(Sort sort);
  Result setPriority(uint32_t id, uint8_t priority);
  Result setDue(uint32_t id, int32_t due);
  Result add(std::string_view text, int32_t created = 0, int32_t due = 0);
  Result edit(uint32_t id, std::string_view text);
  Result toggle(uint32_t id);
  Result erase(uint32_t id);
  bool appendLoaded(uint32_t id, bool completed, std::string_view text, uint8_t priority = 0, int32_t created = 0,
                    int32_t due = 0);
  bool setNextId(uint32_t id);
  static bool validText(std::string_view text);
  static bool validDay(int32_t day) { return day >= 0 && day <= MAX_DAY; }
  static bool blank(std::string_view text) { return trimmed(text).empty(); }
  uint32_t checksum(uint32_t generation) const;

 private:
  std::array<Task, MAX_TASKS> tasks{};
  size_t count = 0;
  uint32_t next = 1;
  Sort order = Sort::AddedOrder;
  static std::string_view trimmed(std::string_view text);
};
