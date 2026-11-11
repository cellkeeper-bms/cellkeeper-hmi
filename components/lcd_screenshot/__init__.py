"""A development aid: serves the LCD's frame buffer over the web server, so the screens can be
captured exactly (tools/screenshot.py turns it into PNG files).

  lcd_screenshot:
    display_id: my_display
    lvgl_id: my_lvgl

GET /screenshot.raw          the frame buffer as it is now (width x height, RGB565 little-endian)
GET /screenshot.raw?page=N   shows LVGL page N first (the order the pages are declared in)

It uses the web server's authentication. Leave it out of site builds: it adds nothing to Home
Assistant and is only wanted on a bench."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_DISPLAY_ID, CONF_ID

DEPENDENCIES = ["web_server_base", "display", "lvgl"]

CONF_LVGL_ID = "lvgl_id"

lcd_screenshot_ns = cg.esphome_ns.namespace("lcd_screenshot")
LcdScreenshot = lcd_screenshot_ns.class_("LcdScreenshot", cg.Component)
MipiDsi = cg.esphome_ns.namespace("mipi_dsi").class_("MipiDsi")
LvglComponent = cg.esphome_ns.namespace("lvgl").class_("LvglComponent")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LcdScreenshot),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        cv.Required(CONF_DISPLAY_ID): cv.use_id(MipiDsi),
        cv.Required(CONF_LVGL_ID): cv.use_id(LvglComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_web_server_base(await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])))
    cg.add(var.set_display(await cg.get_variable(config[CONF_DISPLAY_ID])))
    cg.add(var.set_lvgl(await cg.get_variable(config[CONF_LVGL_ID])))
