import esphome.codegen as cg
from esphome.components import sensor, i2c
import esphome.config_validation as cv

from esphome.const import (
    CONF_NAME,
    CONF_INTERNAL,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ICON,
    CONF_ENTITY_CATEGORY,

    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,

    DEVICE_CLASS_POWER,
    UNIT_WATT,

    DEVICE_CLASS_VOLTAGE,
    UNIT_VOLT,

    DEVICE_CLASS_CURRENT,
    UNIT_AMPERE,

    STATE_CLASS_MEASUREMENT
)

from . import hp_psu_ns

HPPSUI2CComponent = hp_psu_ns.class_(
    "HPPSUI2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONF_INTAKE_TEMP    = "intake_temperature"
CONF_INTERNAL_TEMP  = "internal_temperature"
CONF_AVG_TEMP       = "average_temperature"

CONF_INPUT_VOLTAGE  = "ac_voltage"
CONF_INPUT_CURRENT  = "ac_current"
CONF_INPUT_POWER    = "ac_power"

CONF_OUTPUT_VOLTAGE = "dc_voltage"
CONF_OUTPUT_CURRENT = "dc_current"
CONF_OUTPUT_POWER   = "dc_power"

CONF_FAN_TARGET_RPM = "fan_target_rpm"
CONF_FAN_ACTUAL_RPM = "fan_actual_rpm"

# Fan / temp control
CONF_TEMP_MIN    = "temp_min"
CONF_TEMP_MAX    = "temp_max"
CONF_RPM_MIN     = "rpm_min"
CONF_RPM_MAX     = "rpm_max"
CONF_TEMP_ADJUST = "temp_adjust"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HPPSUI2CComponent),

            # Temperatures
            cv.Optional(CONF_INTAKE_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INTERNAL_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AVG_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # AC Side
            cv.Optional(CONF_INPUT_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INPUT_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INPUT_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # DC Side
            cv.Optional(CONF_OUTPUT_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OUTPUT_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OUTPUT_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # Fan RPM
            cv.Optional(CONF_FAN_TARGET_RPM): sensor.sensor_schema(
                unit_of_measurement="RPM",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FAN_ACTUAL_RPM): sensor.sensor_schema(
                unit_of_measurement="RPM",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # Fan / temp control (optional, defaults handled in C++)
            cv.Optional(CONF_TEMP_MIN,    default=40):    cv.int_range(min=-50, max=200),
            cv.Optional(CONF_TEMP_MAX,    default=90):    cv.int_range(min=-50, max=200),
            cv.Optional(CONF_RPM_MIN,     default=3400):  cv.positive_int,
            cv.Optional(CONF_RPM_MAX,     default=14000): cv.positive_int,
            cv.Optional(CONF_TEMP_ADJUST, default=18):    cv.int_,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(default_address=0x58))
    .extend(cv.ENTITY_BASE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Temperature sensors
    if intake_temp_config := config.get(CONF_INTAKE_TEMP):
        sens = await sensor.new_sensor(intake_temp_config)
        cg.add(var.set_intake_tmp_c(sens))

    if internal_temp_config := config.get(CONF_INTERNAL_TEMP):
        sens = await sensor.new_sensor(internal_temp_config)
        cg.add(var.set_internal_tmp_c(sens))  # fixed: was missing set_ prefix

    if avg_temp_config := config.get(CONF_AVG_TEMP):
        sens = await sensor.new_sensor(avg_temp_config)
        cg.add(var.set_tmp_avg(sens))  # fixed: was missing set_ prefix

    # AC sensors
    if input_voltage_config := config.get(CONF_INPUT_VOLTAGE):
        sens = await sensor.new_sensor(input_voltage_config)
        cg.add(var.set_volt_in(sens))

    if input_current_config := config.get(CONF_INPUT_CURRENT):
        sens = await sensor.new_sensor(input_current_config)
        cg.add(var.set_amp_in(sens))

    if input_power_config := config.get(CONF_INPUT_POWER):
        sens = await sensor.new_sensor(input_power_config)
        cg.add(var.set_watt_in(sens))

    # DC sensors
    if output_voltage_config := config.get(CONF_OUTPUT_VOLTAGE):
        sens = await sensor.new_sensor(output_voltage_config)
        cg.add(var.set_volt_out(sens))

    if output_current_config := config.get(CONF_OUTPUT_CURRENT):
        sens = await sensor.new_sensor(output_current_config)
        cg.add(var.set_amp_out(sens))

    if output_power_config := config.get(CONF_OUTPUT_POWER):
        sens = await sensor.new_sensor(output_power_config)
        cg.add(var.set_watt_out(sens))

    # RPM sensors
    if fan_target_config := config.get(CONF_FAN_TARGET_RPM):
        sens = await sensor.new_sensor(fan_target_config)
        cg.add(var.set_rpm_target(sens))

    if fan_actual_config := config.get(CONF_FAN_ACTUAL_RPM):
        sens = await sensor.new_sensor(fan_actual_config)
        cg.add(var.set_rpm_read(sens))

    # Use the entity's disabled_by_default flag to also disable hardware polling.
    # A PSU slot that is disabled_by_default is treated as physically unpopulated:
    # setup() and update() short-circuit so no I2C probing or NAN spam occurs.
    cg.add(var.set_psu_disabled(config[CONF_DISABLED_BY_DEFAULT]))

    # Fan / temp control config
    cg.add(var.set_temp_min(config[CONF_TEMP_MIN]))
    cg.add(var.set_temp_max(config[CONF_TEMP_MAX]))
    cg.add(var.set_rpm_min(config[CONF_RPM_MIN]))
    cg.add(var.set_rpm_max(config[CONF_RPM_MAX]))
    cg.add(var.set_temp_adjust(config[CONF_TEMP_ADJUST]))

    return var
