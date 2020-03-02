# LED Trailing Edge Dimmer
Trailing edge dimmer for mains powered LED lights including remote control

## What is a trailing edge dimmer?
Alternating current (a.c.) lamps may have their brightness adjusted by altering the power delivered to the bulb and hence the energy consumed. There are a variety of methods of adjusting the power. The trailing edge dimmer design involves supplying the alternating current from the start of its waveform (as the it passes through zero potential difference between the supply conductors) then removing the supply before the end of the first half of the waveform duty cycle. This reduces the time that the lamp receives power and hence limits the available energy, resulting in lower brightness or dimming of the bulb. There are many advantages to trailing edge dimming, particularly that LED light bulbs may be more precisely controlled with less stress and hence longer life and lower operating noise. By comparison, leading edge dimmers turn the power on part way through the cycle.

There are two hardware elements to this project:

- Leading edge dimmer lamp controller
- Remote control panel

## Controller
The controller detects zero crossover of mains a.c. line and controls the duty period of supplied power, turning on at zero crossover and turning off a period later within the half cycle. It is controlled by an I2C bus, expecting a simple two byte message:

  Byte 0: Index of lamp [0..MAX_LAMPS]
  Byte 1: Lamp brightness level [0..255]

A solid state relay (SSR) is a device that allows the mains supply to be passed or interrupted. If the controller receives value 0 it will turn the lamp off, i.e. not trigger the SSR. If the controller receives value 255 it will turn the lamp full on, i.e. not turning SSR off hence lamp receives full mains duty cycle.

There is a button on the controller to toggle all lamps on at full brightness and off.

## Remote Control Panel
The remote control panel has a rotary (endless) encoder with push switch for each lamp. It sends I2C messages to the controller. Pressing the switch will toggle the lamp on (at previously set brightness) and off. Rotating the encoder whilst the lamp is on will adjust the brightness between a minimum and maximum level (configured via web interface - not yet implemented).

The remote control panel will act as a (Philips Hue emulated) device to an Amazon Echo (Alexa) system. The following phrases may be used:

  Alexa, turn on lamp 1
  Alexa, turn off lamp 1
  Alexa, set lamp 1 to 50 [percent]
  Alexa, dim lamp 1
  
The lamp name may be changed using the Alexa app. This will not change the name displayed in the web interface.

The panel will advertise its hostname via mDNS which will be riban-control-xxxxxxxx where xxxxxxxx is the device serial number.

There is web interface to the remote control panel on HTTP port 8000. The web interface allows:

- Toggle each lamp on / off
- Update firmware

# Hardware

Both devices are based on the ESP8266 microcontroller, specifically the WEMOS Mini. (Other ESP8266 devices may be used by adjusting the GPIO pin names.) The WEMOS is chosen due to its small size (fits within the switch enclosure) and good support by Arduino IDE. The table below describes the pin usage. See [wiki](https://github.com/riban-bw/LED-Trailing-Edge-Dimmer/wiki/ESP8266-Pin-Selection) for reasons for pin selection.

| WEMOS | ESP8266 | Remote use     | Controller use | WEMOS / ESP8266 Notes |
| ----- | ------- | -------------- | -------------- | --------------------- |
|   D0  | GPIO 16 | Button 2       | None           | Only weak pull-down available |
|   D1  | GPIO 5  | I2C Clock      | I2C Clock      | I2C Clock |
|   D2  | GPIO 4  | I2C Data       | I2C Data       | I2C Data |
|   D3  | GPIO 0  | Encoder 2 CLK  | Button         | Hardware 10K pullup (Boot mode selection) |
|   D4  | GPIO 2  | LED            | LED            | Hardware 10K pullup (Boot mode selection) LED|
|   D5  | GPIO 14 | Encoder 2 DATA | Lamp 1 PWM     | |
|   D6  | GPIO 12 | Encoder 1 DATA | Lamp 2 PWM     | |
|   D7  | GPIO 13 | Encoder 1 CLK  | Zero crossover | |
|   D8  | GPIO 15 | Button 1       | None           | Hardware 10K pulldown (Boot mode selection) |
|   TX  | GPIO 1  | None           | None           | Serial Tx |
|   RX  | GPIO 3  | None           | None           | Serial Rx |

Connect encoder common pin to ground and switch (button) common pin to 3.3V (via 1K resistor).