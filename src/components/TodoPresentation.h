#pragma once

#include <TodoStore.h>

int32_t todoToday();
bool todoOverdue(const TodoModel::Task& task, int32_t today);
const char* todoPriority(uint8_t priority);
void formatTodoShortDate(int32_t day, char* text, size_t size);
void formatTodoDates(const TodoModel::Task& task, int32_t today, char* text, size_t size);
void formatTodoFullDate(int32_t day, char* text, size_t size, bool weekday = true);
void formatTodoRelative(int32_t due, int32_t today, char* text, size_t size);
const char* todoStatusText(TodoStore::Status status);
