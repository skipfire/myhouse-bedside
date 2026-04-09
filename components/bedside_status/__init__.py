import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import time as time_comp
from esphome.const import CONF_ID

CONF_TIME_ID = "time_id"

AUTO_LOAD = []
DEPENDENCIES = ["network", "wifi"]

bedside_status_ns = cg.esphome_ns.namespace("bedside_status")
BedsideStatus = bedside_status_ns.class_("BedsideStatus", cg.PollingComponent)

CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_RST_PIN = "rst_pin"
CONF_DC_PIN = "dc_pin"
CONF_CS_PIN = "cs_pin"
CONF_BUSY_PIN = "busy_pin"
CONF_SCREEN_POWER_PIN = "screen_power_pin"
CONF_STATUS_URL = "status_url"
CONF_VERIFY_SSL = "verify_ssl"
CONF_FOOTER_IP_LEFT = "footer_ip_left"
CONF_DISPLAY_STATUS_FILTER = "display_status_filter"
CONF_TEXT_SIZE = "text_size"
CONF_FOOTER_TEXT_SIZE = "footer_text_size"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BedsideStatus),
        cv.Required(CONF_SCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RST_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_SCREEN_POWER_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_STATUS_URL): cv.string,
        cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
        cv.Optional(CONF_FOOTER_IP_LEFT, default=True): cv.boolean,
        cv.Optional(CONF_DISPLAY_STATUS_FILTER, default=3): cv.int_range(min=0, max=255),
        cv.Optional(CONF_TEXT_SIZE, default=24): cv.int_range(min=12, max=48),
        cv.Optional(CONF_FOOTER_TEXT_SIZE, default=16): cv.int_range(min=8, max=48),
        cv.Optional(CONF_TIME_ID): cv.use_id(time_comp.RealTimeClock),
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_pin_sck(await cg.gpio_pin_expression(config[CONF_SCK_PIN])))
    cg.add(var.set_pin_mosi(await cg.gpio_pin_expression(config[CONF_MOSI_PIN])))
    cg.add(var.set_pin_rst(await cg.gpio_pin_expression(config[CONF_RST_PIN])))
    cg.add(var.set_pin_dc(await cg.gpio_pin_expression(config[CONF_DC_PIN])))
    cg.add(var.set_pin_cs(await cg.gpio_pin_expression(config[CONF_CS_PIN])))
    cg.add(var.set_pin_busy(await cg.gpio_pin_expression(config[CONF_BUSY_PIN])))

    if CONF_SCREEN_POWER_PIN in config:
        cg.add(
            var.set_screen_power_pin(
                await cg.gpio_pin_expression(config[CONF_SCREEN_POWER_PIN])
            )
        )

    cg.add(var.set_status_url(config[CONF_STATUS_URL]))
    cg.add(var.set_verify_ssl(config[CONF_VERIFY_SSL]))
    cg.add(var.set_footer_ip_left(config[CONF_FOOTER_IP_LEFT]))
    cg.add(var.set_display_status_filter(config[CONF_DISPLAY_STATUS_FILTER]))
    cg.add(var.set_text_size(config[CONF_TEXT_SIZE]))
    cg.add(var.set_footer_text_size(config[CONF_FOOTER_TEXT_SIZE]))

    if CONF_TIME_ID in config:
        t = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(t))
