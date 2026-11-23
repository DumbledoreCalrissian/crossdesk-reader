#include "CoverflowSheet.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "HomeAppMenu.h"
#include "components/UITheme.h"
#include "components/WeatherPresentation.h"
#include "components/themes/lyra/LyraTheme.h"
#include "fontIds.h"
#include "util/ReadingCalendarClock.h"
#include "util/ReadingCalendarMonth.h"
#include "util/WeatherStore.h"

namespace {
// Matches the quick-controls sheet: square, 2 px rule, 72x5 grab bar.
constexpr int RULE = 2;
constexpr int GRAB_W = 72;
constexpr int GRAB_H = 5;
constexpr int GRAB_BAND = 44;
constexpr int PAD = 16;
constexpr int TILE = 56;
constexpr int TILE_RADIUS = 6;
constexpr int ICON = 32;
constexpr int TILE_ROW = 106;
constexpr int DIVIDER_GAP = 12;
constexpr int CARD_H = 90;
constexpr int CARD_GAP = 12;
constexpr int WEATHER_H = 64;
constexpr int WEATHER_ICON = 48;
constexpr int BORDER = 2;
constexpr int INSET = 14;
constexpr int BAR_AREA_H = 40;
constexpr int CHEVRON = 6;

void drawChevron(const GfxRenderer& renderer, const int x, const int y) {
  renderer.drawLine(x, y - CHEVRON, x + CHEVRON, y, BORDER, true);
  renderer.drawLine(x + CHEVRON, y, x, y + CHEVRON, BORDER, true);
}
}  // namespace

void CoverflowSheet::open(const HomeMenuItem slot) {
  itemTotal = 0;
  for (int i = 0; i < appMenuItemCount(slot); ++i) {
    const HomeMenuItem item = appMenuItemAt(i, slot);
    const bool hasCard =
        item == HomeMenuItem::STATISTICS || item == HomeMenuItem::TODO || item == HomeMenuItem::WEATHER;
    if (!hasCard && itemTotal < MAX_ITEMS) items[itemTotal++] = item;
  }
  chosenItem = HomeMenuItem::NONE;
  loadReading();
  todo.load(renderer, todoRect().width - 3 * INSET - CHEVRON);
  loadWeather();
  shown = true;
}

// Same figures as the Desk calendar, from the shared loader.
void CoverflowSheet::loadReading() {
  tm date{};
  int32_t today = 0;
  std::unique_ptr<readingcalendar::Store> ledger;
  readingState = homecalendar::load(ledger, date, today);
  if (readingState != homecalendar::State::Ready) return;
  const auto& model = ledger->model();
  goal = readingcalendar::GOALS[ledger->goalIndex()];
  const int year = date.tm_year + 1900;
  const unsigned month = static_cast<unsigned>(date.tm_mon + 1);
  monthMinutes = model.minutes(readingcalendar::civilDay(year, month, 1), today);
  streak = model.streak(today);
  for (int i = 0; i < 7; ++i) {
    const int32_t day = today - 6 + i;
    week[i] =
        WeekDay{static_cast<uint32_t>(model.minutes(day, day)), model.achieved(day), readingcalendar::weekday(day)};
  }
}

