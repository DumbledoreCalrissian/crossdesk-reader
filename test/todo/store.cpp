#include <ArduinoJson.h>
#include <HalStorage.h>
#include <TodoStore.h>

#include <cassert>
#include <iostream>
#include <memory>

using S = TodoStore::Status;
using C = TodoStore::Change;
static constexpr const char* A = "/.crosspoint/todo-a.json";
static constexpr const char* B = "/.crosspoint/todo-b.json";

// Frozen 0.2.0 checksum rules, calculated from JSON without the new model.
static uint32_t oldChecksum(JsonDocument& doc) {
  uint32_t hash = 2166136261u;
  auto byte = [&hash](uint8_t b) { hash = (hash ^ b) * 16777619u; };
  auto number = [&byte](uint32_t n) {
    for (int i = 0; i < 4; ++i) {
      byte(n & 0xff);
      n >>= 8;
    }
  };
  number(1);
  number(doc["generation"].as<uint32_t>());
  number(doc["nextId"].as<uint32_t>());
  number(doc["tasks"].size());
  for (JsonObjectConst task : doc["tasks"].as<JsonArrayConst>()) {
    number(task["id"].as<uint32_t>());
    byte(task["completed"].as<bool>());
    for (const char* p = task["text"].as<const char*>(); *p; ++p) byte(static_cast<uint8_t>(*p));
    byte(0);
  }
  return hash;
}

