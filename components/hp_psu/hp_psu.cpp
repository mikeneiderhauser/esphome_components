#include "hp_psu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hp_psu {

static const char *const TAG = "hp_psu.sensor";

// ---------------------------------------------------------------------------
// I2C primitives
// ---------------------------------------------------------------------------

bool HPPSUI2CComponent::readReg(uint16_t reg, uint16_t &out) {
    uint8_t data[3] = {0, 0, 0};

    // Checksum is the 'secret sauce' — the PSU ignores reads without it
    uint16_t cs = reg + (static_cast<uint16_t>(this->address_) << 1);
    data[0] = static_cast<uint8_t>(reg);
    data[1] = static_cast<uint8_t>(((0xff - cs) + 1) & 0xff);

    auto write_err = I2CDevice::write(data, 2);
    if (write_err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d",
                 this->address_, reg, write_err);
        return false;
    }

    auto read_err = I2CDevice::read(data, 3);
    if (read_err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C read error on reg 0x%02X: %d",
                 this->address_, reg, read_err);
        return false;
    }

    out = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[0]);
    return true;
}

void HPPSUI2CComponent::writeReg(uint8_t reg, uint16_t val) {
    uint8_t valLSB = val & 0xff;
    uint8_t valMSB = val >> 8;
    uint16_t cs = (static_cast<uint16_t>(this->address_) << 1) + reg + valLSB + valMSB;

    uint8_t data[4] = {
        reg,
        valLSB,
        valMSB,
        static_cast<uint8_t>(((0xff - cs) + 1) & 0xff)
    };

    auto err = I2CDevice::write(data, 4);
    if (err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d",
                 this->address_, reg, err);
    }
}

// ---------------------------------------------------------------------------
// Stat collection — each returns false only if the prime read fails.
// Individual register reads within a method are best-effort; stale values
// are retained on failure rather than forcing a full-cycle skip.
// ---------------------------------------------------------------------------

bool HPPSUI2CComponent::getPowerInStats() {
    uint16_t raw = 0;

    // Prime read — required by HP PSU I2C protocol before fresh data is available
    if (!this->readReg(REG_TMP_INTAKE_READ, raw)) return false;

    if (this->readReg(REG_VOLT_IN, raw))
        this->stats_.volt_in = static_cast<float>(raw) / 32.0f;

    if (this->readReg(REG_AMP_IN, raw))
        this->stats_.amp_in = static_cast<float>(raw) / 64.0f;

    if (this->readReg(REG_WATT_IN, raw))
        this->stats_.watt_in = static_cast<float>(raw);

    return true;
}

bool HPPSUI2CComponent::getPowerOutStats() {
    uint16_t raw = 0;

    // Prime read
    if (!this->readReg(REG_TMP_INTAKE_READ, raw)) return false;

    if (this->readReg(REG_VOLT_OUT, raw))
        this->stats_.volt_out = static_cast<float>(raw) / 256.0f;

    if (this->readReg(REG_AMP_OUT, raw))
        this->stats_.amp_out = static_cast<float>(raw) / 64.0f;

    if (this->readReg(REG_WATT_OUT, raw))
        this->stats_.watt_out = static_cast<float>(raw);

    return true;
}

bool HPPSUI2CComponent::getTemperatureStats() {
    uint16_t raw = 0;

    // Prime read
    if (!this->readReg(REG_TMP_INTAKE_READ, raw)) return false;

    // Intake temperature
    if (this->readReg(REG_TMP_INTAKE_READ, raw)) {
        float t = this->rawToC(static_cast<float>(raw) / 32.0f);
        if (t > TMP_OUTLIER_MIN && t < TMP_OUTLIER_MAX)
            this->stats_.intake_tmp_c = t;
    }

    // Internal temperature
    if (this->readReg(REG_TMP_INTERNAL_READ, raw)) {
        float t = this->rawToC(static_cast<float>(raw) / 32.0f);
        if (t > TMP_OUTLIER_MIN && t < TMP_OUTLIER_MAX)
            this->stats_.internal_tmp_c = t;
    }

    this->stats_.tmp_avg = (this->stats_.intake_tmp_c + this->stats_.internal_tmp_c) / 2.0f;
    return true;
}

