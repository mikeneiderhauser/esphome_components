#include "hp_psu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hp_psu {

static const char *const TAG = "hp_psu.sensor";

// Returns true on success, false on I2C error. Result placed in out.
bool HPPSUI2CComponent::readReg(uint16_t reg, uint16_t &out) {
    uint16_t cs = 0;
    uint8_t  regCS = 0;
    uint8_t  data[3] = {0, 0, 0};

    cs = reg + ((this->address_) << 1);
    regCS = ((0xff - cs) + 1) & 0xff;  // checksum 'secret sauce'
    data[0] = reg;
    data[1] = regCS;

    auto write_err = I2CDevice::write(data, 2);
    if (write_err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d", this->address_, reg, write_err);
        return false;
    }

    auto read_err = I2CDevice::read(data, 3);
    if (read_err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C read error on reg 0x%02X: %d", this->address_, reg, read_err);
        return false;
    }

    out = ((uint16_t)data[1] << 8) | (uint16_t)data[0];
    return true;
}

void HPPSUI2CComponent::writeReg(uint8_t reg, uint16_t val) {
    uint8_t  valLSB = 0;
    uint8_t  valMSB = 0;
    uint16_t cs = 0;
    uint8_t  regCS = 0;
    uint8_t  data[4] = {0, 0, 0, 0};

    valLSB = val & 0xff;
    valMSB = val >> 8;
    cs = ((this->address_) << 1) + reg + valLSB + valMSB;
    regCS = ((0xff - cs) + 1) & 0xff;

    data[0] = reg;
    data[1] = valLSB;
    data[2] = valMSB;
    data[3] = regCS;

    auto err = I2CDevice::write(data, 4);
    if (err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d", this->address_, reg, err);
    }
}

void HPPSUI2CComponent::publishNAN() {
    if (this->volt_in_ != nullptr)      this->volt_in_->publish_state(NAN);
    if (this->amp_in_ != nullptr)       this->amp_in_->publish_state(NAN);
    if (this->watt_in_ != nullptr)      this->watt_in_->publish_state(NAN);
    if (this->volt_out_ != nullptr)     this->volt_out_->publish_state(NAN);
    if (this->amp_out_ != nullptr)      this->amp_out_->publish_state(NAN);
    if (this->watt_out_ != nullptr)     this->watt_out_->publish_state(NAN);
    if (this->intake_tmp_c_ != nullptr) this->intake_tmp_c_->publish_state(NAN);
    if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->publish_state(NAN);
    if (this->tmp_avg_ != nullptr)      this->tmp_avg_->publish_state(NAN);
    if (this->rpm_read_ != nullptr)     this->rpm_read_->publish_state(NAN);
    if (this->rpm_target_ != nullptr)   this->rpm_target_->publish_state(NAN);
}

void HPPSUI2CComponent::printStats() {
    ESP_LOGV(TAG, "ETMP: %f", this->stats.intake_tmp_c);
    ESP_LOGV(TAG, "ITMP: %f", this->stats.internal_tmp_c);
    ESP_LOGV(TAG, "AVG: %f", this->stats.tmp_avg);
    ESP_LOGV(TAG, "RPM: %d", this->stats.rpm_read);
    ESP_LOGV(TAG, "TGT: %d", this->stats.rpm_tgt);
    ESP_LOGV(TAG, "V_IN: %f", this->stats.volt_in);
    ESP_LOGV(TAG, "A_IN: %f", this->stats.amp_in);
    ESP_LOGV(TAG, "W_IN: %f", this->stats.watt_in);
    ESP_LOGV(TAG, "V_OUT: %f", this->stats.volt_out);
    ESP_LOGV(TAG, "A_OUT: %f", this->stats.amp_out);
    ESP_LOGV(TAG, "W_OUT: %f", this->stats.watt_out);
}

bool HPPSUI2CComponent::getPowerInStats() {
    // Prime read
    if (!this->readReg(REG_TMP_INTAKE_READ, this->r)) return false;

    if (this->readReg(REG_VOLT_IN, this->r))
        this->stats.volt_in = (float)this->r / 32.0f;

    if (this->readReg(REG_AMP_IN, this->r))
        this->stats.amp_in = (float)this->r / 64.0f;

    if (this->readReg(REG_WATT_IN, this->r))
        this->stats.watt_in = (float)this->r;

    return true;
}

