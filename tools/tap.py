#!/usr/bin/env python3
"""Taps the LCD of a bench device from here, and saves what it then shows.

  tools/tap.py <device address> <step> [<step> ...] [--save picture.png]

A step is "x,y" (a tap at that point), "page=N" (show page N, in the rail's order) or "wait=MS".
For a device built with `hmi_ui: screenshot: true` only. Where things are on a page: the
simulator's tests find them by label (sim/harness.h); a picture taken with this tool shows them.
The web server's user name and password are read from secrets.yaml, as tools/screenshot.py does.
"""

import argparse
import base64
import re
import sys
import time
import urllib.request

from screenshot import secret, to_image


def request(address, query):
    req = urllib.request.Request(f"http://{address}/screenshot.raw" + (f"?{query}" if query else ""))
    token = base64.b64encode(f"{secret('web_server_username')}:{secret('web_server_password')}".encode()).decode()
    req.add_header("Authorization", "Basic " + token)
    with urllib.request.urlopen(req, timeout=30) as reply:
        fmt = reply.headers.get("X-Screen-Format", "")
        data = reply.read()
    m = re.match(r"(\d+)x(\d+) (\w+)", fmt)
    if not m:
        sys.exit(f"unexpected reply (format header {fmt!r}, {len(data)} bytes)")
    return int(m.group(1)), int(m.group(2)), m.group(3), data


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("address")
    parser.add_argument("steps", nargs="+")
    parser.add_argument("--save", help="save the screen after the last step as this PNG file")
    args = parser.parse_args()
    shown = None
    for step in args.steps:
        if step.startswith("wait="):
            time.sleep(int(step[5:]) / 1000.0)
        elif step.startswith("page="):
            shown = request(args.address, f"page={int(step[5:])}")
        elif re.fullmatch(r"\d+,\d+", step):
            shown = request(args.address, f"tap={step}")
        else:
            sys.exit(f"not a step: {step}")
    if args.save:
        if shown is None:
            shown = request(args.address, "")
        width, height, fmt, data = shown
        to_image(width, height, fmt, data, False).save(args.save)
        print(args.save)


if __name__ == "__main__":
    main()
