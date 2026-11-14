#include <gtest/gtest.h>

#include "ReadingCalendar.h"
#include "ReadingCalendarMonth.h"
using namespace readingcalendar;

namespace {
constexpr uint32_t MIN = 60000;
int32_t d(int y, unsigned m, unsigned day) { return civilDay(y, m, day); }
}  // namespace

TEST(CalendarMonth, DateHelpersRoundTrip) {
  for (int32_t date = d(1999, 12, 25); date < d(2030, 3, 5); date += 17) {
    const Month m = monthOf(date);
    EXPECT_GE(date, d(m.year, m.month, 1));
    EXPECT_LT(date, d(m.next().year, m.next().month, 1));
  }
  EXPECT_EQ(monthOf(d(2024, 2, 29)), (Month{2024, 2}));
  EXPECT_EQ(monthOf(d(2026, 1, 1)), (Month{2026, 1}));
  EXPECT_EQ(monthOf(d(2025, 12, 31)), (Month{2025, 12}));
  EXPECT_EQ(weekday(d(2026, 10, 1)), 3);   // Thursday
  EXPECT_EQ(weekday(d(2026, 11, 16)), 0);  // Monday
  EXPECT_EQ((Month{2026, 1}).previous(), (Month{2025, 12}));
  EXPECT_EQ((Month{2025, 12}).next(), (Month{2026, 1}));
}

TEST(CalendarMonth, NoHistoryOrClockBehind) {
  Model m;
  Month first, last;
  MonthSummary s;
  EXPECT_FALSE(monthRange(m, d(2026, 11, 15), first, last));
  EXPECT_FALSE(summarizeMonth(m, d(2026, 11, 15), {2026, 11}, s));
  m.add(d(2026, 11, 10), MIN, 30);
  EXPECT_FALSE(monthRange(m, d(2026, 11, 9), first, last));
  EXPECT_TRUE(monthRange(m, d(2026, 11, 10), first, last));
  EXPECT_EQ(first, (Month{2026, 11}));
}

TEST(CalendarMonth, FullPastMonth) {
  Model m;
  m.add(d(2026, 9, 1), 40 * MIN, 30);  // history starts with September
  m.add(d(2026, 9, 30), 20 * MIN, 30);
  m.add(d(2026, 10, 1), 42 * MIN, 30);
  m.add(d(2026, 10, 2), 35 * MIN, 30);
  m.add(d(2026, 10, 4), 12 * MIN, 30);
  m.add(d(2026, 10, 11), 95 * MIN, 30);
  m.add(d(2026, 10, 12), 95 * MIN, 30);  // tie: earliest wins
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 11, 15), {2026, 10}, s));
  EXPECT_EQ(s.dayCount, 31);
  EXPECT_EQ(s.firstWeekday, 3);
  EXPECT_EQ(s.today, 0);
  EXPECT_EQ(s.trackedDays, 31);
  EXPECT_EQ(s.daysRead, 5);
  EXPECT_EQ(s.goalDays, 4);
  EXPECT_EQ(s.minutes, 279u);
  EXPECT_EQ(s.bestDay, 11);
  EXPECT_EQ(s.bestMilliseconds, 95 * MIN);
  EXPECT_EQ(s.longestRun, 2);
  EXPECT_EQ(s.longestRunStart, 1);
  EXPECT_EQ(s.streak, 0u);
  EXPECT_EQ(s.levels[0], DayLevel::Goal);
  EXPECT_EQ(s.levels[2], DayLevel::None);
  EXPECT_EQ(s.levels[3], DayLevel::BelowGoal);
  EXPECT_EQ(s.levels[10], DayLevel::HourPlus);
  EXPECT_TRUE(s.hasComparison);
  EXPECT_EQ(s.comparedDays, 30);
  EXPECT_EQ(s.previousMinutes, 60u);
}

TEST(CalendarMonth, MinutesRoundDownAfterAdding) {
  Model m;
  m.add(d(2026, 10, 1), 30000, 30);
  m.add(d(2026, 10, 2), 30000, 30);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 10, 2), {2026, 10}, s));
  EXPECT_EQ(s.minutes, 1u);
  EXPECT_EQ(s.daysRead, 2);
}

TEST(CalendarMonth, FirstMonthHasUntrackedDaysAndNoComparison) {
  Model m;
  m.add(d(2026, 10, 18), 20 * MIN, 30);
  m.add(d(2026, 10, 20), 31 * MIN, 30);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 10, 31), {2026, 10}, s));
  EXPECT_EQ(s.levels[16], DayLevel::Untracked);
  EXPECT_EQ(s.levels[17], DayLevel::BelowGoal);
  EXPECT_EQ(s.levels[18], DayLevel::None);
  EXPECT_EQ(s.trackedDays, 14);
  EXPECT_FALSE(s.hasComparison);
  MonthSummary before;
  EXPECT_FALSE(summarizeMonth(m, d(2026, 10, 31), {2026, 9}, before));
}

