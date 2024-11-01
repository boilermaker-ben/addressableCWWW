from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import esp32_rmt, light
from esphome.const import (
    CONF_CHIPSET,
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_CHANNELS,
    CONF_RMT_CHANNEL,
    CONF_COLOR_TEMPERATURE, 
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE
)

CODEOWNERS = ["@jesserockz","@ben_pow"]
DEPENDENCIES = ["esp32"]

esp32_rmt_led_strip_channels_ns = cg.esphome_ns.namespace("esp32_rmt_led_strip_channels")
ESP32RMTLEDStripChannelsLightOutput = esp32_rmt_led_strip_channels_ns.class_(
    "ESP32RMTLEDStripChannelsLightOutput", light.AddressableLight
)

rmt_channel_t = cg.global_ns.enum("rmt_channel_t")

@dataclass
class LEDStripTimings:
    bit0_high: int
    bit0_low: int
    bit1_high: int
    bit1_low: int

CHIPSETS = {
    "WS2812": LEDStripTimings(400, 1000, 1000, 400),
    "WS2811": LEDStripTimings(400, 1000, 1000, 400), #NOT SUPPORTED AS INPUT UNTIL esp32_rmt CODE UPDATED
    "SK6812": LEDStripTimings(300, 900, 600, 600),
    "APA106": LEDStripTimings(350, 1360, 1360, 350),
    "SM16703": LEDStripTimings(300, 900, 900, 300),
}

CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"

CONF_COLD_WHITE_COLOR_TEMPERATURE = 'cold_white_color_temperature'
CONF_WARM_WHITE_COLOR_TEMPERATURE = 'warm_white_color_temperature'
CONF_CHANNELS = 'channels'

DEFAULT_COLD_WHITE_COLOR_TEMPERATURE = "6000 K"
DEFAULT_WARM_WHITE_COLOR_TEMPERATURE = "3000 K"

# Define mappings for the allowed channels
allowed_channel_map = {
    'red': 'R',
    'green': 'G',
    'blue': 'B',
    'cold_white': 'C',
    'warm_white': 'W',
    'white': 'W',  # map both 'white' and 'warm_white' to 'W'
    'unused': 'N',
}

# Checks for valid characters, max of 4 channels, uniqueness, RGB as a set, W present if C, and then confirms color settings
def _validate(config):
    value = config[CONF_CHANNELS]

    reverse_channel_map = {v: k for k, v in allowed_channel_map.items()}

    # Convert list to string format if necessary
    if isinstance(value, list):
        value = ''.join([allowed_channel_map[item.lower()] for item in value if item.lower() in allowed_channel_map])
        input_format = "list"
    elif isinstance(value, str):
        value = value.upper()
        input_format = "string"
    else:
        raise cv.Invalid("Invalid format for channels. Expected a string or a list of valid channel names.")

    # Validate that 4 or fewer channels are supported (limitation of the "get_internal_view" function of addressable_light platform)
    if len(value) > 4:
        raise cv.Invalid("A maximum of 4 channel values are supported.")

    for char in value:
        if char not in allowed_channel_map.values():
            raise cv.Invalid(
                f"Invalid character '{char}' in channels string. Expected one of {', '.join(set(allowed_channel_map.values()))}."
            )

    # Validate uniqueness of channels (except 'N')
    if len(set(value)) != len(value) and 'N' not in value:
        if input_format == "list":
            raise cv.Invalid(
                "Channels must be unique except for 'unused'."
            )
        else:            
            raise cv.Invalid(
                "Channels must be unique except for 'N'."
            )

    rgb_chars = ['R', 'G', 'B']
    missing_rgb_chars = [char for char in rgb_chars if char not in value]

    if len(missing_rgb_chars) == 3:  # Check for all RGB characters missing
        pass
    else:
        if missing_rgb_chars:
            if input_format == "list":
                missing_channels = ', '.join([reverse_channel_map[char] for char in missing_rgb_chars])
            else:
                missing_channels = ', '.join(missing_rgb_chars)
            raise cv.Invalid(
                f"Missing character(s) '{missing_channels}' in channels."
            )

    # Check for W in presence of C
    if 'C' in value and 'W' not in value:
        if input_format == "list":
            raise cv.Invalid("Cannot have 'cold_white' without 'warm_white' in the channels list.")
        else:
            raise cv.Invalid("Cannot have 'C' without 'W' in the channels string.")

    if 'C' in value and 'R' in value:
        raise cv.Invalid("Color temperature and RGB not currently supported.")

    validate_temperature_settings(config)
    
    config[CONF_CHANNELS] = value
    
    return config

# Check to only allow color temperature settings when C and W channels present
def validate_temperature_settings(config):
    value = config[CONF_CHANNELS]
    if isinstance(value, list):
        value = ''.join([allowed_channel_map[item.lower()] for item in value if item.lower() in allowed_channel_map]).upper()
    else:
        value = value.upper()

    # Check for the presence of both C and W
    if 'C' not in value and 'W' not in value and (config.get(CONF_COLD_WHITE_COLOR_TEMPERATURE) == DEFAULT_COLD_WHITE_COLOR_TEMPERATURE and
            config.get(CONF_WARM_WHITE_COLOR_TEMPERATURE) == DEFAULT_WARM_WHITE_COLOR_TEMPERATURE):
            config.pop(CONF_COLD_WHITE_COLOR_TEMPERATURE, None)
            config.pop(CONF_WARM_WHITE_COLOR_TEMPERATURE, None)
    
    # if 'C' in value and 'W' in value:
    #     pass
    # else:
    #     if CONF_COLD_WHITE_COLOR_TEMPERATURE in config or CONF_WARM_WHITE_COLOR_TEMPERATURE in config:
    #         raise cv.Invalid(
    #             "Values for 'cold_white_color_temperature' and 'warm_white_color_temperature' are only allowed when both 'C' (cold_white) and 'W' (warm_white) are present in channels."
    #         )

    return config

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ESP32RMTLEDStripChannelsLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_CHANNELS): cv.Any(cv.string_strict, [cv.one_of('red', 'green', 'blue', 'cold_white', 'warm_white', 'white', 'unused')]),
            cv.Required(CONF_RMT_CHANNEL): esp32_rmt.validate_rmt_channel(tx=True),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, DEFAULT_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, DEFAULT_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Inclusive(
                CONF_BIT0_HIGH,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT0_LOW,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT1_HIGH,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT1_LOW,
                "custom",
            ): cv.positive_time_period_nanoseconds,
        }
    ),
    cv.has_none_or_all_keys(
        [CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ),
    cv.has_exactly_one_key(CONF_CHIPSET, CONF_BIT0_HIGH),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    if CONF_CHIPSET in config:
        chipset = CHIPSETS[config[CONF_CHIPSET]]
        cg.add(
            var.set_led_params(
                chipset.bit0_high,
                chipset.bit0_low,
                chipset.bit1_high,
                chipset.bit1_low,
            )
        )
    else:
        cg.add(
            var.set_led_params(
                config[CONF_BIT0_HIGH],
                config[CONF_BIT0_LOW],
                config[CONF_BIT1_HIGH],
                config[CONF_BIT1_LOW],
            )
        )

    cg.add(var.set_channels(config[CONF_CHANNELS]))
    
    if CONF_COLD_WHITE_COLOR_TEMPERATURE in config:
        cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    if CONF_WARM_WHITE_COLOR_TEMPERATURE in config:
        cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    
    cg.add(
        var.set_rmt_channel(
            getattr(rmt_channel_t, f"RMT_CHANNEL_{config[CONF_RMT_CHANNEL]}")
        )
    )
