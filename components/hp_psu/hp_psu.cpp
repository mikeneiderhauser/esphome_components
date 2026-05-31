#include "hp_psu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hp_psu {

static const char *const TAG = "hp_psu.sensor";

// ---------------------------------------------------------------------------
// I2C primitives
// ---------------------------------------------------------------------------

bool HPPSUI2CComponent::readReg(uint16_t reg, uint16_t &out, bool quiet) {
    uint8_t data[3] = {0, 0, 0};

    // Checksum is the 'secret sauce' — the PSU ignores reads without it
    uint16_t cs = reg + (static_cast<uint16_t>(this->address_) << 1);
    data[0] = static_cast<uint8_t>(reg);
    data[1] = static_cast<uint8_t>(((0xff - cs) + 1) & 0xff);

    auto write_err = I2CDevice::write(data, 2);
    if (write_err != i2c::ERROR_OK) {
        if (quiet) ESP_LOGV(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d",
                            this->address_, reg, write_err);
        else       ESP_LOGW(TAG, "HP PSU 0x%02X I2C write error on reg 0x%02X: %d",
                            this->address_, reg, write_err);
        return false;
    }

    auto read_err = I2CDevice::read(data, 3);
    if (read_err != i2c::ERROR_OK) {
        if (quiet) ESP_LOGV(TAG, "HP PSU 0x%02X I2C read error on reg 0x%02X: %d",
                            this->address_, reg, read_err);
        else       ESP_LOGW(TAG, "HP PSU 0x%02X I2C read error on reg 0x%02X: %d",
                            this->address_, reg, read_err);
        return false;
    }

    // Validate the PSU's reply checksum (data = [LSB, MSB, checksum]).
    // A valid frame satisfies (LSB + MSB + CS) & 0xFF == 0. This rejects
    // corrupt-but-ACKed reads (e.g. the 0xAA55 bus-glitch pattern) that a
    // bus-level ErrorCode check alone would let through.
    if (static_cast<uint8_t>(data[0] + data[1] + data[2]) != 0) {
        if (quiet) ESP_LOGV(TAG, "HP PSU 0x%02X bad reply checksum on reg 0x%02X",
                            this->address_, reg);
        else       ESP_LOGW(TAG, "HP PSU 0x%02X bad reply checksum on reg 0x%02X",
                            this->address_, reg);
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
// Stat collection — each returns false if the first (guard) read fails, which
// indicates the device is unreachable. Subsequent reads are best-effort; stale
// values are retained on failure rather than forcing a full-cycle skip.
// Note: the DPS-1200 protocol needs no "prime"/dummy read — each register is a
// single self-contained read.
// ---------------------------------------------------------------------------

bool HPPSUI2CComponent::getPowerInStats() {
    uint16_t raw = 0;

    if (!this->readReg(REG_VOLT_IN, raw)) return false;
    this->stats_.volt_in = static_cast<float>(raw) / 32.0f;
    if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW volt_in=%u", this->address_, raw);

    if (this->readReg(REG_AMP_IN, raw)) {
        this->stats_.amp_in = static_cast<float>(raw) / 64.0f;
        if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW amp_in=%u", this->address_, raw);
    }

    if (this->readReg(REG_WATT_IN, raw)) {
        this->stats_.watt_in = static_cast<float>(raw);
        if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW watt_in=%u", this->address_, raw);
    }

    return true;
}

bool HPPSUI2CComponent::getPowerOutStats() {
    uint16_t raw = 0;

    if (!this->readReg(REG_VOLT_OUT, raw)) return false;
    this->stats_.volt_out = static_cast<float>(raw) / 256.0f;
    if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW volt_out=%u", this->address_, raw);

    if (this->readReg(REG_AMP_OUT, raw)) {
        this->stats_.amp_out = static_cast<float>(raw) / 64.0f;
        if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW amp_out=%u", this->address_, raw);
    }

    if (this->readReg(REG_WATT_OUT, raw)) {
        this->stats_.watt_out = static_cast<float>(raw);
        if (this->debug_raw_) ESP_LOGI(TAG, "0x%02X RAW watt_out=%u", this->address_, raw);
    }

    return true;
}

bool HPPSUI2CComponent::getTemperatureStats() {
    uint16_t raw = 0;

    // Intake temperature (guard read)
    if (!this->readReg(REG_TMP_INTAKE_READ, raw)) return false;
    {
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

// Slow-changing peaks, min, and status flags — all OPTIONAL/diagnostic.
// Best-effort: each read is independent, only checksum-valid and non-sentinel
// (0xFFFF = unsupported on some variants) values are published, and the group
// never reports failure so it can't trip the device-health/warning logic.
// Scales match the live power readings (raw watts, amps/64) for consistency.
bool HPPSUI2CComponent::getPeakFlagStats() {
    uint16_t raw = 0;

    // Each register is only read if a sensor is configured for it, so dropping
    // an unsupported reading from the YAML also drops its I2C transaction.
    if (this->peak_watt_in_ != nullptr && this->readReg(REG_PEAK_WATTS_IN, raw, true) && raw != 0xFFFF) {
        this->stats_.peak_watt_in = static_cast<float>(raw);
        this->peak_watt_in_->publish_state(this->stats_.peak_watt_in);
    }
    if (this->min_amp_in_ != nullptr && this->readReg(REG_MIN_AMPS_IN, raw, true) && raw != 0xFFFF) {
        this->stats_.min_amp_in = static_cast<float>(raw) / 64.0f;
        this->min_amp_in_->publish_state(this->stats_.min_amp_in);
    }
    if (this->peak_amp_out_ != nullptr && this->readReg(REG_PEAK_AMPS_OUT, raw, true) && raw != 0xFFFF) {
        this->stats_.peak_amp_out = static_cast<float>(raw) / 64.0f;
        this->peak_amp_out_->publish_state(this->stats_.peak_amp_out);
    }
    if (this->flags_ != nullptr && this->readReg(REG_FLAGS, raw, true)) {
        this->stats_.flags = raw;
        this->flags_->publish_state(this->stats_.flags);
    }
    return true;  // optional registers never fail the cycle
}

// 32-bit cumulative energy + PSU runtime — also optional/best-effort.
bool HPPSUI2CComponent::getEnergyStats() {
    uint16_t lo = 0, hi = 0;

    // WATT_SECONDS_IN spans two consecutive registers (low, then high word).
    // Only read if an energy sensor is configured.
    if (this->energy_in_ != nullptr &&
        this->readReg(REG_WATT_SEC_IN_LO, lo, true) && this->readReg(REG_WATT_SEC_IN_HI, hi, true)) {
        uint32_t watt_sec_raw = (static_cast<uint32_t>(hi) << 16) | lo;
        if (watt_sec_raw != 0xFFFFFFFF) {
            // raw/4 = watt-seconds; /3600 -> watt-hours  =>  raw / 14400
            this->stats_.energy_wh = static_cast<float>(watt_sec_raw) / 14400.0f;
            this->energy_in_->publish_state(this->stats_.energy_wh);
            if (this->debug_raw_)
                ESP_LOGI(TAG, "0x%02X RAW watt_sec=%u", this->address_, watt_sec_raw);
        }
    }

    uint16_t raw = 0;
    if (this->runtime_ != nullptr && this->readReg(REG_ON_SECONDS, raw, true) && raw != 0xFFFF) {
        this->stats_.runtime_s = static_cast<float>(raw) / 2.0f;
        this->runtime_->publish_state(this->stats_.runtime_s);
    }

    return true;  // optional registers never fail the cycle
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
    if (this->peak_watt_in_ != nullptr)   this->peak_watt_in_->publish_state(NAN);
    if (this->min_amp_in_ != nullptr)     this->min_amp_in_->publish_state(NAN);
    if (this->peak_amp_out_ != nullptr)   this->peak_amp_out_->publish_state(NAN);
    if (this->runtime_ != nullptr)        this->runtime_->publish_state(NAN);
    if (this->energy_in_ != nullptr)      this->energy_in_->publish_state(NAN);
    if (this->flags_ != nullptr)          this->flags_->publish_state(NAN);
}

// disabled_by_default is set at compile time by ESPHome's Python codegen
// based on the YAML config — no runtime setter needed.

// ---------------------------------------------------------------------------
// ESPHome lifecycle
// ---------------------------------------------------------------------------

void HPPSUI2CComponent::setup() {
    if (this->psu_disabled_) {
        ESP_LOGD(TAG, "HP PSU 0x%02X marked disabled — skipping setup and polling.", this->address_);
        this->device_present_ = false;
        return;
    }

    ESP_LOGD(TAG, "Setting up HP PSU 0x%02X (temp %d–%d°C, rpm %d–%d, adjust %d°F)",
             this->address_, this->temp_min_, this->temp_max_,
             this->rpm_min_, this->rpm_max_, this->temp_adjust_);

    auto err = this->write(nullptr, 0);
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
    if (this->psu_disabled_)
        return;  // unused slot — stay completely silent

    // Re-probe if device was absent or accumulated too many errors
    if (!this->device_present_ || this->i2c_error_count_ >= I2C_MAX_ERRORS) {
        auto err = this->write(nullptr, 0);
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
            // Temperature + RPM + fan control merged: reading internal temp and
            // applying the fan target in the same pass keeps the control loop
            // coherent (no cross-cycle lag).
            this->stats_idx_ = 3;
            {
                bool temp_ok = this->getTemperatureStats();
                bool rpm_ok = this->getRPMStats();
                ok = temp_ok && rpm_ok;

                // Fan control using float interpolation on the freshly-read
                // internal temperature (falls back to last good value if the
                // read failed, since stats_ retains it).
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
                    if (this->intake_tmp_c_ != nullptr)   this->intake_tmp_c_->publish_state(this->stats_.intake_tmp_c);
                    if (this->internal_tmp_c_ != nullptr) this->internal_tmp_c_->publish_state(this->stats_.internal_tmp_c);
                    if (this->tmp_avg_ != nullptr)        this->tmp_avg_->publish_state(this->stats_.tmp_avg);
                    if (this->rpm_read_ != nullptr)       this->rpm_read_->publish_state(this->stats_.rpm_read);
                    if (this->rpm_target_ != nullptr)     this->rpm_target_->publish_state(this->stats_.rpm_tgt);
                }
            }
            break;

        case 3:
            // Optional diagnostics — publishes internally, never fails the cycle.
            this->stats_idx_ = 4;
            ok = this->getPeakFlagStats();
            break;

        case 4:
        default:
            // Optional diagnostics — publishes internally, never fails the cycle.
            this->stats_idx_ = 0;
            ok = this->getEnergyStats();
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
