# Todo

Open **Todo** on Home (in the Desk theme, from the menu button on Home), then **New task**. Type a description and tap **OK**.

Desk Home also shows the first three unfinished tasks in the selected sort order
beside the current book. Priority marks precede each task; `*` marks an overdue
task when the clock is trusted and there is room.
Tap anywhere in that widget to open Todo. It shows "No open tasks" when every
task is done, and "Tasks unavailable" when the saved tasks cannot be read. On Picco, Up/Down page the recent-book
strip; with a Confirm key they select books and the menu button.

- The header shows how many tasks you have, for example "Todo (12/32)".
- Open tasks are listed first, then completed ones. Tap the header arrows to
  choose Priority (highest first), Due date (soonest first, no date last),
  Created (newest first, unknown last), or Added order. Ties keep saved order.
  Done tasks always stay below in saved order. Sorting never changes task
  order in the files.
- Tap a checkbox to complete or reopen a task. The task moves to its group.
- Tap an open task's priority button to cycle none → ! → !! → !!! → none.
  Each change saves immediately. Done tasks keep their priority but hide the
  button. Priority and date changes do not move rows while this list is open.
  The chosen order is recalculated when Todo is opened from Home or a sort is
  selected (even the current sort). Returning from text entry, cancelling sort,
  and entering/leaving the delete view do not re-sort. New tasks, and reopened
  tasks absent from the opening snapshot, follow its open tasks until then.
- New task stays above the scrolling rows. Up to six task rows are visible;
  Up/Down and vertical swipes page the rows. Lists that fit do not scroll.
  Date lines show creation and/or due dates when saved. Overdue needs a trusted
  local clock and is shown only for open tasks; an untrusted clock leaves the
  absolute dates visible. Due-date editing and creation-date recording are
  added by the next milestone.
- Tap its description to edit it. Long descriptions wrap or truncate in the list;
  editing shows the full text. Remove all the text and tap **OK** to delete the
  task after a confirmation.
- Tap the header bin to select tasks for deletion. **Select done** selects all
  completed tasks, and **Select all** / **Select none** selects or clears every
  task. Then tap **Delete selected** and confirm. Back cancels the selection.
- The header arrow returns to Home. The bottom-edge upward Home gesture also works.

Changes save immediately. Todo works offline and supports up to 32 tasks,
with 192 UTF-8 bytes per description. Invalid text is rejected. Blank text is
rejected for a new task; for an existing task it asks to delete the task.
Swipe to scroll longer lists.

## Save errors and recovery

If saving fails, the previous tasks remain visible. Resolve card access and retry.
Saves are `/.crosspoint/todo-a.json` and `/.crosspoint/todo-b.json`; back up both.

If no intact save survives damaged data, editing is disabled and files are kept.
Exit Todo, back up the saves and restore an intact copy. To deliberately start
empty, move both JSON files out of `/.crosspoint/` and reopen Todo. Leave other
files in that directory alone: they include settings, reading positions and Sudoku.


## Version 1 optional metadata

The storage layer accepts optional task fields `priority` (0 to 3), `created`
and `due` (civil-day numbers, with 0 meaning unknown/no date). Dates must be
between 0 and 2932896 (31 December 9999). Missing fields default to 0.
The optional file-level `sort` is 0 for Added order, 1 for Priority, 2 for Due
date, and 3 for Created; missing means Added order. Default values are omitted
when saving. The list and sort screen display these fields; the task edit
screen and date assignment are the next milestone.

Malformed or out-of-range optional values invalidate that snapshot, using the
same recovery rules as other invalid data. The task order on disk never changes.
Version stays 1, and the checksum still covers only version, generation, nextId,
task count, and each task's id, completed flag and text. Release 0.2.0 can read
new snapshots but drops the optional fields on its next save. Optional metadata
therefore has range validation but no checksum protection.

The task limit remains 32 and the file and parse budgets remain 32 KB each.
Host checks cover 32 unique maximum-length descriptions made entirely of JSON
escape characters, every optional field, and maximum valid numeric counters.
The resulting saved snapshot is 15,298 bytes and reloads within the parse budget.
Each task is 208 bytes; the host model is 6,672 bytes (target size may differ
with `size_t`). A mutation keeps one checked model copy until saving succeeds;
failed saves retain the original model and active slot. No additional model copy
or long-lived heap allocation was added.
