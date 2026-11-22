#include "TodoPresentation.h"

#include <I18n.h>

#include <cstdio>

#include "util/ReadingCalendarClock.h"
#include "util/ReadingCalendarMonth.h"

namespace {
constexpr StrId MONTHS[] = {StrId::STR_WEATHER_JAN, StrId::STR_WEATHER_FEB, StrId::STR_WEATHER_MAR,
                            StrId::STR_WEATHER_APR, StrId::STR_WEATHER_MAY, StrId::STR_WEATHER_JUN,
                            StrId::STR_WEATHER_JUL, StrId::STR_WEATHER_AUG, StrId::STR_WEATHER_SEP,
                            StrId::STR_WEATHER_OCT, StrId::STR_WEATHER_NOV, StrId::STR_WEATHER_DEC};
}  // namespace

void formatTodoShortDate(int32_t day, char* text, size_t size) {
  if (!day) {
    if (size) text[0] = '\0';
    return;
  }
  const auto month = readingcalendar::monthOf(day);
  const unsigned date = day - readingcalendar::civilDay(month.year, month.month, 1) + 1;
  std::snprintf(text, size, tr(STR_TODO_SHORT_DATE), date, I18N.get(MONTHS[month.month - 1]));
}

int32_t todoToday() { return readingcalendar::localMilliseconds() / readingcalendar::Model::MAX_DAY_MS; }

bool todoOverdue(const TodoModel::Task& task, int32_t today) {
  return !task.completed && today > 0 && task.due > 0 && task.due < today;
}

const char* todoPriority(uint8_t priority) {
  static constexpr const char* MARKS[] = {"–", "!", "!!", "!!!"};
  return MARKS[priority <= 3 ? priority : 0];
}

void formatTodoDates(const TodoModel::Task& task, int32_t today, char* text, size_t size) {
  char created[40];
  char due[40];
  formatTodoShortDate(task.created, created, sizeof(created));
  formatTodoShortDate(task.due, due, sizeof(due));
  if (todoOverdue(task, today)) {
    std::snprintf(text, size, tr(STR_TODO_OVERDUE_DATE), due);
  } else if (task.created && task.due) {
    std::snprintf(text, size, tr(STR_TODO_CREATED_DUE), created, due);
  } else if (task.due) {
    std::snprintf(text, size, tr(STR_TODO_DUE_DATE), due);
  } else {
    std::snprintf(text, size, "%s", created);
  }
}

void formatTodoFullDate(int32_t day, char* text, size_t size, bool weekday) {
  if (!day) {
    std::snprintf(text, size, "%s", tr(STR_TODO_NO_DATE));
    return;
  }
  static constexpr StrId WEEKDAYS[] = {StrId::STR_TODO_MON, StrId::STR_TODO_TUE, StrId::STR_TODO_WED,
                                       StrId::STR_TODO_THU, StrId::STR_TODO_FRI, StrId::STR_TODO_SAT,
                                       StrId::STR_TODO_SUN};
  const auto month = readingcalendar::monthOf(day);
  const unsigned date = day - readingcalendar::civilDay(month.year, month.month, 1) + 1;
  if (weekday) {
    std::snprintf(text, size, tr(STR_TODO_FULL_DATE), I18N.get(WEEKDAYS[readingcalendar::weekday(day)]), date,
                  I18N.get(MONTHS[month.month - 1]), month.year);
  } else {
    std::snprintf(text, size, tr(STR_TODO_YEAR_DATE), date, I18N.get(MONTHS[month.month - 1]), month.year);
  }
}

void formatTodoRelative(int32_t due, int32_t today, char* text, size_t size) {
  if (!due || !today) {
    if (size) text[0] = '\0';
    return;
  }
  const int32_t days = due - today;
  if (!days)
    std::snprintf(text, size, "%s", tr(STR_TODO_DUE_TODAY));
  else if (days == 1)
    std::snprintf(text, size, "%s", tr(STR_TODO_DUE_ONE));
  else if (days == -1)
    std::snprintf(text, size, "%s", tr(STR_TODO_OVERDUE_ONE));
  else if (days > 0)
    std::snprintf(text, size, tr(STR_TODO_DUE_IN), static_cast<long>(days));
  else
    std::snprintf(text, size, tr(STR_TODO_OVERDUE_BY), static_cast<long>(-days));
}

const char* todoStatusText(TodoStore::Status status) {
  switch (status) {
    case TodoStore::Status::Ok:
      return nullptr;
    case TodoStore::Status::InvalidText:
      return tr(STR_TODO_INVALID_TEXT);
    case TodoStore::Status::Full:
      return tr(STR_TODO_FULL);
    case TodoStore::Status::StorageError:
      return tr(STR_TODO_SAVE_ERROR);
    case TodoStore::Status::InvalidData:
      return tr(STR_TODO_DATA_ERROR);
    case TodoStore::Status::OutOfMemory:
      return tr(STR_TODO_MEMORY_ERROR);
  }
  return nullptr;
}
