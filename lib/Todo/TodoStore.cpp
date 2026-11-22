#include "TodoStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstddef>
#include <cstdlib>

namespace {
// ArduinoJson owns these blocks and returns them through this allocator.
// Bound parser expansion independently of the file-size limit.
class JsonBudget final : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Block {
    size_t bytes;
  };
  static constexpr size_t LIMIT = 32768;
  size_t used = 0;

 public:
  void* allocate(size_t bytes) override {
    if (bytes > LIMIT - used) return nullptr;
    auto* block = static_cast<Block*>(std::malloc(sizeof(Block) + bytes));
    if (!block) return nullptr;
    block->bytes = bytes;
    used += bytes;
    return block + 1;
  }
  void deallocate(void* pointer) override {
    if (!pointer) return;
    auto* block = static_cast<Block*>(pointer) - 1;
    used -= block->bytes;
    std::free(block);
  }
  void* reallocate(void* pointer, size_t bytes) override {
    if (!pointer) return allocate(bytes);
    auto* old = static_cast<Block*>(pointer) - 1;
    const size_t previous = old->bytes;
    if (bytes > LIMIT - used + previous) return nullptr;
    auto* block = static_cast<Block*>(std::realloc(old, sizeof(Block) + bytes));
    if (!block) return nullptr;
    block->bytes = bytes;
    used = used - previous + bytes;
    return block + 1;
  }
};
struct FileReader {
  HalFile& file;
  int read() { return file.read(); }
  size_t readBytes(char* buffer, size_t length) {
    const int bytes = file.read(buffer, length);
    return bytes > 0 ? static_cast<size_t>(bytes) : 0;
  }
};
}  // namespace

TodoStore::Read TodoStore::read(const char* path, TodoModel& model, uint32_t& generation) {
  if (!Storage.exists(path)) return Read::Missing;
  auto file = Storage.open(path);
  if (!file || file.isDirectory() || file.size() > MAX_FILE_BYTES) return Read::Invalid;
  if (!file.size()) return Read::Empty;
  JsonBudget budget;
  JsonDocument doc(&budget);
  FileReader reader{file};
  if (deserializeJson(doc, reader, DeserializationOption::NestingLimit(4))) return Read::Invalid;
  // Reject trailing bytes as well as valid JSON with an incomplete schema.
  for (int c = file.read(); c != -1; c = file.read()) {
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') return Read::Invalid;
  }
  if (!doc["version"].is<unsigned>() || doc["version"].as<unsigned>() != 1 || !doc["generation"].is<uint32_t>() ||
      !doc["nextId"].is<uint32_t>() || !doc["checksum"].is<uint32_t>() || !doc["tasks"].is<JsonArray>())
    return Read::Invalid;
  generation = doc["generation"].as<uint32_t>();
  if (!generation) return Read::Invalid;
  auto array = doc["tasks"].as<JsonArrayConst>();
  if (array.size() > TodoModel::MAX_TASKS) return Read::Invalid;
  if (!doc["sort"].isUnbound()) {
    if (!doc["sort"].is<uint8_t>() || doc["sort"].as<uint8_t>() > static_cast<uint8_t>(TodoModel::Sort::Created))
      return Read::Invalid;
    model.setSort(static_cast<TodoModel::Sort>(doc["sort"].as<uint8_t>()));
  }
  for (JsonObjectConst task : array) {
    if (!task["id"].is<uint32_t>() || !task["completed"].is<bool>() || !task["text"].is<const char*>())
      return Read::Invalid;
    for (const char* key : {"priority", "created", "due"}) {
      if (!task[key].isUnbound() && !task[key].is<int32_t>()) return Read::Invalid;
    }
    const int32_t priority = task["priority"] | int32_t{0};
    if (priority < 0 || priority > 3) return Read::Invalid;
    const JsonString text = task["text"].as<JsonString>();
    if (!model.appendLoaded(task["id"].as<uint32_t>(), task["completed"].as<bool>(),
                            std::string_view(text.c_str(), text.size()), static_cast<uint8_t>(priority),
                            task["created"] | int32_t{0}, task["due"] | int32_t{0}))
      return Read::Invalid;
  }
  if (!model.setNextId(doc["nextId"].as<uint32_t>()) || model.checksum(generation) != doc["checksum"].as<uint32_t>())
    return Read::Invalid;
  return Read::Valid;
}