void CoverflowSheet::loadWeather() {
  weatherState = WeatherState::Unavailable;
  weatherIcon = nullptr;
  auto store = makeUniqueNoThrow<weather::Store>();
  if (!store) {
    LOG_ERR("HOME", "OOM: weather cache (%u bytes)", unsigned(sizeof(weather::Store)));
    return;
  }
  store->load();
  if (!store->available()) return;
  const auto& data = store->model();
  if (!data.configured) {
    weatherState = WeatherState::Setup;
    return;
  }
  if (!data.cached) {
    weatherState = WeatherState::NoForecast;
    return;
  }
  const auto& f = data.forecast;
  weatherIcon = weatherui::icon(f.code, f.isDay);
  weatherui::temperature(weatherTemp, sizeof(weatherTemp), f.temperature, data.fahrenheit);
  snprintf(weatherTitle, sizeof(weatherTitle), "%s · %s", data.location.name, weatherui::condition(f.code));
  char chance[20];
  char precip[32];
  char updated[48];
  weatherui::percent(chance, sizeof(chance), f.days[0].chance);
  snprintf(precip, sizeof(precip), tr(STR_WEATHER_PRECIP_VALUE), chance);
  weatherui::updated(updated, sizeof(updated), f);
  snprintf(weatherDetail, sizeof(weatherDetail), "%s · %s", precip, updated);
  weatherState = WeatherState::Ready;
}

int CoverflowSheet::todoCardHeight() const {
  return 2 * INSET + renderer.getLineHeight(UI_10_FONT_ID) + HomeTodoPreview::GAP +
         static_cast<int>(HomeTodoPreview::ROWS) * HomeTodoPreview::rowHeight(renderer);
}

int CoverflowSheet::top() const {
  const int height =
      GRAB_BAND + TILE_ROW + DIVIDER_GAP + CARD_H + CARD_GAP + todoCardHeight() + CARD_GAP + WEATHER_H + PAD;
  return std::max(0, renderer.getScreenHeight() - height);
}

// The whole column cell is the touch target; the button is drawn centred in it.
Rect CoverflowSheet::tileCell(const int index) const {
  const int width = (renderer.getScreenWidth() - 2 * PAD) / std::max(1, itemCount());
  return Rect{PAD + index * width, top() + GRAB_BAND, width, TILE_ROW};
}

int CoverflowSheet::dividerY() const { return top() + GRAB_BAND + TILE_ROW; }

Rect CoverflowSheet::readingRect() const {
  return Rect{PAD, dividerY() + DIVIDER_GAP, renderer.getScreenWidth() - 2 * PAD, CARD_H};
}

Rect CoverflowSheet::todoRect() const {
  const Rect card = readingRect();
  return Rect{card.x, card.y + card.height + CARD_GAP, card.width, todoCardHeight()};
}

Rect CoverflowSheet::weatherRect() const {
  const Rect card = todoRect();
  return Rect{card.x, card.y + card.height + CARD_GAP, card.width, WEATHER_H};
}

void CoverflowSheet::draw() const {
  if (!shown) return;
  const int y = top();
  const int width = renderer.getScreenWidth();
  renderer.fillRect(0, y, width, renderer.getScreenHeight() - y, false);
  renderer.fillRect(0, y, width, RULE, true);
  renderer.fillRect((width - GRAB_W) / 2, y + (GRAB_BAND - GRAB_H) / 2, GRAB_W, GRAB_H, true);
  for (int i = 0; i < itemCount(); ++i) drawTile(i);
  // Light dotted divider between the apps and the widgets.
  for (int x = PAD; x < width - PAD; x += 4) renderer.drawPixel(x, dividerY(), true);
  drawReading();
  drawTodo();
  drawWeather();
}

void CoverflowSheet::drawTile(const int index) const {
  const HomeMenuItem item = items[index];
  const Rect cell = tileCell(index);
  const int x = cell.x + (cell.width - TILE) / 2;
  const int y = cell.y;
  renderer.drawRoundedRect(x, y, TILE, TILE, BORDER, TILE_RADIUS, true);
  if (const uint8_t* bitmap = LyraTheme::menuIcon(appMenuIcon(item))) {
    renderer.drawIcon(bitmap, x + (TILE - ICON) / 2, y + (TILE - ICON) / 2, ICON);
  }
  // Two lines, so "Files" and longer translations fit a narrow cell.
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  UITheme::drawCenteredWrappedText(renderer, Rect{cell.x + 2, y + TILE + 6, cell.width - 4, 2 * lineH}, SMALL_FONT_ID,
                                   appMenuLabel(item), 2, true, EpdFontFamily::REGULAR,
                                   UITheme::TextVerticalAlignment::TOP);
}

