#pragma once

#include <string>

// Saved reading percentage, or -1 when unavailable. Never builds reading caches.
// With readOnly, stale TXT/Markdown caches are left for the reader to handle.
int loadBookProgress(const std::string& path, bool readOnly = false);
