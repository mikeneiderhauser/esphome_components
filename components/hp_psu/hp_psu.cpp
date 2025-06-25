#include "hp_psu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hp_psu {

static const char *const TAG = "hp_psu.sensor";

uint16_t HPPSUI2CComponent::readReg(uint16_t reg) {
    uint16_t cs = 0;
    uint8_t  regCS = 0;
    uint8_t  data[3] = {0,0,0};
    
    cs=reg+((this->address_)<<1);
    regCS=((0xff-cs)+1)&0xff;  //#this is the 'secret sauce' - if you don't add the checksum byte when reading a register the PSU will play dumb
    data[0] = reg;
    data[1] = regCS;
    
    // Send read request
    //Wire.beginTransmission((this->address));
    //Wire.write(data, 2);
    I2CDevice::write(data, 2);
    //Wire.endTransmission();

    I2CDevice::read(data ,3);
    return ((uint16_t)data[1] << 8) | (uint16_t)data[0];

    /*
    Wire.requestFrom((uint8_t)(this->address), (uint8_t)3);
    unsigned long ms_prev = millis();
    unsigned long ms_cur = millis();
    
    while(ms_cur - ms_prev < I2CREAD_TIMEOUT_MS) {
        ms_cur = millis();
        if (Wire.available()) {
            
            //data[0] = Wire.read(); // LSB
            //data[1] = Wire.read(); // MSB
            //data[2] = Wire.read(); // Checksum -> ignoring
            #ifdef PSU_SERIAL_DEBUG
            Serial.println(((uint16_t)data[1] << 8) | (uint16_t)data[0], HEX);
            #endif
            return ((uint16_t)data[1] << 8) | (uint16_t)data[0];
        }
    }

    // TODO do we want to set disabled on bad read?
    // this->enabled = false;
    // TODO implement bad reg read counter then disable power supply
    return 0xFFFF;  // Bad read
    */
}

