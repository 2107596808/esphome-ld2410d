import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
    UNIT_DECIBEL,
)

from .. import CONF_LD2410D_ID, LD2410DComponent

DEPENDENCIES = ["ld2410d"]

TOTAL_GATES = 16

CONF_DETECTION_DISTANCE = "detection_distance"
CONF_MOVE_ENERGY = "move_energy"
CONF_STILL_ENERGY = "still_energy"


def energy_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_DETECTION_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CENTIMETER,
            device_class=DEVICE_CLASS_DISTANCE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        **{
            cv.Optional(f"g{i}_{CONF_MOVE_ENERGY}"): energy_schema()
            for i in range(TOTAL_GATES)
        },
        **{
            cv.Optional(f"g{i}_{CONF_STILL_ENERGY}"): energy_schema()
            for i in range(TOTAL_GATES)
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    if CONF_DETECTION_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DETECTION_DISTANCE])
        cg.add(parent.set_detection_distance_sensor(sens))
    for i in range(TOTAL_GATES):
        key = f"g{i}_{CONF_MOVE_ENERGY}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(parent.set_move_energy_sensor(i, sens))
        key = f"g{i}_{CONF_STILL_ENERGY}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(parent.set_still_energy_sensor(i, sens))
