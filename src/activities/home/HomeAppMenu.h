#pragma once

#include "activities/ActivityManager.h"
#include "components/themes/BaseTheme.h"

// Apps menu shared by the Desk panel and the Coverflow sheet: the Home
// shortcut order plus Weather. slot: Plugins, OPDS or NONE.
HomeMenuItem appMenuItemAt(int index, HomeMenuItem slot);
int appMenuItemCount(HomeMenuItem slot);
const char* appMenuLabel(HomeMenuItem item);
UIIcon appMenuIcon(HomeMenuItem item);
