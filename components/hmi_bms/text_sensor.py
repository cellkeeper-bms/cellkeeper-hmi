import subprocess
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import HMIBMS

CONF_SERIAL = "serial"
CONF_BOARD_REVISION = "board_revision"
CONF_BMS_FIRMWARE = "bms_firmware"
CONF_HMI_FIRMWARE = "hmi_firmware"
CONF_EVENT_HISTORY = "event_history"
CONF_CELLS_BALANCING = "cells_balancing"
CONF_CELLS_BALANCING_ACTIVE = "cells_balancing_active"
CONF_SETTINGS_WRITE_STATUS = "settings_write_status"
CONF_LOG = "log"
CONF_LOG_LINES = "log_lines"

# Complete list of event types from C++
EVENT_TYPES = [
    "CONTACTOR_POS_STUCK_OPEN",
    "CONTACTOR_POS_STUCK_CLOSED",
    "CONTACTOR_NEG_STUCK_OPEN",
    "CONTACTOR_NEG_STUCK_CLOSED",
    "CONTACTOR_PRE_STUCK_OPEN",
    "CONTACTOR_PRE_STUCK_CLOSED",
    "CONTACTOR_PRECHARGE_VOLTAGE_TOO_HIGH",
    "CONTACTOR_PRECHARGE_CURRENT_TOO_HIGH",
    "CONTACTOR_POS_UNEXPECTED_OPEN",
    "CONTACTOR_NEG_UNEXPECTED_OPEN",
    "CONTACTOR_CLOSING_FAILED",
    "SUPPLY_VOLTAGE_STALE",
    "BATTERY_VOLTAGE_STALE",
    "BATTERY_TEMPERATURE_STALE",
    "CURRENT_STALE",
    "CELL_VOLTAGES_STALE",
    "SUPPLY_VOLTAGE_3V3_LOW",
    "SUPPLY_VOLTAGE_3V3_HIGH",
    "SUPPLY_VOLTAGE_5V_LOW",
    "SUPPLY_VOLTAGE_5V_HIGH",
    "SUPPLY_VOLTAGE_12V_LOW",
    "SUPPLY_VOLTAGE_12V_HIGH",
    "SUPPLY_VOLTAGE_CONTACTOR_LOW",
    "SUPPLY_VOLTAGE_CONTACTOR_VERY_LOW",
    "SUPPLY_VOLTAGE_CONTACTOR_HIGH",
    "BATTERY_VOLTAGE_HIGH",
    "BATTERY_VOLTAGE_VERY_HIGH",
    "BATTERY_VOLTAGE_LOW",
    "BATTERY_VOLTAGE_VERY_LOW",
    "CELL_VOLTAGE_HIGH",
    "CELL_VOLTAGE_VERY_HIGH",
    "CELL_VOLTAGE_LOW",
    "CELL_VOLTAGE_VERY_LOW",
    "SOFT_CHARGE_BUFFER_EXCEEDED",
    "OVERCURRENT_CHARGING",
    "OVERCURRENT_DISCHARGING",
    "BATTERY_TEMPERATURE_HIGH",
    "BATTERY_TEMPERATURE_VERY_HIGH",
    "BATTERY_TEMPERATURE_LOW",
    "BATTERY_TEMPERATURE_VERY_LOW",
    "VOLTAGE_MISMATCH",
    "BMB_READ_ERROR",
    "BMB_CRC_MISMATCH",
    "CELL_VOLTAGE_GLITCH",
    "MODULE_TEMPERATURE_GLITCH",
    "INVERTER_DETECTED",
    "ESTOP_PRESSED",
    "BOOT_NORMAL",
    "BOOT_WATCHDOG",
    "LOOP_OVERRUN",
    "RESTARTING",
    "LINK_VOLTAGE_MISMATCH",
    "FUSE_VOLTAGE_DROP_HIGH",
    "ISOLATION_ASYMMETRY",
    "CONTACTOR_PORT_ENERGIZED",
    "BOARD_MISMATCH",
    "SUPERVISOR_NOT_ENABLED",
    "SUPERVISOR_DISABLED_CONTACTORS",
    "BMB_MODULE_ID_CHECK_FAILED",
    "BMB_MODULE_ID_UNKNOWN",
    "BMB_MODULE_ID_CONFLICT"
]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(HMIBMS),
    cv.Optional(CONF_SERIAL): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_BOARD_REVISION): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_BMS_FIRMWARE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_HMI_FIRMWARE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_EVENT_HISTORY): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_CELLS_BALANCING): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_CELLS_BALANCING_ACTIVE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_SETTINGS_WRITE_STATUS): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_LOG): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_LOG_LINES): cv.ensure_list(text_sensor.text_sensor_schema()),
})

# Add optional text sensor for each event type
for event_type in EVENT_TYPES:
    # Use lowercase name as config key, e.g. "error_contactor_pos_stuck_open"
    key = "error_" + event_type.lower()
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(key): text_sensor.text_sensor_schema()
    })

def hmi_build():
    """This HMI firmware's commit, "08e3c7a" or "08e3c7a-dirty", from the repo holding this
    component; "unknown" outside git."""
    repo = Path(__file__).resolve().parent
    try:
        commit = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"], cwd=repo, capture_output=True, text=True, check=True
        ).stdout.strip()
        dirty = subprocess.run(["git", "diff", "--quiet", "HEAD", "--"], cwd=repo, capture_output=True).returncode != 0
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    return f"{commit}-dirty" if dirty else commit


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_SERIAL in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL])
        cg.add(parent.set_serial_text_sensor(sens))
    if CONF_BOARD_REVISION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BOARD_REVISION])
        cg.add(parent.set_board_revision_text_sensor(sens))
    if CONF_BMS_FIRMWARE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BMS_FIRMWARE])
        cg.add(parent.set_bms_firmware_text_sensor(sens))
    if CONF_HMI_FIRMWARE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_HMI_FIRMWARE])
        cg.add(parent.set_hmi_firmware_text_sensor(sens))
        cg.add(parent.set_hmi_build(hmi_build()))
    if CONF_EVENT_HISTORY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_EVENT_HISTORY])
        cg.add(parent.set_event_history_text_sensor(sens))
    if CONF_CELLS_BALANCING in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CELLS_BALANCING])
        cg.add(parent.set_cells_balancing_text_sensor(sens))
    if CONF_CELLS_BALANCING_ACTIVE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CELLS_BALANCING_ACTIVE])
        cg.add(parent.set_cells_balancing_active_text_sensor(sens))
    if CONF_SETTINGS_WRITE_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SETTINGS_WRITE_STATUS])
        cg.add(parent.set_settings_status_text_sensor(sens))
    if CONF_LOG in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LOG])
        cg.add(parent.set_log_text_sensor(sens))
    if CONF_LOG_LINES in config:
        for conf in config[CONF_LOG_LINES]:
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(parent.add_log_line_sensor(sens))
        
    for i, event_type in enumerate(EVENT_TYPES):
        key = "error_" + event_type.lower()
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(parent.set_error_sensor(i, sens))