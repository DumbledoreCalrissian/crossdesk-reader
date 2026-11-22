#!/usr/bin/env python3
"""Targeted regression for the inherited button-only empty Todo screen."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
art = root.parent / 'qa-artifacts/todo-features'
art.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='todo-button-only-', dir=art) as folder:
    sd = Path(folder)
    (sd / '.crosspoint').mkdir()
    (sd / '.crosspoint/settings.json').write_text(json.dumps(dict(uiTheme=0, deskThemeMigrated=True)))
    steps = ('0:WAIT:Entering activity: Home;300:DOWN;600:DOWN;900:DOWN;1200:ENTER;'
             '1200:WAIT:Entering activity: Todo;300:WAIT:SCREEN:.*New task.*No tasks yet;'
             f'400:SHOT:{art}/button-only-empty.bmp;700:ENTER;'
             '700:WAIT:Entering activity: KeyboardEntry;300:QUIT')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd),
               CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=steps)
    result = subprocess.run([str(root / '.pio/build/simulator/program')], cwd=root,
                            env=env, capture_output=True, text=True, timeout=45)
    output = result.stdout + result.stderr
    (art / 'button-only-empty.log').write_text(output)
    assert result.returncode == 0 and 'WAIT timed out' not in output
    assert 'Entering activity: KeyboardEntry' in output
    assert not list((sd / '.crosspoint').glob('todo-?.json'))
print('Button-only empty Todo shows New task; Confirm opens text entry without saving')
