#!/usr/bin/env python3
"""Saves the LCD's screens as PNG files, from a device built with the lcd_screenshot component.

  tools/screenshot.py <device address> <output folder> [page ...]

With no pages given it captures every page (0, 1, 2 ... until the device stops showing a new one
is not detectable, so the count is taken from --pages, default 10). The web server's user name
and password are read from secrets.yaml (web_server_username / web_server_password).
Needs Pillow and requests-free standard library only for the download: run it with the ESPHome
virtualenv's python, which has Pillow.
"""

import argparse
import base64
import re
import sys
import urllib.request
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parent.parent


def secret(name):
    text = (REPO / "secrets.yaml").read_text(encoding="utf-8")
    m = re.search(r"^" + name + r":\s*[\"']?([^\"'\r\n]+)", text, re.M)
    if not m:
        sys.exit(f"secrets.yaml has no {name}")
    return m.group(1).strip()


def fetch(address, page):
    url = f"http://{address}/screenshot.raw" + ("" if page is None else f"?page={page}")
    request = urllib.request.Request(url)
    token = base64.b64encode(f"{secret('web_server_username')}:{secret('web_server_password')}".encode()).decode()
    request.add_header("Authorization", "Basic " + token)
    with urllib.request.urlopen(request, timeout=30) as reply:
        fmt = reply.headers.get("X-Screen-Format", "")
        data = reply.read()
    m = re.match(r"(\d+)x(\d+) (\w+)", fmt)
    if not m:
        sys.exit(f"unexpected reply (format header {fmt!r}, {len(data)} bytes)")
    return int(m.group(1)), int(m.group(2)), m.group(3), data


def to_image(width, height, fmt, data, swap_rb):
    if fmt == "rgb565le":
        # Pillow's "BGR;16" raw mode is 5-6-5 with red in the top bits, little-endian
        image = Image.frombytes("RGB", (width, height), data, "raw", "BGR;16")
    elif fmt == "rgb888":
        image = Image.frombytes("RGB", (width, height), data, "raw", "RGB")
    else:
        sys.exit(f"unknown pixel format {fmt}")
    if swap_rb:
        r, g, b = image.split()
        image = Image.merge("RGB", (b, g, r))
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("address")
    parser.add_argument("out")
    parser.add_argument("pages", nargs="*", type=int)
    parser.add_argument("--pages", dest="count", type=int, default=10, help="how many pages the UI has")
    parser.add_argument("--current", action="store_true", help="capture what is on screen now, without changing page")
    parser.add_argument("--swap-rb", action="store_true", help="swap red and blue (if the panel is BGR)")
    parser.add_argument("--prefix", default="page")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    pages = [None] if args.current else (args.pages or list(range(args.count)))
    for page in pages:
        width, height, fmt, data = fetch(args.address, page)
        name = f"{args.prefix}-current.png" if page is None else f"{args.prefix}-{page:02d}.png"
        to_image(width, height, fmt, data, args.swap_rb).save(out / name)
        print(f"{name}: {width}x{height} {fmt}, {len(data)} bytes")


if __name__ == "__main__":
    main()
