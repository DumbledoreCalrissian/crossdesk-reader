# Todo checks

See `docs/picco-todo.md` for commands, persistence format, resource bounds, and
physical testing requirements. `run.sh` tests the real model/store with
ArduinoJson and injected storage failures. `simulator.py` exercises the actual
activity stack and host SD across process restarts using scripted touch only.
No device is accessed, uploaded, or flashed.

`visuals.py` captures all five built-in themes (empty, wrapped/completed tasks,
delete selection and errors), checks that opening/selecting screens does not
rewrite task snapshots, and saves PNGs under `../qa-artifacts/todo-native/`.
There is one fixed UI font tier; no adjustable scale setting exists.

`native_appearance.py` compares actual Settings selection backgrounds with Todo
in Classic, Lyra and RoundedRaff. It checks that a completion checkbox moves
focus to its task, leaves New task unhighlighted, and does not open a keyboard;
a subsequent QA Confirm must edit that task. PNGs are saved under
`../qa-artifacts/todo-native-reference/`. Both capture scripts need Pillow in the
workspace venv. `visuals.py` also records Home to inspect the checklist icon.
The touch workflow tests the last checkbox in a full 32-task list.

`features.py` checks priority cycling without row jumps, order after restart,
all sort modes, saved-order preservation, Desk widget order, and trusted versus
untrusted date lines. `run.sh` also checks display-order ties and mapping after
toggle/add/delete with no heap storage for ordering.

`button_only.py` is a targeted preservation check for the empty New task row
and Confirm-to-text-entry flow. It uses the existing `simulator` (X4) host
profile, built separately; no extra hardware target or device test is implied.

`orientations.py` checks paging, checkbox routing and sort under all four saved
reader orientation preferences. Todo remains portrait, as other non-reader
screens do; this does not claim rotated landscape rendering.
