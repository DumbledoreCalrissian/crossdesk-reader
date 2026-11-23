#pragma once

#include "BookReadingStats.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"

class BookStatisticsActivity final : public Activity {
 public:
  // page/pageCount (1-based) add "(page/count)" for fixed-page books; 0 hides it.
  BookStatisticsActivity(GfxRenderer& renderer, MappedInputManager& input, BookReadingStats* statistics, int percent,
                         uint32_t page = 0, uint32_t pageCount = 0)
      : Activity("BookStatistics", renderer, input),
        statistics(statistics),
        percent(percent),
        page(page),
        pageCount(pageCount) {}
  void onEnter() override;
  void loop() override;
  void prepareForSleep() override { sleeping = true; }
  void render(RenderLock&&) override;

 private:
  enum class Action { None, Close, ConfirmReset, Reset, Summary, Retry };
  void showSummary();
  void formatProgress(char* out, size_t size) const;
  BookReadingStats* statistics;  // Retained parent owns the sole collector/store.
  int percent;
  uint32_t page;
  uint32_t pageCount;
  OptionPopup popup;
  Action pending = Action::None;
  bool confirmingReset = false;
  bool sleeping = false;
};
