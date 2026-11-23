#include <HalStorage.h>
#include <Serialization.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

#include "Epub/BookMetadataCache.h"
#include "ZipFile.h"
#include "test/ScopedTempDir.h"

// Counting allocator: heap bytes allocated while `counting` is set and not yet freed.
namespace heap {
bool counting = false;
size_t live = 0;

struct alignas(std::max_align_t) Header {
  size_t size;
  bool counted;
};

void* allocate(const size_t size) {
  auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
  if (!header) return nullptr;
  header->size = size;
  header->counted = counting;
  if (counting) live += size;
  return header + 1;
}

void release(void* p) {
  if (!p) return;
  auto* header = static_cast<Header*>(p) - 1;
  if (header->counted) live -= header->size;
  std::free(header);
}
}  // namespace heap

void* operator new(const size_t size) {
  void* p = heap::allocate(size);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](const size_t size) { return operator new(size); }
void* operator new(const size_t size, const std::nothrow_t&) noexcept { return heap::allocate(size); }
void* operator new[](const size_t size, const std::nothrow_t&) noexcept { return heap::allocate(size); }
void operator delete(void* p) noexcept { heap::release(p); }
void operator delete[](void* p) noexcept { heap::release(p); }
void operator delete(void* p, size_t) noexcept { heap::release(p); }
void operator delete[](void* p, size_t) noexcept { heap::release(p); }

namespace {

constexpr size_t LUT_OFFSET_FIELD = sizeof(uint8_t);
constexpr size_t METADATA_START = LUT_OFFSET_FIELD + sizeof(uint32_t) + 2 * sizeof(uint16_t);
const BookMetadataCache::BookMetadata metadata{"A book", "An author", "en", "cover.jpg", "Text/chapter0.xhtml"};

std::string chapterHref(const int i) { return "OEBPS/Text/chapter" + std::to_string(i) + ".xhtml"; }
// Empty chapters, some across the window edge at item 32, give equal running totals.
uint32_t chapterBytes(const int i) {
  return (i % 37 == 5 || (i >= 30 && i < 35)) ? 0 : 1500 + static_cast<uint32_t>(i) * 7919 % 30000;
}

// Writes book.bin through the cache's own writer; returns every chapter's cumulative size.
std::vector<uint32_t> buildBook(const std::string& dir, const int spineCount) {
  zipEntrySizes.clear();
  std::vector<uint32_t> cumulative;
  for (int i = 0; i < spineCount; i++) {
    zipEntrySizes[chapterHref(i)] = chapterBytes(i);
    cumulative.push_back((i > 0 ? cumulative.back() : 0) + chapterBytes(i));
  }
  BookMetadataCache cache(dir);
  EXPECT_TRUE(cache.beginWrite() && cache.beginContentOpfPass());
  for (int i = 0; i < spineCount; i++) cache.createSpineEntry(chapterHref(i));
  EXPECT_TRUE(cache.endContentOpfPass() && cache.beginTocPass());
  for (int i = 0; i < spineCount; i++) cache.createTocEntry("Chapter " + std::to_string(i), chapterHref(i), "", 1);
  EXPECT_TRUE(cache.endTocPass() && cache.endWrite());
  EXPECT_TRUE(cache.buildBookBin("book.epub", metadata));
  EXPECT_TRUE(cache.cleanupTmpFiles());
  return cumulative;
}

}  // namespace

TEST(BookMetadataCacheLoad, RejectsTruncatedHeaderAndCanReloadAfterRebuild) {
  const ScopedTempDir tempDir;
  const auto dir = tempDir.path().string();
  const auto path = tempDir.path() / "book.bin";
  BookMetadataCache cache(dir);
  buildBook(dir, 2);
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.coreMetadata.title, metadata.title);
  EXPECT_EQ(cache.coreMetadata.textReferenceHref, metadata.textReferenceHref);

  std::filesystem::resize_file(path, METADATA_START - 1);
  EXPECT_FALSE(cache.load());
  EXPECT_FALSE(cache.isLoaded());
  EXPECT_EQ(cache.getCumulativeSize(0), 0u);

  const auto cumulative = buildBook(dir, 2);
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.getCumulativeSize(1), cumulative.back());
}

TEST(BookMetadataCacheLoad, RejectsLookupTableOutsideFile) {
  const ScopedTempDir tempDir;
  const auto dir = tempDir.path().string();
  buildBook(dir, 2);
  {
    std::fstream file(tempDir.path() / "book.bin", std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.seekp(LUT_OFFSET_FIELD));
    serialization::writePod(file, UINT32_MAX);
    ASSERT_TRUE(file);
  }
  BookMetadataCache cache(dir);
  EXPECT_FALSE(cache.load());
}

