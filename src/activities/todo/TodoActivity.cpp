#include "TodoActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>

#include "activities/util/ConfirmationActivity.h"
#include "activities/util/TextEntry.h"
#include "components/TodoPresentation.h"
#include "components/UITheme.h"
#include "components/UiScreenHeader.h"
#include "components/icons/headerIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/todoIcons.h"

namespace fui = freeink::ui;

namespace {
struct SortOption {
  TodoModel::Sort sort;
  StrId label;
  StrId hint;
};
constexpr SortOption SORTS[] = {
    {TodoModel::Sort::Priority, StrId::STR_TODO_SORT_PRIORITY, StrId::STR_TODO_SORT_PRIORITY_HINT},
    {TodoModel::Sort::DueDate, StrId::STR_TODO_SORT_DUE, StrId::STR_TODO_SORT_DUE_HINT},
    {TodoModel::Sort::Created, StrId::STR_TODO_SORT_CREATED, StrId::STR_TODO_SORT_CREATED_HINT},
    {TodoModel::Sort::AddedOrder, StrId::STR_TODO_SORT_ADDED, StrId::STR_TODO_SORT_ADDED_HINT}};
// Keeps the icon slot on unselected delete rows so their text does not shift.
constexpr auto BLANK_BITS = [] {
  std::array<uint8_t, 24 * 24 / 8> bits{};
  bits.fill(0xFF);
  return bits;
}();
const freeink::Icon BLANK_ICON = {24, 24, 0, BLANK_BITS.data()};
}  // namespace

void TodoActivity::onEnter() {
  UiListActivity::onEnter();
  {
    RenderLock lock(*this);
    status = store.load();
    order.reset(store.model());
    pencil = fui::bitmapFromIcon(todo_pencil_24);
  }
  app.on(ACTION_BACK, action, this);
  app.on(ACTION_CHECK, action, this);
  app.on(ACTION_HEADER, action, this);
  app.on(ACTION_DELETE_BAR, action, this);
  if (app.handlerOverflowed()) LOG_ERR("TODO", "Action handler capacity exceeded");
}

int TodoActivity::listCount() const {
  return view == View::Sort ? 4 : static_cast<int>(store.model().size()) + firstTaskRow();
}

unsigned TodoActivity::selectedCount() const { return std::popcount(selection); }

uint32_t TodoActivity::allMask() const {
  const auto size = store.model().size();
  return size >= 32 ? UINT32_MAX : (uint32_t{1} << size) - 1;
}

int TodoActivity::modelIndex(int row) const { return order.modelIndex(store.model(), row); }

int TodoActivity::rowOf(size_t index) const { return order.rowOf(store.model(), index); }

void TodoActivity::selectDone() {
  {
    RenderLock lock(*this);
    selection = 0;
    for (size_t i = 0; i < store.model().size(); ++i)
      if (store.model().at(i)->completed) selection |= uint32_t{1} << i;
  }
  requestUpdate();
}

void TodoActivity::selectAll() {
  {
    RenderLock lock(*this);
    selection = selection == allMask() ? 0 : allMask();
  }
  requestUpdate();
}

void TodoActivity::action(const fui::ActionEvent& event, void* context) {
  auto* self = static_cast<TodoActivity*>(context);
  self->app.clearTapFlash();
  switch (event.action) {
    case ACTION_BACK:
      self->onBackButton();
      break;
    case ACTION_CHECK:
      if (event.value >= PRIORITY_BASE)
        self->cyclePriority(event.value - PRIORITY_BASE);
      else
        self->toggleTask(event.value);
      break;
    case ACTION_HEADER:
      switch (static_cast<HeaderAction>(event.value)) {
        case HeaderAction::Delete:
          if (self->store.model().size()) self->setView(View::DeleteSelection);
          break;
        case HeaderAction::Sort:
          self->setView(View::Sort);
          break;
      }
      break;
    case ACTION_DELETE_BAR:
      switch (event.value) {
        case DELETE_SELECTED:
          self->confirmDelete();
          break;
        case SELECT_DONE:
          self->selectDone();
          break;
        case SELECT_ALL:
          self->selectAll();
          break;
        case NEW_TASK:
          self->enterText(0);
          break;
      }
      break;
  }
}

void TodoActivity::setView(View next) {
  {
    RenderLock lock(*this);
    closeRouting();
    app.clearTapFlash();
    view = next;
    selection = 0;
    nav.reset();
  }
  requestUpdate();
}

void TodoActivity::onBackButton() {
  switch (view) {
    case View::Tasks:
      activityManager.goHome(HomeMenuItem::TODO);
      break;
    case View::DeleteSelection:
    case View::Sort:
      setView(View::Tasks);
      break;
  }
}

