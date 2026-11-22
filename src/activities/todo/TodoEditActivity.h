#pragma once

#include <TodoStore.h>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

class TodoEditActivity final : public Activity, private UiAppHost {
 public:
  TodoEditActivity(GfxRenderer& renderer, MappedInputManager& input, TodoStore& store, uint32_t id)
      : Activity("TodoEdit", renderer, input), UiAppHost(renderer), store(store), id(id) {}
  void onEnter() override;
  void onCovered() override { closeRouting(); }
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Action : freeink::ui::ActionId { Back = 1, Control };
  enum class Command : int16_t { Text, Save, MinusDay, PlusDay, MinusWeek, PlusWeek, Today, Tomorrow, NoDate };
  TodoStore& store;  // The retained Todo parent owns persistence and outlives this screen.
  const uint32_t id;
  TodoModel::Task draft{};
  TodoStore::Status status = TodoStore::Status::Ok;
  bool available = false;
  bool waitForNavigationRelease = false;
  ButtonNavigator buttons;
  // Reuse large UI property blocks instead of placing them on the render stack.
  freeink::ui::HeaderProps headerProps{};
  freeink::ui::ButtonProps buttonProps{};
  char dateText[96]{};
  char detailText[128]{};
  static void screenFn(UiScreen& screen, void* context);
  static void action(const freeink::ui::ActionEvent& event, void* context);
  void buildScreen(UiScreen& screen);
  void command(Command command);
  void enterText();
  void save();
  void confirmDelete();
  void discard();
  void step(int delta);
  void armNavigationGuard();
  void button(UiScreen& screen, freeink::ui::Rect rect, const char* label, Command command, bool enabled = true);
};
