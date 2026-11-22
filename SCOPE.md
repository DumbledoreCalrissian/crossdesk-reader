# Scope of CrossDesk Reader

CrossDesk Reader is an independently maintained BOOX Picco build derived from
CrossPoint Reader. EPUB reading is the primary use case. Todo, Pomodoro and Sudoku are
intentional downstream features, even though upstream has a different product
policy. Upstream acceptance rules do not decide this fork's scope.

## Authority and current work

The owner's current request defines the work being authorized. This document
sets the fork's product boundaries; [AGENTS.md](AGENTS.md) defines agent workflow.
The [reading-statistics handoff](docs/reading-stats-handoff.md) records the current
statistics milestones and supersedes earlier multi-format plans. The workspace
`READING_STATS_PLAN.md`, when present, contains the detailed accepted plan.

Historical logs, archived review snapshots, research clones and upstream plans
are evidence or reference material. They do not authorize another feature or
replace current instructions. Resolve a real conflict against the current
request and implementation; ask the owner only when the answer remains unclear.

## Accepted product areas

- EPUB reading, Library, typography, bookmarks, dictionary and clippings.
- Maintenance of the existing Todo, foreground Pomodoro and Sudoku applications,
  including their storage, navigation and Sudoku pack downloads.
- EPUB reading statistics on Picco, within the milestones below.
- The Desk Home theme (owner approved, 14 November 2026) with the widgets
  authorized below, and the Coverflow Home theme described below. Other new
  themes still need their own owner request.
- Text entry: the full keyboard and the T9 keypad described below.
- Picco controls follow the owner's button and touch rules recorded in the
  workspace `qa-artifacts/consistent-controls/MILESTONES.md` and `REVIEWS.md`
  (touch first; no double clicks, key chords or hold-time tiers; Up/Down only
  move, page or change a value). Flag a conflict with them instead of deciding.
- Small shared UI, input, HAL and resource fixes that a concrete accepted feature
  needs, with existing defaults and ownership preserved.

These areas are maintenance boundaries, not permission to start every possible
feature within them. Tie each addition to the owner's request. Prefer existing
mechanisms and the smallest maintainable implementation. Account for new state,
settings, allocations, dependencies and failure paths.

## Reading-statistics boundaries

Milestones 1 to 3 are complete (`119344fb`, `99b6a4d4`, `11afc67b`): a
collector with recoverable per-book storage, EPUB reader collection with a
book-statistics/reset view, and EPUB percentages in Library and progress/time
on Home across the existing themes. Further statistics work needs a new owner
request. The per-book collector (`EpubReadingStats`) is the only measure of
reading time in this fork; other features reuse it rather than measuring again.

### Desk reading calendar (owner approved, 14 November 2026)

On Picco, record qualifying EPUB reading durations by trusted local date for a
bounded 366-day history. Desk Home may show a monthly calendar, total minutes
read in the current month, and consecutive days meeting the daily reading goal.
Monthly minutes include qualifying reading on days below the goal. The goal
defaults to 30 minutes and is editable under Reading settings; changes apply
from today onward, preserving historical goal results. The widget has no
button focus; tapping it opens the Statistics page below. Existing per-book statistics
and resets remain separate. Calendar history starts with this feature; no
historical daily values can be inferred from cumulative book totals.

This is a narrow exception to the calendar/aggregation exclusions below, not
authorization for session counts, other totals screens or services. Weather has
its own authorization below.

No statistics integration for XTC, TXT or Markdown, and no additional device
validation target is required. No overall totals/settings page, aggregation,
calendar/session counts, estimates, database or background service. New themes,
a carousel and Home navigation redesign are separate requests, not dependencies
or later statistics milestones. Shared EpubReaderActivity also handles other
formats; gate statistics on actual EPUB paths.

### Reading statistics page (owner approved, 15 November 2026)

On Picco, a read-only Statistics page shows one month of the existing reading
calendar at a time: a shaded day grid, minutes read, days read, days meeting the
goal, best day, longest goal run, and a comparison with the previous month (or,
for the current month, the same days of the previous month). Months before the
first recorded day or outside the 366-day history are not shown. It is opened
from the Desk calendar, Desk Apps, or the other Home themes’ menus (M4 entry
points authorized by the owner, 15 November 2026), and navigated by month. It derives everything from the
calendar history and adds no new persistent state, collection, settings, sessions,
per-book monthly figures, pages-per-day or background work. Goal editing stays in
Reading settings. This narrows, and does not lift, the exclusions above for other
totals screens, aggregation and session counts.

## Desk weather (owner approved, 15 November 2026)

