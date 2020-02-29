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

The remote control panel has a rotary (endless) encoder with push switch for each lamp. It sends I2C messages to the controller. Pressing the switch will toggle the lamp on (at previously set brightenss) and off. Rotating the encoder whilst the lamp is on will adjust the brightness between a minimum and maximum level (configured via web interface - not yet implemented).

The remote control panel will act as a (Philips Hue emulated) device to an Amazon Echo (Alexa) system. The following phrases may be used:

  Alexa, turn on kithen lights
  Alexa, turn off kitchen lights
  Alexa, set kitchen lights to 50 [percent]
  Alexa, dim kitchen lights
  
There is web interface to the remote control panel on HTTP port 8000. The panel will advertise its hostname via mDNS which will be riban-control-xxxxxxxx where xxxxxxxx is the device serial number.