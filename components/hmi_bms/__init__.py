import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, __version__
from esphome.core import CORE

DEPENDENCIES = ["uart"]

_LOGGER = logging.getLogger(__name__)
BOOT_HEAP_LD = "boot_heap.ld"

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


def check_boot_heap_at_link():
    """Make the link fail when a build leaves under 110,000 bytes for the P4's start-up heap.

    boot_heap.ld says why. Only where it was measured: pre-v3 ESP32-P4 (engineering_sample),
    ESPHome 2026.10 or later (2026.7 builds boot with less), ESP-IDF built directly (not through
    PlatformIO). Elsewhere tools/boot_heap.py still reads the number from the ELF."""
    try:
        if tuple(int(x) for x in __version__.split(".")[:2]) < (2026, 10):
            return
    except ValueError:
        return
    if not CORE.is_esp32 or not getattr(CORE, "using_toolchain_esp_idf", False):
        return
    from esphome.components import esp32

    if esp32.get_esp32_variant() != esp32.VARIANT_ESP32P4:
        return
    if not CORE.config.get("esp32", {}).get("engineering_sample"):
        return
    ld = CORE.relative_build_path(BOOT_HEAP_LD)
    if any(c.isspace() for c in str(ld)):
        _LOGGER.warning("Build path has a space: start-up RAM not checked at link; "
                        "run tools/boot_heap.py on the ELF before flashing")
        return
    esp32.add_extra_build_file(BOOT_HEAP_LD, Path(__file__).parent / BOOT_HEAP_LD)
    cg.add_build_flag(f"-Wl,-T,{ld}")


async def to_code(config):
    check_boot_heap_at_link()

    # Allocate the component at setup time instead of in static storage
    # (what cg.new_Pvariable does). Static data sits in the small block of
    # internal RAM that is all the heap there is until FreeRTOS starts; the
    # Wi-Fi co-processor driver and the system tasks fill the rest of it. With
    # this object there, about 2 KB more state made the P4 assert in
    # vApplicationGetIdleTaskMemory at every boot (desk display, 2026-11-20).
    # The link now fails below 110,000 bytes there (boot_heap.ld); tools/boot_heap.py
    # prints the number for a build.
    var = cg.Pvariable(config[CONF_ID], cg.RawExpression(f"new {config[CONF_ID].type}()"))
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_bypass_crc(config[CONF_BYPASS_CRC]))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_dump_raw(config[CONF_DUMP_RAW]))
    cg.add(var.set_disable_usb_pullups(config[CONF_DISABLE_USB_PULLUPS]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    if config[CONF_LOG_SYNC_PREFIX]:
        cg.add(var.set_log_sync_prefix(config[CONF_LOG_SYNC_PREFIX]))