const fui::ListRowControls* TodoActivity::provideControls(void* context, uint16_t) {
  return &static_cast<TodoActivity*>(context)->rowControls;
}

void TodoActivity::provideRow(void* context, uint16_t index, fui::ListItem& item) {
  auto& self = *static_cast<TodoActivity*>(context);
  self.rowControls = {};
  item.actionValue = static_cast<int16_t>(index);
  item.enabled = self.store.canEdit();
  if (self.view == View::Sort) {
    if (index >= std::size(SORTS)) return;
    const auto& option = SORTS[index];
    item.label = I18N.get(option.label);
    item.subtitle = I18N.get(option.hint);
    item.bold = true;
    item.icon = fui::bitmapFromIcon(self.store.model().sort() == option.sort ? todo_radio_on : todo_radio_off);
    return;
  }
  if (self.firstTaskRow() && index == 0) {
    item.label = tr(STR_TODO_NEW);
    return;
  }
  const int taskIndex = self.modelIndex(index - self.firstTaskRow());
  if (taskIndex < 0) return;
  const auto* task = self.store.model().at(taskIndex);
  if (!task) return;
  item.label = task->text;
  item.strikethrough = task->completed;
  formatTodoDates(*task, self.today, self.dateLabel, sizeof(self.dateLabel));
  item.subtitle = self.dateLabel[0] ? self.dateLabel : nullptr;
  self.rowControls.subtitleBold = todoOverdue(*task, self.today);
  if (self.view == View::Tasks) {
    item.checkbox = true;
    item.checked = task->completed;
    self.rowControls.checkboxSize = 40;
    self.rowControls.labelIcon = &self.pencil;
    if (!task->completed) {
      item.value = todoPriority(task->priority);
      self.rowControls.trailingAction = ACTION_CHECK;
      self.rowControls.trailingValue = static_cast<int16_t>(index + PRIORITY_BASE);
    }
  } else {
    const bool selected = self.selection & (uint32_t{1} << taskIndex);
    item.icon = fui::bitmapFromIcon(selected ? icon_trash_2_24 : BLANK_ICON);
  }
}

void TodoActivity::buildHeader(UiScreen& screen) {
  fui::HeaderProps header;
  const char* title = view == View::Sort ? tr(STR_TODO_SORT_TITLE) : tr(STR_TODO_DELETE_TITLE);
  if (view == View::Tasks) {
    std::snprintf(titleLabel, sizeof(titleLabel), tr(STR_TODO_COUNT), static_cast<unsigned>(store.model().size()),
                  static_cast<unsigned>(TodoModel::MAX_TASKS));
    title = titleLabel;
  }
  const auto band = prepareUiHeader(screen, renderer, header, title, ACTION_BACK);
  if (view == View::Tasks) {
    header.trailingIcon = fui::bitmapFromIcon(icon_trash_2_32);
    header.trailingAction = ACTION_HEADER;
    header.trailingEnabled = store.canEdit();
    header.trailingValue = static_cast<int16_t>(HeaderAction::Delete);
    header.trailingAdjacentIcon = fui::bitmapFromIcon(todo_sort_32);
    header.trailingAdjacentAction = ACTION_HEADER;
    header.trailingAdjacentValue = static_cast<int16_t>(HeaderAction::Sort);
  }
  fui::header(screen.frame(), band, header);
}

void TodoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  today = todoToday();
  buildHeader(screen);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0, 0, 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (view == View::DeleteSelection) {
    std::snprintf(deleteLabel, sizeof(deleteLabel), tr(STR_TODO_DELETE_SELECTED), selectedCount());
    fui::ButtonProps button;
    button.label = deleteLabel;
    button.action = ACTION_DELETE_BAR;
    button.value = DELETE_SELECTED;
    button.enabled = selection != 0 && store.canEdit();
    const int16_t inset = static_cast<int16_t>(theme.spaceLg * 2);
    const auto rect = screen.takeBottom(theme.rowHeight, theme.spaceLg).inset(fui::Insets{0, inset, 0, inset});
    screen.button(button, rect);

    const auto row = screen.takeBottom(theme.rowHeight, theme.spaceLg).inset(fui::Insets{0, inset, 0, inset});
    const int16_t half = static_cast<int16_t>((row.width - theme.spaceLg) / 2);
    fui::ButtonProps done;
    done.label = tr(STR_TODO_SELECT_DONE);
    done.action = ACTION_DELETE_BAR;
    done.value = SELECT_DONE;
    done.enabled = false;
    for (size_t i = 0; i < store.model().size(); ++i) done.enabled |= store.model().at(i)->completed;
    done.enabled &= store.canEdit();
    screen.button(done, fui::Rect{row.x, row.y, half, row.height});
    fui::ButtonProps all;
    all.label = selection && selection == allMask() ? tr(STR_TODO_SELECT_NONE) : tr(STR_TODO_SELECT_ALL);
    all.action = ACTION_DELETE_BAR;
    all.value = SELECT_ALL;
    all.enabled = store.canEdit() && store.model().size();
    screen.button(all, fui::Rect{static_cast<int16_t>(row.x + row.width - half), row.y, half, row.height});
  }
  if (const char* error = statusText()) {
    fui::TextAreaProps text;
    text.text = error;
    text.showCaret = false;
    text.style = screen.theme().smallText;
    text.style.maxLines = 3;
    const auto inset = static_cast<int16_t>(theme.listInset + theme.listSidePadding);
    const auto band = screen.takeTop(static_cast<int16_t>(uiTarget.lineHeight(text.style.font) * 3));
    fui::textArea(screen.frame(), band.inset(fui::Insets{0, inset, 0, inset}), text);
  }

  if (view == View::Tasks && !firstTaskRow()) {
    fui::ButtonProps add;
    add.label = tr(STR_TODO_NEW);
    add.icon = fui::bitmapFromIcon(todo_plus_24);
    add.action = ACTION_DELETE_BAR;
    add.value = NEW_TASK;
    add.enabled = store.canEdit();
    add.styles = theme.button;
    add.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    add.styles.normal.borderWidth = 2;
    add.radius = 10;
    const auto band = screen.takeTop(72, 18).inset(fui::Insets{0, 24, 0, 24});
    screen.button(add, band);
  }
  fui::ListProps props;
  props.count = static_cast<uint16_t>(listCount());
  props.rowProvider = provideRow;
  props.rowProviderCtx = this;
  props.action = ACTION_ROW;
  props.checkboxAction = view == View::Tasks ? ACTION_CHECK : fui::NO_ACTION;
  props.checkboxWidth = 80;
  props.rowHeight = 88;
  props.rowPaddingY = 0;
  props.rowGap = 10;
  props.rowInset = 16;
  props.sidePadding = 0;
  props.separatorPaint = fui::Paint::solid(fui::Color::Black);
  props.balanceWrappedLabelWithValue = false;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  props.valueText = screen.theme().bodyText;
  props.valueText.bold = true;
  props = screen.resolveListProps(props);
  if (view == View::Sort) {
    props.rowStyles = theme.button;
    props.rowStyles.normal.border = fui::Paint::solid(fui::Color::Black);
    props.rowStyles.normal.borderWidth = 2;
    props.rowRadius = 10;
    props.rowInset = 24;
    props.sidePadding = 16;
    syncListViewport(screen, props);
    props.partialTrailingRow = false;
    screen.list(props, std::min<int16_t>(screen.body().height, 382));
    fui::TextAreaProps help;
    help.text = tr(STR_TODO_SORT_HELP);
    help.style = theme.smallText;
    help.style.maxLines = 3;
    help.style.align = fui::TextAlign::Center;
    help.showCaret = false;
    auto body = screen.body().inset(fui::Insets{24, 40, 0, 40});
    body.height = std::min<int16_t>(body.height, uiTarget.lineHeight(help.style.font) * 3);
    fui::textArea(screen.frame(), body, help);
    return;
  }
  const int16_t maxHeight = static_cast<int16_t>(props.rowHeight * 6 + props.rowGap * 5);
  if (screen.body().height > maxHeight) screen.takeBottom(screen.body().height - maxHeight);
  syncListViewport(screen, props);
  props.partialTrailingRow = false;
  // Row controls use the base list's navigation but draw through the SDK's
  // opt-in path. Provider storage lives until the next row-provider call.
  const fui::ListRowControlsProvider controls{provideControls, this};
  if (!store.model().size()) {
    if (firstTaskRow()) screen.list(props, props.rowHeight);
    if (status == TodoStore::Status::Ok) screen.centeredText(tr(STR_TODO_EMPTY));
  } else {
    screen.list<true>(props, 0, fui::LayoutAnchor::Top, controls);
  }
}

void TodoActivity::cyclePriority(int index) {
  {
    RenderLock lock(*this);
    if (view != View::Tasks) return;
    const int taskIndex = modelIndex(index - firstTaskRow());
    if (taskIndex < 0) return;
    const auto& task = *store.model().at(taskIndex);
    if (task.completed) return;
    status = store.setPriority(task.id, (task.priority + 1) % 4);
    nav.selected = index;
  }
  requestUpdate();
}