TEST(CalendarMonth, CurrentMonthComparesSameDays) {
  Model m;
  for (unsigned day = 1; day <= 31; ++day) m.add(d(2026, 10, day), 10 * MIN, 30);
  m.add(d(2026, 11, 1), 31 * MIN, 30);
  m.add(d(2026, 11, 2), 45 * MIN, 30);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 11, 3), {2026, 11}, s));
  EXPECT_EQ(s.today, 3);
  EXPECT_EQ(s.levels[2], DayLevel::None);
  EXPECT_EQ(s.levels[3], DayLevel::Future);
  EXPECT_EQ(s.trackedDays, 3);
  EXPECT_EQ(s.streak, 2u);
  EXPECT_TRUE(s.hasComparison);
  EXPECT_EQ(s.comparedDays, 3);
  EXPECT_EQ(s.previousMinutes, 30u);
  MonthSummary future;
  EXPECT_FALSE(summarizeMonth(m, d(2026, 11, 3), {2026, 12}, future));
}

TEST(CalendarMonth, CurrentMonthLongerThanPrevious) {
  Model m;
  for (unsigned day = 1; day <= 29; ++day) m.add(d(2024, 2, day), MIN, 30);
  m.add(d(2024, 3, 31), MIN, 30);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2024, 3, 31), {2024, 3}, s));
  EXPECT_EQ(s.comparedDays, 29);
  EXPECT_EQ(s.previousMinutes, 29u);
  MonthSummary feb;
  ASSERT_TRUE(summarizeMonth(m, d(2024, 3, 31), {2024, 2}, feb));
  EXPECT_EQ(feb.dayCount, 29);
  EXPECT_EQ(feb.firstWeekday, 3);
}

TEST(CalendarMonth, GoalChangesKeepEachDaysResult) {
  Model m;
  m.add(d(2026, 10, 1), 10 * MIN, 10);
  m.add(d(2026, 10, 2), 10 * MIN, 30);
  m.add(d(2026, 10, 3), 70 * MIN, 60);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 10, 3), {2026, 10}, s));
  EXPECT_EQ(s.levels[0], DayLevel::Goal);
  EXPECT_EQ(s.levels[1], DayLevel::BelowGoal);
  EXPECT_EQ(s.levels[2], DayLevel::HourPlus);
  EXPECT_EQ(s.goalDays, 2);
  EXPECT_EQ(s.longestRun, 1);
  EXPECT_EQ(s.longestRunStart, 1);
}

TEST(CalendarMonth, EmptyTrackedMonth) {
  Model m;
  m.add(d(2026, 8, 31), 5 * MIN, 30);
  m.add(d(2026, 10, 1), 5 * MIN, 30);
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, d(2026, 10, 1), {2026, 9}, s));
  EXPECT_EQ(s.trackedDays, 30);
  EXPECT_EQ(s.daysRead, 0);
  EXPECT_EQ(s.minutes, 0u);
  EXPECT_EQ(s.bestDay, 0);
  EXPECT_EQ(s.longestRun, 0);
  EXPECT_FALSE(s.hasComparison);  // August is only partly tracked
}

TEST(CalendarMonth, WindowClipsOldMonths) {
  Model m;
  const int32_t start = d(2025, 10, 10);
  m.add(start, 40 * MIN, 30);
  m.add(start + 5, 40 * MIN, 30);
  const int32_t latest = start + int32_t(Model::CAPACITY) - 1;  // start still retained
  m.add(latest, 40 * MIN, 30);
  EXPECT_EQ(firstTrackedDay(m), start);
  m.add(latest + 1, 40 * MIN, 30);  // start drops out; the oldest retained record becomes the start
  EXPECT_EQ(firstTrackedDay(m), start + 5);
  Month first, last;
  ASSERT_TRUE(monthRange(m, latest + 1, first, last));
  EXPECT_EQ(first, (Month{2025, 10}));
  MonthSummary s;
  ASSERT_TRUE(summarizeMonth(m, latest + 1, first, s));
  EXPECT_EQ(s.levels[9], DayLevel::Untracked);
  EXPECT_EQ(s.levels[13], DayLevel::Untracked);
  EXPECT_EQ(s.levels[14], DayLevel::Goal);
  EXPECT_EQ(s.trackedDays, 17);
  EXPECT_FALSE(s.hasComparison);
}

TEST(CalendarMonth, RejectsInvalidMonths) {
  Model m;
  m.add(d(2026, 10, 1), MIN, 30);
  MonthSummary s;
  EXPECT_FALSE(summarizeMonth(m, d(2026, 10, 1), {2026, 0}, s));
  EXPECT_FALSE(summarizeMonth(m, d(2026, 10, 1), {2026, 13}, s));
}
