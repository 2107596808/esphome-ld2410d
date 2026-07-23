import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_LD2410D_ID, LD2410DComponent

DEPENDENCIES = ["ld2410d"]

CONF_VERSION = "version"
CONF_SN = "sn"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:chip",
        ),
        cv.Optional(CONF_SN): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:identifier",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    if CONF_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_VERSION])
        cg.add(parent.set_version_text_sensor(sens))
    if CONF_SN in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SN])
        cg.add(parent.set_sn_text_sensor(sens))