void HPPSUI2CComponent::writeReg(uint8_t reg, uint16_t val) {
    uint8_t  valLSB = 0;
    uint8_t  valMSB = 0;
    uint16_t cs = 0;
    uint8_t  regCS = 0;
    uint8_t  data[4] = {0,0,0,0};

    // Most of the following is taken from the dump work from this github repo
    // https://github.com/raplin/DPS-1200FB/blob/master/DPS-1200FB.py

    // Extract MSB and LSB from RPM
    valLSB=val&0xff;
    valMSB=val>>8;
    // calculate checksum - the checksum is the 'secret sauce'
    cs=((this->address_)<<1)+reg+valLSB+valMSB;
    regCS=((0xff-cs)+1)&0xff;

    // pack the data
    data[0] = reg;
    data[1] = valLSB;
    data[2] = valMSB;
    data[3] = regCS;

    // write to psu
    // I2C Transaction
    //Wire.beginTransmission((this->address));
    //Wire.write(data,4);
    I2CDevice::write(data, 4);
    //Wire.endTransmission();
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

void HPPSUI2CComponent::getPowerInStats(){
    // initial read to prime
    this->r = this->readReg(REG_TMP_INTAKE_READ);

    this->r = this->readReg(REG_VOLT_IN);
    if (this->r != 0xFFFF) {
        this->stats.volt_in = (this->r)/32;
    }

    this->r = this->readReg(REG_AMP_IN);
    if (this->r != 0xFFFF) {
        #ifdef EXPORT_RAW_PWR
        this->stats.amp_in = (float)this->r;
        #else
        //this->stats.amp_in = this->r/ 128;
        this->stats.amp_in = (this->r)/64;
        #endif
    }

    this->r= this->readReg(REG_WATT_IN);
    if (this->r != 0xFFFF) {
        #ifdef EXPORT_RAW_PWR
        this->stats.watt_in = (float)this->r;
        #else
        //this->stats.watt_in = this->r/ 2;
        this->stats.watt_in = (float)this->r;
        #endif
    }
}

void HPPSUI2CComponent::getPowerOutStats() {
    // initial read to prime
    this->r = this->readReg(REG_TMP_INTAKE_READ);

    this->r = this->readReg(REG_VOLT_OUT);
    if (this->r != 0xFFFF) {
        this->stats.volt_out = (this->r)/256;
    }

    this->r = this->readReg(REG_AMP_OUT);
    if (this->r != 0xFFFF) {
        #ifdef EXPORT_RAW_PWR
        this->stats.amp_out = (float)this->r;
        #else
        //this->stats.amp_out = this->r/ 128;
        this->stats.amp_out = (this->r)/64;
        #endif
    }

    this->r = this->readReg(REG_WATT_OUT);
    if (this->r != 0xFFFF) {
        #ifdef EXPORT_RAW_PWR
        this->stats.watt_out = (float)this->r;
        #else
        //this->stats.watt_out = this->r/ 2;
        this->stats.watt_out = (float)this->r;
        #endif
    }
}

void HPPSUI2CComponent::getTemperatureStats() {
    // initial read to prime
    this->r = this->readReg(REG_TMP_INTAKE_READ);

    this->r = this->readReg(REG_TMP_INTAKE_READ);
    this->stats.intake_tmp_c = this->r;
    if (this->r != 0xFFFF) {
        this->tmp_c_reading = this->f2c(((this->r)/32) + ADJUST_TMP_F);
        if(this->tmp_c_reading < TMP_OUTLIER_MAX) {
            this->stats.intake_tmp_c = this->tmp_c_reading;    
        }
    }

    this->r = this->readReg(REG_TMP_INTERNAL_READ);
    if (this->r != 0xFFFF) {
        this->tmp_c_reading = this->f2c(((this->r)/32) + ADJUST_TMP_F);
        if(this->tmp_c_reading < TMP_OUTLIER_MAX) {
            this->stats.internal_tmp_c = this->tmp_c_reading;    
        }
    }

    this->stats.tmp_avg = (this->stats.intake_tmp_c + this->stats.internal_tmp_c)/2;
}

void HPPSUI2CComponent::getRPMStats() {

    this->r = this->readReg(REG_RPM_READ);
    if (this->r != 0xFFFF) {
        this->stats.rpm_read = this->r;
    }
}

void HPPSUI2CComponent::setRPM(uint16_t rpm_value) {
    this->stats.rpm_tgt = rpm_value;
    this->writeReg(REG_RPM_WRITE, rpm_value);
}

void HPPSUI2CComponent::setup() {
    if(this->is_disabled_by_default()) {
        // short circuit setup
        ESP_LOGD(TAG, "HP PSU 0x%02X is marked as disabled_by_default. Skipping setup.", this->address_);
        this->device_present = false;
        rpm_read_->set_disabled_by_default(this->is_disabled_by_default());
        rpm_target_->set_disabled_by_default(this->is_disabled_by_default());

        intake_tmp_c_->set_disabled_by_default(this->is_disabled_by_default());
        internal_tmp_c_->set_disabled_by_default(this->is_disabled_by_default());
        tmp_avg_->set_disabled_by_default(this->is_disabled_by_default());

        volt_in_->set_disabled_by_default(this->is_disabled_by_default());
        amp_in_->set_disabled_by_default(this->is_disabled_by_default());
        watt_in_->set_disabled_by_default(this->is_disabled_by_default());

        volt_out_->set_disabled_by_default(this->is_disabled_by_default());
        amp_out_->set_disabled_by_default(this->is_disabled_by_default());
        watt_out_->set_disabled_by_default(this->is_disabled_by_default());
        return;
    }

    ESP_LOGD(TAG, "Setting up HP PSU 0x%02X", this->address_);

    // Check for the i2c device during setup to ensure its connected
    // TODO update this state every N polling interval
    auto err = this->bus_->writev(this->address_, nullptr, 0);
    if (err == 0) { // ERROR_OK
        this->device_present = true;
    }
    else
    {
        ESP_LOGW(TAG, "HP PSU Device at address 0x%02X is not in i2c scan. Setting Device Present to false.", this->address_);
        this->device_present = false;
    }
    // if error do this
    //ESP_LOGW(TAG, "Timeout loading NVM.");
    //this->mark_failed();
    //return; // only on error
}

void HPPSUI2CComponent::update() {
    if(this->is_disabled_by_default()) {
        // short circuit update
        ESP_LOGV(TAG, "HP PSU 0x%02X is marked as disabled_by_default. Skipping update.", this->address_);
        this->device_present = false;
        return;
    }

    if (! this->device_present){
        ESP_LOGW(TAG, "HP PSU Device at address 0x%02X is not in i2c scan. Skipping stats collection.", this->address_);
        this->status_set_warning();
        return;
    }

    ESP_LOGV(TAG, "Updating HP PSU  0x%02X", this->address_);
    //this->getStats();
    if (this->stats_idx == 0) {
        this->getPowerInStats();
        //this->printStats();
        this->stats_idx++;

        // Publish Sensor Data
        if (this->volt_in_ != nullptr) {
            this->volt_in_->publish_state(this->stats.volt_in);
        }

        if (this->amp_in_ != nullptr) {
            this->amp_in_->publish_state(this->stats.amp_in);
        }

        if (this->watt_in_ != nullptr) {
            this->watt_in_->publish_state(this->stats.watt_in);
        }

    } else if (this->stats_idx == 1) {
        this->getPowerOutStats();
        //this->printStats();
        this->stats_idx++;

        // Publish Sensor Data
        if (this->volt_out_ != nullptr) {
            this->volt_out_->publish_state(this->stats.volt_out);
        }

        if (this->amp_out_ != nullptr) {
            this->amp_out_->publish_state(this->stats.amp_out);
        }

        if (this->watt_out_ != nullptr) {
            this->watt_out_->publish_state(this->stats.watt_out);
        }

    } else if (this->stats_idx == 2) {
        this->getTemperatureStats();
        //this->printStats();
        this->stats_idx++;

        // Publish Sensor Data
        if (this->intake_tmp_c_ != nullptr) {
            this->intake_tmp_c_->publish_state(this->stats.intake_tmp_c);
        }

        if (this->internal_tmp_c_ != nullptr) {
            this->internal_tmp_c_->publish_state(this->stats.internal_tmp_c);
        }

        if (this->tmp_avg_ != nullptr) {
            this->tmp_avg_->publish_state(this->stats.tmp_avg);
        }
    } else if (this->stats_idx == 3) {
        this->getRPMStats();
        //this->printStats();
        this->stats_idx = 0; // reset stats idx

        // handle fan control thresholds based on internal temp
        // temps were read in previous cycle
        if ( this->stats.internal_tmp_c > this->TMAX) {
            // handle above tmp max
            this->stats.rpm_tgt = RPM_MAX;
        } else if (this->stats.internal_tmp_c < this->TMIN) {
            // handle below tmp min
            this->stats.rpm_tgt = RPM_MIN;
        } else {
            // dynamically control rpm based on temp scale
            this->stats.rpm_tgt = map(this->stats.internal_tmp_c, TMIN, TMAX, RPM_MIN, RPM_MAX);
        }

        this->setRPM(this->stats.rpm_tgt);

        // Publish Sensor Data
        if (this->rpm_read_ != nullptr) {
            this->rpm_read_->publish_state(this->stats.rpm_read);
        }

        if (this->rpm_target_ != nullptr) {
            this->rpm_target_->publish_state(this->stats.rpm_tgt);
        }
    }

    this->status_clear_warning();
}

void HPPSUI2CComponent::dump_config() {
    char buffer[50];
    ESP_LOGCONFIG("HP_PSU: ", "DUMP");
    sprintf(buffer, "Address: 0x%02X", this->address_);
    ESP_LOGCONFIG("HP_PSU: ", buffer);
}


}  // namespace hp_psu
}  // namespace esphome
