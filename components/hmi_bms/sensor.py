import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_CELSIUS,
    ICON_EMPTY,
)
from . import HMIBMS, hmi_bms_ns

CONF_HMI_BMS_ID = "hmi_bms_id"
CONF_MILLIS = "millis"
#CONF_SERIAL = "serial"
CONF_SYSTEM_REQUEST = "system_request"
CONF_SOC = "soc"
CONF_CURRENT_MA = "current_mA"
CONF_CHARGE_RAW = "charge_raw"
CONF_BATTERY_VOLTAGE_MV = "battery_voltage_mV"
CONF_OUTPUT_VOLTAGE_MV = "output_voltage_mV"
CONF_POS_CONTACTOR_VOLTAGE_MV = "pos_contactor_voltage_mV"
CONF_NEG_CONTACTOR_VOLTAGE_MV = "neg_contactor_voltage_mV"
CONF_TEMPERATURE_MIN_DC = "temperature_min_dC"
CONF_TEMPERATURE_MAX_DC = "temperature_max_dC"
CONF_TEMPERATURE_DELTA = "temperature_delta"
CONF_CELL_VOLTAGE_MIN_MV = "cell_voltage_min_mV"
CONF_CELL_VOLTAGE_MAX_MV = "cell_voltage_max_mV"
CONF_CELL_VOLTAGE_DELTA = "cell_voltage_delta"
CONF_CELL_COUNT = "cell_count"
CONF_CELL_VOLTAGES = "cell_voltages"
CONF_BALANCING_TIMES = "balancing_times"
CONF_SYSTEM_SM = "system_sm"
CONF_CONTACTOR_SM = "contactor_sm"
CONF_SOC_VOLTAGE_BASED = "soc_voltage_based"
CONF_SOC_BASIC_COUNT = "soc_basic_count"
CONF_CAPACITY_MC = "capacity_mC"
CONF_SUPPLY_VOLTAGE_3V3_MV = "supply_voltage_3V3_mV"
CONF_SUPPLY_VOLTAGE_5V_MV = "supply_voltage_5V_mV"
CONF_SUPPLY_VOLTAGE_12V_MV = "supply_voltage_12V_mV"
CONF_SUPPLY_VOLTAGE_CONTACTOR_MV = "supply_voltage_contactor_mV"
#CONF_CELL_VOLTAGE_WORKING_MIN_MV = "cell_voltage_working_min_mV"
#CONF_CELL_VOLTAGE_WORKING_MAX_MV = "cell_voltage_working_max_mV"
CONF_CELL_VOLTAGE_LIMIT_MIN_MV = "cell_voltage_limit_min_mV"
CONF_CELL_VOLTAGE_LIMIT_MAX_MV = "cell_voltage_limit_max_mV"
CONF_SOC_SCALING_MIN = "soc_scaling_min"
CONF_SOC_SCALING_MAX = "soc_scaling_max"
CONF_VOLTAGE_LIMIT_OFFSET_LOWER = "voltage_limit_offset_lower"
CONF_VOLTAGE_LIMIT_OFFSET_UPPER = "voltage_limit_offset_upper"
CONF_CELL_VOLTAGE_SOFT_MIN_MV = "cell_voltage_soft_min_mV"
CONF_CELL_VOLTAGE_SOFT_MAX_MV = "cell_voltage_soft_max_mV"
CONF_AUTO_BALANCING_PERIOD_MS = "auto_balancing_period_ms"
CONF_BALANCING_PERIODS_PER_MV = "balancing_periods_per_mV"
CONF_BALANCE_MIN_OFFSET_MV = "balance_min_offset_mV"
CONF_MINIMUM_BALANCING_VOLTAGE_MV = "minimum_balancing_voltage_mV"
CONF_LAST_EVENT = "last_event"
CONF_HIGHEST_EVENT_LEVEL = "highest_event_level"
#CONF_EVENT_HISTORY = "event_history"
#CONF_BALANCING_CELLS = "balancing_cells"
CONF_CHARGE_CURRENT_LIMIT = "charge_current_limit"
CONF_DISCHARGE_CURRENT_LIMIT = "discharge_current_limit"
CONF_TEMP_CHARGE_CURRENT_LIMIT = "temp_charge_current_limit"
CONF_TEMP_DISCHARGE_CURRENT_LIMIT = "temp_discharge_current_limit"
CONF_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT = "cell_voltage_charge_current_limit"
CONF_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT = "cell_voltage_discharge_current_limit"
CONF_WORKING_CHARGE_CURRENT_LIMIT = "working_charge_current_limit"
CONF_WORKING_DISCHARGE_CURRENT_LIMIT = "working_discharge_current_limit"
CONF_DELTA_CHARGE_CURRENT_LIMIT = "delta_charge_current_limit"
CONF_HARD_CHARGE_CURRENT_LIMIT = "hard_charge_current_limit"
CONF_HARD_DISCHARGE_CURRENT_LIMIT = "hard_discharge_current_limit"
CONF_INVERTER_SOC = "inverter_soc"
CONF_PACK_POS_TO_CHASSIS = "pack_pos_to_chassis"
CONF_PACK_NEG_TO_CHASSIS = "pack_neg_to_chassis"
CONF_ISOLATION_CHECK_AGE = "isolation_check_age"
CONF_BOARD_STRAP_COUNTS = "board_strap_counts"
CONF_BMS_CHIP_TEMPERATURE = "bms_chip_temperature"
CONF_EKF_CAPACITY = "ekf_capacity"
CONF_SOC_FANCY_COUNT = "soc_fancy_count"
CONF_OVERCURRENT_CHARGE_BUFFER = "overcurrent_charge_buffer"
CONF_OVERCURRENT_DISCHARGE_BUFFER = "overcurrent_discharge_buffer"
CONF_SOFT_CHARGE_BUFFER = "soft_charge_buffer"
CONF_BALANCING_STATE = "balancing_state"
CONF_CALIBRATION_STATE = "calibration_state"
# The BMS's serial and CAN counters since it started (registers 101-108)
COUNTERS = {
    "uart0_packets_received": 101,
    "uart0_packets_sent": 102,
    "uart0_crc_errors": 103,
    "uart1_packets_received": 104,
    "uart1_packets_sent": 105,
    "uart1_crc_errors": 106,
    "can_frames_sent": 107,
    "can_frames_received": 108,
}
CONF_SHUNT_DIE_TEMPERATURE = "shunt_die_temperature"
CONF_SHUNT_NTC_TEMPERATURE = "shunt_ntc_temperature"
CONF_ISOLATION_RESISTANCE = "isolation_resistance"
CONF_INVERTER_FULL_CAPACITY = "inverter_full_capacity"
CONF_INVERTER_REMAINING_CAPACITY = "inverter_remaining_capacity"
CONF_INVERTER_MIN_VOLTAGE_LIMIT = "inverter_min_voltage_limit"
CONF_INVERTER_MAX_VOLTAGE_LIMIT = "inverter_max_voltage_limit"
CONF_MODULE_TEMPERATURES = "module_temperatures"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_HMI_BMS_ID): cv.use_id(HMIBMS),
    cv.Optional(CONF_MILLIS): sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    #cv.Optional(CONF_SERIAL): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_SYSTEM_REQUEST): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_SOC): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CURRENT_MA): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CHARGE_RAW): sensor.sensor_schema(
        unit_of_measurement="Ah",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BATTERY_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OUTPUT_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_POS_CONTACTOR_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_NEG_CONTACTOR_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMPERATURE_MIN_DC): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMPERATURE_MAX_DC): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMPERATURE_DELTA): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_MIN_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_MAX_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_DELTA): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_COUNT): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGES): cv.ensure_list(sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )),
    cv.Optional(CONF_BALANCING_TIMES): cv.ensure_list(sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )),
    cv.Optional(CONF_SYSTEM_SM): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_CONTACTOR_SM): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_SOC_VOLTAGE_BASED): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SOC_BASIC_COUNT): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CAPACITY_MC): sensor.sensor_schema(
        unit_of_measurement="Ah",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SUPPLY_VOLTAGE_3V3_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SUPPLY_VOLTAGE_5V_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SUPPLY_VOLTAGE_12V_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SUPPLY_VOLTAGE_CONTACTOR_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_PACK_POS_TO_CHASSIS): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_PACK_NEG_TO_CHASSIS): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BOARD_STRAP_COUNTS): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        for key in (CONF_BMS_CHIP_TEMPERATURE, CONF_SHUNT_DIE_TEMPERATURE, CONF_SHUNT_NTC_TEMPERATURE)
    },
    cv.Optional(CONF_EKF_CAPACITY): sensor.sensor_schema(
        unit_of_measurement="Ah",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SOC_FANCY_COUNT): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        for key in (CONF_OVERCURRENT_CHARGE_BUFFER, CONF_OVERCURRENT_DISCHARGE_BUFFER, CONF_SOFT_CHARGE_BUFFER)
    },
    **{
        cv.Optional(key): sensor.sensor_schema(accuracy_decimals=0)
        for key in (CONF_BALANCING_STATE, CONF_CALIBRATION_STATE)
    },
    **{
        cv.Optional(key): sensor.sensor_schema(accuracy_decimals=0, state_class="total_increasing")
        for key in COUNTERS
    },
    cv.Optional(CONF_ISOLATION_CHECK_AGE): sensor.sensor_schema(
        unit_of_measurement="s",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ISOLATION_RESISTANCE): sensor.sensor_schema(
        unit_of_measurement="Ω",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SOC_SCALING_MIN): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SOC_SCALING_MAX): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_VOLTAGE_LIMIT_OFFSET_LOWER): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_VOLTAGE_LIMIT_OFFSET_UPPER): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_SOFT_MIN_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_SOFT_MAX_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_AUTO_BALANCING_PERIOD_MS): sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BALANCING_PERIODS_PER_MV): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BALANCE_MIN_OFFSET_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MINIMUM_BALANCING_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_LAST_EVENT): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_HIGHEST_EVENT_LEVEL): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    #cv.Optional(CONF_EVENT_HISTORY): text_sensor.text_sensor_schema(),
    #cv.Optional(CONF_BALANCING_CELLS): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_DISCHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMP_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMP_DISCHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_WORKING_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_WORKING_DISCHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_DELTA_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_HARD_CHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_HARD_DISCHARGE_CURRENT_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INVERTER_SOC): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INVERTER_FULL_CAPACITY): sensor.sensor_schema(
        unit_of_measurement="Ah",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INVERTER_REMAINING_CAPACITY): sensor.sensor_schema(
        unit_of_measurement="Ah",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INVERTER_MIN_VOLTAGE_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_INVERTER_MAX_VOLTAGE_LIMIT): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MODULE_TEMPERATURES): cv.ensure_list(sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )),
})

