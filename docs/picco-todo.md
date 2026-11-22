# Todo

Open **Todo** on Home (in the Desk theme, from the menu button on Home), then **New task**. Type a description and tap **OK**.

Desk Home also shows the first three unfinished tasks beside the current book.
Tap anywhere in that widget to open Todo. It shows "No open tasks" when every
task is done, and "Tasks unavailable" when the saved tasks cannot be read. On Picco, Up/Down page the recent-book
strip; with a Confirm key they select books and the menu button.

- The header shows how many tasks you have, for example "Todo (12/32)".
- Open tasks are listed first, then completed ones. Both keep the order in
  which they were added; the saved files keep that order too.
- Tap a checkbox to complete or reopen a task. The task moves to its group.
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