Add a Weather screen and read-only cached weather widget beside the Desk calendar.
Use Open-Meteo city search and three-day forecasts, with temperature, humidity,
precipitation probability and amount. Choose a location explicitly; do not infer
or preconfigure one. City search and refresh are manual through the existing
Wi-Fi flow, with bounded responses, cancellation and cleanup. No background
updates, automatic location detection or new worker/task. Cache on SD, retain
last successful data on refresh failure and show its timestamp/stale status.
The widget opens Weather by touch, without joining Home's button focus ring;
first setup is accessible through Desk's app menu. Existing other-theme Home
menus and behavior remain unchanged. This is a narrow authorization for this
connector and does not authorize additional services or weather alerts.

## Coverflow Home theme (owner approved, 23 November 2026)

A "Coverflow" theme on Picco whose Home screen shows only the recent books'
covers in a horizontal coverflow: a large centre cover with its neighbours
peeking at the screen edges, standing on a dithered floor shadow. Tapping the
centre cover opens the book; a horizontal swipe or Up/Down moves between
covers, and tapping a side cover brings it to the centre. Back does nothing on
this Home. An optional line under the cover, controlled by one On/Off setting,
shows the centre book's title and progress. Portrait layout; other
orientations reuse it scaled to fit.

A swipe up on Home opens a bottom sheet, styled like the quick-controls sheet,
with the apps and Settings. This swipe is an owner-approved exception to the
control rules, for this theme only. Below the apps, the sheet shows a mini
reading widget (month minutes, streak, and the last 7 days against the daily
goal, all from the existing reading calendar) and a mini weather widget
(cached data only, no network); tapping them opens Statistics or Weather.
No other widgets, and no new persistent state beyond the theme setting, the
progress setting and existing thumbnails. Other themes stay unchanged.

## Sync on sleep (owner approved, 21 November 2026)

Port upstream `589522ec` (Sync On Sleep, bookmark sync and reading-session
upload) and adapt it to this fork:

- Sync On Sleep is an opt-in KOReader Sync setting, off by default. It syncs
  only the book open when the device sleeps, through the existing saved Wi-Fi
  networks, with bounded join time and a low-battery skip. It never moves the
  server position backwards; that stays a manual sync.
- Bookmark sync and session upload go only to a server that offers the
  CrossPoint Sync extensions. A plain KOSync server receives progress only.
- Reading sessions are uploaded only for EPUB, and only while sync credentials
  for a CrossPoint server are set. Each session's duration comes from the
  existing statistics collector, so per-book totals, the calendar and the server
  agree. No second time measurement on the device.
- Sessions wait in a bounded queue on the SD card and are removed once the
  server accepts them. No permanent local session history, session counts on
  screen, or new statistics UI.

This does not authorize other sync services, background sync while awake, or
uploading statistics from other formats.

## T9 keypad (owner approved, 21 November 2026)

A "Keyboard type" setting on touch devices chooses the full keyboard (default)
or a T9 phone keypad. The keypad uses multi-tap only: letter, digit and symbol
pages that reach every printable ASCII character, a case key, an explicit
commit key and a 1 s commit timeout. It serves every text field, including
passwords and URLs, places the cursor by tapping visible text, and has a
"Keypad height" setting (Small 42%, Medium 52%, Large 62% of the screen). It
types Latin letters only; plain text fields in a non-Latin layout keep the full
keyboard. The owner decided against dictionary-based prediction: no word lists,
learned words or other languages' letter groups.

All three password fields (Wi-Fi, KOReader Sync, OPDS) start hidden, with a
show/hide toggle on both keyboards. This deliberately departs from upstream
`8eea7f86`, which shows Wi-Fi and OPDS passwords during entry; keep the fork's
behaviour when merging upstream changes to those screens.

### Todo priorities, dates and sorting (owner approved, 22 November 2026)

Each Todo task may carry a priority (none, !, !!, !!!) set by a button in its
row, a creation date taken only from a trusted local date, and an optional
due date set on a Todo edit screen. A header button sorts open tasks by
priority, due date, creation date or added order; done tasks stay below.
The Desk Todo widget uses the same order and marks. Rows are larger (about
six tasks per page) with a tap-to-edit hint. No reminders, alarms,
notifications, recurring tasks, categories, sync or background work. The
task limit stays 32.

## Compatibility and resources

Target new feature work and verification at Picco and `simulator_picco`.
Inherited other-device and other-format support is not scope drift and must not
be deleted merely to narrow the fork's identity. Preserve existing behavior
when touching shared code. Broaden checks only for a concrete changed contract;
do not turn a Picco task into a new device port or multi-device project.

Use the pinned SDK through the firmware HAL and existing UI components. Budget
Picco internal RAM and PSRAM separately, including peak coexistence and largest
blocks. Preserve fallible allocations and storage recovery. Simulator memory,
power and restart behavior cannot establish physical hardware results.

## New proposals and upstream work

Additional apps, themes, formats, devices, connectors and architectural changes
need a separate owner requirement before implementation. Their presence in an
upstream roadmap or research project does not make them planned work here.
If proposing a change upstream, evaluate it separately against upstream's own
rules; downstream acceptance does not imply upstream acceptance.
