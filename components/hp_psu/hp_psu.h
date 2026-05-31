#pragma once

#include "esphome.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hp_psu {


#define I2CADDR_BASE 0x58
#define I2CREAD_TIMEOUT_MS 50
//#define PSU_SERIAL_DEBUG

#define TMP_OUTLIER_MAX 400

// I2C Registers
#define REG_RPM_WRITE (0x40)
#define REG_TMP_INTAKE_READ (0x0d<<1)   // SCALE 32
#define REG_TMP_INTERNAL_READ (0x0e<<1) // SCALE 32
#define REG_RPM_READ (0x0f<<1)          // SCALE 1

#define REG_VOLT_IN (0x04<<1)   // Scale 32
#define REG_AMP_IN (0x05<<1)    // Scale 128
#define REG_WATT_IN (0x06<<1)   // Scale 2
#define REG_VOLT_OUT (0x07<<1)  // Scale 254.5
#define REG_AMP_OUT (0x08<<1)   // Scale 128
#define REG_WATT_OUT (0x09<<1)  // Scale 2

// Defaults (overridable via YAML)
#define DEFAULT_DTMP_MIN  40
#define DEFAULT_DTMP_MAX  90
#define DEFAULT_RPM_MIN   3400
#define DEFAULT_RPM_MAX   14000
#define DEFAULT_ADJUST_TMP_F 18

typedef struct PSUStats {
        uint16_t rpm_read = 0;
        uint16_t rpm_tgt = 0;

        float intake_tmp_c = 0.0;
        float internal_tmp_c = 0.0;
        float tmp_avg = 0.0;

        float volt_in = 0.0;
        float amp_in = 0.0;
        float watt_in = 0.0;

        float volt_out = 0.0;
        float amp_out = 0.0;
        float watt_out = 0.0;
    } PSUStats;


class HPPSUI2CComponent : public esphome::EntityBase, public esphome::PollingComponent, public i2c::I2CDevice {
    public:
        // Sensor setters
        void set_rpm_read(sensor::Sensor *rpm_read)             { rpm_read_ = rpm_read; }
        void set_rpm_target(sensor::Sensor *rpm_target)         { rpm_target_ = rpm_target; }

        void set_intake_tmp_c(sensor::Sensor *intake_tmp_c)     { intake_tmp_c_ = intake_tmp_c; }
        void set_internal_tmp_c(sensor::Sensor *internal_tmp_c) { internal_tmp_c_ = internal_tmp_c; }
        void set_tmp_avg(sensor::Sensor *tmp_avg)               { tmp_avg_ = tmp_avg; }

        void set_volt_in(sensor::Sensor *volt_in)   { volt_in_ = volt_in; }
        void set_amp_in(sensor::Sensor *amp_in)     { amp_in_ = amp_in; }
        void set_watt_in(sensor::Sensor *watt_in)   { watt_in_ = watt_in; }

        void set_volt_out(sensor::Sensor *volt_out) { volt_out_ = volt_out; }
        void set_amp_out(sensor::Sensor *amp_out)   { amp_out_ = amp_out; }
        void set_watt_out(sensor::Sensor *watt_out) { watt_out_ = watt_out; }

        // Fan / temp control config setters
        void set_temp_min(int temp_min)       { temp_min_ = temp_min; }
        void set_temp_max(int temp_max)       { temp_max_ = temp_max; }
        void set_rpm_min(uint16_t rpm_min)    { rpm_min_ = rpm_min; }
        void set_rpm_max(uint16_t rpm_max)    { rpm_max_ = rpm_max; }
        void set_temp_adjust(int temp_adjust) { temp_adjust_ = temp_adjust; }

        void setup() override;
        void update() override;
        void dump_config() override;

    private:
        PSUStats stats;
        uint16_t r = 0;
        uint8_t stats_idx = 0;
        uint8_t stats_idx_max = 4; // power in, power out, temp, rpm
        float tmp_c_reading;

        // Fan control parameters (configurable via YAML, default to defines)
        int temp_min_ = DEFAULT_DTMP_MIN;
        int temp_max_ = DEFAULT_DTMP_MAX;
        uint16_t rpm_min_ = DEFAULT_RPM_MIN;
        uint16_t rpm_max_ = DEFAULT_RPM_MAX;
        int temp_adjust_ = DEFAULT_ADJUST_TMP_F;

        bool device_present = false;
        uint8_t i2c_error_count_ = 0;
        static const uint8_t I2C_MAX_ERRORS = 3;

        float f2c(uint16_t temp) { return (temp - 32) * .5556; }
        bool readReg(uint16_t reg, uint16_t &out);
        void writeReg(uint8_t reg, uint16_t val);
        bool getPowerInStats();
        bool getPowerOutStats();
        bool getTemperatureStats();
        bool getRPMStats();
        void printStats();
        void setRPM(uint16_t rpm_value);
        void publishNAN();

    protected:
        sensor::Sensor  *rpm_read_{nullptr};
        sensor::Sensor  *rpm_target_{nullptr};

        sensor::Sensor  *intake_tmp_c_{nullptr};
        sensor::Sensor  *internal_tmp_c_{nullptr};
        sensor::Sensor  *tmp_avg_{nullptr};

        sensor::Sensor  *volt_in_{nullptr};
        sensor::Sensor  *amp_in_{nullptr};
        sensor::Sensor  *watt_in_{nullptr};

        sensor::Sensor  *volt_out_{nullptr};
        sensor::Sensor  *amp_out_{nullptr};
        sensor::Sensor  *watt_out_{nullptr};

}; // end class

}  // namespace hp_psu
}  // namespace esphome
