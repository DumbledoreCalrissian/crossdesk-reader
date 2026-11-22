#pragma once

#include <Bitmap.h>
#include <FreeInkUICore.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <array>
#include <string>

class GfxRenderer;

// Screen-lifetime thumbnail decoding and PSRAM region snapshots.
class HomeCoverCache {
 public:
  // Slot count; Coverflow caches one centre cover per recent book.
  static constexpr size_t MAX_COVERS = 10;
  explicit HomeCoverCache(GfxRenderer& renderer) : renderer(renderer) {}
  // capacity 0: one screen of snapshots, enough for Desk and Cover Grid.
  void begin(size_t capacity = 0);
  void prepare();
  void invalidate();
  void invalidate(size_t index);
  // artShift nudges the art right of centre (Desk's look); 0 keeps it flush with the frame.
  bool paint(freeink::ui::Rect rect, size_t index, const std::string& path, int artShift = 3);

 private:
  struct CachedCover {
    freeink::ui::Rect rect{};
    size_t offset = 0;
    size_t bytes = 0;
    bool valid = false;
  };
  HalMemory::PsramBuffer coverCache;
  std::array<CachedCover, MAX_COVERS> cachedCovers{};
  size_t coverCacheCapacity = 0;
  size_t coverCacheUsed = 0;
  int coverCacheOrientation = -1;
  bool coverCacheInverted = false;

  HalFile coverFile;
  Bitmap coverBitmap{coverFile};
  GfxRenderer& renderer;
};
