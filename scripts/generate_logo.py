"""Generate the boot/sleep logo, the web icon and the README logo from src/images/logo.svg.

The web icon and docs/images/crossdesk-logo.svg are identical; edit logo.svg, not them.

Run from the firmware root; needs Pillow and rsvg-convert (librsvg):
    python3 scripts/generate_logo.py && ./bin/clang-format-fix -g
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

# Web icon and README logo: the logo on a rounded lavender tile.
svg = logo.read_text()
if not svg.startswith('<svg'):
    raise SystemExit('logo.svg must start with its <svg> element')
x, y, w, _ = (float(v) for v in re.search(r'viewBox="([^"]+)"', svg).group(1).split())
body = svg[svg.index('>') + 1:svg.rindex('</svg>')]
size, pad = 512, 92
scale = (size - 2 * pad) / w
icon = (
    f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size}" height="{size}">'
    '<defs><radialGradient id="cp-bg" cx="50%" cy="50%" r="70%"><stop offset="0" stop-color="#b9a7e6"/>'
    '<stop offset="1" stop-color="#937fd0"/></radialGradient></defs>'
    f'<rect width="{size}" height="{size}" rx="80" fill="url(#cp-bg)"/>'
    f'<g transform="translate({pad} {pad}) scale({scale:.4f}) translate({-x} {-y})">{body}</g></svg>\n')
(root / 'src/network/html/assets/crosspoint-icon.svg').write_text(icon)
(root / 'docs/images').mkdir(exist_ok=True)
(root / 'docs/images/crossdesk-logo.svg').write_text(icon)
print('Wrote Logo120.png, Logo120.h, crosspoint-icon.svg and docs/images/crossdesk-logo.svg')
