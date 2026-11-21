#!/usr/bin/env python3
"""How much internal RAM a firmware leaves for start-up, read from its ELF. Run it before an OTA.

  tools/boot_heap.py <firmware.elf> ... [--min BYTES] [--warn BYTES]
  tools/boot_heap.py --builds <ESPHome data dir> [--min BYTES] [--warn BYTES]

Why: on the ESP32-P4 (pre-v3 silicon, as on the Guition JC1060P470C and the Waveshare 7B), static
data (.data/.bss in internal RAM) sits in the same block of internal RAM that is the only heap until
FreeRTOS starts. The ESP-hosted Wi-Fi co-processor driver and the system tasks fill the rest. If too
little is left, the P4 asserts in vApplicationGetIdleTaskMemory before app_main at every boot. OTA
cannot fix that: the display has to be flashed over USB in download mode, with its BMS link
unplugged (on the USB variants the link shares the USB D+/D- pins). Steps: components/hmi_ui/README.md.
The build itself fails below the limit on these chips (components/hmi_bms/boot_heap.ld).

Measured with ESPHome 2026.10.2 (ESP-IDF 5.5.6):
  desk display, c522861   106,624 bytes   asserted at every boot
  wheel balancer          108,720 bytes   asserted;  108,928 booted
  desk display, a63453f   113,360 bytes   boots
so a build fails below 110,000 bytes and gets a warning below 112,000. Builds of older ESPHome
(2026.7.x: ck0 and ck1 boot at 104,960) need less at start-up; they are shown but not judged.

The number is the free space from _heap_start to the end of the heap region that holds it, as
ESP-IDF's own region table in the ELF (soc_memory_regions) describes it, minus any reserved
region inside that span (soc_reserved_memory_region_start.._end). For a pre-v3 P4 that end is
0x4ff3afc0 (SOC_ROM_STACK_START - SOC_ROM_STACK_SIZE in heap/port/esp32p4/memory_layout.c); the
RAM above it is the ROM's start-up stack, which becomes heap only once FreeRTOS runs.

With --builds it checks every build/<name>/build/firmware.elf under an ESPHome data directory
(esphome-wsl: ~/esphome/data/<repo>; plain ESPHome: <repo>/.esphome) and names the device file
each was built from (storage/<file>.yaml.json; several files share build/ck0, the newest wins).
Exit status: 0 all pass (or nothing built), 1 a build is below --min, 2 bad input.
Plain Python 3, no packages needed.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

FAIL_BELOW = 110_000
WARN_BELOW = 112_000
# Thresholds were measured on this ESPHome; older ones need less start-up RAM (see above)
JUDGED_FROM = (2026, 10)


class Elf:
    """Just enough of a 32-bit little-endian ELF: sections and the symbol table."""

    def __init__(self, path):
        self.data = Path(path).read_bytes()
        d = self.data
        if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
            raise ValueError("not a 32-bit little-endian ELF")
        shoff, = struct.unpack_from("<I", d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
        self.sections = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
                "<10I", d, shoff + i * shentsize)
            self.sections.append(dict(name=name, type=typ, addr=addr, off=off, size=size, link=link,
                                      entsize=entsize))
        self.symbols = {}
        for s in self.sections:
            if s["type"] != 2:  # SHT_SYMTAB
                continue
            strtab = self.sections[s["link"]]
            for j in range(s["size"] // 16):
                st_name, value, size, info, other, shndx = struct.unpack_from(
                    "<IIIBBH", d, s["off"] + j * 16)
                end = d.index(b"\0", strtab["off"] + st_name)
                name = d[strtab["off"] + st_name:end].decode("ascii", "replace")
                if name:
                    self.symbols[name] = (value, size)

    def read(self, addr, size):
        for s in self.sections:
            if s["type"] not in (0, 8) and s["addr"] <= addr and addr + size <= s["addr"] + s["size"]:
                o = s["off"] + addr - s["addr"]
                return self.data[o:o + size]
        raise ValueError(f"no section holds 0x{addr:08x}")


def boot_heap(elf):
    """(free bytes, heap start, region end) of the start-up heap block that follows static data."""
    syms = elf.symbols
    heap_start = syms.get("_heap_start_low", syms.get("_heap_start", (None,)))[0]
    if heap_start is None or "soc_memory_regions" not in syms:
        raise ValueError("no _heap_start or soc_memory_regions symbol (not an ESP-IDF build?)")
    addr, size = syms["soc_memory_regions"]
    # soc_memory_region_t: start, size, type, iram_address, bool startup_stack (20 bytes)
    if size == 0 or size % 20:
        raise ValueError(f"soc_memory_regions is {size} bytes, not a list of 20-byte entries")
    raw = elf.read(addr, size)
    region = None
    for i in range(size // 20):
        start, length, _type, _iram, startup = struct.unpack_from("<IIIIB", raw, i * 20)
        if start <= heap_start < start + length:
            if startup:
                raise ValueError("_heap_start lies in the ROM's start-up stack region")
            region = (start, start + length)
    if region is None:
        raise ValueError(f"_heap_start 0x{heap_start:08x} is in no heap region")
    end = region[1]
    free = end - heap_start
    # Anything reserved inside the span (none on today's builds) is not heap either
    if "soc_reserved_memory_region_start" in syms and "soc_reserved_memory_region_end" in syms:
        r0 = syms["soc_reserved_memory_region_start"][0]
        r1 = syms["soc_reserved_memory_region_end"][0]
        raw = elf.read(r0, r1 - r0) if r1 > r0 else b""
        for i in range(len(raw) // 8):
            a, b = struct.unpack_from("<II", raw, i * 8)
            free -= max(0, min(b, end) - max(a, heap_start))
    return free, heap_start, end


def build_info(elf_path):
    """ESPHome's build_info.json beside build/firmware.elf, if there is one."""
    p = Path(elf_path).resolve().parent.parent / "build_info.json"
    try:
        return json.loads(p.read_text())
    except (OSError, ValueError):
        return {}


