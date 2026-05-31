#pragma once

#include "esphome.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hp_psu {

// I2C Registers
#define REG_RPM_WRITE         (0x40)
#define REG_TMP_INTAKE_READ   (0x0d<<1)  // Scale 32
#define REG_TMP_INTERNAL_READ (0x0e<<1)  // Scale 32
#define REG_RPM_READ          (0x0f<<1)  // Scale 1
#define REG_VOLT_IN           (0x04<<1)  // Scale 32
#define REG_AMP_IN            (0x05<<1)  // Scale 64
#define REG_WATT_IN           (0x06<<1)  // Scale 1
#define REG_VOLT_OUT          (0x07<<1)  // Scale 256
#define REG_AMP_OUT           (0x08<<1)  // Scale 64
#define REG_WATT_OUT          (0x09<<1)  // Scale 1

// Temperature outlier rejection bounds (°C)
#define TMP_OUTLIER_MIN -50.0f
#define TMP_OUTLIER_MAX 400.0f

// Defaults — all overridable via YAML
#define DEFAULT_TEMP_MIN    40
#define DEFAULT_TEMP_MAX    90
#define DEFAULT_RPM_MIN     3400
#define DEFAULT_RPM_MAX     14000
#define DEFAULT_TEMP_ADJUST 18   // raw register offset applied before F→C conversion

// Number of update cycles before cycling back to 0
#define STATS_CYCLE_COUNT 4

struct PSUStats {
    uint16_t rpm_read{0};
    uint16_t rpm_tgt{0};

    float intake_tmp_c{0.0f};
    float internal_tmp_c{0.0f};
    float tmp_avg{0.0f};

    float volt_in{0.0f};
    float amp_in{0.0f};
    float watt_in{0.0f};

    float volt_out{0.0f};
    float amp_out{0.0f};
    float watt_out{0.0f};
};


class HPPSUI2CComponent : public esphome::EntityBase, public esphome::PollingComponent, public i2c::I2CDevice {
 public:
    // Sensor setters
    void set_rpm_read(sensor::Sensor *s)       { rpm_read_ = s; }
    void set_rpm_target(sensor::Sensor *s)     { rpm_target_ = s; }
    void set_intake_tmp_c(sensor::Sensor *s)   { intake_tmp_c_ = s; }
    void set_internal_tmp_c(sensor::Sensor *s) { internal_tmp_c_ = s; }
    void set_tmp_avg(sensor::Sensor *s)        { tmp_avg_ = s; }
    void set_volt_in(sensor::Sensor *s)        { volt_in_ = s; }
    void set_amp_in(sensor::Sensor *s)         { amp_in_ = s; }
    void set_watt_in(sensor::Sensor *s)        { watt_in_ = s; }
    void set_volt_out(sensor::Sensor *s)       { volt_out_ = s; }
    void set_amp_out(sensor::Sensor *s)        { amp_out_ = s; }
    void set_watt_out(sensor::Sensor *s)       { watt_out_ = s; }

    // Fan / temp control setters
    void set_temp_min(int v)     { temp_min_ = v; }
    void set_temp_max(int v)     { temp_max_ = v; }
    void set_rpm_min(uint16_t v) { rpm_min_ = v; }
    void set_rpm_max(uint16_t v) { rpm_max_ = v; }
    void set_temp_adjust(int v)  { temp_adjust_ = v; }

    void setup() override;
    void update() override;
    void dump_config() override;

 private:
    PSUStats stats_;
    uint8_t  stats_idx_{0};

    // Fan / temp control (configurable, defaults match original behaviour)
    int      temp_min_{DEFAULT_TEMP_MIN};
    int      temp_max_{DEFAULT_TEMP_MAX};
    uint16_t rpm_min_{DEFAULT_RPM_MIN};
    uint16_t rpm_max_{DEFAULT_RPM_MAX};
    int      temp_adjust_{DEFAULT_TEMP_ADJUST};

    bool    device_present_{false};
    uint8_t i2c_error_count_{0};
    static const uint8_t I2C_MAX_ERRORS{3};

    // Convert a raw PSU register temperature value to Celsius.
    // temp_adjust_ compensates for the PSU's internal offset in raw F-scale units.
    float rawToC(float raw) const {
        return ((raw + static_cast<float>(temp_adjust_)) - 32.0f) * 0.5556f;
    }

    // Float-precision linear interpolation (replaces Arduino map() integer cast)
    float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) const {
        return out_min + (x - in_min) * (out_max - out_min) / (in_max - in_min);
    }

    bool readReg(uint16_t reg, uint16_t &out);
    void writeReg(uint8_t reg, uint16_t val);

    bool getPowerInStats();
    bool getPowerOutStats();
    bool getTemperatureStats();
    bool getRPMStats();

    void setRPM(uint16_t rpm_value);
    void publishNAN();
    void disableAllSensors();

 protected:
    sensor::Sensor *rpm_read_{nullptr};
    sensor::Sensor *rpm_target_{nullptr};
    sensor::Sensor *intake_tmp_c_{nullptr};
    sensor::Sensor *internal_tmp_c_{nullptr};
    sensor::Sensor *tmp_avg_{nullptr};
    sensor::Sensor *volt_in_{nullptr};
    sensor::Sensor *amp_in_{nullptr};
    sensor::Sensor *watt_in_{nullptr};
    sensor::Sensor *volt_out_{nullptr};
    sensor::Sensor *amp_out_{nullptr};
    sensor::Sensor *watt_out_{nullptr};
};

}  // namespace hp_psu
}  // namespace esphome
