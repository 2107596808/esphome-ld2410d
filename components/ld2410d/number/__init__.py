import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_TIMEOUT,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_DECIBEL,
    UNIT_METER,
    UNIT_SECOND,
)

from .. import CONF_LD2410D_ID, LD2410DComponent, ld2410d_ns

DEPENDENCIES = ["ld2410d"]

TOTAL_GATES = 16

MaxDistanceNumber = ld2410d_ns.class_("MaxDistanceNumber", number.Number)
TimeoutNumber = ld2410d_ns.class_("TimeoutNumber", number.Number)
MoveThresholdNumber = ld2410d_ns.class_("MoveThresholdNumber", number.Number)
StillThresholdNumber = ld2410d_ns.class_("StillThresholdNumber", number.Number)
TriggerCoefficientNumber = ld2410d_ns.class_("TriggerCoefficientNumber", number.Number)
HoldCoefficientNumber = ld2410d_ns.class_("HoldCoefficientNumber", number.Number)
MicroCoefficientNumber = ld2410d_ns.class_("MicroCoefficientNumber", number.Number)

CONF_MAX_DISTANCE = "max_distance"
CONF_MOVE_THRESHOLD = "move_threshold"
CONF_STILL_THRESHOLD = "still_threshold"
CONF_TRIGGER_COEFFICIENT = "trigger_coefficient"
CONF_HOLD_COEFFICIENT = "hold_coefficient"
CONF_MICRO_COEFFICIENT = "micro_coefficient"


def coefficient_schema(cls):
    return number.number_schema(
        cls,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:tune",
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_MAX_DISTANCE): number.number_schema(
            MaxDistanceNumber,
            unit_of_measurement=UNIT_METER,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TIMEOUT): number.number_schema(
            TimeoutNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer",
        ),
        cv.Optional(CONF_TRIGGER_COEFFICIENT): coefficient_schema(
            TriggerCoefficientNumber
        ),
        cv.Optional(CONF_HOLD_COEFFICIENT): coefficient_schema(HoldCoefficientNumber),
        cv.Optional(CONF_MICRO_COEFFICIENT): coefficient_schema(MicroCoefficientNumber),
        **{
            cv.Optional(f"g{i}_{CONF_MOVE_THRESHOLD}"): number.number_schema(
                MoveThresholdNumber,
                unit_of_measurement=UNIT_DECIBEL,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:motion-sensor",
            )
            for i in range(TOTAL_GATES)
        },
        **{
            cv.Optional(f"g{i}_{CONF_STILL_THRESHOLD}"): number.number_schema(
                StillThresholdNumber,
                unit_of_measurement=UNIT_DECIBEL,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:motion-sensor",
            )
            for i in range(TOTAL_GATES)
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])

    if CONF_MAX_DISTANCE in config:
        n = await number.new_number(
            config[CONF_MAX_DISTANCE], min_value=0.7, max_value=10.0, step=0.1
        )
        await cg.register_parented(n, parent)
        cg.add(parent.set_max_distance_number(n))

    if CONF_TIMEOUT in config:
        n = await number.new_number(
            config[CONF_TIMEOUT], min_value=0, max_value=65535, step=1
        )
        await cg.register_parented(n, parent)
        cg.add(parent.set_timeout_number(n))

    if CONF_TRIGGER_COEFFICIENT in config:
        n = await number.new_number(
            config[CONF_TRIGGER_COEFFICIENT], min_value=1.0, max_value=20.0, step=0.1
        )
        await cg.register_parented(n, parent)

    if CONF_HOLD_COEFFICIENT in config:
        n = await number.new_number(
            config[CONF_HOLD_COEFFICIENT], min_value=1.0, max_value=20.0, step=0.1
        )
        await cg.register_parented(n, parent)

    if CONF_MICRO_COEFFICIENT in config:
        n = await number.new_number(
            config[CONF_MICRO_COEFFICIENT], min_value=1.0, max_value=20.0, step=0.1
        )
        await cg.register_parented(n, parent)

    for i in range(TOTAL_GATES):
        key = f"g{i}_{CONF_MOVE_THRESHOLD}"
        if key in config:
            n = await number.new_number(
                config[key], i, min_value=0, max_value=95, step=0.01
            )
            await cg.register_parented(n, parent)
            cg.add(parent.set_move_threshold_number(i, n))
        key = f"g{i}_{CONF_STILL_THRESHOLD}"
        if key in config:
            n = await number.new_number(
                config[key], i, min_value=0, max_value=95, step=0.01
            )
            await cg.register_parented(n, parent)
            cg.add(parent.set_still_threshold_number(i, n))
