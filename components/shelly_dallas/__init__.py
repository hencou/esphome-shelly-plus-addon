"""
Shelly Plus Add-On 1-Wire Component for ESPHome

This component provides support for DS18B20 temperature sensors connected
to the Shelly Plus Add-On, which uses a dual-GPIO 1-Wire interface with
galvanic isolation (separate TX and RX pins).

Supported devices:
- Shelly 2PM Gen3: TX=GPIO9, RX=GPIO21
- Shelly 1PM Gen3: TX=GPIO9, RX=GPIO21
- Shelly Plus 1/1PM: TX=GPIO0, RX=GPIO1
- Shelly Plus 2PM: TX=GPIO0, RX=GPIO1

GitHub: https://github.com/majoto85/esphome-shelly-plus-addon
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

CODEOWNERS = ["@majoto85"]
MULTI_CONF = True

shelly_dallas_ns = cg.esphome_ns.namespace("shelly_dallas")
ShellyDallasComponent = shelly_dallas_ns.class_(
    "ShellyDallasComponent", cg.PollingComponent
)

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
