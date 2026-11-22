# Reading calendar

The Desk Home theme shows a monthly reading calendar, total minutes read this
month, a streak of days meeting your reading goal and the current goal. The widget
updates when you enter Home. Tapping the totals or goal line opens **Settings → Reader** with
**Daily reading goal** selected;
tap the day grid to open **Statistics** for the current month. On Picco, Up/Down on
Home page the recent-book strip; with a Confirm key they select books and the
app menu.

Choose **Settings → Reader → Daily reading goal**. The default is 30 minutes;
choices are 5, 10, 15, 20, 30, 45 and 60 minutes. Reading in short bursts adds up
throughout the day, across EPUB books. A calendar day is marked when its total
reaches the goal. Monthly minutes include qualifying reading on days below the
goal, too, and are rounded down after adding the month's time.

Changing the goal applies to today and future days. Past days keep their recorded
goals. Without a trusted date, a goal edit sets the goal for future dated reading
and does not rewrite stored days. A streak continues across months; yesterday's streak remains visible
until today ends, giving you today to meet your goal.

This uses the existing EPUB reading-time rules: visible-page stays of two seconds
to five minutes count; menus, loading and longer idle stays do not. Other formats
are unchanged. Daily history starts with this feature; old cumulative book totals
cannot reconstruct past reading dates. Resetting a book's statistics does not
reset the calendar.

## Monthly Statistics

Statistics is read-only. Use the left/right month arrows or page buttons to
browse from the oldest retained month to the current month. An arrow disappears
at each limit. Statistics also opens from **Apps → Statistics** in Desk and
from the Home menu in the other themes. Use the chart icon in Cover Grid or the
**Reading** card in the Coverflow apps sheet. Back or the Home gesture returns
to the covers with the sheet closed in Coverflow. Desk selects **Apps** and the
other themes select the Statistics shortcut. Dates and figures on Statistics
have no tap action.

The grid distinguishes no reading, reading below the recorded daily goal,
meeting the goal, and an hour or more on a goal day. Its legend explains the
shades. A folded corner marks the best day (the earliest if tied), today has an
outline, future dates are dotted, and dates before tracking are bare numbers.
Past goal results stay unchanged when you edit today's goal.

The figures show total minutes, days with reading, days meeting their goal,
the best day and the longest goal run within a past month. The current month
shows figures so far and the current goal streak, which can continue from the
previous month. Durations are rounded down to minutes. A comparison shows the
signed difference from the full previous month, or from the same dates of the
previous month for the current month (up to that month's last day). It is hidden
if that previous period is not fully tracked.

An empty month shows only minutes and days read with a message. The first
retained month notes when tracking began; older dates cannot be reconstructed
once they leave the 366-day window. With no history, Statistics explains how to
start. It does not edit goals, reset history or change any saved reading data.
Goal editing stays in **Settings → Reader → Daily reading goal**.

## Clock and saves

Sync the device clock before using the calendar. When the clock is untrusted,
the widget explains this; tapping it opens **Settings → System** with **Clock**
selected. If history cannot be read, the widget says so and asks you to check the
SD card; that state has no tap action. On devices without a hardware clock, the
clock messages are text only. Reading without a trusted date
still counts toward book statistics, but cannot be assigned to a calendar day.
Large clock corrections and timezone/DST changes during a page stay can exclude
that stay from daily history; book totals are unaffected. If the local date is
before the newest recorded day (for example after correcting an accidentally
advanced clock), the widget asks you to check the clock (tapping it opens
**Settings → System** with **Clock** selected) and daily tracking pauses until the date catches up. Existing history is preserved.

Daily history and the chosen goal are saved together on the SD card in
`/.crosspoint/reading-calendar-a.bin` and `reading-calendar-b.bin`. Keep both when
backing up the card. The two copies protect against an interrupted save; they do
not replace a backup on another device. Daily details retain a rolling 366 days;
a carried streak count allows a longer uninterrupted streak to continue.

A failed goal save keeps the previous setting and shows an error. Check the clock and card
access, then retry. Reader save failures are logged and retried while that reader
is open, including on exit; a power cut or leaving after repeated save failures
can lose unsaved time. Per-book statistics and calendar saves are separate.

If neither calendar copy is readable, back up both before restoring a good copy.
To deliberately start fresh with the default goal, move both calendar files out
of `/.crosspoint/`, then restart. Leave the per-book `reading-stats/` folder and
other application files alone.
