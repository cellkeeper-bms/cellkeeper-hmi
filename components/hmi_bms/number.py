import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_MODE,
    CONF_ENTITY_CATEGORY,
    ENTITY_CATEGORY_CONFIG,
)
from . import HMIBMS, hmi_bms_ns

HMIBMSNumber = hmi_bms_ns.class_("HMIBMSNumber", number.Number)

CONF_VOLTAGE_LIMIT_OFFSET_LOWER = "voltage_limit_offset_lower"
CONF_VOLTAGE_LIMIT_OFFSET_UPPER = "voltage_limit_offset_upper"
CONF_CELL_VOLTAGE_WORKING_MIN_MV = "cell_voltage_working_min_mV"
CONF_CELL_VOLTAGE_WORKING_MAX_MV = "cell_voltage_working_max_mV"
CONF_CELL_VOLTAGE_SOFT_MIN_MV = "cell_voltage_soft_min_mV"
CONF_CELL_VOLTAGE_SOFT_MAX_MV = "cell_voltage_soft_max_mV"
CONF_AUTO_BALANCING_PERIOD_MS = "auto_balancing_period_ms"
CONF_BALANCING_PERIODS_PER_MV = "balancing_periods_per_mV"
CONF_BALANCE_MIN_OFFSET_MV = "balance_min_offset_mV"
CONF_MINIMUM_BALANCING_VOLTAGE_MV = "minimum_balancing_voltage_mV"
CONF_BALANCING_START_VOLTAGE_MV = "balancing_start_voltage_mV"
CONF_SOC_SET = "soc_set"
CONF_SOC_SCALING_MIN = "soc_scaling_min"
CONF_SOC_SCALING_MAX = "soc_scaling_max"
CONF_MAX_CHARGE_CURRENT = "max_charge_current"
CONF_MAX_DISCHARGE_CURRENT = "max_discharge_current"

# HMI Types
HMI_TYPE_UINT16 = 0x12
HMI_TYPE_INT16 = 0x22
HMI_TYPE_UINT32 = 0x14
HMI_TYPE_INT32 = 0x24

# Poll groups (hmi_bms.h PollGroup)
POLL_SLOW = 1
POLL_LIMITS = 3
POLL_SETTINGS = 4

