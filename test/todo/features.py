#!/usr/bin/env python3
"""Priority/sort/date display checks against the real Picco activity stack."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT.parent / 'qa-artifacts' / 'todo-features'
ART.mkdir(exist_ok=True)
PROGRAM = ROOT / '.pio/build/simulator_picco/program'
HOME = '0:WAIT:Entering activity: Home;400:TAP:330,395;400:WAIT:Entering activity: Todo;'


def seed(sd, *, trusted=True, sort=1, tasks=None):
    folder = sd / '.crosspoint'
    folder.mkdir()
    (folder / 'settings.json').write_text(json.dumps(dict(uiTheme=5, deskThemeMigrated=True,
                                                          shortPwrBtn=0, clockHasBeenSynced=int(trusted))))
    tasks = tasks if tasks is not None else [dict(id=1, completed=False, text='Alpha task', priority=0, created=20003),
             dict(id=2, completed=False, text='Beta task', priority=3, due=20000),
             dict(id=3, completed=False, text='Gamma task', priority=2, created=20002, due=20001),
             dict(id=4, completed=True, text='Done task', priority=3)]
    data = b''.join(struct.pack('<I', x) for x in (1, 1, len(tasks) + 1, len(tasks)))
    for task in tasks:
        data += struct.pack('<I', task['id']) + bytes([task['completed']]) + task['text'].encode() + b'\0'
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    doc = dict(version=1, generation=1, nextId=len(tasks) + 1, checksum=checksum, tasks=tasks, sort=sort)
    (folder / 'todo-a.json').write_text(json.dumps(doc))
    return doc


def latest(sd):
    return max((json.loads(p.read_text()) for p in (sd / '.crosspoint').glob('todo-?.json')),
               key=lambda d: d['generation'])


def run(sd, name, script):
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
               CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
    result = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=45)
    output = result.stdout + result.stderr
    (ART / (name + '.log')).write_text(output)
    assert result.returncode == 0 and 'WAIT timed out' not in output, name
    assert 'capacity exceeded' not in output.lower() and 'Action handler capacity exceeded' not in output, name
    return output


def ordered(output, *names):
    screens = [line for line in output.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (' in line]
    return any(all(line.find(a) < line.find(b) for a, b in zip(names, names[1:]))
               and all(n in line for n in names) for line in screens)


def main():
    with tempfile.TemporaryDirectory(prefix='todo-features-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd)
        out = run(sd, 'priority-freeze', HOME + '300:TAP:420,245;700:SHOT:' + str(ART / 'priority-freeze.bmp') + ';1000:QUIT')
        assert ordered(out, 'Beta task', 'Gamma task', 'Alpha task', 'Done task')
        saved = latest(sd)
        assert [t['id'] for t in saved['tasks']] == [1, 2, 3, 4]
        assert saved['tasks'][1].get('priority', 0) == 0 and saved['generation'] == 2
        out = run(sd, 'priority-restart', HOME + '400:QUIT')
        assert ordered(out, 'Gamma task', 'Alpha task', 'Beta task', 'Done task')
        assert latest(sd) == saved
        # The first row is Gamma. Cycle 2→3→0→1→2 without moving it.
        out = run(sd, 'priority-cycle', HOME + '300:TAP:420,245;650:TAP:420,245;1000:TAP:420,245;1350:TAP:420,245;1800:QUIT')
        assert latest(sd)['tasks'][2]['priority'] == 2
        assert latest(sd)['generation'] == saved['generation'] + 4
        assert ordered(out, 'Gamma task', 'Alpha task', 'Beta task', 'Done task')
        # Completed row has no cycler; right-side tap opens the editor, not a priority change.
        before = latest(sd)
        run(sd, 'done-no-cycler', HOME + '300:TAP:420,539;300:WAIT:Entering activity: TodoEdit;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before
        # Sort radio positions are Priority, Due date, Created, Added order.
        expected = [(1, ('Gamma task', 'Alpha task', 'Beta task', 'Done task')),
                    (2, ('Beta task', 'Gamma task', 'Alpha task', 'Done task')),
                    (3, ('Alpha task', 'Gamma task', 'Beta task', 'Done task')),
                    (0, ('Alpha task', 'Beta task', 'Gamma task', 'Done task'))]
        for option, (sort, names) in enumerate(expected):
            out = run(sd, 'sort-' + str(sort), HOME + '300:TAP:390,50;300:WAIT:SCREEN:.*Sort tasks;'
                      + f'300:TAP:240,{145 + option * 98};700:QUIT')
            assert ordered(out, *names)
            assert latest(sd).get('sort', 0) == sort
            assert [t['id'] for t in latest(sd)['tasks']] == [1, 2, 3, 4]
            # Home must show the same first three open tasks in this order.
            out = run(sd, 'desk-sort-' + str(sort), '0:WAIT:Entering activity: Home;500:QUIT')
            home = next(line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and '| Apps |' in line and '| Todo |' in line)
            assert all(home.find(a) < home.find(b) for a, b in zip(names[:3], names[1:3]))
        out = run(sd, 'trusted-date-lines', HOME + '400:SHOT:' + str(ART / 'trusted-date-lines.bmp') + ';700:QUIT')
        assert 'Overdue · due' in out
        # Due/date display with an untrusted clock must contain no overdue state.
        settings = json.loads((sd / '.crosspoint/settings.json').read_text())
        settings['clockHasBeenSynced'] = 0
        (sd / '.crosspoint/settings.json').write_text(json.dumps(settings))
        out = run(sd, 'untrusted-date-lines', HOME + '400:SHOT:' + str(ART / 'untrusted-date-lines.bmp') + ';700:QUIT')
        screens = '\n'.join(line for line in out.splitlines() if line.startswith('[SIM] SCREEN:'))
        assert 'Overdue' not in screens and 'due ' in screens
    with tempfile.TemporaryDirectory(prefix='todo-six-rows-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=0, tasks=[dict(id=i + 1, completed=False, text=f'Task row {i + 1:02}') for i in range(32)])
        out = run(sd, 'sixth-priority', HOME + '300:TAP:420,734;700:QUIT')
        screens = [line for line in out.splitlines() if line.startswith('[SIM] SCREEN:') and 'Todo (32/32)' in line]
        assert screens and all(line.count('Task row ') == 6 for line in screens)
        assert latest(sd)['tasks'][5]['priority'] == 1
        assert all(t.get('priority', 0) == 0 for i, t in enumerate(latest(sd)['tasks']) if i != 5)
        before = latest(sd)
        run(sd, 'sort-cancel', HOME + '300:TAP:390,50;300:WAIT:SCREEN:.*Sort tasks;300:TAP:30,50;700:QUIT')
        assert latest(sd) == before
    print('Todo feature checks passed: cycle/freeze/restart, all sorts, Desk order, dates, saved order')


if __name__ == '__main__':
    main()