void TodoActivity::toggleTask(int index) {
  {
    RenderLock lock(*this);
    if (view != View::Tasks) return;
    const int taskIndex = modelIndex(index - firstTaskRow());
    if (taskIndex < 0) return;
    status = store.change(TodoStore::Change::Toggle, store.model().at(taskIndex)->id);
    nav.selected = rowOf(taskIndex) + firstTaskRow();
  }
  requestUpdate();
}

void TodoActivity::activateIndex(int index) {
  app.clearTapFlash();
  switch (view) {
    case View::Sort: {
      if (index < 0 || index >= static_cast<int>(std::size(SORTS))) return;
      {
        RenderLock lock(*this);
        status = store.setSort(SORTS[index].sort);
        if (status == TodoStore::Status::Ok) order.reset(store.model());
      }
      if (status == TodoStore::Status::Ok)
        setView(View::Tasks);
      else
        requestUpdate();
      return;
    }
    case View::Tasks: {
      if (firstTaskRow() && index == 0) {
        enterText(0);
        return;
      }
      const int taskIndex = modelIndex(index - firstTaskRow());
      if (taskIndex >= 0) enterText(store.model().at(taskIndex)->id);
      return;
    }
    case View::DeleteSelection: {
      const int taskIndex = modelIndex(index - firstTaskRow());
      if (taskIndex < 0) return;
      {
        RenderLock lock(*this);
        selection ^= uint32_t{1} << taskIndex;
      }
      requestUpdate();
      return;
    }
  }
}

void TodoActivity::enterText(uint32_t id) {
  if (!id && store.model().size() == TodoModel::MAX_TASKS) {
    {
      RenderLock lock(*this);
      status = TodoStore::Status::Full;
    }
    requestUpdate();
    return;
  }
  const auto* task = store.model().find(id);
  auto keyboard = makeTextEntry(renderer, mappedInput, id ? tr(STR_TODO_EDIT) : tr(STR_TODO_NEW),
                                task ? task->text : "", TodoModel::MAX_TEXT_BYTES);
  if (!keyboard) {
    LOG_ERR("TODO", "Keyboard allocation failed: %u bytes", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    {
      RenderLock lock(*this);
      status = TodoStore::Status::OutOfMemory;
    }
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this, id](const ActivityResult& result) {
    if (result.isCancelled) return;
    if (const auto* entry = std::get_if<KeyboardResult>(&result.data)) acceptText(id, entry->text);
  });
}

void TodoActivity::acceptText(uint32_t id, const std::string& text) {
  // Saving an existing task with no text offers to delete it.
  if (id && TodoModel::blank(text)) {
    confirmDeleteTask(id);
    return;
  }
  RenderLock lock(*this);
  status = store.change(id ? TodoStore::Change::Edit : TodoStore::Change::Add, id, text);
  if (status == TodoStore::Status::Ok && !id) {
    closeRouting();
    nav.requestSelection(rowOf(store.model().size() - 1) + firstTaskRow());
  }
}

void TodoActivity::confirmDeleteTask(uint32_t id) {
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
  startActivityForResult(std::move(confirmation), [this, id](const ActivityResult& result) {
    if (result.isCancelled) return;
    RenderLock lock(*this);
    status = store.eraseTasks(&id, 1);
  });
}

void TodoActivity::confirmDelete() {
  if (view != View::DeleteSelection || !selection) return;
  char body[64];
  std::snprintf(body, sizeof(body), tr(STR_TODO_DELETE_SELECTED), selectedCount());
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TODO_DELETE_CONFIRM), body,
                                                              tr(STR_CANCEL), tr(STR_TODO_DELETE));
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
    if (result.isCancelled) return;
    bool deleted;
    {
      RenderLock lock(*this);
      uint32_t ids[TodoModel::MAX_TASKS];
      size_t count = 0;
      for (size_t i = 0; i < store.model().size(); ++i)
        if (selection & (uint32_t{1} << i)) ids[count++] = store.model().at(i)->id;
      status = store.eraseTasks(ids, count);
      deleted = status == TodoStore::Status::Ok;
    }
    if (deleted) setView(View::Tasks);
  });
}

const char* TodoActivity::statusText() const {
  switch (status) {
    case TodoStore::Status::Ok:
      return nullptr;
    case TodoStore::Status::InvalidText:
      return tr(STR_TODO_INVALID_TEXT);
    case TodoStore::Status::Full:
      return tr(STR_TODO_FULL);
    case TodoStore::Status::StorageError:
      return tr(STR_TODO_SAVE_ERROR);
    case TodoStore::Status::InvalidData:
      return tr(STR_TODO_DATA_ERROR);
    case TodoStore::Status::OutOfMemory:
      return tr(STR_TODO_MEMORY_ERROR);
  }
  return nullptr;
}