bool HPPSUI2CComponent::getRPMStats() {
    uint16_t raw = 0;
    if (!this->readReg(REG_RPM_READ, raw)) return false;
    this->stats_.rpm_read = raw;
    return true;
}

// ---------------------------------------------------------------------------
// Fan control
// ---------------------------------------------------------------------------

void HPPSUI2CComponent::setRPM(uint16_t rpm_value) {
    this->stats_.rpm_tgt = rpm_value;
    this->writeReg(REG_RPM_WRITE, rpm_value);
}

// ---------------------------------------------------------------------------
// Sensor helpers
// ---------------------------------------------------------------------------

void HPPSUI2CComponent::publishNAN() {
    if (this->volt_in_ != nullptr)        this->volt_in_->publish_state(NAN);
    if (this->amp_in_ != nullptr)         this->amp_in_->publish_state(NAN);
    if (this->watt_in_ != nullptr)        this->watt_in_->publish_state(NAN);
    if (this->volt_out_ != nullptr)       this->volt_out_->publish_state(NAN);
    if (this->amp_out_ != nullptr)        this->amp_out_->publish_state(NAN);
    if (this->watt_out_ != nullptr)       this->watt_out_->publish_state(NAN);
    if (this->intake_tmp_c_ != nullptr)   this->intake_tmp_c_->publish_state(NAN);
    if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->publish_state(NAN);
    if (this->tmp_avg_ != nullptr)        this->tmp_avg_->publish_state(NAN);
    if (this->rpm_read_ != nullptr)       this->rpm_read_->publish_state(NAN);
    if (this->rpm_target_ != nullptr)     this->rpm_target_->publish_state(NAN);
}

void HPPSUI2CComponent::disableAllSensors() {
    if (this->rpm_read_ != nullptr)       this->rpm_read_->set_disabled_by_default(true);
    if (this->rpm_target_ != nullptr)     this->rpm_target_->set_disabled_by_default(true);
    if (this->intake_tmp_c_ != nullptr)   this->intake_tmp_c_->set_disabled_by_default(true);
    if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->set_disabled_by_default(true);
    if (this->tmp_avg_ != nullptr)        this->tmp_avg_->set_disabled_by_default(true);
    if (this->volt_in_ != nullptr)        this->volt_in_->set_disabled_by_default(true);
    if (this->amp_in_ != nullptr)         this->amp_in_->set_disabled_by_default(true);
    if (this->watt_in_ != nullptr)        this->watt_in_->set_disabled_by_default(true);
    if (this->volt_out_ != nullptr)       this->volt_out_->set_disabled_by_default(true);
    if (this->amp_out_ != nullptr)        this->amp_out_->set_disabled_by_default(true);
    if (this->watt_out_ != nullptr)       this->watt_out_->set_disabled_by_default(true);
}

// ---------------------------------------------------------------------------
// ESPHome lifecycle
// ---------------------------------------------------------------------------

void HPPSUI2CComponent::setup() {
    if (this->is_disabled_by_default()) {
        ESP_LOGD(TAG, "HP PSU 0x%02X disabled_by_default — skipping setup.", this->address_);
        this->device_present_ = false;
        this->disableAllSensors();
        return;
    }

    ESP_LOGD(TAG, "Setting up HP PSU 0x%02X (temp %d–%d°C, rpm %d–%d, adjust %d°F)",
             this->address_, this->temp_min_, this->temp_max_,
             this->rpm_min_, this->rpm_max_, this->temp_adjust_);

    auto err = this->bus_->writev(this->address_, nullptr, 0);
    if (err == i2c::ERROR_OK) {
        this->device_present_ = true;
        this->i2c_error_count_ = 0;
        ESP_LOGD(TAG, "HP PSU 0x%02X found on I2C bus.", this->address_);
    } else {
        ESP_LOGW(TAG, "HP PSU 0x%02X not found on I2C bus during setup.", this->address_);
        this->device_present_ = false;
    }
}

