# Monitoring OpenTherm using MQTT

Using this [OpenTherm Gateway
module](https://diyless.com/product/esp8266-opentherm-gateway)
the OpenTherm communication between thermostat and heater can be monitored.
It allows for monitoring of all packet transfers over the OpenTherm
interface.

The original motivation for this small project is to observe when the
heater is turned on by the thermostat such that a separate pump can
be switched on as well. Using the ESP OpenTherm Gateway and [this
python package](https://github.com/joshuisken/otmqtt) many MQTT
sensors are created, using auto-discovery, in Home-Assistant to enable
this. 

## OpenTherm protocol

In practice all OpenTherm communication is monitored.

Over the  interface continuous communication takes places
between the master (ex. a thermostat) and a slave (ex. a heater).
The basic packet is a 32-bit word in which an 8-bit segment indicates
a "register".
See [this spec](http://files.domoticaforum.eu/uploads/Manuals/Opentherm/Opentherm%20Protocol%20v2-2.pdf)
from 2003.

## MQTT

All data transfers are send to a MQTT broker, default with topics:

- `esp/mqtt_ot/master`: hexadecimal packet sent by the master
- `esp/mqtt_ot/slave`: hexadecimal packet sent by the slave as response

The OpenTherm Gateway module also has a temperature sensor

- `esp/mqtt_ot/temp`: temperature in degrees Celsius

The state of the sensor itself can be monitored on `esp/mqtt_ot/state`
and a timeout (of 5 seconds) of the OpenTherm protocol can be
monitored using `esp/mqtt_ot/active`.

To reduce MQTT traffic each "register" transfer is cached in a
hash table. When identical to the previously sent value it is not
sent to the MQTT broker.
This cache is cleared by publishing a payload `clear` to topic
`esp/mqtt_ot/cmd`.

Additionally the ESP can be rebooted by sending the command `reset` to
topic `esp/mqtt_ot/cmd`.

## Over the air update

Using `EspMQTTClient` both web-based update and over-the-air update
are enabled. Secrets, such as the WIFI SSID and Password and the MQTT
broker credentials, are stored in `secrets.h`.

You can create a `secrets.hh` with the real secrets, see the Makefile.
A pre-commit setting exist to avoid exposing these secrets in the
repo.

