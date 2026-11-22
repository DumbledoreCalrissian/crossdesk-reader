#include "HomeAppMenu.h"

#include <I18n.h>

#include "HomeMenuOrder.h"

HomeMenuItem appMenuItemAt(const int index, const HomeMenuItem slot) {
  int count = 0;
  while (homeMenuItemAt(count, slot) != HomeMenuItem::NONE) ++count;
  if (index == count) return HomeMenuItem::WEATHER;
  return homeMenuItemAt(index, slot);
}

int appMenuItemCount(const HomeMenuItem slot) {
  int count = 0;
  while (appMenuItemAt(count, slot) != HomeMenuItem::NONE) ++count;
  return count;
}

const char* appMenuLabel(const HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::WEATHER:
      return tr(STR_WEATHER);
    case HomeMenuItem::STATISTICS:
      return tr(STR_STATS_TITLE);
    case HomeMenuItem::FILE_BROWSER:
      return tr(STR_BROWSE_FILES);
    case HomeMenuItem::LIBRARY:
      return tr(STR_LIBRARY);
    case HomeMenuItem::PLUGINS:
      return tr(STR_PLUGINS);
    case HomeMenuItem::OPDS_BROWSER:
      return tr(STR_OPDS_BROWSER);
    case HomeMenuItem::SETTINGS_MENU:
      return tr(STR_SETTINGS_TITLE);
    case HomeMenuItem::TODO:
      return tr(STR_TODO);
    case HomeMenuItem::POMODORO:
      return tr(STR_POMODORO);
    case HomeMenuItem::SUDOKU:
      return tr(STR_SUDOKU);
    default:
      return "";
  }
}

UIIcon appMenuIcon(const HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return Folder;
    case HomeMenuItem::LIBRARY:
      return Library;
    case HomeMenuItem::PLUGINS:
      return Plugins;
    case HomeMenuItem::OPDS_BROWSER:
      return Book;
    case HomeMenuItem::SETTINGS_MENU:
      return Settings;
    case HomeMenuItem::TODO:
      return Todo;
    case HomeMenuItem::POMODORO:
      return Pomodoro;
    case HomeMenuItem::SUDOKU:
      return Sudoku;
    case HomeMenuItem::WEATHER:
      return Weather;
    case HomeMenuItem::STATISTICS:
      return Statistics;
    default:
      return None;
  }
}
