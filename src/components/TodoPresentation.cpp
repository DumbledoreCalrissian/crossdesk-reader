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
void shortDate(int32_t day, char* text, size_t size) {
  if (!day) {
    if (size) text[0] = '\0';
    return;
  }
  const auto month = readingcalendar::monthOf(day);
  const unsigned date = day - readingcalendar::civilDay(month.year, month.month, 1) + 1;
  std::snprintf(text, size, tr(STR_TODO_SHORT_DATE), date, I18N.get(MONTHS[month.month - 1]));
}
}  // namespace

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
  shortDate(task.created, created, sizeof(created));
  shortDate(task.due, due, sizeof(due));
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
