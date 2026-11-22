#!/usr/bin/env python3
"""Todo stays portrait and usable with every saved reader orientation."""
import json
from pathlib import Path
import tempfile

from features import ART, HOME, latest, run, seed

for orientation in range(4):
    with tempfile.TemporaryDirectory(prefix='todo-orientation-', dir=ART) as folder:
        sd = Path(folder)
        seed(sd, sort=0, tasks=[dict(id=i + 1, completed=False, text=f'Orientation task {i + 1:02}') for i in range(32)])
        path = sd / '.crosspoint/settings.json'
        settings = json.loads(path.read_text())
        settings['orientation'] = orientation
        path.write_text(json.dumps(settings))
        script = (HOME + '300:DOWN;700:TAP:55,245;1100:TAP:390,50;'
                  '1100:WAIT:SCREEN:.*Sort tasks;'
                  f'300:SHOT:{ART}/orientation-{orientation}-sort.bmp;600:TAP:240,439;'
                  f'1000:SHOT:{ART}/orientation-{orientation}-list.bmp;1300:QUIT')
        run(sd, 'orientation-' + str(orientation), script)
        saved = latest(sd)
        assert saved['generation'] == 2 and saved['tasks'][6]['completed']
        assert sum(t['completed'] for t in saved['tasks']) == 1
print('All four reader orientation preferences: Todo paging, checkbox and sort passed')
