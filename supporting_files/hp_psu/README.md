The instructions below are targetd for homeassistant / esphome integrated installs.

Create a new device in ESPHome targeting an ESP32.

From this file, copy the api_encryption_key, ota_password, and ap_password. These will be needed later.

Copy the files from the root of this folder into your espohme directory
- common/*
- v2/*


Copy the hp-psu-example.yaml contents into your newly created device file to replace all of the old contents.  Substitute the api_encryption_key, ota_password, and ap_password values saved earlier into the substitutions keys at the top of the file.

Depending on your devices, modify common/HPPSU_CTRL_r2.0.yaml in the `packages` section to select wifi or ethernet.  The r2.0 defaults to Ethernet.

For first flash, follow the ESPHome documentation on connecting to the boards to flash. This section will be expanded on at a later date.

https://esphome.io/guides/physical_device_connection.html
