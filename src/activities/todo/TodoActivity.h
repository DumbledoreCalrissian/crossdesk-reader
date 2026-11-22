#pragma once
#include <TodoOrder.h>
#include <TodoStore.h>

#include "activities/UiListActivity.h"

class TodoActivity final : public UiListActivity {
 public:
  TodoActivity(GfxRenderer& renderer, MappedInputManager& input) : UiListActivity("Todo", renderer, input) {}
  void onEnter() override;
  void onCovered() override { closeRouting(); }

 private:
  static constexpr auto ACTION_BACK = ACTION_USER;
  static constexpr auto ACTION_CHECK = ACTION_USER + 1;
  static constexpr auto ACTION_HEADER = ACTION_USER + 2;
  // The host holds six handlers, so the delete-view buttons share one action.
  static constexpr auto ACTION_DELETE_BAR = ACTION_USER + 3;
  enum DeleteBar : int16_t { DELETE_SELECTED, SELECT_DONE, SELECT_ALL, NEW_TASK };
  enum class HeaderAction : int16_t { Delete, Sort };
  static constexpr int16_t PRIORITY_BASE = TodoModel::MAX_TASKS + 1;
  TodoStore store;
  TodoOrder order;
  freeink::ui::ListRowControls rowControls{};
  freeink::ui::BitmapRef pencil{};
  char dateLabel[128]{};
  int32_t today = 0;
  TodoStore::Status status = TodoStore::Status::Ok;
  enum class View { Tasks, DeleteSelection, Sort };
  View view = View::Tasks;
  uint32_t selection = 0;
  char deleteLabel[64]{};
  char titleLabel[32]{};
  const char* statusText() const;
  unsigned selectedCount() const;
  uint32_t allMask() const;
  int modelIndex(int row) const;
  int rowOf(size_t index) const;
  void selectDone();
  void selectAll();
  void confirmDeleteTask(uint32_t id);
  int firstTaskRow() const { return view == View::Tasks && !mappedInput.hasTouch() ? 1 : 0; }
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void buildHeader(UiScreen& screen);
  void activateIndex(int index) override;
  void onBackButton() override;
  void enterText(uint32_t id);
  void editTask(uint32_t id);
  void acceptText(uint32_t id, const std::string& text);
  void confirmDelete();
  void setView(View next);
  void toggleTask(int index);
  void cyclePriority(int index);
  static const freeink::ui::ListRowControls* provideControls(void* context, uint16_t index);
  void drawChrome() override {}
  void drawFooter() override {}
  static void provideRow(void* context, uint16_t index, freeink::ui::ListItem& item);
  static void action(const freeink::ui::ActionEvent& event, void* context);
};