bool HPPSUI2CComponent::getPowerOutStats() {
    // Prime read
    if (!this->readReg(REG_TMP_INTAKE_READ, this->r)) return false;

    if (this->readReg(REG_VOLT_OUT, this->r))
        this->stats.volt_out = (float)this->r / 256.0f;

    if (this->readReg(REG_AMP_OUT, this->r))
        this->stats.amp_out = (float)this->r / 64.0f;

    if (this->readReg(REG_WATT_OUT, this->r))
        this->stats.watt_out = (float)this->r;

    return true;
}

bool HPPSUI2CComponent::getTemperatureStats() {
    // Prime read
    if (!this->readReg(REG_TMP_INTAKE_READ, this->r)) return false;

    if (this->readReg(REG_TMP_INTAKE_READ, this->r)) {
        this->tmp_c_reading = this->f2c(((float)this->r / 32.0f) + this->temp_adjust_);
        if (this->tmp_c_reading < TMP_OUTLIER_MAX) {
            this->stats.intake_tmp_c = this->tmp_c_reading;
        }
    }

    if (this->readReg(REG_TMP_INTERNAL_READ, this->r)) {
        this->tmp_c_reading = this->f2c(((float)this->r / 32.0f) + this->temp_adjust_);
        if (this->tmp_c_reading < TMP_OUTLIER_MAX) {
            this->stats.internal_tmp_c = this->tmp_c_reading;
        }
    }

    this->stats.tmp_avg = (this->stats.intake_tmp_c + this->stats.internal_tmp_c) / 2.0f;
    return true;
}

bool HPPSUI2CComponent::getRPMStats() {
    if (this->readReg(REG_RPM_READ, this->r)) {
        this->stats.rpm_read = this->r;
        return true;
    }
    return false;
}

void HPPSUI2CComponent::setRPM(uint16_t rpm_value) {
    this->stats.rpm_tgt = rpm_value;
    this->writeReg(REG_RPM_WRITE, rpm_value);
}

void HPPSUI2CComponent::setup() {
    if (this->is_disabled_by_default()) {
        ESP_LOGD(TAG, "HP PSU 0x%02X is marked as disabled_by_default. Skipping setup.", this->address_);
        this->device_present = false;

        // Guard against optional sensors not being configured
        if (rpm_read_ != nullptr)       rpm_read_->set_disabled_by_default(true);
        if (rpm_target_ != nullptr)     rpm_target_->set_disabled_by_default(true);
        if (intake_tmp_c_ != nullptr)   intake_tmp_c_->set_disabled_by_default(true);
        if (internal_tmp_c_ != nullptr) internal_tmp_c_->set_disabled_by_default(true);
        if (tmp_avg_ != nullptr)        tmp_avg_->set_disabled_by_default(true);
        if (volt_in_ != nullptr)        volt_in_->set_disabled_by_default(true);
        if (amp_in_ != nullptr)         amp_in_->set_disabled_by_default(true);
        if (watt_in_ != nullptr)        watt_in_->set_disabled_by_default(true);
        if (volt_out_ != nullptr)       volt_out_->set_disabled_by_default(true);
        if (amp_out_ != nullptr)        amp_out_->set_disabled_by_default(true);
        if (watt_out_ != nullptr)       watt_out_->set_disabled_by_default(true);
        return;
    }

    ESP_LOGD(TAG, "Setting up HP PSU 0x%02X (temp_min=%d temp_max=%d rpm_min=%d rpm_max=%d)",
             this->address_, this->temp_min_, this->temp_max_, this->rpm_min_, this->rpm_max_);

    auto err = this->bus_->writev(this->address_, nullptr, 0);
    if (err == i2c::ERROR_OK) {
        this->device_present = true;
        this->i2c_error_count_ = 0;
    } else {
        ESP_LOGW(TAG, "HP PSU 0x%02X not found on I2C bus during setup.", this->address_);
        this->device_present = false;
    }
}

