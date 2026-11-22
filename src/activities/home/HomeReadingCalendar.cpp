#include "HomeReadingCalendar.h"

#include <Logging.h>
#include <Memory.h>

#include "util/ReadingCalendarClock.h"

namespace homecalendar {
State load(std::unique_ptr<readingcalendar::Store>& ledger, tm& date, int32_t& today) {
  ledger.reset();
  const auto local = readingcalendar::localMilliseconds(&date);
  if (!local) return State::NoClock;
  auto store = makeUniqueNoThrow<readingcalendar::Store>();
  if (!store) {
    LOG_ERR("HOME", "OOM: calendar (%u bytes)", unsigned(sizeof(readingcalendar::Store)));
    return State::Unavailable;
  }
  store->load();
  today = int32_t(local / readingcalendar::Model::MAX_DAY_MS);
  if (!store->canSave()) return State::Unavailable;
  if (today < store->model().latestDay()) return State::ClockBehind;
  store->model().changeGoal(today, readingcalendar::GOALS[store->goalIndex()]);
  ledger = std::move(store);
  return State::Ready;
}
}  // namespace homecalendar