TodoStore::Status TodoStore::load() {
  auto candidate = makeUniqueNoThrow<TodoModel>();
  if (!candidate) {
    LOG_ERR("TODO", "Load allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  bool invalid = false;
  tasks.clear();
  generation = 0;
  activeSlot = -1;
  writable = false;
  for (int slot = 0; slot < 2; ++slot) {
    candidate->clear();
    uint32_t revision = 0;
    const auto result = read(PATHS[slot], *candidate, revision);
    if (result == Read::Invalid) invalid = true;
    if (result == Read::Valid && revision > generation) {
      tasks = *candidate;
      generation = revision;
      activeSlot = slot;
    }
  }
  if (invalid) LOG_ERR("TODO", "Invalid snapshot; %s", activeSlot >= 0 ? "using intact copy" : "editing disabled");
  writable = activeSlot >= 0 || !invalid;
  return writable ? Status::Ok : Status::InvalidData;
}

bool TodoStore::save(const TodoModel& model) {
  if (!writable || generation == UINT32_MAX || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  JsonBudget budget;
  JsonDocument doc(&budget);
  doc["version"] = 1;
  doc["generation"] = generation + 1;
  doc["nextId"] = model.nextId();
  doc["checksum"] = model.checksum(generation + 1);
  if (model.sort() != TodoModel::Sort::AddedOrder) doc["sort"] = static_cast<uint8_t>(model.sort());
  auto array = doc["tasks"].to<JsonArray>();
  for (size_t i = 0; i < model.size(); ++i) {
    const auto& task = *model.at(i);
    auto entry = array.add<JsonObject>();
    entry["id"] = task.id;
    entry["completed"] = task.completed;
    entry["text"] = task.text;
    if (task.priority) entry["priority"] = task.priority;
    if (task.created) entry["created"] = task.created;
    if (task.due) entry["due"] = task.due;
  }
  if (doc.overflowed()) return false;
  const size_t expected = measureJson(doc);
  if (expected > MAX_FILE_BYTES) return false;
  auto file = Storage.open(TEMP, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  const bool complete = serializeJson(doc, file) == expected;
  if (!file.close() || !complete) return false;
  // FAT cannot replace a destination atomically. Only discard the inactive
  // snapshot; the active snapshot survives every failure before promotion.
  const int target = activeSlot == 0 ? 1 : 0;
  if (Storage.exists(PATHS[target]) && !Storage.remove(PATHS[target])) return false;
  if (!Storage.rename(TEMP, PATHS[target])) return false;
  activeSlot = target;
  ++generation;
  return true;
}

TodoStore::Status TodoStore::change(Change change, uint32_t id, std::string_view text) {
  if (!writable) return Status::InvalidData;
  auto candidate = makeUniqueNoThrow<TodoModel>(tasks);
  if (!candidate) {
    LOG_ERR("TODO", "Edit allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  TodoModel::Result result = TodoModel::Result::NotFound;
  switch (change) {
    case Change::Add:
      result = candidate->add(text);
      break;
    case Change::Edit:
      result = candidate->edit(id, text);
      break;
    case Change::Toggle:
      result = candidate->toggle(id);
      break;
    case Change::Delete:
      result = candidate->erase(id);
      break;
  }
  return commit(*candidate, result);
}

TodoStore::Status TodoStore::saveTask(uint32_t id, std::string_view text, int32_t due, int32_t created) {
  if (!writable) return Status::InvalidData;
  auto candidate = makeUniqueNoThrow<TodoModel>(tasks);
  if (!candidate) {
    LOG_ERR("TODO", "Task allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  if (!id) return commit(*candidate, candidate->add(text, created, due));
  const auto edited = candidate->edit(id, text);
  if (edited != TodoModel::Result::Ok && edited != TodoModel::Result::Unchanged) return commit(*candidate, edited);
  const auto dated = candidate->setDue(id, due);
  return commit(*candidate, dated == TodoModel::Result::Unchanged ? edited : dated);
}

TodoStore::Status TodoStore::setPriority(uint32_t id, uint8_t priority) {
  if (!writable) return Status::InvalidData;
  auto candidate = makeUniqueNoThrow<TodoModel>(tasks);
  if (!candidate) {
    LOG_ERR("TODO", "Priority allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  return commit(*candidate, candidate->setPriority(id, priority));
}

TodoStore::Status TodoStore::setSort(TodoModel::Sort sort) {
  if (!writable) return Status::InvalidData;
  auto candidate = makeUniqueNoThrow<TodoModel>(tasks);
  if (!candidate) {
    LOG_ERR("TODO", "Sort allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  return commit(*candidate, candidate->setSort(sort));
}

TodoStore::Status TodoStore::commit(const TodoModel& model, TodoModel::Result result) {
  switch (result) {
    case TodoModel::Result::Unchanged:
      return Status::Ok;
    case TodoModel::Result::InvalidText:
      return Status::InvalidText;
    case TodoModel::Result::Full:
    case TodoModel::Result::IdExhausted:
      return Status::Full;
    case TodoModel::Result::NotFound:
    case TodoModel::Result::InvalidData:
      return Status::InvalidData;
    case TodoModel::Result::Ok:
      break;
  }
  if (!save(model)) {
    LOG_ERR("TODO", "Save failed; previous snapshot retained");
    return Status::StorageError;
  }
  tasks = model;
  return Status::Ok;
}

TodoStore::Status TodoStore::eraseTasks(const uint32_t* ids, size_t count) {
  if (!writable) return Status::InvalidData;
  if (!count) return Status::Ok;
  if (!ids || count > TodoModel::MAX_TASKS) return Status::InvalidData;
  auto candidate = makeUniqueNoThrow<TodoModel>(tasks);
  if (!candidate) {
    LOG_ERR("TODO", "Delete allocation failed: %u bytes", static_cast<unsigned>(sizeof(TodoModel)));
    return Status::OutOfMemory;
  }
  for (size_t i = 0; i < count; ++i) {
    if (candidate->erase(ids[i]) != TodoModel::Result::Ok) return Status::InvalidData;
  }
  if (!save(*candidate)) {
    LOG_ERR("TODO", "Batch delete failed; previous snapshot retained");
    return Status::StorageError;
  }
  tasks = *candidate;
  return Status::Ok;
}
