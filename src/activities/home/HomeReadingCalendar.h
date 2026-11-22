#pragma once

#include <cstdint>
#include <ctime>
#include <memory>

#include "util/ReadingCalendarStore.h"

// Reading calendar as the Home widgets show it (Desk calendar, Coverflow sheet).
namespace homecalendar {
enum class State : uint8_t { Ready, NoClock, ClockBehind, Unavailable };

// Ready: ledger holds the loaded calendar with the current goal applied to
// this display copy only (Home never saves a ledger); date and today are local.
State load(std::unique_ptr<readingcalendar::Store>& ledger, tm& date, int32_t& today);
}  // namespace homecalendar
