/** Mains powered LED lamp dimmer
 *  Trailing edge dimmer for LED lamps
 */
#include <Wire.h>

#define MAX_DUTY_FACTOR 37 // Multiplier factor to calculate SSR ON duty cycle: 40 for 50Hz (40*255 = 10.2ms), 33 for 60Hz (33 * 255 = 8.4ms)
#define FADE_STEP_DURATION 4000 // Period in microseconds between each step of fade: 4ms ~ 0..100% in 1s. Increase for slower fade
#define LAMP_QUANT 2 // Quantity of controlled lamps
#define LAMP1_PIN  0 // GPI pin feeding SSR driving lamp
#define LAMP2_PIN  1 // GPI pin feeding SSR driving lamp
#define ZERO_PIN   2 // Zero crossover detection pin (INT is 2,3 on arduino mini)
#define TOGGLE_PIN 3 // GPI pin to toggle on/off state of lamps (INT is 2,3 on arduino mini)
#define SDA_PIN    4 // I2C Data pin
#define SCL_PIN    5 // I2C CLK pin

const int16_t I2C_SLAVE = 0x08; //!@todo Define address from config / jumpers

// Defines structure of a lamp
struct LAMP {
  byte value = 0; // Current output level [0..255]
  byte targetValue = 0; // Requested value that lamp will ramp to [0..255]
  byte pin; // GPI pin to drive SSR
  unsigned long countdown = 0; // microseconds remaining before SSR should be turned off
  unsigned long fade = FADE_STEP_DURATION;
  // Function to initialise lamp
  void init(byte pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, 0);
  };
  // Function to set SSR ON period - call at each zero crossover
  void trigger() {
    countdown = MAX_DUTY_FACTOR * value;
    processTime(0);
  };
  // Function to handle time advance - lDuration=period since last process
  void processTime(unsigned long lDuration) {
    if(lDuration > countdown)
      countdown = 0;
    else
      countdown = countdown - lDuration;
    if(countdown)
      digitalWrite(pin, HIGH); // Turn on SSR (on next zero crossover)
    else
      digitalWrite(pin, LOW); // Turn off SSR (end of duty cycle)
    if(fade > lDuration)
      fade -= lDuration;
    else {
      if(targetValue > value)
        ++value;
      else if(targetValue < value)
        --value;
      fade = FADE_STEP_DURATION;
    }    
  };
  // Function to set lamp dim level [0..255]
  void setValue(byte nValue) {
    targetValue = nValue;
  };
};

LAMP g_lamps[LAMP_QUANT];
volatile bool g_bZeroCrossover = false; // True at mains zero crossover
volatile bool g_bToggleState = true; // True when toggle button released
volatile unsigned long g_lLastToggle = 0; // Time of last toggle button state change

/** Handle I2C receive events
 *  @param  nSize Quantity of bytes in received message
 */
void onWireRx(size_t nSize) {
  while(Wire.available() > 1) {
    byte nLamp = Wire.read();
    byte nValue = Wire.read();
    if(nLamp < LAMP_QUANT)
      g_lamps[nLamp].setValue(nValue);
  }
}

/** Handle toggle button state change interrupt */
void onToggle() {
  g_bToggleState = true;
}

/** @brief  Handle zero crossover interrupt */
void onZeroCrossover() {
  g_bZeroCrossover = true;
}

/** Handle button toggle */
void handleToggle() {
  if(digitalRead(TOGGLE_PIN)) {
    for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp)
      g_lamps[nLamp].setValue(0);
  } else {
    for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp)
      g_lamps[nLamp].setValue(255);
  }
}

void setup() {
  g_lamps[0].init(1);
  g_lamps[1].init(2);
  Wire.begin(SDA_PIN, SCL_PIN, I2C_SLAVE);
  Wire.onReceive(onWireRx);
  attachInterrupt(digitalPinToInterrupt(ZERO_PIN), onZeroCrossover, FALLING);
  attachInterrupt(digitalPinToInterrupt(TOGGLE_PIN), onToggle, CHANGE);
}

void loop() {
  static long int lThenMicros = micros(); // Time of last loop
  static long int lDebounce = 0; // Toggle button debounce counter

  if(g_bZeroCrossover) {
    // Zero crossover occured (detected by ISR handler) so initialise lamps' duty cycle
    for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp)
      g_lamps[nLamp].trigger();
    g_bZeroCrossover = false;
  }

  // Caculate elapsed time since last cycle and progress duty cycle
  unsigned long lNowMicros = micros();
  unsigned long lLoopDuration = lNowMicros - lThenMicros;
  for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp) {
    g_lamps[nLamp].processTime(lLoopDuration); // Update each lamp status based on time since last update
  }
  lThenMicros = lNowMicros;

  if(g_bToggleState) {
    if(lDebounce > 50000) {
      g_bToggleState = false;
      lDebounce = 0;
      handleToggle();
    } else {
    lDebounce += lLoopDuration;
    }
  }
}
