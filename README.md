# Remote Antenna Switch Client
Client side of my Remote Antenna Switch

Related repositories: \
[Remote Antenna Switch Server](https://github.com/dkoole/antenna_switch_server) \
[DXLog antenna switch plugin](https://github.com/dkoole/DXLogAntennaSwitch)

## SD Card
GPIO12 on ESP32 is a bootstrapping pin and used to select the output voltage of an internal regulator to power the internal flash. This makes it by default not usable for the HSPI bus to connect the SD Card. This can be fixed by burning flash voltage selection efuses. This sets the voltage of the internal flash to 3.3v and GPIO12 will not be used as an bootstrapping ping anymore.

`python3 <esp_idf_path>/components/esptool_py/esptool/espefuse.py set_flash_voltage 3.3V`

## Configuration requirements
Important: Enable FATFS long filename support, put it on the stack.

When something went wrong parsing the config file a led starts blinking