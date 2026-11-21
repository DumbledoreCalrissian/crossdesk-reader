#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class SyncRequest;

// Bounded queue of EPUB reading sessions waiting for upload to a CrossPoint Sync
// server (/.crosspoint/reading_sessions.jsonl, one JSON object per line):
//   doc    32-hex sync document id      start  Unix time of the first counted page
//   secs   reading seconds counted by the EPUB statistics collector; start + secs
//          can be earlier than the real end, because pauses are not counted
//   tz     UTC offset in minutes at the start
//   from/to  whole-book progress in basis points (0..10000)
// Sessions are removed once the server accepts them; it is not a local history.
namespace readingSessionLog {

constexpr size_t MAX_QUEUE_BYTES = 32 * 1024;
// Shorter sessions are rejected by the server, so they are not queued.
constexpr uint32_t MIN_SECONDS = 10;

// True when queued sessions can reach a server: credentials are set and the
// server type is not plain KOSync.
bool enabled();

// Queues one session. When the queue is full, the oldest sessions are dropped.
bool append(const std::string& document, int64_t startTime, uint32_t seconds, uint16_t startProgressBp,
            uint16_t endProgressBp);

// Uploads queued sessions, oldest first, and removes the ones the server accepted
// or that it would always reject. One call sends a bounded number.
bool upload(SyncRequest& sync);

}  // namespace readingSessionLog
