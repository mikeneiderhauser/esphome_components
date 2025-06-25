import esphome.codegen as cg
from esphome.components import sensor,i2c
import esphome.config_validation as cv
import esphome.cpp_helpers

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

CONF_INTAKE_TEMP = "intake_temperature"
CONF_INTERNAL_TEMP = "internal_temperature"
CONF_AVG_TEMP = "average_temperature"

CONF_INPUT_VOLTAGE = "ac_voltage"
CONF_INPUT_CURRENT = "ac_current"
CONF_INPUT_POWER = "ac_power"

CONF_OUTPUT_VOLTAGE = "dc_voltage" 
CONF_OUTPUT_CURRENT = "dc_current"
CONF_OUTPUT_POWER = "dc_power"

CONF_FAN_TARGET_RPM = "fan_target_rpm"
CONF_FAN_ACTUAL_RPM = "fan_actual_rpm"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            # Temperatures
            cv.GenerateID(): cv.declare_id(HPPSUI2CComponent),
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
            # FAN RPM
            cv.Optional(CONF_FAN_TARGET_RPM): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FAN_ACTUAL_RPM): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
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
    await esphome.cpp_helpers.setup_entity(var, config)

    # Temperature
    if intake_temp_config := config.get(CONF_INTAKE_TEMP):
        sens = await sensor.new_sensor(intake_temp_config)
        cg.add(var.set_intake_tmp_c(sens))

    if internal_temp_config := config.get(CONF_INTERNAL_TEMP):
        sens = await sensor.new_sensor(internal_temp_config)
        cg.add(var.internal_tmp_c(sens))

    if avg_temp_config := config.get(CONF_AVG_TEMP):
        sens = await sensor.new_sensor(avg_temp_config)
        cg.add(var.tmp_avg(sens))

    # AC
    if input_voltage_config := config.get(CONF_INPUT_VOLTAGE):
        sens = await sensor.new_sensor(input_voltage_config)
        cg.add(var.set_volt_in(sens))

    if input_current_config := config.get(CONF_INPUT_CURRENT):
        sens = await sensor.new_sensor(input_current_config)
        cg.add(var.set_amp_in(sens))

    if input_power_config := config.get(CONF_INPUT_POWER):
        sens = await sensor.new_sensor(input_power_config)
        cg.add(var.set_watt_in(sens))

    # DC
    if output_voltage_config := config.get(CONF_OUTPUT_VOLTAGE):
        sens = await sensor.new_sensor(output_voltage_config)
        cg.add(var.set_volt_out(sens))

    if output_current_config := config.get(CONF_OUTPUT_CURRENT):
        sens = await sensor.new_sensor(output_current_config)
        cg.add(var.set_amp_out(sens))

    if output_power_config := config.get(CONF_OUTPUT_POWER):
        sens = await sensor.new_sensor(output_power_config)
        cg.add(var.set_watt_out(sens))

    # RPM
    if fan_target_config := config.get(CONF_FAN_TARGET_RPM):
        sens = await sensor.new_sensor(fan_target_config)
        cg.add(var.set_rpm_target(sens))

    if fan_actual_config := config.get(CONF_FAN_ACTUAL_RPM):
        sens = await sensor.new_sensor(fan_actual_config)
        cg.add(var.set_rpm_read(sens))

    return var
