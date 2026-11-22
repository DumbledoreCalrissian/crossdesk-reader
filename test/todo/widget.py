#!/usr/bin/env python3
"""Desk Todo priority/date columns against the real simulator."""
from pathlib import Path
import tempfile
from features import ART, seed, run, latest


def main():
    tasks = [dict(id=1, completed=False, text='Long priority task with enough text to truncate', priority=3, due=20777),
             dict(id=2, completed=False, text='Medium priority task', priority=1, due=20781),
             dict(id=3, completed=False, text='No priority task', due=20782)]
    for trusted in (True, False):
        with tempfile.TemporaryDirectory(prefix='todo-widget-', dir=ART) as folder:
            sd = Path(folder)
            before = seed(sd, trusted=trusted, sort=0, tasks=tasks)
            name = 'widget-trusted' if trusted else 'widget-untrusted'
            output = run(sd, name, '0:WAIT:Entering activity: Home;500:SHOT:' + str(ART / (name + '.bmp')) + ';800:QUIT')
            home = next(line for line in output.splitlines() if line.startswith('[SIM] SCREEN:') and '| Apps |' in line and '| Todo |' in line)
            assert all(text in home for text in ('!!!', 'Long priority', 'Medium priority', 'No priority', '20 Nov', '24 Nov', '25 Nov')), home
            assert ('* ' in home) == trusted, home
            assert latest(sd) == before
    print('Todo widget checks passed: priority/date columns, no-priority fallback, trusted/untrusted, read only')


if __name__ == '__main__':
    main()
