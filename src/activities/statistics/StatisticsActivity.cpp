#include "StatisticsActivity.h"

#include <FreeInkUIIcon.h>
#include <HalClock.h>
#include <HalDisplay.h>
#ifdef SIMULATOR
#include <HalMemory.h>
#endif
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "activities/settings/SettingsActivity.h"
#include "components/UITheme.h"
#include "components/UiScreenHeader.h"
#include "components/icons/opdsIcons.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"
#include "util/ReadingCalendarClock.h"
#include "util/ReadingCalendarStore.h"

namespace fui = freeink::ui;
namespace {
constexpr StrId MONTHS[] = {StrId::STR_CAL_JAN, StrId::STR_CAL_FEB, StrId::STR_CAL_MAR, StrId::STR_CAL_APR,
                            StrId::STR_CAL_MAY, StrId::STR_CAL_JUN, StrId::STR_CAL_JUL, StrId::STR_CAL_AUG,
                            StrId::STR_CAL_SEP, StrId::STR_CAL_OCT, StrId::STR_CAL_NOV, StrId::STR_CAL_DEC};
constexpr StrId WEEKDAYS[] = {StrId::STR_WEATHER_MON, StrId::STR_WEATHER_TUE, StrId::STR_WEATHER_WED,
                              StrId::STR_WEATHER_THU, StrId::STR_WEATHER_FRI, StrId::STR_WEATHER_SAT,
                              StrId::STR_WEATHER_SUN};

void fold(const GfxRenderer& renderer, int x, int y, int size) {
  for (int row = 0; row < size; ++row) renderer.drawLine(x + row, y + row, x + size, y + row, false);
  renderer.drawLine(x, y, x + size, y);
  renderer.drawLine(x + size, y, x + size, y + size);
  renderer.drawLine(x, y, x + size, y + size);
}
}  // namespace

void StatisticsActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  {
    RenderLock lock(*this);
    load();
#ifdef SIMULATOR
    // TEMP(M2): sizes are real; simulator heap totals are configurable mocks.
    const auto internal = HalMemory::getInternalHeap();
    const auto psram = HalMemory::getPsramHeap();
    LOG_DBG("STATS", "Sizes: activity=%u model=%u store=%u; mock internal free/largest=%u/%u PSRAM=%u/%u",
            unsigned(sizeof(*this)), unsigned(sizeof(readingcalendar::Model)), unsigned(sizeof(readingcalendar::Store)),
            unsigned(internal.freeBytes), unsigned(internal.largestBlockBytes), unsigned(psram.freeBytes),
            unsigned(psram.largestBlockBytes));
#endif
    const char* hint = nullptr;
    switch (state) {
      case State::NoClock:
        hint = tr(STR_CAL_NO_CLOCK_HINT);
        break;
      case State::ClockBehind:
        hint = tr(STR_CAL_CLOCK_BEHIND_HINT);
        break;
      case State::Unavailable:
        hint = tr(STR_CAL_UNAVAILABLE_HINT);
        break;
      case State::NoHistory:
        hint = tr(STR_STATS_NO_READING_HINT);
        break;
      case State::Month:
        break;
    }
    if (hint) {
      const auto content = UITheme::getContentArea(renderer);
      const int width = content.width - 2 * UITheme::getInstance().getMetrics().contentSidePadding;
      const auto lines = renderer.wrappedText(UI_10_FONT_ID, hint, width, 3);
      for (size_t i = 0; i < lines.size() && i < 3; ++i)
        snprintf(hintLines[i], sizeof(hintLines[i]), "%s", lines[i].c_str());
    }
  }
  for (auto id : {Back, Previous, Next, Clock}) app.on(id, action, this);
  app.setScreen(screenFn, this);
  requestUpdate();
}
void StatisticsActivity::load() {
  state = State::NoClock;
  const auto local = readingcalendar::localMilliseconds();
  if (!local) return;
  today = int32_t(local / readingcalendar::Model::MAX_DAY_MS);
  state = State::Unavailable;
  auto store = makeUniqueNoThrow<readingcalendar::Store>();
  if (!store) {
    LOG_ERR("STATS", "OOM: calendar store (%u bytes)", unsigned(sizeof(readingcalendar::Store)));
    return;
  }
  store->load();
  if (!store->canSave()) return;
  if (today < store->model().latestDay()) {
    state = State::ClockBehind;
    return;
  }
  // Check before changeGoal creates today's display-only record.
  if (!readingcalendar::monthRange(store->model(), today, first, last)) {
    state = State::NoHistory;
    return;
  }
  model = makeUniqueNoThrow<readingcalendar::Model>(store->model());
  if (!model) {
    LOG_ERR("STATS", "OOM: calendar model (%u bytes)", unsigned(sizeof(readingcalendar::Model)));
    return;
  }
  goal = readingcalendar::GOALS[store->goalIndex()];
  model->changeGoal(today, goal);
  store.reset();
  readingcalendar::monthRange(*model, today, first, last);
  state = State::Month;
  show(last);
}
void StatisticsActivity::onExit() {
  closeRouting();
  model.reset();
  Activity::onExit();
}
bool StatisticsActivity::handleHomeGesture() {
  activate(Back);
  return true;
}
void StatisticsActivity::screenFn(UiScreen& screen, void* context) {
  static_cast<StatisticsActivity*>(context)->buildScreen(screen);
}
void StatisticsActivity::action(const fui::ActionEvent& event, void* context) {
  static_cast<StatisticsActivity*>(context)->activate(event.action);
}
void StatisticsActivity::show(readingcalendar::Month month) {
  readingcalendar::summarizeMonth(*model, today, month, summary);
}
void StatisticsActivity::step(int delta) {
  RenderLock lock(*this);
  if (state != State::Month || (delta < 0 ? summary.month == first : summary.month == last)) return;
  show(delta < 0 ? summary.month.previous() : summary.month.next());
  closeRouting();
  requestUpdate();
}
void StatisticsActivity::activate(fui::ActionId id) {
  app.clearTapFlash();
  switch (id) {
    case Back:
      activityManager.goHome();
      return;
    case Previous:
      step(-1);
      return;
    case Next:
      step(1);
      return;
    case Clock:
      if ((state == State::NoClock || state == State::ClockBehind) && halClock.isAvailable())
        activityManager.goToSettings(SettingsSection::General, {nullptr, SettingAction::ClockSettings});
      return;
  }
}
void StatisticsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activate(Back);
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
      (state == State::NoClock || state == State::ClockBehind)) {
    activate(Clock);
    return;
  }
  ButtonNavigator::onNextPress([this] { step(1); });
  ButtonNavigator::onPreviousPress([this] { step(-1); });
}
void StatisticsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
void StatisticsActivity::drawHeaderMonth(UiScreen& screen, const fui::Rect& band, int titleOffset, int actionOffset) {
  char label[80];
  const auto month = state == State::Month ? summary.month : readingcalendar::monthOf(today);
  snprintf(label, sizeof(label), tr(STR_CAL_MONTH_YEAR), I18N.get(MONTHS[month.month - 1]), month.year);
  const int width = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
  const int right = band.x + band.width;
  const int textRight = right - 40;
  const int y = band.y + (band.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2 + titleOffset;
  renderer.drawText(UI_10_FONT_ID, textRight - width, y, label, true, EpdFontFamily::BOLD);
  fui::ButtonProps button;
  button.styles = fui::plainStyles(fui::Paint::solid(fui::Color::Black));
  button.inputMask = fui::InputTouch;
  const int buttonY = band.y + 4 + actionOffset;
  if (state == State::Month && !(summary.month == first)) {
    button.action = Previous;
    button.icon = fui::bitmapFromIcon(icon_page_prev_24);
    screen.button(button, fui::Rect{int16_t(textRight - width - 44), int16_t(buttonY), 44, 44});
  }
  if (state == State::Month && !(summary.month == last)) {
    button.action = Next;
    button.icon = fui::bitmapFromIcon(icon_page_next_24);
    screen.button(button, fui::Rect{int16_t(right - 44), int16_t(buttonY), 44, 44});
  }
}
int StatisticsActivity::drawGrid(int x, int y, int width) const {
  const int col = width / 7;
  const int rows = (summary.firstWeekday + summary.dayCount + 6) / 7;
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  const int row = landscape ? 32 : (rows == 6 ? 52 : 60);
  for (unsigned i = 0; i < 7; ++i) {
    const auto* label = I18N.get(WEEKDAYS[i]);
    renderer.drawText(SMALL_FONT_ID, x + i * col + (col - renderer.getTextWidth(SMALL_FONT_ID, label)) / 2, y, label,
                      true);
  }
  y += renderer.getLineHeight(SMALL_FONT_ID) + 8;
  for (unsigned i = 0; i < summary.dayCount; ++i) {
    const int slot = summary.firstWeekday + i;
    const int cx = x + (slot % 7) * col + 3, cy = y + (slot / 7) * row;
    const int w = col - 6, h = row - 6;
    const auto level = summary.levels[i];
    switch (level) {
      case readingcalendar::DayLevel::Untracked:
        break;
      case readingcalendar::DayLevel::Future:
        for (int dx = 0; dx < w; dx += 4) renderer.drawPixel(cx + dx, cy + h - 1, true);
        break;
      case readingcalendar::DayLevel::None:
        renderer.drawRect(cx, cy, w, h);
        break;
      case readingcalendar::DayLevel::BelowGoal:
        renderer.fillRectDither(cx, cy, w, h, Color::LightGray);
        break;
      case readingcalendar::DayLevel::Goal:
        renderer.fillRectDither(cx, cy, w, h, Color::DarkGray);
        break;
      case readingcalendar::DayLevel::HourPlus:
        renderer.fillRect(cx, cy, w, h, true);
        break;
    }
    char number[4];
    snprintf(number, sizeof(number), "%u", i + 1);
    const int textW = renderer.getTextWidth(SMALL_FONT_ID, number);
    renderer.fillRect(cx + 2, cy + 1, textW + 2, renderer.getLineHeight(SMALL_FONT_ID) - 2, false);
    renderer.drawText(SMALL_FONT_ID, cx + 2, cy, number, true);
    if (i + 1 == summary.bestDay) fold(renderer, cx + w - 16, cy, 15);
    if (i + 1 == summary.today) renderer.drawRect(cx - 2, cy - 2, w + 4, h + 4, 2, true);
  }
  return y + rows * row;
}
int StatisticsActivity::drawLegend(int x, int y, int width) const {
  constexpr StrId LABELS[] = {StrId::STR_STATS_LEGEND_NONE, StrId::STR_STATS_LEGEND_UNDER, StrId::STR_STATS_LEGEND_GOAL,
                              StrId::STR_STATS_LEGEND_HOUR, StrId::STR_STATS_LEGEND_BEST};
  constexpr Color COLORS[] = {Color::White, Color::LightGray, Color::DarkGray, Color::Black};
  const int limit = x + width;
  const int start = x;
  for (unsigned i = 0; i < 5; ++i) {
    const char* label = I18N.get(LABELS[i]);
    const int itemWidth = 18 + renderer.getTextWidth(SMALL_FONT_ID, label) + 12;
    if (x + itemWidth > limit) {
      x = start;
      y += renderer.getLineHeight(SMALL_FONT_ID) + 6;
    }
    if (i == 4)
      fold(renderer, x, y + 2, 14);
    else {
      renderer.fillRectDither(x, y + 4, 14, 14, COLORS[i]);
      renderer.drawRect(x, y + 4, 14, 14);
    }
    renderer.drawText(SMALL_FONT_ID, x + 18, y, label, true);
    x += itemWidth;
  }
  return y + renderer.getLineHeight(SMALL_FONT_ID) + 12;
}
void StatisticsActivity::drawTable(int x, int y, int width) const {
  const int font = renderer.getScreenWidth() > renderer.getScreenHeight() ? SMALL_FONT_ID : UI_10_FONT_ID;
  const int rowHeight = renderer.getLineHeight(font) + 10;
  auto row = [&](const char* label, const char* value) {
    renderer.drawText(font, x + 4, y + 5, label, true);
    renderer.drawText(font, x + width - 4 - renderer.getTextWidth(font, value, EpdFontFamily::BOLD), y + 5, value, true,
                      EpdFontFamily::BOLD);
    y += rowHeight;
    renderer.drawLine(x, y, x + width - 1, y);
  };
  auto duration = [](char* text, size_t size, uint64_t minutes) {
    if (minutes < 60)
      snprintf(text, size, tr(STR_CAL_MINUTES), static_cast<unsigned long long>(minutes));
    else
      snprintf(text, size, tr(STR_STATS_HOURS), static_cast<unsigned long long>(minutes / 60), unsigned(minutes % 60));
  };
  char value[80];
  renderer.drawLine(x, y, x + width - 1, y, 2, true);
  duration(value, sizeof(value), summary.minutes);
  row(tr(STR_STATS_MINUTES_READ), value);
  const bool partial = summary.trackedDays < summary.dayCount;
  snprintf(value, sizeof(value),
           summary.today ? tr(STR_STATS_DAYS_OF_SO_FAR)
           : partial     ? tr(STR_STATS_DAYS_TRACKED)
                         : tr(STR_STATS_DAYS_OF),
           unsigned(summary.daysRead), unsigned(summary.trackedDays));
  row(tr(STR_STATS_DAYS_READ), value);
  if (summary.daysRead) {
    snprintf(value, sizeof(value), summary.goalDays == 1 ? tr(STR_CAL_DAY) : tr(STR_CAL_DAYS),
             unsigned(summary.goalDays));
    row(tr(STR_STATS_GOAL_MET), value);
    char time[32];
    duration(time, sizeof(time), summary.bestMilliseconds / 60000);
    snprintf(value, sizeof(value), tr(STR_STATS_BEST_VALUE),
             I18N.get(WEEKDAYS[(summary.firstWeekday + summary.bestDay - 1) % 7]), unsigned(summary.bestDay),
             I18N.get(MONTHS[summary.month.month - 1]), time);
    row(tr(STR_STATS_BEST_DAY), value);
    const unsigned run = summary.today ? summary.streak : summary.longestRun;
    snprintf(time, sizeof(time), run == 1 ? tr(STR_CAL_DAY) : tr(STR_CAL_DAYS), run);
    if (!summary.today && summary.longestRun) {
      snprintf(value, sizeof(value), tr(STR_STATS_RUN_VALUE), time, unsigned(summary.longestRunStart),
               unsigned(summary.longestRunStart + summary.longestRun - 1));
    } else
      snprintf(value, sizeof(value), "%s", time);
    row(summary.today ? tr(STR_STATS_CURRENT_STREAK) : tr(STR_STATS_LONGEST_RUN), value);
    if (summary.hasComparison) {
      char label[64];
      const char* previous = I18N.get(MONTHS[summary.month.previous().month - 1]);
      snprintf(label, sizeof(label), summary.today ? tr(STR_STATS_VS_SAME_DAYS) : tr(STR_STATS_VS), previous,
               unsigned(summary.comparedDays));
      const int64_t difference = int64_t(summary.minutes) - int64_t(summary.previousMinutes);
      duration(time, sizeof(time), uint64_t(difference < 0 ? -difference : difference));
      snprintf(value, sizeof(value), difference < 0 ? tr(STR_STATS_NEGATIVE) : tr(STR_STATS_POSITIVE), time);
      row(label, value);
    }
  } else {
    snprintf(value, sizeof(value), tr(STR_STATS_NO_READING_IN), I18N.get(MONTHS[summary.month.month - 1]));
    renderer.drawText(SMALL_FONT_ID, x + 4, y + 20, value, true);
    y += 40;
  }
  const auto tracked = readingcalendar::firstTrackedDay(*model);
  if (readingcalendar::monthOf(tracked) == summary.month) {
    const unsigned day = tracked - readingcalendar::civilDay(summary.month.year, summary.month.month, 1) + 1;
    snprintf(value, sizeof(value), tr(STR_STATS_TRACKING_BEGAN), day, I18N.get(MONTHS[summary.month.month - 1]));
    renderer.drawText(SMALL_FONT_ID, x + 4, y + 16, value, true);
  }
}
void StatisticsActivity::drawMessage(UiScreen& screen, const char* title, bool clockButton) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto area = UITheme::getContentArea(renderer);
  const int x = area.x + metrics.contentSidePadding, width = area.width - 2 * metrics.contentSidePadding;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  int y = top + (renderer.getScreenHeight() - top) / 4;
  renderer.drawText(UI_12_FONT_ID, x + (width - renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD)) / 2,
                    y, title, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;
  for (const auto& line : hintLines) {
    if (!line[0]) break;
    renderer.drawText(UI_10_FONT_ID, x + (width - renderer.getTextWidth(UI_10_FONT_ID, line)) / 2, y, line, true);
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
  if (!clockButton || !halClock.isAvailable()) return;
  fui::ButtonProps button;
  button.label = tr(STR_CAL_CLOCK_LINK);
  button.action = Clock;
  button.text = screen.theme().bodyText;
  button.styles.normal.foreground = fui::Paint::solid(fui::Color::Black);
  button.styles.normal.border = fui::Paint::solid(fui::Color::Black);
  button.styles.normal.borderWidth = 1;
  button.radius = screen.theme().listRowRadius;
  const int w = renderer.getTextWidth(UI_10_FONT_ID, button.label) + 48;
  screen.button(button, fui::Rect{int16_t(x + (width - w) / 2), int16_t(y + 18), int16_t(w), 48});
}
void StatisticsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto area = UITheme::getContentArea(renderer);
  const int x = area.x + metrics.contentSidePadding, width = area.width - 2 * metrics.contentSidePadding;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  {
    fui::HeaderProps header;
    const auto band = prepareUiHeader(screen, renderer, header, tr(STR_STATS_TITLE), Back);
    if (state == State::Month || state == State::NoHistory) {
      char label[64];
      const auto month = state == State::Month ? summary.month : readingcalendar::monthOf(today);
      snprintf(label, sizeof(label), tr(STR_CAL_MONTH_YEAR), I18N.get(MONTHS[month.month - 1]), month.year);
      header.rightReserve = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD) + 88;
    }
    fui::header(screen.frame(), band, header);
    if (state == State::Month || state == State::NoHistory)
      drawHeaderMonth(screen, band, header.titleOffsetY, header.actionOffsetY);
  }
  switch (state) {
    case State::NoClock:
      drawMessage(screen, tr(STR_CAL_NO_CLOCK), true);
      return;
    case State::ClockBehind:
      drawMessage(screen, tr(STR_CAL_CLOCK_BEHIND), true);
      return;
    case State::Unavailable:
      drawMessage(screen, tr(STR_CAL_UNAVAILABLE), false);
      return;
    case State::NoHistory:
      drawMessage(screen, tr(STR_STATS_NO_READING), false);
      return;
    case State::Month:
      break;
  }
  char subtitle[80];
  snprintf(subtitle, sizeof(subtitle), summary.today ? tr(STR_STATS_GOAL_SO_FAR) : tr(STR_STATS_GOAL), unsigned(goal));
  renderer.drawText(SMALL_FONT_ID, x, top, subtitle, true);
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  const int gridWidth = landscape ? (width - 24) / 2 : width;
  int y = drawGrid(x, top + renderer.getLineHeight(SMALL_FONT_ID) + 12, gridWidth);
  y = drawLegend(x, y + 4, gridWidth);
  if (landscape)
    drawTable(x + gridWidth + 24, top + 28, width - gridWidth - 24);
  else
    drawTable(x, y, width);
}
