# LED-Trailing-Edge-Dimmer
Trailing edge dimmer for mains powered LED lights including remote control

There are two hardware elements to this project:

- Leading edge dimmer lamp controller
- Remote control panel

The controller detects zero crossover of mains a.c. line and controls the duty period of supplied power, turning on at zero crossover and turning off a period later within the half cycle. It is controlled by an I2C bus, expecting a simple two byte message:

  Byte 0: Index of lamp [0..MAX_LAMPS]
  Byte 1: Lamp brightness level [0..255]
  
If the controller receives value 0 it will turn the lamp off, i.e. not trigger the Solid State Relay (SSR). If the controller receives value 255 it will turn the lamp full on, i.e. not turning SSR off hence lamp receives full mains duty cycle.

There is a button on the controller to toggle all lamps on at full brightness and off.

The remote control panel has a rotary (endless) encoder with push switch for each lamp. It sends I2C messages to the controller. Pressing the switch will toggle the lamp on (at previously set brightness) and off. Rotating the encoder whilst the lamp is on will adjust the brightness between a minimum and maximum level (configured via web interface - not yet implemented).

The remote control panel will act as a (Philips Hue emulated) device to an Amazon Echo (Alexa) system. The following phrases may be used:

  Alexa, turn on kithen lights
  Alexa, turn off kitchen lights
  Alexa, set kitchen lights to 50 [percent]
  Alexa, dim kitchen lights
  
The lamp name may be changed using the Alexa app. This will not change the name displayed in the web interface.

The panel will advertise its hostname via mDNS which will be riban-control-xxxxxxxx where xxxxxxxx is the device serial number.

There is web interface to the remote control panel on HTTP port 8000. The web interface allows:

- Toggle each lamp on / off
- Update firmware

# Hardware

Both devices are based on the ESP8266 microcontroller, specifically the WEMOS Mini. (Other ESP8266 devices may be used by adjusting the GPIO pin names.) The WEMOS is chosen due to its small size (fits within the switch enclosure) and good support by Arduino IDE.

| WEMOS | ESP8266 | Use            | Notes |
| ----- | ------- | -------------- | ----- |
|   D0  | GPIO 16 | Button 2       | Hardware 10K pulldown (Boot mode selection) |
|   D1  | GPIO 5  | I2C Clock      | I2C Clock |
|   D2  | GPIO 4  | I2C Data       | I2C Data |
|   D3  | GPIO 0  | Encoder 2 CLK  | Hardware 10K pullup |
|   D4  | GPIO 2  |                | LED Hardware (Boot mode selection) |
|   D5  | GPIO 14 | Encoder 2 DATA | |
|   D6  | GPIO 12 | Encoder 1 DATA | |
|   D7  | GPIO 13 | Encoder 1 CLK  | |
|   D8  | GPIO 15 | Button 1       | Hardware 10K pulldown |
|   TX  | GPIO 1  | None           | Serial Tx |
|   RX  | GPIO 3  | None           | Serial Rx |

Connect encoder common pin to ground and switch (button) common pin to 3.3V (via 1K resistor).