"""The LCD UI without LVGL: draws the pages itself and puts them on the display.

  hmi_ui:
    display_id: my_display
    touchscreen_id: my_touchscreen
    pin_code: !secret hmi_pin
    fill: !lambda |-          # every frame: put the present values into the model `m`
      m.soc = id(soc).state;
    on_command:               # what the UI asks for, each with what to do about it
      - lambda: if (command == hmiui::Command::STOP) id(btn_web_stop).press();
    on_write_setting: ...     # (index, value)
    on_settings_unlocked: ... # (unlocked)
    on_brightness: ...        # (percent)
    on_log_enabled: ...       # (on)
    on_wifi_join: ...         # (ssid, password)
    on_idle: ...              # untouched for five minutes: dim
    on_wake: ...              # touched again
    screenshot: true          # bench only: GET /screenshot.raw[?page=N], for tools/screenshot.py

The pages, the widgets and what the model holds are plain C++ in this folder (README.md); this
file and hmi_ui.* are the only part that knows ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import display, touchscreen, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_DISPLAY_ID, CONF_ID

DEPENDENCIES = ["display", "touchscreen", "web_server_base"]

CONF_TOUCHSCREEN_ID = "touchscreen_id"
CONF_PIN_CODE = "pin_code"
CONF_FILL = "fill"
CONF_SCREENSHOT = "screenshot"

hmi_ui_ns = cg.esphome_ns.namespace("hmi_ui")
HmiUi = hmi_ui_ns.class_("HmiUi", cg.Component)
hmiui_ns = cg.global_ns.namespace("hmiui")
Model = hmiui_ns.struct("Model")
Command = hmiui_ns.enum("Command", is_class=True)

# config key, the trigger's getter, the arguments its automation gets
AUTOMATIONS = [
    ("on_command", "get_command_trigger", [(Command, "command")]),
    ("on_write_setting", "get_setting_trigger", [(cg.int_, "index"), (cg.float_, "value")]),
    ("on_settings_unlocked", "get_unlock_trigger", [(cg.bool_, "unlocked")]),
    ("on_brightness", "get_brightness_trigger", [(cg.int_, "percent")]),
    ("on_log_enabled", "get_log_trigger", [(cg.bool_, "on")]),
    ("on_wifi_join", "get_wifi_trigger", [(cg.std_string, "ssid"), (cg.std_string, "password")]),
    ("on_idle", "get_idle_trigger", []),
    ("on_wake", "get_wake_trigger", []),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HmiUi),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        cv.Required(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(touchscreen.Touchscreen),
        cv.Required(CONF_PIN_CODE): cv.string_strict,
        cv.Required(CONF_FILL): cv.lambda_,
        cv.Optional(CONF_SCREENSHOT, default=False): cv.boolean,
        **{cv.Optional(key): automation.validate_automation(single=True) for key, _, _ in AUTOMATIONS},
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_display(await cg.get_variable(config[CONF_DISPLAY_ID])))
    cg.add(var.set_touchscreen(await cg.get_variable(config[CONF_TOUCHSCREEN_ID])))
    cg.add(var.set_web_server_base(await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])))
    cg.add(var.set_screenshot(config[CONF_SCREENSHOT]))
    cg.add(var.set_pin(config[CONF_PIN_CODE]))
    fill = await cg.process_lambda(config[CONF_FILL], [(Model.operator("ref"), "m")], return_type=cg.void)
    cg.add(var.set_fill(fill))
    for key, getter, args in AUTOMATIONS:
        if key in config:
            await automation.build_automation(getattr(var, getter)(), args, config[key])
