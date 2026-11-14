#include "ReadingCalendarMonth.h"

namespace readingcalendar {
namespace {
int32_t firstDate(const Month m) { return civilDay(m.year, m.month, 1); }
unsigned length(const Month m) { return static_cast<unsigned>(firstDate(m.next()) - firstDate(m)); }
bool valid(const Month m) { return m.month >= 1 && m.month <= 12 && m.year > 1970 && m.year < 10000; }

// Milliseconds read on [first, last], or false when part of the range is untracked.
bool rangeMilliseconds(const Model& model, const int32_t tracked, const int32_t first, const int32_t last,
                       uint64_t& total) {
  if (!tracked || first < tracked) return false;
  total = 0;
  for (int32_t date = first; date <= last; ++date) {
    if (const auto* day = model.find(date)) total += day->milliseconds;
  }
  return true;
}
}  // namespace

int32_t firstTrackedDay(const Model& model) {
  const int32_t latest = model.latestDay();
  if (latest <= 0) return 0;
  const int32_t floor = latest - int32_t(Model::CAPACITY) + 1;
  int32_t first = 0;
  for (const auto& day : model.records()) {
    if (day.date >= floor && day.date <= latest && (!first || day.date < first)) first = day.date;
  }
  return first;
}

Month monthOf(const int32_t date) {
  // Inverse of civilDay (Howard Hinnant's civil_from_days).
  const int32_t z = date + 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  return {static_cast<int>(yoe) + era * 400 + (month <= 2), month};
}

uint8_t weekday(const int32_t date) {
  // Day 0 (1970-01-01) was a Thursday.
  const int32_t w = (date + 3) % 7;
  return static_cast<uint8_t>(w < 0 ? w + 7 : w);
}

bool monthRange(const Model& model, const int32_t today, Month& first, Month& last) {
  const int32_t tracked = firstTrackedDay(model);
  if (!tracked || today < model.latestDay()) return false;
  first = monthOf(tracked);
  last = monthOf(today);
  return true;
}

bool summarizeMonth(const Model& model, const int32_t today, const Month month, MonthSummary& out) {
  const int32_t tracked = firstTrackedDay(model);
  if (!valid(month) || !tracked || today < model.latestDay()) return false;
  const Month last = monthOf(today);
  const int32_t start = firstDate(month);
  if (start < firstDate(monthOf(tracked)) || start > firstDate(last)) return false;

  out = {};
  out.month = month;
  out.dayCount = static_cast<uint8_t>(length(month));
  out.firstWeekday = weekday(start);
  const bool current = month == last;
  if (current) out.today = static_cast<uint8_t>(today - start + 1);

  uint64_t total = 0;
  uint8_t run = 0;
  for (unsigned i = 0; i < out.dayCount; ++i) {
    const int32_t date = start + int32_t(i);
    const uint8_t dayNumber = static_cast<uint8_t>(i + 1);
    auto& level = out.levels[i];
    if (date > today) {
      level = DayLevel::Future;
      run = 0;
      continue;
    }
    if (date < tracked) {
      level = DayLevel::Untracked;
      run = 0;
      continue;
    }
    ++out.trackedDays;
    const auto* day = model.find(date);
    const uint32_t ms = day ? day->milliseconds : 0;
    total += ms;
    const bool goal = model.achieved(date);
    level = goal ? (ms >= MonthSummary::HOUR_MS ? DayLevel::HourPlus : DayLevel::Goal)
                 : (ms ? DayLevel::BelowGoal : DayLevel::None);
    if (ms) ++out.daysRead;
    if (ms > out.bestMilliseconds) {
      out.bestMilliseconds = ms;
      out.bestDay = dayNumber;
    }
    if (goal) {
      ++out.goalDays;
      if (++run > out.longestRun) {
        out.longestRun = run;
        out.longestRunStart = static_cast<uint8_t>(dayNumber - run + 1);
      }
    } else {
      run = 0;
    }
  }
  out.minutes = total / 60000;
  if (current) out.streak = model.streak(today);

  const Month prior = month.previous();
  const unsigned priorLength = length(prior);
  const unsigned compared = current && out.today < priorLength ? out.today : priorLength;
  uint64_t priorTotal = 0;
  const int32_t priorStart = firstDate(prior);
  if (rangeMilliseconds(model, tracked, priorStart, priorStart + int32_t(compared) - 1, priorTotal)) {
    out.hasComparison = true;
    out.comparedDays = static_cast<uint8_t>(compared);
    out.previousMinutes = priorTotal / 60000;
  }
  return true;
}
}  // namespace readingcalendar
