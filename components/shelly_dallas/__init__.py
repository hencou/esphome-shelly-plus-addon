import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

CODEOWNERS = ["@local"]
MULTI_CONF = True

shelly_dallas_ns = cg.esphome_ns.namespace("shelly_dallas")
ShellyDallasComponent = shelly_dallas_ns.class_("ShellyDallasComponent", cg.PollingComponent)

CONF_PIN_TX = "pin_tx"
CONF_PIN_RX = "pin_rx"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasComponent),
        cv.Required(CONF_PIN_TX): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_RX): pins.internal_gpio_input_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_tx = await cg.gpio_pin_expression(config[CONF_PIN_TX])
    cg.add(var.set_pin_tx(pin_tx))

    pin_rx = await cg.gpio_pin_expression(config[CONF_PIN_RX])
    cg.add(var.set_pin_rx(pin_rx))
