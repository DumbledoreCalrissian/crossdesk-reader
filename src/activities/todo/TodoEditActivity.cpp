#include "TodoEditActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "activities/util/ConfirmationActivity.h"
#include "activities/util/TextEntry.h"
#include "components/TodoPresentation.h"
#include "components/UITheme.h"
#include "components/UiScreenHeader.h"
#include "components/icons/todoIcons.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
const fui::HeaderProps EMPTY_HEADER{};
const fui::ButtonProps EMPTY_BUTTON{};
}  // namespace

void TodoEditActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    const auto* task = store.model().find(id);
    available = store.canEdit() && (!id || task);
    if (task) draft = *task;
    if (!available) status = TodoStore::Status::InvalidData;
    armNavigationGuard();
    resetUi();
    app.on(Back, action, this);
    app.on(Control, action, this);
    app.setScreen(screenFn, this);
    if (app.handlerOverflowed()) LOG_ERR("TODO", "Edit handler capacity exceeded");
  }
  if (!id && available)
    enterText();
  else
    requestUpdate();
}

void TodoEditActivity::screenFn(UiScreen& screen, void* context) {
  static_cast<TodoEditActivity*>(context)->buildScreen(screen);
}

void TodoEditActivity::action(const fui::ActionEvent& event, void* context) {
  auto& self = *static_cast<TodoEditActivity*>(context);
  self.app.clearTapFlash();
  switch (event.action) {
    case Back:
      self.discard();
      break;
    case Control:
      self.command(static_cast<Command>(event.value));
      break;
  }
}

void TodoEditActivity::discard() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void TodoEditActivity::enterText() {
  auto keyboard = makeTextEntry(renderer, mappedInput, id ? tr(STR_TODO_EDIT) : tr(STR_TODO_NEW), draft.text,
                                TodoModel::MAX_TEXT_BYTES);
  if (!keyboard) {
    LOG_ERR("TODO", "Text entry allocation failed");
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    armNavigationGuard();
    if (result.isCancelled) {
      if (!id) discard();
      return;
    }
    const auto* entry = std::get_if<KeyboardResult>(&result.data);
    if (!entry) return;
    RenderLock lock(*this);
    if (!TodoModel::blank(entry->text) && !TodoModel::validText(entry->text)) {
      status = TodoStore::Status::InvalidText;
      return;
    }
    const size_t length = TodoModel::blank(entry->text) ? 0 : entry->text.size();
    std::memcpy(draft.text, entry->text.data(), length);
    draft.text[length] = '\0';
    status = !id && !length ? TodoStore::Status::InvalidText : TodoStore::Status::Ok;
  });
}

void TodoEditActivity::save() {
  if (id && TodoModel::blank(draft.text)) {
    confirmDelete();
    return;
  }
  bool saved;
  {
    RenderLock lock(*this);
    status = store.saveTask(id, draft.text, draft.due, id ? 0 : todoToday());
    saved = status == TodoStore::Status::Ok;
  }
  if (saved)
    finish();
  else
    requestUpdate();
}

void TodoEditActivity::confirmDelete() {
  const auto* task = store.model().find(id);
  if (!task) return;
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TODO_DELETE_ONE_CONFIRM),
                                                              task->text, tr(STR_CANCEL), tr(STR_TODO_DELETE));
  if (!confirmation) {
    LOG_ERR("TODO", "Confirmation allocation failed: %u bytes", static_cast<unsigned>(sizeof(ConfirmationActivity)));
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
    armNavigationGuard();
    if (result.isCancelled) return;
    bool saved;
    {
      RenderLock lock(*this);
      status = store.eraseTasks(&id, 1);
      saved = status == TodoStore::Status::Ok;
    }
    if (saved) finish();
  });
}

void TodoEditActivity::step(int delta) {
  {
    RenderLock lock(*this);
    draft.due = TodoModel::shiftDue(draft.due, todoToday(), delta);
  }
  requestUpdate();
}

void TodoEditActivity::armNavigationGuard() {
  waitForNavigationRelease = mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                             mappedInput.isPressed(MappedInputManager::Button::NavPrevious);
}