# How each register is decoded (low nibble = size in bytes), as in hmi_protocol.h
T_UINT8 = 0x11
T_UINT16 = 0x12
T_INT16 = 0x22
T_UINT32 = 0x14
T_INT32 = 0x24
T_UINT64 = 0x18
T_INT64 = 0x28
T_FLOAT = 0x34

# Poll groups (hmi_bms.h PollGroup): FAST every poll, the others once per 10-poll cycle
POLL_FAST = 0
POLL_SLOW = 1
POLL_MODULE_TEMPS = 2
POLL_LIMITS = 3
POLL_SETTINGS = 4

MC_TO_AH = 1.0 / 3600000.0

# Sensors read straight from one BMS register: key -> (register, type, scale, poll group).
# Entity value = raw value * scale; units and filters come from the schema and the YAML.
REGISTER_SENSORS = {
    CONF_MILLIS: (2, T_UINT64, 1.0, POLL_SLOW),
    CONF_SOC: (3, T_UINT16, 0.01, POLL_SLOW),
    CONF_CURRENT_MA: (4, T_INT32, 1.0, POLL_FAST),
    CONF_CHARGE_RAW: (5, T_INT64, MC_TO_AH, POLL_SLOW),
    CONF_BATTERY_VOLTAGE_MV: (6, T_INT32, 1.0, POLL_FAST),
    CONF_OUTPUT_VOLTAGE_MV: (7, T_INT32, 1.0, POLL_FAST),
    CONF_POS_CONTACTOR_VOLTAGE_MV: (8, T_INT32, 1.0, POLL_SLOW),
    CONF_NEG_CONTACTOR_VOLTAGE_MV: (9, T_INT32, 1.0, POLL_SLOW),
    CONF_TEMPERATURE_MIN_DC: (10, T_FLOAT, 1.0, POLL_SLOW),
    CONF_TEMPERATURE_MAX_DC: (11, T_FLOAT, 1.0, POLL_SLOW),
    CONF_CELL_VOLTAGE_MIN_MV: (12, T_INT16, 1.0, POLL_SLOW),
    CONF_CELL_VOLTAGE_MAX_MV: (13, T_INT16, 1.0, POLL_SLOW),
    CONF_SYSTEM_REQUEST: (14, T_UINT8, 1.0, POLL_SLOW),
    CONF_SYSTEM_SM: (15, T_UINT8, 1.0, POLL_SLOW),
    CONF_CONTACTOR_SM: (16, T_UINT8, 1.0, POLL_FAST),
    CONF_SOC_VOLTAGE_BASED: (17, T_UINT16, 0.01, POLL_SLOW),
    CONF_SOC_BASIC_COUNT: (18, T_UINT16, 0.01, POLL_SLOW),
    CONF_CAPACITY_MC: (20, T_UINT32, MC_TO_AH, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_3V3_MV: (21, T_UINT16, 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_5V_MV: (22, T_UINT16, 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_12V_MV: (23, T_UINT16, 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_CONTACTOR_MV: (24, T_UINT16, 1.0, POLL_SLOW),
    # Settings as read-only sensors (the number platform has the editable ones)
    CONF_SOC_SCALING_MIN: (27, T_INT16, 0.01, POLL_SETTINGS),
    CONF_SOC_SCALING_MAX: (28, T_INT16, 0.01, POLL_SETTINGS),
    CONF_VOLTAGE_LIMIT_OFFSET_LOWER: (29, T_INT16, 1.0, POLL_SETTINGS),
    CONF_VOLTAGE_LIMIT_OFFSET_UPPER: (30, T_INT16, 1.0, POLL_SETTINGS),
    CONF_CELL_VOLTAGE_SOFT_MIN_MV: (31, T_UINT16, 1.0, POLL_SETTINGS),
    CONF_CELL_VOLTAGE_SOFT_MAX_MV: (32, T_UINT16, 1.0, POLL_SETTINGS),
    CONF_AUTO_BALANCING_PERIOD_MS: (33, T_UINT32, 1.0, POLL_SETTINGS),
    CONF_BALANCING_PERIODS_PER_MV: (34, T_UINT16, 1.0, POLL_SETTINGS),
    CONF_BALANCE_MIN_OFFSET_MV: (35, T_UINT16, 1.0, POLL_SETTINGS),
    CONF_MINIMUM_BALANCING_VOLTAGE_MV: (36, T_UINT16, 1.0, POLL_SETTINGS),
    CONF_INVERTER_SOC: (37, T_INT16, 0.01, POLL_SLOW),
    CONF_INVERTER_FULL_CAPACITY: (38, T_INT16, 0.1, POLL_SLOW),
    CONF_INVERTER_REMAINING_CAPACITY: (39, T_INT16, 0.1, POLL_SLOW),
    CONF_INVERTER_MIN_VOLTAGE_LIMIT: (40, T_INT16, 0.1, POLL_SLOW),
    CONF_INVERTER_MAX_VOLTAGE_LIMIT: (41, T_INT16, 0.1, POLL_SLOW),
    # Current limits in 0.1 A; 6553.5 A = no limit
    CONF_CHARGE_CURRENT_LIMIT: (45, T_UINT16, 0.1, POLL_LIMITS),
    CONF_DISCHARGE_CURRENT_LIMIT: (46, T_UINT16, 0.1, POLL_LIMITS),
    CONF_TEMP_CHARGE_CURRENT_LIMIT: (47, T_UINT16, 0.1, POLL_LIMITS),
    CONF_TEMP_DISCHARGE_CURRENT_LIMIT: (48, T_UINT16, 0.1, POLL_LIMITS),
    CONF_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT: (49, T_UINT16, 0.1, POLL_LIMITS),
    CONF_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT: (50, T_UINT16, 0.1, POLL_LIMITS),
    CONF_WORKING_CHARGE_CURRENT_LIMIT: (51, T_UINT16, 0.1, POLL_LIMITS),
    CONF_WORKING_DISCHARGE_CURRENT_LIMIT: (52, T_UINT16, 0.1, POLL_LIMITS),
    CONF_DELTA_CHARGE_CURRENT_LIMIT: (53, T_UINT16, 0.1, POLL_LIMITS),
    CONF_HARD_CHARGE_CURRENT_LIMIT: (54, T_UINT16, 0.1, POLL_LIMITS),
    CONF_HARD_DISCHARGE_CURRENT_LIMIT: (55, T_UINT16, 0.1, POLL_LIMITS),
    CONF_SHUNT_DIE_TEMPERATURE: (57, T_FLOAT, 1.0, POLL_SLOW),
    CONF_SHUNT_NTC_TEMPERATURE: (58, T_FLOAT, 1.0, POLL_SLOW),
    # Isolation monitor, calculated on the BMS (registers 62-65)
    CONF_ISOLATION_RESISTANCE: (62, T_FLOAT, 1.0, POLL_SLOW),
    CONF_PACK_POS_TO_CHASSIS: (63, T_FLOAT, 1.0, POLL_SLOW),
    CONF_PACK_NEG_TO_CHASSIS: (64, T_FLOAT, 1.0, POLL_SLOW),
    CONF_ISOLATION_CHECK_AGE: (65, T_FLOAT, 1.0, POLL_SLOW),
    CONF_BOARD_STRAP_COUNTS: (67, T_FLOAT, 1.0, POLL_SLOW),
    CONF_BMS_CHIP_TEMPERATURE: (70, T_FLOAT, 1.0, POLL_SLOW),
    # Health and diagnostics (registers 96-110; older BMS firmware doesn't answer)
    CONF_EKF_CAPACITY: (96, T_FLOAT, 1.0, POLL_SLOW),
    CONF_SOC_FANCY_COUNT: (97, T_UINT16, 0.01, POLL_SLOW),
    CONF_OVERCURRENT_CHARGE_BUFFER: (98, T_FLOAT, 1.0, POLL_SLOW),
    CONF_OVERCURRENT_DISCHARGE_BUFFER: (99, T_FLOAT, 1.0, POLL_SLOW),
    CONF_SOFT_CHARGE_BUFFER: (100, T_FLOAT, 1.0, POLL_SLOW),
    **{key: (reg, T_UINT32, 1.0, POLL_SLOW) for key, reg in COUNTERS.items()},
    CONF_BALANCING_STATE: (109, T_UINT8, 1.0, POLL_SLOW),
    CONF_CALIBRATION_STATE: (110, T_UINT8, 1.0, POLL_SLOW),
}
REG_MODULE_TEMPS_START = 0x200  # float degC, one per module (BMB)

# Calculated in the component rather than read: key -> setter
DERIVED_SENSORS = {
    CONF_TEMPERATURE_DELTA: "set_temperature_delta_sensor",
    CONF_CELL_VOLTAGE_DELTA: "set_cell_voltage_delta_sensor",
    CONF_CELL_COUNT: "set_cell_count_sensor",
    CONF_LAST_EVENT: "set_last_event_sensor",
    CONF_HIGHEST_EVENT_LEVEL: "set_highest_event_level_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HMI_BMS_ID])

    for key, (reg, type_, scale, group) in REGISTER_SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(parent.add_register_sensor(reg, type_, scale, group, sens))
    for key, setter in DERIVED_SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(parent, setter)(sens))

    if CONF_CELL_VOLTAGES in config:
        for conf in config[CONF_CELL_VOLTAGES]:
            sens = await sensor.new_sensor(conf)
            cg.add(parent.add_cell_voltage_sensor(sens))
    if CONF_BALANCING_TIMES in config:
        for conf in config[CONF_BALANCING_TIMES]:
            sens = await sensor.new_sensor(conf)
            cg.add(parent.add_balancing_time_sensor(sens))
    if CONF_MODULE_TEMPERATURES in config:
        for i, conf in enumerate(config[CONF_MODULE_TEMPERATURES]):
            sens = await sensor.new_sensor(conf)
            cg.add(parent.add_register_sensor(REG_MODULE_TEMPS_START + i, T_FLOAT, 1.0, POLL_MODULE_TEMPS, sens))
