import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
)

from .. import CONF_LD2410D_ID, LD2410DComponent

DEPENDENCIES = ["ld2410d"]

CONF_HAS_TARGET = "has_target"
CONF_MOVING_TARGET = "moving_target"
CONF_STILL_TARGET = "still_target"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY
        ),
        cv.Optional(CONF_MOVING_TARGET): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOTION
        ),
        cv.Optional(CONF_STILL_TARGET): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    if CONF_HAS_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_TARGET])
        cg.add(parent.set_has_target_binary_sensor(sens))
    if CONF_MOVING_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_MOVING_TARGET])
        cg.add(parent.set_moving_target_binary_sensor(sens))
    if CONF_STILL_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_STILL_TARGET])
        cg.add(parent.set_still_target_binary_sensor(sens))
