#!/usr/bin/env python3
"""Lists the entities of a validated ESPHome config.

Reads the output of `esphome config <device>.yaml` on stdin and prints one line per entity:
domain, name, unit, device class, state class and decimals. tools/check.sh keeps these lists in
tests/entities/ so that a change to the entities a device publishes (which Home Assistant, the
CellKeeper integration and recorded history depend on) shows up in a diff.
"""

import sys

import yaml

DOMAINS = (
    "sensor",
    "text_sensor",
    "binary_sensor",
    "number",
    "switch",
    "button",
    "select",
    "light",
)
FIELDS = ("unit_of_measurement", "device_class", "state_class", "accuracy_decimals")


class Loader(yaml.SafeLoader):
    pass


def _any_tag(loader, tag_suffix, node):
    # !secret, !lambda, !include and the like: the value doesn't matter here
    if isinstance(node, yaml.ScalarNode):
        return loader.construct_scalar(node)
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)


Loader.add_multi_constructor("!", _any_tag)


def walk(domain, node, out):
    """Every mapping with a name under a domain is an entity (a platform's own entry, or one of
    the per-value entries of a platform like hmi_bms)."""
    if isinstance(node, list):
        for item in node:
            walk(domain, item, out)
    elif isinstance(node, dict):
        name = node.get("name")
        if isinstance(name, str) and name:
            fields = [str(node.get(f, "")) for f in FIELDS]
            internal = " internal" if node.get("internal") else ""
            disabled = " disabled" if node.get("disabled_by_default") else ""
            out.append(" | ".join([domain, name] + fields).rstrip() + internal + disabled)
        for key, value in node.items():
            if key not in ("filters", "on_value", "on_press", "on_turn_on", "on_turn_off"):
                walk(domain, value, out)


def main():
    config = yaml.load(sys.stdin.read(), Loader=Loader)
    out = []
    for domain in DOMAINS:
        walk(domain, config.get(domain, []), out)
    out.sort()
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