void HPPSUI2CComponent::update() {
    if (this->is_disabled_by_default()) {
        ESP_LOGV(TAG, "HP PSU 0x%02X disabled_by_default. Skipping update.", this->address_);
        return;
    }

    // Re-probe device presence if it was previously absent or accumulated errors
    if (!this->device_present || this->i2c_error_count_ >= I2C_MAX_ERRORS) {
        auto err = this->bus_->writev(this->address_, nullptr, 0);
        if (err == i2c::ERROR_OK) {
            if (!this->device_present) {
                ESP_LOGI(TAG, "HP PSU 0x%02X is back on I2C bus.", this->address_);
            }
            this->device_present = true;
            this->i2c_error_count_ = 0;
            this->status_clear_warning();
        } else {
            ESP_LOGW(TAG, "HP PSU 0x%02X not found on I2C bus. Skipping stats.", this->address_);
            this->device_present = false;
            this->status_set_warning();
            this->publishNAN();
            return;
        }
    }

    ESP_LOGV(TAG, "Updating HP PSU 0x%02X (cycle %d)", this->address_, this->stats_idx);

    bool ok = false;

    if (this->stats_idx == 0) {
        ok = this->getPowerInStats();
        this->stats_idx++;
        if (ok) {
            if (this->volt_in_ != nullptr)  this->volt_in_->publish_state(this->stats.volt_in);
            if (this->amp_in_ != nullptr)   this->amp_in_->publish_state(this->stats.amp_in);
            if (this->watt_in_ != nullptr)  this->watt_in_->publish_state(this->stats.watt_in);
        }

    } else if (this->stats_idx == 1) {
        ok = this->getPowerOutStats();
        this->stats_idx++;
        if (ok) {
            if (this->volt_out_ != nullptr) this->volt_out_->publish_state(this->stats.volt_out);
            if (this->amp_out_ != nullptr)  this->amp_out_->publish_state(this->stats.amp_out);
            if (this->watt_out_ != nullptr) this->watt_out_->publish_state(this->stats.watt_out);
        }

    } else if (this->stats_idx == 2) {
        ok = this->getTemperatureStats();
        this->stats_idx++;
        if (ok) {
            if (this->intake_tmp_c_ != nullptr)   this->intake_tmp_c_->publish_state(this->stats.intake_tmp_c);
            if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->publish_state(this->stats.internal_tmp_c);
            if (this->tmp_avg_ != nullptr)        this->tmp_avg_->publish_state(this->stats.tmp_avg);
        }

    } else if (this->stats_idx == 3) {
        ok = this->getRPMStats();
        this->stats_idx = 0; // reset cycle

        // Fan control based on internal temp from previous cycle
        if (this->stats.internal_tmp_c > this->temp_max_) {
            this->stats.rpm_tgt = this->rpm_max_;
        } else if (this->stats.internal_tmp_c < this->temp_min_) {
            this->stats.rpm_tgt = this->rpm_min_;
        } else {
            this->stats.rpm_tgt = (uint16_t)map(
                (long)this->stats.internal_tmp_c,
                (long)this->temp_min_, (long)this->temp_max_,
                (long)this->rpm_min_,  (long)this->rpm_max_
            );
        }
        this->setRPM(this->stats.rpm_tgt);

        if (ok) {
            if (this->rpm_read_ != nullptr)   this->rpm_read_->publish_state(this->stats.rpm_read);
            if (this->rpm_target_ != nullptr) this->rpm_target_->publish_state(this->stats.rpm_tgt);
        }
    }

    if (!ok) {
        this->i2c_error_count_++;
        ESP_LOGW(TAG, "HP PSU 0x%02X I2C error on cycle %d (%d/%d)",
                 this->address_, this->stats_idx, this->i2c_error_count_, I2C_MAX_ERRORS);
        this->status_set_warning();
    } else {
        this->i2c_error_count_ = 0;
        this->status_clear_warning();
    }
}

void HPPSUI2CComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "HP PSU:");
    ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
    ESP_LOGCONFIG(TAG, "  Temp Min: %d°C  Temp Max: %d°C", this->temp_min_, this->temp_max_);
    ESP_LOGCONFIG(TAG, "  RPM Min: %d  RPM Max: %d", this->rpm_min_, this->rpm_max_);
    ESP_LOGCONFIG(TAG, "  Temp Adjust: %d°F", this->temp_adjust_);
}


}  // namespace hp_psu
}  // namespace esphome
