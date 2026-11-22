#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "HomeReadingCalendar.h"
#include "HomeTodoPreview.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Coverflow's apps menu: a bottom sheet styled like the quick-controls sheet,
// with app buttons, then mini reading, Todo and cached-weather cards. Apps
// that have a card (Statistics, Todo, Weather) get no button.
class CoverflowSheet final {
 public:
  enum class Action : uint8_t { None, Close, Open };

  CoverflowSheet(GfxRenderer& renderer, MappedInputManager& input) : renderer(renderer), input(input) {}
  // Reads the reading calendar, Todo and saved weather once; called on the input task.
  void open(HomeMenuItem slot);
  void close() { shown = false; }
  bool isOpen() const { return shown; }
  void draw() const;
  Action handleTouch();
  // The item the last Open action chose.
  HomeMenuItem chosen() const { return chosenItem; }

 private:
  enum class WeatherState : uint8_t { Ready, Setup, NoForecast, Unavailable };
  struct WeekDay {
    uint32_t minutes = 0;
    bool goalMet = false;
    uint8_t weekday = 0;  // Monday = 0
  };

  void loadReading();
  void loadWeather();
  int itemCount() const { return static_cast<int>(itemTotal); }
  int todoCardHeight() const;
  int top() const;
  Rect tileCell(int index) const;
  int dividerY() const;
  Rect readingRect() const;
  Rect todoRect() const;
  Rect weatherRect() const;
  void drawTile(int index) const;
  void drawReading() const;
  void drawTodo() const;
  void drawWeather() const;
  bool tapped(Rect rect) const;

  GfxRenderer& renderer;
  MappedInputManager& input;
  // Set last in open(), after the data below; draw() runs on the render task.
  std::atomic<bool> shown{false};
  // Desk's apps minus those with a card here, built in open(). Weather,
  // Statistics and Todo leave at most six, so they fit one row.
  static constexpr size_t MAX_ITEMS = 6;
  std::array<HomeMenuItem, MAX_ITEMS> items{};
  size_t itemTotal = 0;
  HomeMenuItem chosenItem = HomeMenuItem::NONE;

  homecalendar::State readingState = homecalendar::State::Unavailable;
  uint64_t monthMinutes = 0;
  unsigned streak = 0;
  unsigned goal = 0;
  std::array<WeekDay, 7> week{};

  HomeTodoPreview todo;

  WeatherState weatherState = WeatherState::Unavailable;
  const uint8_t* weatherIcon = nullptr;
  char weatherTemp[16]{};
  char weatherTitle[96]{};
  char weatherDetail[64]{};
};
