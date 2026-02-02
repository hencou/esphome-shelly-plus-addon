"""
Shelly Dallas Temperature Sensor for ESPHome

Provides DS18B20/DS18S20/DS1822 temperature sensor support for the
Shelly Plus Add-On's dual-GPIO 1-Wire interface.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_INDEX,
    CONF_RESOLUTION,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from . import shelly_dallas_ns, ShellyDallasComponent

DEPENDENCIES = ["shelly_dallas"]

ShellyDallasTemperatureSensor = shelly_dallas_ns.class_(
    "ShellyDallasTemperatureSensor", sensor.Sensor
)

CONF_SHELLY_DALLAS_ID = "shelly_dallas_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ShellyDallasTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_SHELLY_DALLAS_ID): cv.use_id(ShellyDallasComponent),
            cv.Optional(CONF_ADDRESS): cv.hex_uint64_t,
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12),
        }
    )
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SHELLY_DALLAS_ID])
    var = await sensor.new_sensor(config)

    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    elif CONF_INDEX in config:
        cg.add(var.set_index(config[CONF_INDEX]))

    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_parent(hub))
    cg.add(hub.register_sensor(var))