void HPPSUI2CComponent::update() {
    if (this->is_disabled_by_default()) {
        ESP_LOGV(TAG, "HP PSU 0x%02X disabled_by_default — skipping update.", this->address_);
        return;
    }

    // Re-probe if device was absent or accumulated too many errors
    if (!this->device_present_ || this->i2c_error_count_ >= I2C_MAX_ERRORS) {
        auto err = this->bus_->writev(this->address_, nullptr, 0);
        if (err == i2c::ERROR_OK) {
            if (!this->device_present_)
                ESP_LOGI(TAG, "HP PSU 0x%02X is back on I2C bus.", this->address_);
            this->device_present_ = true;
            this->i2c_error_count_ = 0;
            this->status_clear_warning();
        } else {
            ESP_LOGW(TAG, "HP PSU 0x%02X not found on I2C bus — skipping stats.", this->address_);
            this->device_present_ = false;
            this->status_set_warning();
            this->publishNAN();
            return;
        }
    }

    // Save cycle index before incrementing so error logs reference the correct cycle
    const uint8_t cycle = this->stats_idx_;
    bool ok = false;

    ESP_LOGV(TAG, "HP PSU 0x%02X update cycle %d", this->address_, cycle);

    switch (cycle) {
        case 0:
            this->stats_idx_ = 1;
            ok = this->getPowerInStats();
            if (ok) {
                if (this->volt_in_ != nullptr)  this->volt_in_->publish_state(this->stats_.volt_in);
                if (this->amp_in_ != nullptr)   this->amp_in_->publish_state(this->stats_.amp_in);
                if (this->watt_in_ != nullptr)  this->watt_in_->publish_state(this->stats_.watt_in);
            }
            break;

        case 1:
            this->stats_idx_ = 2;
            ok = this->getPowerOutStats();
            if (ok) {
                if (this->volt_out_ != nullptr) this->volt_out_->publish_state(this->stats_.volt_out);
                if (this->amp_out_ != nullptr)  this->amp_out_->publish_state(this->stats_.amp_out);
                if (this->watt_out_ != nullptr) this->watt_out_->publish_state(this->stats_.watt_out);
            }
            break;

        case 2:
            this->stats_idx_ = 3;
            ok = this->getTemperatureStats();
            if (ok) {
                if (this->intake_tmp_c_ != nullptr)   this->intake_tmp_c_->publish_state(this->stats_.intake_tmp_c);
                if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->publish_state(this->stats_.internal_tmp_c);
                if (this->tmp_avg_ != nullptr)        this->tmp_avg_->publish_state(this->stats_.tmp_avg);
            }
            break;

        case 3:
        default:
            this->stats_idx_ = 0;
            ok = this->getRPMStats();

            // Fan control using float interpolation — internal temp from previous cycle
            uint16_t target_rpm;
            if (this->stats_.internal_tmp_c >= static_cast<float>(this->temp_max_)) {
                target_rpm = this->rpm_max_;
            } else if (this->stats_.internal_tmp_c <= static_cast<float>(this->temp_min_)) {
                target_rpm = this->rpm_min_;
            } else {
                target_rpm = static_cast<uint16_t>(this->mapFloat(
                    this->stats_.internal_tmp_c,
                    static_cast<float>(this->temp_min_), static_cast<float>(this->temp_max_),
                    static_cast<float>(this->rpm_min_),  static_cast<float>(this->rpm_max_)
                ));
            }
            this->setRPM(target_rpm);

            if (ok) {
                if (this->rpm_read_ != nullptr)   this->rpm_read_->publish_state(this->stats_.rpm_read);
                if (this->rpm_target_ != nullptr) this->rpm_target_->publish_state(this->stats_.rpm_tgt);
            }
            break;
    }

    if (!ok) {
        this->i2c_error_count_++;
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C error on cycle %d (%d/%d)",
                 this->address_, cycle, this->i2c_error_count_, I2C_MAX_ERRORS);
        this->status_set_warning();
    } else {
        this->i2c_error_count_ = 0;
        this->status_clear_warning();
    }
}

void HPPSUI2CComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "HP PSU:");
    ESP_LOGCONFIG(TAG, "  Address:     0x%02X", this->address_);
    ESP_LOGCONFIG(TAG, "  Temp range:  %d–%d °C", this->temp_min_, this->temp_max_);
    ESP_LOGCONFIG(TAG, "  RPM range:   %d–%d", this->rpm_min_, this->rpm_max_);
    ESP_LOGCONFIG(TAG, "  Temp adjust: %d (raw F-scale units)", this->temp_adjust_);
}

}  // namespace hp_psu
}  // namespace esphome
