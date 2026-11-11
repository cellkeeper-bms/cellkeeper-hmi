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
from .hmi_map import REGISTERS

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
CONF_SHUNT_DIE_TEMPERATURE = "shunt_die_temperature"
CONF_SHUNT_NTC_TEMPERATURE = "shunt_ntc_temperature"
CONF_ISOLATION_RESISTANCE = "isolation_resistance"
CONF_INVERTER_FULL_CAPACITY = "inverter_full_capacity"
CONF_INVERTER_REMAINING_CAPACITY = "inverter_remaining_capacity"
CONF_INVERTER_MIN_VOLTAGE_LIMIT = "inverter_min_voltage_limit"
CONF_INVERTER_MAX_VOLTAGE_LIMIT = "inverter_max_voltage_limit"
CONF_MODULE_TEMPERATURES = "module_temperatures"
CONF_LINK_VOLTAGE_MV = "link_voltage_mV"
CONF_NAMEPLATE_CAPACITY = "nameplate_capacity"
CONF_LEARNED_CAPACITY = "learned_capacity"
CONF_LEARNED_CAPACITY_SD = "learned_capacity_sd"
CONF_LEARNED_CAPACITY_STATE = "learned_capacity_state"
CONF_LIFETIME_CHARGED = "lifetime_charged"
CONF_LIFETIME_DISCHARGED = "lifetime_discharged"
CONF_LIFETIME_ENERGY_CHARGED = "lifetime_energy_charged"
CONF_LIFETIME_ENERGY_DISCHARGED = "lifetime_energy_discharged"
CONF_SUPERVISOR_CELL_MIN = "supervisor_cell_min"
CONF_SUPERVISOR_CELL_MAX = "supervisor_cell_max"
CONF_SUPERVISOR_CELLS_IN_RANGE = "supervisor_cells_in_range"
CONF_SUPERVISOR_SCANS = "supervisor_scans"
CONF_SUPERVISOR_SCAN_AGE = "supervisor_scan_age"
CONF_OVERCURRENT_CHARGE_BUFFER = "overcurrent_charge_buffer"
CONF_OVERCURRENT_DISCHARGE_BUFFER = "overcurrent_discharge_buffer"
CONF_SOFT_CHARGE_BUFFER = "soft_charge_buffer"
CONF_BALANCING_STATE = "balancing_state"
CONF_CALIBRATION_STATE = "calibration_state"
# The BMS's serial and CAN counters since it started: key -> register name
COUNTERS = {
    "uart0_packets_received": "UART0_PACKETS_RECEIVED",
    "uart0_packets_sent": "UART0_PACKETS_SENT",
    "uart0_crc_errors": "UART0_CRC_ERRORS",
    "uart1_packets_received": "UART1_PACKETS_RECEIVED",
    "uart1_packets_sent": "UART1_PACKETS_SENT",
    "uart1_crc_errors": "UART1_CRC_ERRORS",
    "can_frames_sent": "CAN_FRAMES_SENT",
    "can_frames_received": "CAN_FRAMES_RECEIVED",
}

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
    cv.Optional(CONF_LINK_VOLTAGE_MV): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # Capacity: the label's (nameplate), and what the OCV learner has measured with its
    # 1-sigma uncertainty and state (0 not learning, 1 no data, 2 preliminary, 3 good)
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement="Ah",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        for key in (CONF_NAMEPLATE_CAPACITY, CONF_LEARNED_CAPACITY, CONF_LEARNED_CAPACITY_SD)
    },
    cv.Optional(CONF_LEARNED_CAPACITY_STATE): sensor.sensor_schema(accuracy_decimals=0),
    # Lifetime throughput, kept by the BMS over restarts
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement="Ah",
            accuracy_decimals=1,
            state_class="total_increasing",
        )
        for key in (CONF_LIFETIME_CHARGED, CONF_LIFETIME_DISCHARGED)
    },
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement="kWh",
            accuracy_decimals=3,
            device_class="energy",
            state_class="total_increasing",
        )
        for key in (CONF_LIFETIME_ENERGY_CHARGED, CONF_LIFETIME_ENERGY_DISCHARGED)
    },
    # The supervisor chip's own view (it reads the cell boards itself)
    **{
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        for key in (CONF_SUPERVISOR_CELL_MIN, CONF_SUPERVISOR_CELL_MAX)
    },
    cv.Optional(CONF_SUPERVISOR_CELLS_IN_RANGE): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SUPERVISOR_SCANS): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class="total_increasing",
    ),
    cv.Optional(CONF_SUPERVISOR_SCAN_AGE): sensor.sensor_schema(
        unit_of_measurement="s",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
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

# Poll groups (hmi_bms.h PollGroup): FAST every poll, the others once per 10-poll cycle
POLL_FAST = 0
POLL_SLOW = 1
POLL_LIMITS = 3
POLL_SETTINGS = 4
POLL_NEVER = 5  # a command: published when it is sent, never read
MC_TO_AH = 1.0 / 3600000.0

# Sensors read straight from one BMS register: key -> (register name, scale, poll group).
# The register's ID and wire type come from hmi_map.py (generated from the BMS firmware).
# Entity value = raw value * scale; units and filters come from the schema and the YAML.
REGISTER_SENSORS = {
    CONF_MILLIS: ("MILLIS", 1.0, POLL_SLOW),
    CONF_SOC: ("SOC", 0.01, POLL_SLOW),
    CONF_CURRENT_MA: ("CURRENT", 1.0, POLL_FAST),
    CONF_CHARGE_RAW: ("CHARGE", MC_TO_AH, POLL_SLOW),
    CONF_BATTERY_VOLTAGE_MV: ("BATTERY_VOLTAGE", 1.0, POLL_FAST),
    CONF_OUTPUT_VOLTAGE_MV: ("OUTPUT_VOLTAGE", 1.0, POLL_FAST),
    CONF_POS_CONTACTOR_VOLTAGE_MV: ("POS_CONTACTOR_VOLTAGE", 1.0, POLL_SLOW),
    CONF_NEG_CONTACTOR_VOLTAGE_MV: ("NEG_CONTACTOR_VOLTAGE", 1.0, POLL_SLOW),
    CONF_TEMPERATURE_MIN_DC: ("TEMPERATURE_MIN", 1.0, POLL_SLOW),
    CONF_TEMPERATURE_MAX_DC: ("TEMPERATURE_MAX", 1.0, POLL_SLOW),
    CONF_CELL_VOLTAGE_MIN_MV: ("CELL_VOLTAGE_MIN", 1.0, POLL_SLOW),
    CONF_CELL_VOLTAGE_MAX_MV: ("CELL_VOLTAGE_MAX", 1.0, POLL_SLOW),
    CONF_SYSTEM_REQUEST: ("SYSTEM_REQUEST", 1.0, POLL_NEVER),
    CONF_SYSTEM_SM: ("SYSTEM_STATE", 1.0, POLL_SLOW),
    CONF_CONTACTOR_SM: ("CONTACTORS_STATE", 1.0, POLL_FAST),
    CONF_SOC_VOLTAGE_BASED: ("SOC_VOLTAGE_BASED", 0.01, POLL_SLOW),
    CONF_SOC_BASIC_COUNT: ("SOC_BASIC_COUNT", 0.01, POLL_SLOW),
    CONF_CAPACITY_MC: ("CAPACITY", MC_TO_AH, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_3V3_MV: ("SUPPLY_VOLTAGE_3V3", 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_5V_MV: ("SUPPLY_VOLTAGE_5V", 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_12V_MV: ("SUPPLY_VOLTAGE_12V", 1.0, POLL_SLOW),
    CONF_SUPPLY_VOLTAGE_CONTACTOR_MV: ("SUPPLY_VOLTAGE_CTR", 1.0, POLL_SLOW),
    # Settings as read-only sensors (the number platform has the editable ones)
    CONF_SOC_SCALING_MIN: ("SOC_SCALING_MIN", 0.01, POLL_SETTINGS),
    CONF_SOC_SCALING_MAX: ("SOC_SCALING_MAX", 0.01, POLL_SETTINGS),
    CONF_VOLTAGE_LIMIT_OFFSET_LOWER: ("VOLTAGE_LIMIT_OFFSET_LOWER", 1.0, POLL_SETTINGS),
    CONF_VOLTAGE_LIMIT_OFFSET_UPPER: ("VOLTAGE_LIMIT_OFFSET_UPPER", 1.0, POLL_SETTINGS),
    CONF_CELL_VOLTAGE_SOFT_MIN_MV: ("CELL_VOLTAGE_SOFT_MIN", 1.0, POLL_SETTINGS),
    CONF_CELL_VOLTAGE_SOFT_MAX_MV: ("CELL_VOLTAGE_SOFT_MAX", 1.0, POLL_SETTINGS),
    CONF_AUTO_BALANCING_PERIOD_MS: ("AUTO_BALANCING_PERIOD_MS", 1.0, POLL_SETTINGS),
    CONF_BALANCING_PERIODS_PER_MV: ("BALANCING_PERIODS_PER_MV", 1.0, POLL_SETTINGS),
    CONF_BALANCE_MIN_OFFSET_MV: ("BALANCE_MIN_OFFSET_MV", 1.0, POLL_SETTINGS),
    CONF_MINIMUM_BALANCING_VOLTAGE_MV: ("MINIMUM_BALANCING_VOLTAGE_MV", 1.0, POLL_SETTINGS),
    CONF_INVERTER_SOC: ("INVERTER_SOC", 0.01, POLL_SLOW),
    CONF_INVERTER_FULL_CAPACITY: ("INVERTER_FULL_CAPACITY_DAH", 0.1, POLL_SLOW),
    CONF_INVERTER_REMAINING_CAPACITY: ("INVERTER_REMAINING_CAPACITY_DAH", 0.1, POLL_SLOW),
    CONF_INVERTER_MIN_VOLTAGE_LIMIT: ("INVERTER_MIN_VOLTAGE_LIMIT_DV", 0.1, POLL_SLOW),
    CONF_INVERTER_MAX_VOLTAGE_LIMIT: ("INVERTER_MAX_VOLTAGE_LIMIT_DV", 0.1, POLL_SLOW),
    # Current limits in 0.1 A; 6553.5 A = no limit
    CONF_CHARGE_CURRENT_LIMIT: ("CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_DISCHARGE_CURRENT_LIMIT: ("DISCHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_TEMP_CHARGE_CURRENT_LIMIT: ("TEMP_CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_TEMP_DISCHARGE_CURRENT_LIMIT: ("TEMP_DISCHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT: ("CELL_VOLTAGE_CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT: ("CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_WORKING_CHARGE_CURRENT_LIMIT: ("WORKING_CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_WORKING_DISCHARGE_CURRENT_LIMIT: ("WORKING_DISCHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_DELTA_CHARGE_CURRENT_LIMIT: ("DELTA_CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_HARD_CHARGE_CURRENT_LIMIT: ("HARD_CHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_HARD_DISCHARGE_CURRENT_LIMIT: ("HARD_DISCHARGE_CURRENT_LIMIT_DA", 0.1, POLL_LIMITS),
    CONF_SHUNT_DIE_TEMPERATURE: ("SHUNT_DIE_TEMP", 1.0, POLL_SLOW),
    CONF_SHUNT_NTC_TEMPERATURE: ("SHUNT_NTC_TEMP", 1.0, POLL_SLOW),
    # Isolation monitor, calculated on the BMS: the resistance is the last reading with the
    # contactors open, and the age is how long ago that was (0 = live)
    CONF_ISOLATION_RESISTANCE: ("ISOLATION_OPEN_RESISTANCE", 1.0, POLL_SLOW),
    CONF_PACK_POS_TO_CHASSIS: ("PACK_POS_TO_CHASSIS", 1.0, POLL_SLOW),
    CONF_PACK_NEG_TO_CHASSIS: ("PACK_NEG_TO_CHASSIS", 1.0, POLL_SLOW),
    CONF_ISOLATION_CHECK_AGE: ("ISOLATION_OPEN_AGE", 1.0, POLL_SLOW),
    CONF_BOARD_STRAP_COUNTS: ("BOARD_STRAP_COUNTS", 1.0, POLL_SLOW),
    CONF_BMS_CHIP_TEMPERATURE: ("MCU_TEMPERATURE", 1.0, POLL_SLOW),
    CONF_LINK_VOLTAGE_MV: ("LINK_VOLTAGE", 1.0, POLL_SLOW),
    CONF_NAMEPLATE_CAPACITY: ("NAMEPLATE_CAPACITY", MC_TO_AH, POLL_SLOW),
    CONF_LEARNED_CAPACITY: ("OCV_LEARN_CAPACITY", 1.0, POLL_SLOW),
    CONF_LEARNED_CAPACITY_SD: ("OCV_LEARN_CAPACITY_SD", 1.0, POLL_SLOW),
    CONF_LEARNED_CAPACITY_STATE: ("OCV_LEARN_CAPACITY_STATE", 1.0, POLL_SLOW),
    # mAh and Wh counters that wrap at 2^32 (4.3 million Ah or kWh)
    CONF_LIFETIME_CHARGED: ("LIFETIME_CHARGED_MAH", 0.001, POLL_SLOW),
    CONF_LIFETIME_DISCHARGED: ("LIFETIME_DISCHARGED_MAH", 0.001, POLL_SLOW),
    CONF_LIFETIME_ENERGY_CHARGED: ("LIFETIME_CHARGED_WH", 0.001, POLL_SLOW),
    CONF_LIFETIME_ENERGY_DISCHARGED: ("LIFETIME_DISCHARGED_WH", 0.001, POLL_SLOW),
    # Health and diagnostics. A BMS firmware without these registers doesn't list them, and
    # the sensors stay unknown.
    CONF_BALANCING_STATE: ("BALANCING_STATE", 1.0, POLL_SLOW),
    CONF_CALIBRATION_STATE: ("CALIBRATION_STATE", 1.0, POLL_SLOW),
    CONF_OVERCURRENT_CHARGE_BUFFER: ("OVERCURRENT_CHARGE_BUFFER", 1.0, POLL_SLOW),
    CONF_OVERCURRENT_DISCHARGE_BUFFER: ("OVERCURRENT_DISCHARGE_BUFFER", 1.0, POLL_SLOW),
    CONF_SOFT_CHARGE_BUFFER: ("SOFT_CHARGE_BUFFER", 1.0, POLL_SLOW),
    **{key: (name, 1.0, POLL_SLOW) for key, name in COUNTERS.items()},
}
# Calculated in the component rather than read: key -> setter
DERIVED_SENSORS = {
    CONF_TEMPERATURE_DELTA: "set_temperature_delta_sensor",
    CONF_CELL_VOLTAGE_DELTA: "set_cell_voltage_delta_sensor",
    CONF_CELL_COUNT: "set_cell_count_sensor",
    CONF_LAST_EVENT: "set_last_event_sensor",
    CONF_HIGHEST_EVENT_LEVEL: "set_highest_event_level_sensor",
    CONF_SUPERVISOR_CELL_MIN: "set_supervisor_cell_min_sensor",
    CONF_SUPERVISOR_CELL_MAX: "set_supervisor_cell_max_sensor",
    CONF_SUPERVISOR_CELLS_IN_RANGE: "set_supervisor_cells_in_range_sensor",
    CONF_SUPERVISOR_SCANS: "set_supervisor_scans_sensor",
    CONF_SUPERVISOR_SCAN_AGE: "set_supervisor_scan_age_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HMI_BMS_ID])

    for key, (name, scale, group) in REGISTER_SENSORS.items():
        if key in config:
            reg, type_, _flags = REGISTERS[name]
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
        for conf in config[CONF_MODULE_TEMPERATURES]:
            sens = await sensor.new_sensor(conf)
            cg.add(parent.add_module_temperature_sensor(sens))
