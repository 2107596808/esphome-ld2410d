import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_LD2410D_ID, LD2410DComponent, ld2410d_ns

DEPENDENCIES = ["ld2410d"]

QueryParamsButton = ld2410d_ns.class_("QueryParamsButton", button.Button)
GenerateThresholdButton = ld2410d_ns.class_("GenerateThresholdButton", button.Button)
AutoGainButton = ld2410d_ns.class_("AutoGainButton", button.Button)

CONF_QUERY_PARAMS = "query_params"
CONF_GENERATE_THRESHOLD = "generate_threshold"
CONF_AUTO_GAIN = "auto_gain"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
        cv.Optional(CONF_QUERY_PARAMS): button.button_schema(
            QueryParamsButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:database-search",
        ),
        cv.Optional(CONF_GENERATE_THRESHOLD): button.button_schema(
            GenerateThresholdButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:auto-fix",
        ),
        cv.Optional(CONF_AUTO_GAIN): button.button_schema(
            AutoGainButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:tune-vertical",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    for key in (CONF_QUERY_PARAMS, CONF_GENERATE_THRESHOLD, CONF_AUTO_GAIN):
        if key in config:
            b = await button.new_button(config[key])
            await cg.register_parented(b, parent)
