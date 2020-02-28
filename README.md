![Photon | Smart Temperature and Humidity Monitoring](https://i.imgur.com/FgXHtNs.png)

## Photon | Temperature and Humidity Monitoring via MQTT for Home Assistant

Welcome! This simple project is a great way to start learning the basics of Arduino, MQTT, wifi communication and management, soldering and 3d design. 

## Bill of materials

 - ESP8266 based development borad
	 - Wemos D1 mini (recommended)
	 - NodeMCU v2/v3
- BME280 temperaturen humidity and pressure sensor
- A 0,96" OLED screen
- 3x7 cm protoboard

## Setting up

 1. Download all of the libaries and insert them into your IDE
 2. Open the temphum.ion file and upload it into the board
 3.  Set up the board connection like shown below
 4.  Connect to the generated hotspot
 5. Configure Wifi details and MQTT info using the web interface
 6. Done!

## Connecting to Home Assistant 
I recommend using the [mosquito broker](https://github.com/home-assistant/hassio-addons/tree/master/mosquitto) you can install using Hass.io use the following JSON and start it up (Please change the username and password)

```
logins:
  - username: sensor1
    password: CHNAGEME1
anonymous: false
customize:
  active: false
  folder: mosquitto
certfile: fullchain.pem
keyfile: privkey.pem
require_certificate: false
```

Then add your topics to the config.yaml using the following example:

 ```
 sensor temp1:
platform: mqtt
unit_of_measurement: 'ºC'
state_topic: "room/myroom/sensor/temp"
name: "Temperatura"
icon: mdi:home-thermometer


sensor hum1:
platform: mqtt
unit_of_measurement: '%'
state_topic: "rooms/myroom/sensor/hum"
name: "Humedad"
icon: mdi:water
```

Great job! There is only one more step! Let's make it show it to us on the front page by adding it via Lovelace UI. I ban you laredy know how to do it right? no? fine... I'll put up an example ASAP :)
