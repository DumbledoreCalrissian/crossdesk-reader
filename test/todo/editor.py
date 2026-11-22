#!/usr/bin/env python3
"""Draft ownership, dates, cancellation and keyboard integration on Picco."""
import json
from pathlib import Path
import tempfile

from features import ART, HOME, latest, run, seed

OPEN = HOME + '300:TAP:180,245;300:WAIT:Entering activity: TodoEdit;'
SAVE = '300:TAP:240,750;700:QUIT'
NEW = HOME + '300:TAP:180,130;300:WAIT:Entering activity: KeyboardEntry;'
TYPE_Q = '300:TAP:52,568;700:TAP:420,754;700:WAIT:SCREEN:.*Edit task;'


def settings(sd, **values):
    path = sd / '.crosspoint/settings.json'
    doc = json.loads(path.read_text())
    doc.update(values)
    path.write_text(json.dumps(doc))


def screen_lines(output):
    return '\n'.join(line for line in output.splitlines() if line.startswith('[SIM] SCREEN:'))


def main():
    with tempfile.TemporaryDirectory(prefix='todo-edit-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=0, tasks=[dict(id=1, completed=False, text='Original text', priority=2, created=19999, due=20000)])
        before = latest(sd)
        # Changes are drafts: Back must discard both text and due changes.
        run(sd, 'draft-discard', OPEN + '300:TAP:360,420;700:TAP:180,175;700:WAIT:Entering activity: KeyboardEntry;'
            '300:TAP:52,568;700:TAP:420,754;700:WAIT:SCREEN:.*Edit task;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before
        # Every step button works and an unchanged Save performs no write.
        run(sd, 'date-four-steps', OPEN + '300:TAP:360,420;650:TAP:360,495;1000:TAP:120,420;1350:TAP:120,495;1700:TAP:240,750;2100:QUIT')
        assert latest(sd) == before
        run(sd, 'date-plus-save', OPEN + '300:TAP:360,420;700:TAP:240,750;1100:QUIT')
        task = latest(sd)['tasks'][0]
        assert task['due'] == 20001 and task['created'] == 19999 and task['priority'] == 2
        assert latest(sd)['generation'] == before['generation'] + 1
        run(sd, 'date-rocker', OPEN + '300:DOWN;650:DOWN;1000:UP;1350:TAP:240,750;1750:QUIT')
        assert latest(sd)['tasks'][0]['due'] == 20002
        run(sd, 'date-clear', OPEN + '300:TAP:390,570;700:TAP:240,750;1100:QUIT')
        assert 'due' not in latest(sd)['tasks'][0]
        before = latest(sd)
        settings(sd, clockHasBeenSynced=0)
        out = run(sd, 'date-no-clock', OPEN + '300:TAP:90,570;650:TAP:240,570;1000:TAP:360,420;1350:DOWN;1700:TAP:240,750;2100:QUIT')
        assert latest(sd) == before
        assert 'Sync clock to start a due date.' in out
        assert 'Overdue' not in screen_lines(out) and 'Due in ' not in screen_lines(out)
        # A known due date remains editable without a clock; created is never used as a guessed start.
        doc = latest(sd)
        doc['tasks'][0]['due'] = 20000
        active = max((sd / '.crosspoint').glob('todo-?.json'), key=lambda p: json.loads(p.read_text())['generation'])
        active.write_text(json.dumps(doc)) # optional fields deliberately lie outside the checksum
        out = run(sd, 'known-due-no-clock', OPEN + '300:TAP:360,420;700:TAP:240,750;1100:QUIT')
        assert latest(sd)['tasks'][0]['due'] == 20001
        assert 'Sync clock to use Today and Tomorrow.' in out and 'Overdue' not in screen_lines(out)

    # New task keyboard cancel and Back after a draft never create a task.
    with tempfile.TemporaryDirectory(prefix='todo-new-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, trusted=False, sort=0, tasks=[])
        before = latest(sd)
        run(sd, 'new-keyboard-cancel', NEW + '300:TAP:30,64;700:WAIT:SCREEN:.*Todo.*No tasks yet;300:QUIT')
        assert latest(sd) == before
        run(sd, 'new-back-discard', NEW + TYPE_Q + '300:TAP:30,50;700:QUIT')
        assert latest(sd) == before
        # Keyboard OK alone cannot trigger the editor Save with the same release.
        run(sd, 'new-keyboard-only', NEW + TYPE_Q + '900:QUIT')
        assert latest(sd) == before
        run(sd, 'new-no-clock-save', NEW + TYPE_Q + SAVE)
        task = latest(sd)['tasks'][0]
        assert task['text'] == 'q' and 'created' not in task and 'due' not in task
        settings(sd, clockHasBeenSynced=1)
        run(sd, 'new-trusted-tomorrow', NEW + TYPE_Q + '300:TAP:240,570;700:TAP:240,750;1100:QUIT')
        task = latest(sd)['tasks'][1]
        assert task['created'] > 0 and task['due'] == task['created'] + 1
        # Editing an old unknown-created task must not backfill creation.
        out = run(sd, 'unknown-created-edit', OPEN + '300:TAP:90,570;700:TAP:240,750;1100:QUIT')
        assert 'created' not in latest(sd)['tasks'][0]
        assert 'Created ' not in screen_lines(out)
        assert 'Due today' in out
        # Existing keyboard cancellation returns to the draft, then Back returns to the list.
        before = latest(sd)
        run(sd, 'existing-keyboard-cancel', OPEN + '300:TAP:180,175;300:WAIT:Entering activity: KeyboardEntry;'
            '300:TAP:30,64;700:WAIT:SCREEN:.*Edit task;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before
        settings(sd, keyboardType=1)
        run(sd, 'new-t9-cancel', HOME + '300:TAP:180,130;300:WAIT:Entering activity: T9Entry;300:TAP:30,64;'
            '700:WAIT:SCREEN:.*Todo;300:QUIT')
        assert latest(sd) == before
        run(sd, 'existing-t9-cancel', OPEN + '300:TAP:180,175;300:WAIT:Entering activity: T9Entry;'
            '300:TAP:30,64;700:WAIT:SCREEN:.*Edit task;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before

    with tempfile.TemporaryDirectory(prefix='todo-long-edit-', dir=ART) as folder:
        sd = Path(folder)
        text = ('A long task description with enough words to fill both visible lines. ' * 3)[:192]
        seed(sd, sort=0, tasks=[dict(id=1, completed=False, text=text)])
        before = latest(sd)
        run(sd, 'long-editor', OPEN + f'400:SHOT:{ART}/long-editor.bmp;700:TAP:30,50;1100:QUIT')
        assert latest(sd) == before

    # A rocker hold belongs to the screen where it began. Touch stays usable.
    for name, script, delta in [
        ('held-entry', HOME + '300:DOWN:1800;500:TAP:180,245;2300:TAP:240,750;2700:QUIT', 0),
        ('held-entry-touch', HOME + '300:DOWN:1800;500:TAP:180,245;900:TAP:360,420;2300:TAP:240,750;2700:QUIT', 1),
        ('held-keyboard-return', OPEN + '300:TAP:180,175;300:WAIT:Entering activity: KeyboardEntry;'
         '300:DOWN:1800;700:TAP:420,754;2300:TAP:240,750;2700:QUIT', 0),
    ]:
        with tempfile.TemporaryDirectory(prefix='todo-held-nav-', dir=ART) as folder:
            sd = Path(folder)
            seed(sd, sort=0, tasks=[dict(id=1, completed=False, text='Held key test', due=20000)])
            run(sd, name, script)
            assert latest(sd)['tasks'][0]['due'] == 20000 + delta
            assert latest(sd)['generation'] == 1 + int(delta != 0)

    # A saved date change does not re-sort the parent until it is reopened.
    with tempfile.TemporaryDirectory(prefix='todo-date-order-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=2, tasks=[dict(id=1, completed=False, text='First task', due=20000),
                               dict(id=2, completed=False, text='Second task', due=20001)])
        out = run(sd, 'due-sort-frozen-edit', OPEN + '300:TAP:390,570;700:TAP:240,750;1100:QUIT')
        last = [line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (2/32)' in line][-1]
        assert last.find('First task') < last.find('Second task')
        out = run(sd, 'due-sort-reopen-edit', HOME + '400:QUIT')
        last = [line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (2/32)' in line][-1]
        assert last.find('Second task') < last.find('First task')

    with tempfile.TemporaryDirectory(prefix='todo-created-order-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=3, tasks=[dict(id=1, completed=False, text='Original task', created=20000)])
        out = run(sd, 'created-sort-frozen-add', NEW + TYPE_Q + SAVE)
        last = [line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (2/32)' in line][-1]
        assert 'Original task |' in last and last.find('Original task') < last.rfind('| q')
        assert latest(sd)['tasks'][1]['created'] > 20000
        out = run(sd, 'created-sort-reopen-add', HOME + '400:QUIT')
        last = [line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (2/32)' in line][-1]
        assert last.find('| q') < last.find('Original task')

    # Date boundaries remain honest and must never turn into the no-date sentinel by stepping.
    for due, disabled_x in [(1, 120), (2932896, 360)]:
        with tempfile.TemporaryDirectory(prefix='todo-boundary-', dir=ART) as folder:
            sd = Path(folder)
            seed(sd, trusted=False, sort=0, tasks=[dict(id=1, completed=False, text='Boundary', due=due)])
            before = latest(sd)
            run(sd, 'date-boundary-' + str(due), OPEN + f'300:TAP:{disabled_x},420;700:TAP:240,750;1100:QUIT')
            assert latest(sd) == before

    # A failed save keeps the draft on screen and both stored task fields unchanged.
    with tempfile.TemporaryDirectory(prefix='todo-save-failure-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=0, tasks=[dict(id=1, completed=False, text='Stored text', due=20000)])
        before = latest(sd)
        (sd / '.crosspoint/todo.tmp').mkdir()
        out = run(sd, 'editor-save-failure', OPEN + '300:TAP:360,420;700:TAP:240,750;'
                  '700:WAIT:SCREEN:.*Save failed;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before and 'Previous tasks kept' in out
    print('Todo editor checks passed: drafts, save/discard, dates, rocker, clock trust, creation, T9, bounds, save failure')


if __name__ == '__main__':
    main()
