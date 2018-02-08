TempLogger
This temperature logger project aims at providing a simple temperature sensor providing temperature and relative humidity (as a bonus) updates over MQTT.

The sensor is configurable through a http interface, which will start if the configuration is empty/not valid, or if the config button is pressed. The configuration sets: SSID+password to join, ip configuration (when not using DHCP), sensor identifier, MQTT target.

The sensor should run on batteries and have an autonomy of 9-12 months minimum, this requires to only consume minimum power while inactive and have shortest possible active period. I will use a TPL5110 timer to switch the circuit off while sleeping to minimize battery use. On the software side, I'm using fixed IP addresses as DHCP will take time.

This project is based on ESPIOT sensor