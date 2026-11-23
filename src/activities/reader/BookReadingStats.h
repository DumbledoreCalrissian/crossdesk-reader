#pragma once

#include <cstdint>
#include <string_view>

#include "util/ReadingCalendarTracker.h"
#include "util/ReadingStats.h"
#include "util/ReadingStatsStore.h"

// Reader-owned policy. All access is serialized by RenderLock.
class BookReadingStats {
 public:
  using Departure = readingstats::Collector::Departure;
  using Status = readingstats::Store::Status;

  BookReadingStats(std::string_view path, uint32_t now);
  void pageShown(uint32_t now, bool samePage = false);
  void beginNavigation(uint32_t now, Departure reason);
  void completeNavigation(uint32_t now);
  void loading(uint32_t now);
  void pause(uint32_t now);
  void stop(uint32_t now);
  bool reset();
  bool available() const;
  bool canReset() const { return store.canReset(); }
  bool unsaved() const { return available() && collector.totals() != store.totals(); }
  bool writeFailed() const { return failedWrite; }
  Status status() const { return loadStatus; }
  readingstats::Totals totals() const { return collector.totals(); }

  // Counted reading since the first counted page of this reader visit.
  struct Session {
    uint32_t startedAt = 0;  // millis() of the first counted page
    uint32_t seconds = 0;
  };
  // Returns the visit's session once, after stop(); seconds is 0 when nothing was counted.
  Session takeSession();

 private:
  void save(uint32_t now, bool force);
  std::unique_ptr<readingcalendar::Tracker> calendar;
  uint64_t calendarAccounted = 0;
  void recordCalendar(uint32_t end, uint32_t now, bool force);
  readingstats::Store store;
  readingstats::Collector collector;
  readingstats::Collector beforeNavigation;
  uint32_t navigationAt = 0;
  bool pendingNavigation = false;
  Status loadStatus;
  uint32_t lastCheckpoint;
  bool stopped = false;
  bool sessionStarted = false;
  uint32_t sessionStartedAt = 0;
  uint64_t sessionBase = 0;
  bool failedWrite = false;
};
