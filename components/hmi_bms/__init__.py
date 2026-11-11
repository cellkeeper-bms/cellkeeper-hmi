import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

hmi_bms_ns = cg.esphome_ns.namespace("hmi_bms")
HMIBMS = hmi_bms_ns.class_("HMIBMS", cg.PollingComponent, uart.UARTDevice)

CONF_BYPASS_CRC = "bypass_crc"
CONF_BAUD_RATE = "baud_rate"
CONF_DUMP_RAW = "dump_raw"
CONF_DISABLE_USB_PULLUPS = "disable_usb_pullups"
CONF_ADDRESS = "address"
CONF_LOG_SYNC_PREFIX = "log_sync_prefix"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HMIBMS),
    cv.Optional(CONF_BYPASS_CRC, default=False): cv.boolean,
    cv.Optional(CONF_BAUD_RATE, default=460800): cv.positive_int,
    cv.Optional(CONF_DUMP_RAW, default=False): cv.boolean,
    # ESP32-P4 with the BMS link on its USB pins (GPIO24/25): the USB PHY's pull-up on the
    # receive pin is on at reset and weakens what the BMS and the supervisor send
    cv.Optional(CONF_DISABLE_USB_PULLUPS, default=False): cv.boolean,
    cv.Optional(CONF_ADDRESS, default=0x01): cv.uint8_t,
    cv.Optional(CONF_LOG_SYNC_PREFIX, default=""): cv.string,
}).extend(cv.polling_component_schema("60s")).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_bypass_crc(config[CONF_BYPASS_CRC]))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_dump_raw(config[CONF_DUMP_RAW]))
    cg.add(var.set_disable_usb_pullups(config[CONF_DISABLE_USB_PULLUPS]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    if config[CONF_LOG_SYNC_PREFIX]:
        cg.add(var.set_log_sync_prefix(config[CONF_LOG_SYNC_PREFIX]))
