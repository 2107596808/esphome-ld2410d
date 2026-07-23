import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_LD2410D_ID, LD2410DComponent, ld2410d_ns

DEPENDENCIES = ["ld2410d"]

EngineeringModeSwitch = ld2410d_ns.class_(
    "EngineeringModeSwitch", switch.Switch
)

CONF_ENGINEERING_MODE = "engineering_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_ENGINEERING_MODE): switch.switch_schema(
            EngineeringModeSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:cog",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    if CONF_ENGINEERING_MODE in config:
        sw = await switch.new_switch(config[CONF_ENGINEERING_MODE])
        await cg.register_parented(sw, parent)
        cg.add(parent.set_engineering_mode_switch(sw))