static void metadataChecks() {
  Storage.files.clear();
  auto store = std::make_unique<TodoStore>();
  assert(store->load() == S::Ok);
  assert(store->saveTask(0, "dated", 20785, 20779) == S::Ok);
  assert(store->setPriority(1, 3) == S::Ok);
  assert(store->setSort(TodoModel::Sort::DueDate) == S::Ok);
  const auto writes = Storage.writes;
  assert(store->saveTask(1, "dated", 20785, 1) == S::Ok && Storage.writes == writes);
  assert(store->setPriority(1, 3) == S::Ok && Storage.writes == writes);
  assert(store->setSort(TodoModel::Sort::DueDate) == S::Ok && Storage.writes == writes);
  assert(store->saveTask(1, "lost", -1) == S::InvalidData && Storage.writes == writes);
  assert(store->setPriority(1, 4) == S::InvalidData && Storage.writes == writes);
  assert(store->setSort(static_cast<TodoModel::Sort>(4)) == S::InvalidData && Storage.writes == writes);
  assert(store->saveTask(1, "renamed", 20786, 1) == S::Ok);
  assert(store->model().find(1)->created == 20779);
  auto restarted = std::make_unique<TodoStore>();
  assert(restarted->load() == S::Ok);
  const auto* task = restarted->model().find(1);
  assert(task->priority == 3 && task->created == 20779 && task->due == 20786);
  assert(restarted->model().sort() == TodoModel::Sort::DueDate);
  const auto snapshots = Storage.files;
  for (const auto& [path, json] : snapshots) {
    if (path != A && path != B) continue;
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    assert(doc["version"] == 1 && oldChecksum(doc) == doc["checksum"].as<uint32_t>());
  }

  // Metadata failures must leave both the live model and recovered data intact.
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = snapshots;
    assert(store->load() == S::Ok);
    Storage.writeBudget = failure == 0 ? 12 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRename = failure == 3;
    Storage.failRemove = failure == 4;
    Storage.failDirectory = failure == 5;
    assert(store->setPriority(1, 1) == S::StorageError);
    assert(store->model().find(1)->priority == 3);
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRename = Storage.failRemove = Storage.failDirectory = false;
    assert(restarted->load() == S::Ok && restarted->model().find(1)->priority == 3);
  }

  JsonDocument doc;
  assert(!deserializeJson(doc, snapshots.at(B)));
  const auto generation = doc["generation"].as<uint32_t>();
  for (const char* key : {"priority", "created", "due", "sort"}) {
    for (const char* invalid : {"null", "true", "\"1\"", "1.5", "-1", "4294967296", "{}", "[]"}) {
      JsonDocument bad;
      assert(!deserializeJson(bad, snapshots.at(B)));
      JsonDocument value;
      assert(!deserializeJson(value, invalid));
      if (std::string(key) == "sort")
        bad[key].set(value.as<JsonVariantConst>());
      else
        bad["tasks"][0][key].set(value.as<JsonVariantConst>());
      Storage.files.clear();
      serializeJson(bad, Storage.files[A]);
      assert(restarted->load() == S::InvalidData && !restarted->canEdit());
      Storage.files[B] = snapshots.at(B);
      assert(restarted->load() == S::Ok && restarted->model().find(1)->priority == 3);
    }
  }
  for (const char* key : {"priority", "created", "due", "sort"}) {
    JsonDocument bad;
    assert(!deserializeJson(bad, snapshots.at(B)));
    const int32_t invalid = (std::string(key) == "priority" || std::string(key) == "sort") ? 4 : TodoModel::MAX_DAY + 1;
    if (std::string(key) == "sort")
      bad[key] = invalid;
    else
      bad["tasks"][0][key] = invalid;
    Storage.files.clear();
    serializeJson(bad, Storage.files[A]);
    assert(restarted->load() == S::InvalidData);
  }

  // Simulate an old firmware save dropping metadata, with an unchanged checksum.
  doc.remove("sort");
  for (JsonObject taskJson : doc["tasks"].as<JsonArray>()) {
    taskJson.remove("priority");
    taskJson.remove("created");
    taskJson.remove("due");
  }
  assert(oldChecksum(doc) == doc["checksum"].as<uint32_t>() && doc["generation"] == generation);
  Storage.files.clear();
  serializeJson(doc, Storage.files[A]);
  const auto oldFile = Storage.files[A];
  assert(restarted->load() == S::Ok && Storage.files[A] == oldFile);
  task = restarted->model().find(1);
  assert(task->priority == 0 && task->created == 0 && task->due == 0);
  assert(std::string(task->text) == "renamed" && restarted->model().sort() == TodoModel::Sort::AddedOrder);

  // Every valid text byte can require two JSON bytes (quotes or backslashes).
  Storage.files.clear();
  assert(store->load() == S::Ok);
  for (size_t i = 0; i < TodoModel::MAX_TASKS; ++i) {
    std::string text(TodoModel::MAX_TEXT_BYTES, i % 2 ? '\\' : '"');
    for (size_t bit = 0; bit < 5; ++bit) text[bit] = (i & (1u << bit)) ? '\\' : '"';
    assert(store->saveTask(0, text, TodoModel::MAX_DAY, TodoModel::MAX_DAY) == S::Ok);
    assert(store->setPriority(i + 1, 3) == S::Ok);
  }
  assert(store->setSort(TodoModel::Sort::Created) == S::Ok);
  assert(restarted->load() == S::Ok && restarted->model().size() == 32);
  for (const auto& [path, json] : Storage.files) {
    if (path != A && path != B) continue;
    assert(json.size() < 32768);
    JsonDocument full;
    assert(!deserializeJson(full, json));
    assert(oldChecksum(full) == full["checksum"].as<uint32_t>());
    std::cout << "Full escaped metadata snapshot: " << json.size() << " bytes\n";
  }

  // Largest valid IDs, counters and dates, with unique all-escaped text.
  JsonDocument largest;
  assert(!deserializeJson(largest, Storage.files.at(A)));
  largest["generation"] = UINT32_MAX - 1;
  largest["nextId"] = UINT32_MAX;
  largest["sort"] = static_cast<uint8_t>(TodoModel::Sort::AddedOrder);
  uint32_t id = UINT32_MAX - TodoModel::MAX_TASKS;
  for (JsonObject entry : largest["tasks"].as<JsonArray>()) entry["id"] = id++;
  largest["checksum"] = oldChecksum(largest);
  Storage.files.clear();
  serializeJson(largest, Storage.files[A]);
  assert(store->load() == S::Ok && store->model().size() == 32);
  assert(store->setSort(TodoModel::Sort::Created) == S::Ok);
  assert(Storage.files.at(B).size() < 32768);
  assert(restarted->load() == S::Ok && restarted->model().find(UINT32_MAX - 1));
  assert(restarted->setSort(TodoModel::Sort::Priority) == S::StorageError);
  std::cout << "Largest counters + escaped metadata snapshot: " << Storage.files.at(B).size() << " bytes\n";
}
int main() {
  auto store = std::make_unique<TodoStore>();
  assert(store->load() == S::Ok && !store->model().size());
  assert(store->change(C::Add, 0, "") == S::InvalidText && Storage.files.empty());
  assert(store->change(C::Add, 0, "Café 日本語 😀 & \"quote\"") == S::Ok);
  const auto first = Storage.files[A];
  auto restarted = std::make_unique<TodoStore>();
  assert(restarted->load() == S::Ok && restarted->model().find(1));
  assert(restarted->change(C::Edit, 1, "edited") == S::Ok);
  assert(restarted->change(C::Toggle, 1) == S::Ok);
  const auto writes = Storage.writes;
  assert(restarted->change(C::Edit, 1, "edited") == S::Ok && Storage.writes == writes);
  auto savedFiles = Storage.files;
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = savedFiles;
    auto retry = std::make_unique<TodoStore>();
    assert(retry->load() == S::Ok);
    Storage.writeBudget = failure == 0 ? 12 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRename = failure == 3;
    Storage.failRemove = failure == 4;
    Storage.failDirectory = failure == 5;
    assert(retry->change(C::Edit, 1, "lost") == S::StorageError);
    assert(std::string(retry->model().find(1)->text) == "edited");
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRename = Storage.failRemove = Storage.failDirectory = false;
    auto recovered = std::make_unique<TodoStore>();
    assert(recovered->load() == S::Ok);
    assert(std::string(recovered->model().find(1)->text) == "edited" && recovered->model().find(1)->completed);
  }
  Storage.files = savedFiles;
  auto deleted = std::make_unique<TodoStore>();
  assert(deleted->load() == S::Ok && deleted->change(C::Delete, 1) == S::Ok);
  auto afterDelete = std::make_unique<TodoStore>();
  assert(afterDelete->load() == S::Ok && !afterDelete->model().size());
  assert(afterDelete->change(C::Add, 0, "new") == S::Ok && afterDelete->model().find(2));
  Storage.files.clear();
  Storage.files[A] = "";
  auto empty = std::make_unique<TodoStore>();
  assert(empty->load() == S::Ok);
  Storage.files[A] = "{broken";
  auto corrupt = std::make_unique<TodoStore>();
  assert(corrupt->load() == S::InvalidData && corrupt->change(C::Add, 0, "test") == S::InvalidData);
  assert(Storage.files[A] == "{broken");
  Storage.files[B] = first;
  assert(corrupt->load() == S::Ok && corrupt->model().size() == 1);
  Storage.files[A] = first;
  Storage.files[B] = first;
  Storage.files[B].replace(Storage.files[B].find("Café"), 4, "bad!");
  assert(corrupt->load() == S::Ok && std::string(corrupt->model().find(1)->text).find("Café") == 0);
  Storage.files[A] = "{\"version\":99}";
  Storage.files.erase(B);
  assert(corrupt->load() == S::InvalidData);
  Storage.files[A] = std::string(32769, ' ');
  assert(corrupt->load() == S::InvalidData);
  Storage.files.clear();
  auto full = std::make_unique<TodoStore>();
  assert(full->load() == S::Ok);
  for (size_t i = 0; i < TodoModel::MAX_TASKS; ++i)
    assert(full->change(C::Add, 0, std::string(TodoModel::MAX_TEXT_BYTES, 'a')) == S::Ok);
  assert(full->change(C::Add, 0, "overflow") == S::Full);
  auto fullRestart = std::make_unique<TodoStore>();
  assert(fullRestart->load() == S::Ok && fullRestart->model().size() == 32);
  Storage.files.clear();
  Storage.files[A] = first + "garbage";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files[A] = "{\"version\":1,\"tasks\":[]}";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files[A] = "[" + std::string(32700, '0') + "]";
  assert(fullRestart->load() == S::InvalidData);
  Storage.files.clear();
  auto batch = std::make_unique<TodoStore>();
  assert(batch->load() == S::Ok);
  for (const char* text : {"first", "second", "third"}) assert(batch->change(C::Add, 0, text) == S::Ok);
  uint32_t ids[] = {1, 3};
  const auto batchSnapshot = Storage.files;
  for (int failure = 0; failure < 6; ++failure) {
    Storage.files = batchSnapshot;
    assert(batch->load() == S::Ok);
    Storage.writeBudget = failure == 0 ? 12 : -1;
    Storage.failOpen = failure == 1;
    Storage.failClose = failure == 2;
    Storage.failRename = failure == 3;
    Storage.failRemove = failure == 4;
    Storage.failDirectory = failure == 5;
    assert(batch->eraseTasks(ids, 2) == S::StorageError && batch->model().size() == 3);
    Storage.writeBudget = -1;
    Storage.failOpen = Storage.failClose = Storage.failRename = Storage.failRemove = Storage.failDirectory = false;
    auto recovered = std::make_unique<TodoStore>();
    assert(recovered->load() == S::Ok && recovered->model().size() == 3);
  }
  Storage.files = batchSnapshot;
  assert(batch->load() == S::Ok);
  const auto beforeDelete = Storage.writes;
  assert(batch->eraseTasks(nullptr, 0) == S::Ok && Storage.writes == beforeDelete);
  assert(batch->eraseTasks(nullptr, 2) == S::InvalidData);
  uint32_t bad[] = {1, 99};
  assert(batch->eraseTasks(bad, 2) == S::InvalidData && batch->model().size() == 3 && Storage.writes == beforeDelete);
  uint32_t duplicate[] = {1, 1};
  assert(batch->eraseTasks(duplicate, 2) == S::InvalidData && batch->model().size() == 3);
  assert(batch->eraseTasks(ids, 2) == S::Ok && Storage.writes == beforeDelete + 1);
  auto afterBatch = std::make_unique<TodoStore>();
  assert(afterBatch->load() == S::Ok && afterBatch->model().size() == 1 && afterBatch->model().find(2));
  assert(afterBatch->change(C::Add, 0, "fourth") == S::Ok && afterBatch->model().find(4));
  metadataChecks();
  std::cout << "Todo persistence checks passed, including failure recovery and full-capacity restart\n";
}