void TodoEditActivity::command(Command command) {
  if (!available) return;
  switch (command) {
    case Command::Text:
      enterText();
      return;
    case Command::Save:
      save();
      return;
    case Command::MinusDay:
      step(-1);
      return;
    case Command::PlusDay:
      step(1);
      return;
    case Command::MinusWeek:
      step(-7);
      return;
    case Command::PlusWeek:
      step(7);
      return;
    case Command::Today:
    case Command::Tomorrow: {
      RenderLock lock(*this);
      const int32_t today = todoToday();
      if (today) draft.due = command == Command::Today ? today : TodoModel::shiftDue(0, today, 1);
      break;
    }
    case Command::NoDate: {
      RenderLock lock(*this);
      draft.due = 0;
      break;
    }
  }
  requestUpdate();
}

void TodoEditActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    discard();
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event) return;
  if (touch.routed && app.invalidated()) requestUpdate();
  if (!available) return;
  if (waitForNavigationRelease) {
    armNavigationGuard();
    return;
  }
  buttons.onNext([this] { step(1); });
  buttons.onPrevious([this] { step(-1); });
}

void TodoEditActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void TodoEditActivity::button(UiScreen& screen, fui::Rect rect, const char* label, Command command, bool enabled) {
  buttonProps = EMPTY_BUTTON;
  auto& props = buttonProps;
  props.label = label;
  props.action = Control;
  props.value = static_cast<int16_t>(command);
  props.enabled = enabled && available;
  props.styles = screen.theme().button;
  props.styles.normal.border = fui::Paint::solid(fui::Color::Black);
  props.styles.normal.borderWidth = 2;
  props.radius = 10;
  screen.button(props, rect);
}

