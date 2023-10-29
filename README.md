# Remote Antenna Switch Client
Client side of my Remote Antenna Switch

Related repositories: \
[Remote Antenna Switch Server](https://github.com/dkoole/antenna_switch_server) \
[DXLog antenna switch plugin](https://github.com/dkoole/DXLogAntennaSwitch)

## SD Card
Note that when HSPI SPI bus is used on ESP32, GPIO12 must be low during flashing. 
Fix using jumper?

Important: Enable FATFS long filename support, put it on the stack.

When something went wrong parsing the config file a led starts blinking