#pragma once

#include <TodoModel.h>

int32_t todoToday();
bool todoOverdue(const TodoModel::Task& task, int32_t today);
const char* todoPriority(uint8_t priority);
void formatTodoDates(const TodoModel::Task& task, int32_t today, char* text, size_t size);
