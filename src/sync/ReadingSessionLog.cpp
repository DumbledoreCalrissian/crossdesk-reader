#include "ReadingSessionLog.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <KOReaderCredentialStore.h>
#include <KOReaderSyncClient.h>
#include <Logging.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <esp_mac.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "SyncRequest.h"

namespace {
constexpr char LOG_PATH[] = "/.crosspoint/reading_sessions.jsonl";
constexpr char TMP_PATH[] = "/.crosspoint/reading_sessions.tmp";

constexpr size_t BATCH_SIZE = 20;  // server limit per request
// Bounds one manual sync to 200 sessions when a long history is first uploaded.
constexpr size_t MAX_BATCHES = 10;
constexpr size_t MAX_LINE = 160;

// Server limits; a session outside them would make the server reject its whole batch.
constexpr int64_t MIN_START = 946684800;  // 2000-01-01
constexpr uint32_t MAX_SECONDS = 7 * 86400;
constexpr int64_t MAX_FUTURE_SECONDS = 300;

int utcOffsetMinutes(const time_t at) {
  struct tm local;
  struct tm utc;
  localtime_r(&at, &local);
  gmtime_r(&at, &utc);
  // Reading the UTC fields as local time shifts the result by exactly the zone offset.
  utc.tm_isdst = local.tm_isdst;
  return std::clamp(static_cast<int>((at - mktime(&utc)) / 60), -720, 840);
}

// Reads the line that starts at offset, without its newline, and moves offset past it.
// An overlong line comes back empty. Returns false, leaving offset alone, when no
// complete line is left: the end of the log, an unfinished last line, or a read error.
bool readLine(HalFile& file, size_t& offset, char* line, const size_t size) {
  size_t next = offset;
  bool overlong = false;
  while (file.seek(next)) {
    const int got = file.read(line, size - 1);
    if (got <= 0) return false;
    if (const void* newline = memchr(line, '\n', got)) {
      const size_t length = static_cast<const char*>(newline) - line;
      line[overlong ? 0 : length] = '\0';
      offset = next + length + 1;
      return true;
    }
    if (static_cast<size_t>(got) < size - 1) return false;
    overlong = true;
    next += got;
  }
  return false;
}

bool isDocumentId(const char* document) {
  return strlen(document) == 32 && strspn(document, "0123456789abcdefABCDEF") == 32;
}

// Unique per device: every CrossPoint reader reports the same device id to the server.
void formatSessionId(char* out, const size_t size, const int64_t start, const uint8_t* mac) {
  snprintf(out, size, "%lld-%02x%02x%02x%02x%02x%02x", static_cast<long long>(start), mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
}

// Writes the queue from offset onwards to a temporary file that replaces it.
// A power cut between the two steps leaves the temporary file, which recover() restores.
bool keepFrom(const size_t offset, char* buffer, const size_t size) {
  HalFile source = Storage.open(LOG_PATH);
  if (!source) return false;
  const size_t total = source.size();
  if (offset >= total) {
    source.close();
    return Storage.remove(LOG_PATH);
  }
  HalFile target = Storage.open(TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC);
  bool ok = target && source.seek(offset);
  for (size_t done = offset; ok && done < total;) {
    const int got = source.read(buffer, std::min(size, total - done));
    ok = got > 0 && target.write(buffer, got) == static_cast<size_t>(got);
    done += got > 0 ? got : 0;
  }
  source.close();
  if (target) target.close();
  if (!ok) {
    Storage.remove(TMP_PATH);
    LOG_ERR("RSL", "Failed to shorten the reading session queue");
    return false;
  }
  return Storage.remove(LOG_PATH) && Storage.rename(TMP_PATH, LOG_PATH);
}

void recover() {
  if (!Storage.exists(TMP_PATH)) return;
  if (Storage.exists(LOG_PATH)) {
    Storage.remove(TMP_PATH);
  } else {
    Storage.rename(TMP_PATH, LOG_PATH);
  }
}
}  // namespace

bool readingSessionLog::enabled() {
  return KOREADER_STORE.hasCredentials() && KOREADER_STORE.getServerType() != KOReaderServerType::KOSYNC;
}

bool readingSessionLog::append(const std::string& document, const int64_t startTime, const uint32_t seconds,
                               const uint16_t startProgressBp, const uint16_t endProgressBp) {
  if (!isDocumentId(document.c_str()) || seconds < MIN_SECONDS) return false;
  JsonDocument doc;
  doc["doc"] = document;
  doc["start"] = startTime;
  doc["secs"] = seconds;
  doc["tz"] = utcOffsetMinutes(static_cast<time_t>(startTime));
  doc["from"] = startProgressBp;
  doc["to"] = endProgressBp;
  // One spare byte in front for a newline that ends an unfinished line already in the queue.
  const auto buffer = makeUniqueNoThrow<char[]>(MAX_LINE + 2);
  if (!buffer) return false;
  char* line = buffer.get() + 1;
  size_t length = SyncRequest::serialize(doc, line, MAX_LINE);
  if (length == 0) return false;
  line[length++] = '\n';

  Storage.mkdir("/.crosspoint");
  recover();
  HalFile file = Storage.open(LOG_PATH, O_RDWR | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("RSL", "Failed to open the reading session queue");
    return false;
  }
  size_t sizeBefore = file.size();
  if (sizeBefore + length + 1 > MAX_QUEUE_BYTES) {
    // Full: drop the oldest sessions until this one fits.
    size_t drop = 0;
    while (sizeBefore - drop + length + 1 > MAX_QUEUE_BYTES && readLine(file, drop, line, MAX_LINE + 1)) {
    }
    if (sizeBefore - drop + length + 1 > MAX_QUEUE_BYTES) drop = sizeBefore;
    file.close();
    LOG_INF("RSL", "Session queue full; dropping %u bytes of old sessions", static_cast<unsigned>(drop));
    // The line was overwritten while scanning; build it again after shortening.
    if (!keepFrom(drop, line, MAX_LINE)) return false;
    length = SyncRequest::serialize(doc, line, MAX_LINE);
    if (length == 0) return false;
    line[length++] = '\n';
    file = Storage.open(LOG_PATH, O_RDWR | O_CREAT | O_APPEND);
    if (!file) return false;
    sizeBefore = file.size();
  }
  // A power cut can leave an unfinished line, which would swallow this record.
  char last = '\n';
  if (sizeBefore > 0 && file.seek(sizeBefore - 1)) file.read(&last, 1);
  if (last != '\n') {
    *--line = '\n';
    ++length;
  }
  const bool written = file.write(line, length) == length;
  // A partial line would swallow the next session appended after it.
  if (!written) file.truncate(sizeBefore);
  file.close();
  if (!written) LOG_ERR("RSL", "Failed to queue a reading session");
  return written;
}

bool readingSessionLog::upload(SyncRequest& sync) {
  recover();
  if (!Storage.exists(LOG_PATH)) return true;
  HalFile file = Storage.open(LOG_PATH);
  if (!file) return false;
  // End of the last batch the server accepted. Lines before it are removed afterwards.
  size_t offset = 0;

  uint8_t mac[6] = {};
  esp_efuse_mac_get_default(mac);
  const int64_t now = trustedtime::trustedNow();
  const std::string url = KOREADER_STORE.getBaseUrl() + "/api/v1/stats/sessions";
  bool ok = true;
  size_t consumed = offset;  // end of the last complete line read
  JsonDocument doc;
  // On the heap: the loop task's stack is small and TLS runs below this frame.
  const auto lineBuffer = makeUniqueNoThrow<char[]>(MAX_LINE + 1);
  if (!lineBuffer) return false;
  char* const line = lineBuffer.get();
  char sessionId[40];

  for (size_t batch = 0; ok && batch < MAX_BATCHES; ++batch) {
    // Written straight into the request buffer: every value is a number or checked hex, so nothing needs escaping.
    char* const body = sync.body();
    size_t length =
        snprintf(body, SyncRequest::BUFFER_SIZE, "{\"device_id\":\"%s\",\"items\":[", KOReaderSyncClient::DEVICE_ID);
    int64_t starts[BATCH_SIZE];
    size_t count = 0;
    bool endOfLog = false;
    while (count < BATCH_SIZE) {
      if (!readLine(file, consumed, line, MAX_LINE + 1)) {
        endOfLog = true;
        break;
      }
      JsonDocument entry;
      if (!SyncRequest::parse(entry, line, strlen(line))) continue;
      const char* document = entry["doc"] | "";
      const int64_t start = entry["start"] | static_cast<int64_t>(0);
      const uint32_t seconds = entry["secs"] | static_cast<uint32_t>(0);
      // A session the server would reject (for example a wrong clock) is dropped with its batch.
      if (!isDocumentId(document) || start < MIN_START || seconds < MIN_SECONDS || seconds > MAX_SECONDS ||
          (now > 0 && start + seconds > now + MAX_FUTURE_SECONDS) ||
          std::find(starts, starts + count, start) != starts + count) {
        continue;
      }
      formatSessionId(sessionId, sizeof(sessionId), start, mac);
      length += snprintf(body + length, SyncRequest::BUFFER_SIZE - length,
                         "%s{\"session_id\":\"%s\",\"document\":\"%s\",\"started_at\":%lld,\"ended_at\":%lld,"
                         "\"seconds\":%lu,\"utc_offset_minutes\":%d}",
                         count ? "," : "", sessionId, document, static_cast<long long>(start),
                         static_cast<long long>(start + seconds), static_cast<unsigned long>(seconds),
                         std::clamp(entry["tz"] | 0, -720, 840));
      starts[count++] = start;
    }

    if (count > 0) {
      length += snprintf(body + length, SyncRequest::BUFFER_SIZE - length, "]}");
      if (!sync.sendBody(url, length, doc)) {
        ok = false;
        break;
      }
      // The server lists what it stored; an older server answers 200 without storing anything.
      const JsonArrayConst accepted = doc["accepted_session_ids"].as<JsonArrayConst>();
      for (size_t i = 0; i < count && ok; ++i) {
        formatSessionId(sessionId, sizeof(sessionId), starts[i], mac);
        ok = false;
        for (JsonVariantConst id : accepted) {
          if (strcmp(id | "", sessionId) == 0) ok = true;
        }
      }
      if (!ok) {
        LOG_ERR("RSL", "Server did not acknowledge reading sessions");
        break;
      }
    }
    offset = consumed;
    if (endOfLog) break;
  }
  file.close();
  // An unfinished last line stays for the next session's append to close.
  // The request buffer is free now and copies the rest in a few large writes.
  if (offset > 0 && !keepFrom(offset, sync.body(), SyncRequest::BUFFER_SIZE)) ok = false;
  return ok;
}