void CoverflowSheet::drawReading() const {
  const Rect r = readingRect();
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, BORDER, TILE_RADIUS, true);
  const int textX = r.x + INSET;
  const int headingH = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, textX, r.y + 10, tr(STR_COVERFLOW_READING), true, EpdFontFamily::BOLD);
  if (readingState != homecalendar::State::Ready) {
    const char* message = readingState == homecalendar::State::NoClock       ? tr(STR_CAL_NO_CLOCK)
                          : readingState == homecalendar::State::ClockBehind ? tr(STR_CAL_CLOCK_BEHIND)
                                                                             : tr(STR_CAL_UNAVAILABLE);
    renderer.drawText(SMALL_FONT_ID, textX, r.y + 10 + headingH + 8, message, true);
    return;
  }
  char value[40];
  snprintf(value, sizeof(value), tr(STR_CAL_MINUTES), static_cast<unsigned long long>(monthMinutes));
  renderer.drawText(UI_12_FONT_ID, textX, r.y + 10 + headingH + 2, value, true, EpdFontFamily::BOLD);
  char days[24];
  snprintf(days, sizeof(days), streak == 1 ? tr(STR_CAL_DAY) : tr(STR_CAL_DAYS), streak);
  snprintf(value, sizeof(value), "%s · %s", tr(STR_CAL_STREAK), days);
  renderer.drawText(SMALL_FONT_ID, textX, r.y + r.height - 10 - renderer.getLineHeight(SMALL_FONT_ID), value, true);

  // Last 7 days ending today. A bar reaches the middle line at the goal and is
  // capped at twice the goal; black means the goal was met.
  static constexpr StrId WEEKDAYS[] = {StrId::STR_CAL_MON, StrId::STR_CAL_TUE, StrId::STR_CAL_WED, StrId::STR_CAL_THU,
                                       StrId::STR_CAL_FRI, StrId::STR_CAL_SAT, StrId::STR_CAL_SUN};
  const int stripX = r.x + r.width / 2 - 20;
  const int stripW = r.x + r.width - INSET - stripX;
  const int gap = 6;
  const int barW = (stripW - 6 * gap) / 7;
  const int barTop = r.y + 12;
  const int barBottom = barTop + BAR_AREA_H;
  const uint32_t cap = std::max<uint32_t>(1, 2 * goal);
  for (int i = 0; i < 7; ++i) {
    const int x = stripX + i * (barW + gap);
    const int height = static_cast<int>(std::min(week[i].minutes, cap) * BAR_AREA_H / cap);
    if (height > 0) {
      if (week[i].goalMet) {
        renderer.fillRect(x, barBottom - height, barW, height, true);
      } else {
        renderer.fillRectDither(x, barBottom - height, barW, height, Color::DarkGray);
      }
    }
    renderer.fillRect(x, barBottom, barW, 1, true);
    const char* label = I18N.get(WEEKDAYS[week[i].weekday % 7]);
    renderer.drawText(SMALL_FONT_ID, x + (barW - renderer.getTextWidth(SMALL_FONT_ID, label)) / 2, barBottom + 4, label,
                      true);
  }
  const int goalY = barBottom - BAR_AREA_H / 2;
  for (int x = stripX; x < stripX + stripW; x += 6) renderer.fillRect(x, goalY, 3, 1, true);
}