def version_tuple(v):
    try:
        return tuple(int(x) for x in v.split(".")[:2])
    except (AttributeError, ValueError):
        return None


def check(elf_path, label, fail_below, warn_below):
    """Prints one line for the ELF; returns 'ok', 'warn', 'fail', 'old' or 'error'."""
    try:
        free, start, end = boot_heap(Elf(elf_path))
    except (OSError, ValueError) as e:
        print(f"{label}: cannot read: {e}")
        return "error"
    info = build_info(elf_path)
    version = info.get("esphome_version")
    built = info.get("build_time_str", "")
    about = f"ESPHome {version}, built {built}" if version else str(elf_path)
    head = f"{label}: {free:,} bytes free at start-up (0x{start:08x}..0x{end:08x}; {about})"
    v = version_tuple(version) if version else None
    if v is not None and v < JUDGED_FROM:
        print(f"{head}\n  not judged: thresholds are for ESPHome "
              f"{JUDGED_FROM[0]}.{JUDGED_FROM[1]} and later")
        return "old"
    if free < fail_below:
        print(f"{head}\n  FAIL: below {fail_below:,}. This build would likely assert in "
              f"vApplicationGetIdleTaskMemory at every boot: do not flash it. Move static data "
              f"to the heap (cg.Pvariable with new, see components/hmi_bms/__init__.py).")
        return "fail"
    if free < warn_below:
        print(f"{head}\n  WARNING: below {warn_below:,}, close to the {fail_below:,} limit")
        return "warn"
    print(f"{head}\n  OK")
    return "ok"


def builds_in(data_dir):
    """[(label, elf)] for every build under an ESPHome data directory."""
    data_dir = Path(data_dir)
    sources = {}  # build path -> [(mtime, device file)]
    for s in sorted((data_dir / "storage").glob("*.yaml.json")):
        try:
            bp = json.loads(s.read_text()).get("build_path")
        except (OSError, ValueError):
            continue
        if bp:
            sources.setdefault(str(Path(bp)), []).append((s.stat().st_mtime, s.name[:-len(".json")]))
    out = []
    for elf in sorted((data_dir / "build").glob("*/build/firmware.elf")):
        folder = elf.parent.parent
        files = sources.get(str(folder), [])
        # Storage is written as a compile starts: the newest one before the ELF built it
        before = [f for f in files if f[0] <= elf.stat().st_mtime + 1] or files
        name = max(before)[1] if before else "device file unknown"
        out.append((f"build/{folder.name} ({name})", elf))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("elf", nargs="*", help="firmware.elf of a build")
    ap.add_argument("--builds", metavar="DATA_DIR", help="check every build under this ESPHome data dir")
    ap.add_argument("--min", type=int, default=FAIL_BELOW, help=f"fail below (default {FAIL_BELOW})")
    ap.add_argument("--warn", type=int, default=WARN_BELOW, help=f"warn below (default {WARN_BELOW})")
    a = ap.parse_args()
    targets = [(str(e), e) for e in a.elf]
    if a.builds:
        for folder in sorted(Path(a.builds).glob("build/*/")):
            if not (folder / "build" / "firmware.elf").exists():
                print(f"build/{folder.name}: no firmware.elf (not linked yet, or its link failed)")
        found = builds_in(a.builds)
        if not found:
            print(f"no builds under {a.builds}: nothing to check (compile a device first)")
        targets += found
    if not targets:
        if a.builds:
            return 0
        ap.error("give an ELF or --builds")
    results = [check(elf, label, a.min, max(a.warn, a.min)) for label, elf in targets]
    if "fail" in results:
        return 1
    return 2 if "error" in results else 0


if __name__ == "__main__":
    sys.exit(main())