def number_schema(unit=None, icon=None, accuracy=None, device_class=None, min_val=None, max_val=None, step=None):
    args = {
        "class_": HMIBMSNumber,
        "entity_category": ENTITY_CATEGORY_CONFIG,
    }
    if unit is not None:
        args["unit_of_measurement"] = unit
    if device_class is not None:
        args["device_class"] = device_class
    if icon is not None:
        args["icon"] = icon
        
    schema = number.number_schema(**args).extend({
        cv.Optional("min_value", default=min_val): cv.float_,
        cv.Optional("max_value", default=max_val): cv.float_,
        cv.Optional("step", default=step): cv.float_,
    })
    if accuracy is not None:
        schema = schema.extend({
            cv.Optional(CONF_ACCURACY_DECIMALS, default=accuracy): cv.int_,
        })
    return schema

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(HMIBMS),
    cv.Optional(CONF_VOLTAGE_LIMIT_OFFSET_UPPER): number_schema(unit="dV", accuracy=0, device_class="voltage", min_val=-5000, max_val=5000, step=1),
    cv.Optional(CONF_VOLTAGE_LIMIT_OFFSET_LOWER): number_schema(unit="dV", accuracy=0, device_class="voltage", min_val=-5000, max_val=5000, step=1),
    cv.Optional(CONF_CELL_VOLTAGE_WORKING_MIN_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_CELL_VOLTAGE_WORKING_MAX_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_CELL_VOLTAGE_SOFT_MIN_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_CELL_VOLTAGE_SOFT_MAX_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_AUTO_BALANCING_PERIOD_MS): number_schema(unit="ms", accuracy=0, min_val=0, max_val=3600000, step=1000),
    cv.Optional(CONF_BALANCING_PERIODS_PER_MV): number_schema(accuracy=0, min_val=0, max_val=1000, step=1),
    cv.Optional(CONF_BALANCE_MIN_OFFSET_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=1000, step=1),
    cv.Optional(CONF_MINIMUM_BALANCING_VOLTAGE_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_BALANCING_START_VOLTAGE_MV): number_schema(unit="mV", accuracy=0, device_class="voltage", min_val=0, max_val=5000, step=1),
    cv.Optional(CONF_SOC_SET): number_schema(unit="%", accuracy=0, min_val=0, max_val=100, step=1),
    cv.Optional(CONF_SOC_SCALING_MIN): number_schema(unit="%", accuracy=0, min_val=0, max_val=100, step=1),
    cv.Optional(CONF_SOC_SCALING_MAX): number_schema(unit="%", accuracy=0, min_val=0, max_val=100, step=1),
    # Use protocol-wide max (uint16 in 0.1A => 6553.5A) as fallback.
    # Runtime code narrows max to hard limits when those are read.
    cv.Optional(CONF_MAX_CHARGE_CURRENT): number_schema(unit="A", accuracy=1, device_class="current", min_val=0, max_val=6553.5, step=1),
    cv.Optional(CONF_MAX_DISCHARGE_CURRENT): number_schema(unit="A", accuracy=1, device_class="current", min_val=0, max_val=6553.5, step=1),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_VOLTAGE_LIMIT_OFFSET_UPPER in config:
        conf = config[CONF_VOLTAGE_LIMIT_OFFSET_UPPER]
        n = await number.new_number(conf, parent, 30, HMI_TYPE_INT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_VOLTAGE_LIMIT_OFFSET_LOWER in config:
        conf = config[CONF_VOLTAGE_LIMIT_OFFSET_LOWER]
        n = await number.new_number(conf, parent, 29, HMI_TYPE_INT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_CELL_VOLTAGE_WORKING_MIN_MV in config:
        conf = config[CONF_CELL_VOLTAGE_WORKING_MIN_MV]
        n = await number.new_number(conf, parent, 25, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_CELL_VOLTAGE_WORKING_MAX_MV in config:
        conf = config[CONF_CELL_VOLTAGE_WORKING_MAX_MV]
        n = await number.new_number(conf, parent, 26, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_CELL_VOLTAGE_SOFT_MIN_MV in config:
        conf = config[CONF_CELL_VOLTAGE_SOFT_MIN_MV]
        n = await number.new_number(conf, parent, 31, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_CELL_VOLTAGE_SOFT_MAX_MV in config:
        conf = config[CONF_CELL_VOLTAGE_SOFT_MAX_MV]
        n = await number.new_number(conf, parent, 32, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_AUTO_BALANCING_PERIOD_MS in config:
        conf = config[CONF_AUTO_BALANCING_PERIOD_MS]
        n = await number.new_number(conf, parent, 33, HMI_TYPE_UINT32, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_BALANCING_PERIODS_PER_MV in config:
        conf = config[CONF_BALANCING_PERIODS_PER_MV]
        n = await number.new_number(conf, parent, 34, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_BALANCE_MIN_OFFSET_MV in config:
        conf = config[CONF_BALANCE_MIN_OFFSET_MV]
        n = await number.new_number(conf, parent, 35, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_MINIMUM_BALANCING_VOLTAGE_MV in config:
        conf = config[CONF_MINIMUM_BALANCING_VOLTAGE_MV]
        n = await number.new_number(conf, parent, 36, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_BALANCING_START_VOLTAGE_MV in config:
        conf = config[CONF_BALANCING_START_VOLTAGE_MV]
        n = await number.new_number(conf, parent, 42, HMI_TYPE_UINT16, 1.0, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_SOC_SET in config:
        conf = config[CONF_SOC_SET]
        n = await number.new_number(conf, parent, 3, HMI_TYPE_UINT16, 0.01, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SLOW, n))
    if CONF_SOC_SCALING_MIN in config:
        conf = config[CONF_SOC_SCALING_MIN]
        n = await number.new_number(conf, parent, 27, HMI_TYPE_INT16, 0.01, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_SOC_SCALING_MAX in config:
        conf = config[CONF_SOC_SCALING_MAX]
        n = await number.new_number(conf, parent, 28, HMI_TYPE_INT16, 0.01, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.add_register_number(POLL_SETTINGS, n))
    if CONF_MAX_CHARGE_CURRENT in config:
        conf = config[CONF_MAX_CHARGE_CURRENT]
        n = await number.new_number(conf, parent, 43, HMI_TYPE_UINT16, 0.1, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.set_max_charge_current_number(n))
        cg.add(parent.add_register_number(POLL_LIMITS, n))
    if CONF_MAX_DISCHARGE_CURRENT in config:
        conf = config[CONF_MAX_DISCHARGE_CURRENT]
        n = await number.new_number(conf, parent, 44, HMI_TYPE_UINT16, 0.1, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
        cg.add(parent.set_max_discharge_current_number(n))
        cg.add(parent.add_register_number(POLL_LIMITS, n))