void TodoEditActivity::buildScreen(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto& metrics = UITheme::getInstance().getMetrics();
  headerProps = EMPTY_HEADER;
  auto& header = headerProps;
  const auto band = prepareUiHeader(screen, renderer, header, tr(STR_TODO_EDIT), Back);
  fui::header(screen.frame(), band, header);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight + 12), 24, 12, 24});
  const auto saveBand = screen.takeBottom(80, 12);
  buttonProps = EMPTY_BUTTON;
  auto& saveButton = buttonProps;
  saveButton.label = tr(STR_TODO_SAVE);
  saveButton.action = Control;
  saveButton.value = static_cast<int16_t>(Command::Save);
  saveButton.enabled = available;
  saveButton.styles = theme.button;
  saveButton.styles.normal.background = fui::Paint::solid(fui::Color::Black);
  saveButton.styles.normal.foreground = fui::Paint::solid(fui::Color::White);
  saveButton.radius = 10;
  screen.button(saveButton, saveBand);

  auto textStyle = theme.smallText;
  const int16_t smallH = uiTarget.lineHeight(textStyle.font);
  uiTarget.text(screen.takeTop(smallH, 6), tr(STR_TODO_TASK), textStyle);
  const auto taskBand = screen.takeTop(96, 18);
  buttonProps = EMPTY_BUTTON;
  auto& taskButton = buttonProps;
  taskButton.action = Control;
  taskButton.value = static_cast<int16_t>(Command::Text);
  taskButton.enabled = available;
  taskButton.styles = theme.button;
  taskButton.styles.normal.border = fui::Paint::solid(fui::Color::Black);
  taskButton.styles.normal.borderWidth = 2;
  taskButton.styles.active = taskButton.styles.normal;
  taskButton.styles.selected = taskButton.styles.normal;
  taskButton.styles.focused = taskButton.styles.normal;
  taskButton.radius = 10;
  screen.button(taskButton, taskBand);
  auto bodyStyle = theme.bodyText;
  bodyStyle.maxLines = 2;
  bodyStyle.align = fui::TextAlign::Left;
  const auto label = taskBand.inset(fui::Insets{8, 44, 8, 12});
  uiTarget.text(label, draft.text, bodyStyle);
  uiTarget.bitmap(fui::Rect{static_cast<int16_t>(taskBand.right() - 36), static_cast<int16_t>(taskBand.y + 24), 24, 24},
                  fui::bitmapFromIcon(todo_pencil_24), fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));

  uiTarget.text(screen.takeTop(smallH, 8), tr(STR_TODO_SORT_DUE), textStyle);
  formatTodoFullDate(draft.due, dateText, sizeof(dateText));
  const auto dateBand = screen.takeTop(48, 4);
  const auto& fonts = renderer.getFontMap();
  const bool hasLargeFont = fonts.find(NOTOSANS_16_FONT_ID) != fonts.end();
  const int dateWidth = hasLargeFont ? renderer.getTextWidth(NOTOSANS_16_FONT_ID, dateText, EpdFontFamily::BOLD) : 0;
  if (hasLargeFont && dateWidth <= dateBand.width) {
    renderer.drawText(NOTOSANS_16_FONT_ID, dateBand.x + (dateBand.width - dateWidth) / 2,
                      dateBand.y + (dateBand.height - renderer.getLineHeight(NOTOSANS_16_FONT_ID)) / 2, dateText, true,
                      EpdFontFamily::BOLD);
  } else {
    auto fallback = theme.titleText;
    fallback.bold = true;
    fallback.align = fui::TextAlign::Center;
    uiTarget.text(dateBand, dateText, fallback);
  }
  const int32_t today = todoToday();
  if (const auto* error = todoStatusText(status)) {
    std::snprintf(detailText, sizeof(detailText), "%s", error);
  } else if (!today) {
    std::snprintf(detailText, sizeof(detailText), "%s",
                  draft.due ? tr(STR_TODO_CLOCK_PRESETS) : tr(STR_TODO_CLOCK_START));
  } else {
    formatTodoRelative(draft.due, today, detailText, sizeof(detailText));
  }
  textStyle.align = fui::TextAlign::Center;
  textStyle.maxLines = 2;
  uiTarget.text(screen.takeTop(static_cast<int16_t>(smallH * 2), 8), detailText, textStyle);

  for (int row = 0; row < 2; ++row) {
    const auto controls = screen.takeTop(64, 10);
    const int16_t width = static_cast<int16_t>((controls.width - 12) / 2);
    const int delta = row ? 7 : 1;
    button(screen, fui::Rect{controls.x, controls.y, width, controls.height},
           row ? tr(STR_TODO_MINUS_WEEK) : tr(STR_TODO_MINUS_DAY), row ? Command::MinusWeek : Command::MinusDay,
           TodoModel::shiftDue(draft.due, today, -delta) != draft.due);
    button(screen, fui::Rect{static_cast<int16_t>(controls.right() - width), controls.y, width, controls.height},
           row ? tr(STR_TODO_PLUS_WEEK) : tr(STR_TODO_PLUS_DAY), row ? Command::PlusWeek : Command::PlusDay,
           TodoModel::shiftDue(draft.due, today, delta) != draft.due);
  }
  const auto presets = screen.takeTop(64, 10);
  const int16_t width = static_cast<int16_t>((presets.width - 24) / 3);
  button(screen, fui::Rect{presets.x, presets.y, width, presets.height}, tr(STR_TODO_TODAY), Command::Today, today > 0);
  button(screen, fui::Rect{static_cast<int16_t>(presets.x + width + 12), presets.y, width, presets.height},
         tr(STR_TODO_TOMORROW), Command::Tomorrow, today > 0 && today < TodoModel::MAX_DAY);
  button(screen, fui::Rect{static_cast<int16_t>(presets.right() - width), presets.y, width, presets.height},
         tr(STR_TODO_NO_DATE), Command::NoDate);
  textStyle.maxLines = 1;
  uiTarget.text(screen.takeTop(smallH, 4), tr(STR_TODO_DAY_KEYS), textStyle);
  if (draft.created) {
    formatTodoFullDate(draft.created, dateText, sizeof(dateText), false);
    std::snprintf(detailText, sizeof(detailText), tr(STR_TODO_CREATED), dateText);
    uiTarget.text(screen.takeTop(smallH), detailText, textStyle);
  }
}
