#!/usr/bin/env python3
"""Keep HA dashboards in sync from one source file.

Usage:
  python sync_dashboards.py
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "HA-dashboard.yaml"
TARGETS = [
    {
        "path": ROOT / "HA-dashboard-ck1.yaml",
        "replacements": [
            ("CellKeeper0", "CellKeeper1"),
            ("ck0_", "ck1_"),
            ("ck0-", "ck1-"),
            ("ck0", "ck1"),
            ("CK0", "CK1"),
        ],
    },
]

GENERATED_HEADER = (
    "# AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.\n"
    "# Edit HA-dashboard.yaml and run: python sync_dashboards.py\n\n"
)


def apply_replacements(text: str, replacements: list[tuple[str, str]]) -> str:
    out = text
    for old, new in replacements:
        out = out.replace(old, new)
    return out


def main() -> None:
    source_text = SOURCE.read_text(encoding="utf-8")

    for target in TARGETS:
        rendered = apply_replacements(source_text, target["replacements"])
        rendered = GENERATED_HEADER + rendered
        target_path: Path = target["path"]
        target_path.write_text(rendered, encoding="utf-8")
        print(f"wrote {target_path.name}")


if __name__ == "__main__":
    main()
