#pragma once
#include <memory>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ReadingCalendarMonth.h"

// Read-only monthly reading statistics from the Desk reading calendar.
class StatisticsActivity final : public Activity, private UiAppHost {
 public:
  StatisticsActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("Statistics", renderer, input), UiAppHost(renderer) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  enum Action : freeink::ui::ActionId { Back = 1, Previous, Next, Clock };
  enum class State : uint8_t { Month, NoHistory, NoClock, ClockBehind, Unavailable };
  State state = State::NoClock;
  // A display copy: the page never saves, so today's goal can follow the setting.
  std::unique_ptr<readingcalendar::Model> model;
  readingcalendar::MonthSummary summary;
  readingcalendar::Month first, last;
  int32_t today = 0;
  uint16_t goal = 30;
  char hintLines[3][128]{};
  static void screenFn(UiScreen& screen, void* context);
  static void action(const freeink::ui::ActionEvent& event, void* context);
  void load();
  void show(readingcalendar::Month month);
  void step(int delta);
  void activate(freeink::ui::ActionId id);
  void buildScreen(UiScreen& screen);
  void drawHeaderMonth(UiScreen& screen, const freeink::ui::Rect& band, int titleOffset, int actionOffset);
  int drawGrid(int x, int y, int width) const;
  int drawLegend(int x, int y, int width) const;
  void drawTable(int x, int y, int width) const;
  void drawMessage(UiScreen& screen, const char* title, bool clockButton);
};