void CoverflowSheet::drawTodo() const {
  const Rect r = todoRect();
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, BORDER, TILE_RADIUS, true);
  const int headingH = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, r.x + INSET, r.y + INSET, tr(STR_TODO), true, EpdFontFamily::BOLD);
  drawChevron(renderer, r.x + r.width - INSET - CHEVRON, r.y + INSET + headingH / 2);
  if (todo.available) {
    char open[24];
    snprintf(open, sizeof(open), tr(STR_COVERFLOW_TODO_OPEN), static_cast<unsigned>(todo.open));
    const int openW = renderer.getTextWidth(SMALL_FONT_ID, open);
    renderer.drawText(SMALL_FONT_ID, r.x + r.width - 2 * INSET - CHEVRON - openW,
                      r.y + INSET + (headingH - renderer.getLineHeight(SMALL_FONT_ID)) / 2, open, true);
  }
  todo.drawRows(renderer, r.x + INSET, r.y + INSET + headingH + HomeTodoPreview::GAP, r.width - 3 * INSET - CHEVRON);
}

void CoverflowSheet::drawWeather() const {
  const Rect r = weatherRect();
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, BORDER, TILE_RADIUS, true);
  drawChevron(renderer, r.x + r.width - INSET - CHEVRON, r.y + r.height / 2);
  const int textRight = r.x + r.width - 2 * INSET - CHEVRON;
  const int small = renderer.getLineHeight(SMALL_FONT_ID);
  if (weatherState != WeatherState::Ready) {
    const char* message = weatherState == WeatherState::Setup        ? tr(STR_WEATHER_SETUP)
                          : weatherState == WeatherState::NoForecast ? tr(STR_WEATHER_NO_FORECAST)
                                                                     : tr(STR_WEATHER_UNAVAILABLE_HINT);
    renderer.drawText(UI_10_FONT_ID, r.x + INSET, r.y + 10, tr(STR_WEATHER), true, EpdFontFamily::BOLD);
    const auto text = renderer.truncatedText(SMALL_FONT_ID, message, textRight - r.x - INSET);
    renderer.drawText(SMALL_FONT_ID, r.x + INSET, r.y + r.height - 10 - small, text.c_str(), true);
    return;
  }
  int x = r.x + INSET;
  if (weatherIcon) {
    renderer.drawIcon(weatherIcon, x - 6, r.y + (r.height - WEATHER_ICON) / 2, WEATHER_ICON);
    x += WEATHER_ICON - 2;
  }
  const int tempH = renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, x, r.y + (r.height - tempH) / 2, weatherTemp, true, EpdFontFamily::BOLD);
  x += renderer.getTextWidth(UI_12_FONT_ID, weatherTemp, EpdFontFamily::BOLD) + INSET;
  const auto title = renderer.truncatedText(UI_10_FONT_ID, weatherTitle, textRight - x);
  renderer.drawText(UI_10_FONT_ID, x, r.y + 10, title.c_str(), true);
  const auto detail = renderer.truncatedText(SMALL_FONT_ID, weatherDetail, textRight - x);
  renderer.drawText(SMALL_FONT_ID, x, r.y + r.height - 10 - small, detail.c_str(), true);
}

bool CoverflowSheet::tapped(const Rect rect) const {
  int row = -1;
  return input.rowTouch(row, rect.y, rect.height, 1, rect.x, rect.x + rect.width, rect.height) ==
         MappedInputManager::RowTouch::Tap;
}

CoverflowSheet::Action CoverflowSheet::handleTouch() {
  if (input.wasSwipe() == MappedInputManager::SwipeDir::Down) return Action::Close;
  if (tapped(Rect{0, 0, renderer.getScreenWidth(), top()})) return Action::Close;
  for (int i = 0; i < itemCount(); ++i) {
    if (tapped(tileCell(i))) {
      chosenItem = items[i];
      return Action::Open;
    }
  }
  if (tapped(readingRect())) {
    chosenItem = HomeMenuItem::STATISTICS;
    return Action::Open;
  }
  if (tapped(todoRect())) {
    chosenItem = HomeMenuItem::TODO;
    return Action::Open;
  }
  if (tapped(weatherRect())) {
    chosenItem = HomeMenuItem::WEATHER;
    return Action::Open;
  }
  return Action::None;
}
