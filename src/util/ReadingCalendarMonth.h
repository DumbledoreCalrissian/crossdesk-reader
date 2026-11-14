#pragma once

#include <array>
#include <cstdint>

#include "ReadingCalendar.h"

namespace readingcalendar {
struct Month {
  int year = 0;
  unsigned month = 0;  // 1-12
  bool operator==(const Month&) const = default;
  Month previous() const { return month == 1 ? Month{year - 1, 12} : Month{year, month - 1}; }
  Month next() const { return month == 12 ? Month{year + 1, 1} : Month{year, month + 1}; }
};

// Shade of one grid cell. Untracked: before history starts or outside the retained window.
enum class DayLevel : uint8_t { Untracked, Future, None, BelowGoal, Goal, HourPlus };

struct MonthSummary {
  static constexpr unsigned MAX_DAYS = 31;
  static constexpr uint32_t HOUR_MS = 3600000;

  Month month;
  uint8_t dayCount = 0;
  uint8_t firstWeekday = 0;  // Monday = 0
  uint8_t today = 0;         // Day of month, 0 when the month is not the current one
  std::array<DayLevel, MAX_DAYS> levels{};
  uint8_t trackedDays = 0;  // Days history covers; for the current month, only up to today
  uint8_t daysRead = 0;
  uint8_t goalDays = 0;
  uint64_t minutes = 0;  // Rounded down after adding the month's time
  uint8_t bestDay = 0;   // 0 when nothing was read; earliest day on ties
  uint32_t bestMilliseconds = 0;
  uint8_t longestRun = 0;  // Consecutive goal days inside this month
  uint8_t longestRunStart = 0;
  uint32_t streak = 0;  // Model::streak(today), current month only
  // Previous month: the whole month, or days 1..today for the current month.
  bool hasComparison = false;
  uint8_t comparedDays = 0;
  uint64_t previousMinutes = 0;
};

// Oldest retained record, or 0 without history. Once older days leave the 366-day
// window the model cannot tell when history began, so earlier days count as untracked.
int32_t firstTrackedDay(const Model& model);
Month monthOf(int32_t date);
uint8_t weekday(int32_t date);  // Monday = 0

// Months that can be shown: from the first tracked month to today's month.
// False without history or when today is before the newest recorded day.
bool monthRange(const Model& model, int32_t today, Month& first, Month& last);

// Bounded: one O(366) scan for the history start, at most 62 day lookups and,
// for the current month, Model::streak's walk of up to 366 days.
bool summarizeMonth(const Model& model, int32_t today, Month month, MonthSummary& out);
}  // namespace readingcalendar
