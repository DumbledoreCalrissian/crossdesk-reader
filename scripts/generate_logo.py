"""Generate the boot/sleep logo and the web icon from src/images/logo.svg.

Run from the firmware root; needs Pillow and rsvg-convert (librsvg):
    python3 scripts/generate_logo.py
Logo120.png is also the user-guide EPUB cover image.
"""
from pathlib import Path
import re
import subprocess

from convert_icon import load_image, image_to_c_array

root = Path(__file__).resolve().parents[1]
logo = root / 'src/images/logo.svg'
png = root / 'src/images/Logo120.png'

subprocess.run(['rsvg-convert', '-w', '120', '-h', '120', str(logo), '-o', str(png)], check=True)
(root / 'src/images/Logo120.h').write_text(image_to_c_array(load_image(str(png), 120, 120), 'Logo120'))

# Web icon: the logo on the rounded green tile used by the web UI.
svg = logo.read_text()
if not svg.startswith('<svg'):
    raise SystemExit('logo.svg must start with its <svg> element')
x, y, w, _ = (float(v) for v in re.search(r'viewBox="([^"]+)"', svg).group(1).split())
body = svg[svg.index('>') + 1:svg.rindex('</svg>')]
size, pad = 512, 92
# Tile colours and radius mirror the upstream CrossPoint web icon.
scale = (size - 2 * pad) / w
(root / 'src/network/html/assets/crosspoint-icon.svg').write_text(
    f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size}" height="{size}">'
    '<defs><radialGradient id="cp-bg" cx="50%" cy="50%" r="70%"><stop offset="0" stop-color="#80ad95"/>'
    '<stop offset="1" stop-color="#628a75"/></radialGradient></defs>'
    f'<rect width="{size}" height="{size}" rx="80" fill="url(#cp-bg)"/>'
    f'<g transform="translate({pad} {pad}) scale({scale:.4f}) translate({-x} {-y})">{body}</g></svg>\n')
print('Wrote Logo120.png, Logo120.h and crosspoint-icon.svg')