TEST(BookMetadataCacheLoad, RejectsMetadataStringCrossingLookupTable) {
  const ScopedTempDir tempDir;
  const auto dir = tempDir.path().string();
  buildBook(dir, 2);
  {
    const auto path = tempDir.path() / "book.bin";
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.seekp(METADATA_START));
    // This length still fits in the file, but the title would extend into the spine data.
    const auto length = static_cast<uint32_t>(std::filesystem::file_size(path) - METADATA_START - sizeof(uint32_t));
    serialization::writePod(file, length);
    ASSERT_TRUE(file);
  }
  BookMetadataCache cache(dir);
  EXPECT_FALSE(cache.load());
  EXPECT_TRUE(cache.coreMetadata.title.empty());
}

TEST(BookMetadataCacheLoad, RejectsTruncatedSpineAtOpen) {
  const ScopedTempDir tempDir;
  const auto dir = tempDir.path().string();
  buildBook(dir, 2);
  const auto path = tempDir.path() / "book.bin";
  std::ifstream in(path, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  const auto href = bytes.find(chapterHref(1));
  ASSERT_NE(href, std::string::npos);
  std::filesystem::resize_file(path, href + chapterHref(1).size());
  BookMetadataCache cache(dir);
  EXPECT_FALSE(cache.load());
  EXPECT_FALSE(cache.isLoaded());
}

// A loaded book must not hold heap per chapter: 5,000 chapters may hold at most 1 KB more than 30.
TEST(BookMetadataCacheHeap, ResidentHeapDoesNotGrowWithSpineCount) {
  size_t resident[2] = {};
  for (const int spineCount : {30, 5000}) {
    const ScopedTempDir tempDir;
    const auto cacheDir = tempDir.path().string();
    const std::vector<uint32_t> cumulative = buildBook(cacheDir, spineCount);
    BookMetadataCache cache(cacheDir);
    heap::live = 0;
    heap::counting = true;
    EXPECT_TRUE(cache.load());
    const size_t loaded = heap::live;
    for (int i = 0; i < spineCount; i++) EXPECT_EQ(cache.getCumulativeSize(i), cumulative[i]) << "spine " << i;
    EXPECT_EQ(heap::live, loaded) << spineCount << " chapters";
    heap::counting = false;
    resident[spineCount == 5000] = loaded;
  }
  EXPECT_LE(resident[1], resident[0] + 1024) << "30 chapters: " << resident[0] << " B";
}

// book.bin cut short inside a window: the window answers the total before it, never less.
TEST(BookMetadataCacheSizes, TruncatedWindowFallsBackToTheTotalBeforeIt) {
  const ScopedTempDir tempDir;
  const auto cacheDir = tempDir.path().string();
  const std::vector<uint32_t> cumulative = buildBook(cacheDir, 100);
  const auto path = tempDir.path() / "book.bin";
  std::ifstream in(path, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const size_t href = bytes.find(chapterHref(64));
  ASSERT_NE(href, std::string::npos);
  BookMetadataCache cache(cacheDir);
  ASSERT_TRUE(cache.load());
  EXPECT_EQ(cache.getCumulativeSize(0), cumulative[0]);
  std::filesystem::resize_file(path, href + chapterHref(64).size());  // just before item 64's size
  EXPECT_EQ(cache.getCumulativeSize(63), cumulative[63]);
  EXPECT_EQ(cache.getCumulativeSize(64), cumulative[63]);
  // A lookup into that window tries the refill once and keeps the fallback; the next lookup retries.
  for (int attempt = 0; attempt < 2; attempt++) {
    const int opens = Storage.readOpens;
    EXPECT_EQ(cache.getSpineIndexForSize(cumulative[70]), 95);
    EXPECT_EQ(Storage.readOpens - opens, 1) << "attempt " << attempt;
  }
}

// The lookup answers what a scan of every item answers, for every boundary, and reads at most one window.
TEST(BookMetadataCacheSizes, SpineIndexForSizeMatchesLinearScan) {
  for (const int spineCount : {100, 5000}) {
    const ScopedTempDir tempDir;
    const auto cacheDir = tempDir.path().string();
    const std::vector<uint32_t> cumulative = buildBook(cacheDir, spineCount);
    BookMetadataCache scanned(cacheDir);
    BookMetadataCache cache(cacheDir);
    ASSERT_TRUE(scanned.load() && cache.load());
    std::vector<uint32_t> totals;
    for (int i = 0; i < spineCount; i++) totals.push_back(scanned.getCumulativeSize(i));
    const uint32_t bookSize = cumulative.back();
    std::vector<uint32_t> targets = {0, bookSize - 1, bookSize, bookSize + 1, 0};
    for (const uint32_t total : cumulative) targets.insert(targets.end(), {total - 1, total, total + 1});
    for (const uint32_t size : targets) {
      int expected = -1;
      for (int i = 0; i < spineCount && expected < 0; i++) {
        if (totals[i] >= size) expected = i;
      }
      const int opens = Storage.readOpens;
      EXPECT_EQ(cache.getSpineIndexForSize(size), expected) << spineCount << " items, size " << size;
      EXPECT_LE(Storage.readOpens - opens, 1) << spineCount << " items, size " << size;
    }
  }
}
