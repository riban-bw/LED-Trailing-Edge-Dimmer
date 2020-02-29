/*  ESP8266 Arduino Switch and dimmer control for LED dimmer
 *  Two rotary encoders with push buttons to control lighting 
 *  Serial interface to light contoller
 *  WiFi connection for alexa and http control
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "fauxmoESP.h" // Provides alexa interface

fauxmoESP fauxmo; // Create an instance of the alexa interface
ESP8266WebServer webserver(8000); // Create an instance of a web server
ESP8266HTTPUpdateServer httpUpdater;

#define SERIAL_BAUDRATE   115200 // Debug serial port baudrate
#define LAMP_QUANT        2 // Quantity of lamps configured

// GPI pins
#define SCL_PIN         D1 // GPI pin for I2C clock
#define SDA_PIN         D2 // GPI pin for I2C data
#define ENCODER1_BUTTON D5 // GPIO 14
#define ENCODER1_CLK    D4 // GPIO 2
#define ENCODER1_DATA   D3 // GPIO 0
#define ENCODER2_BUTTON D6 // GPIO 12
#define ENCODER2_CLK    D7 // GPIO 13
#define ENCODER2_DATA   D0 // GPIO 16

const int16_t I2C_MASTER = 0x42; // I2C Master address (this device)
const int16_t I2C_SLAVE = 0x08; // I2C Slave address (lamp controller device)

uint32_t g_nLampFlag = 0; // Bitwise flag indicating index of changed lamp
static const uint8_t anValid[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0}; // Table of valid encoder states
char g_hostname[23];

// Structure describing a lamp controller
struct LAMP {
  bool state = false; // True when lamp on
  bool switchState = true; // False when button pressed
  byte minValue = 20; // Minimum dim level
  byte maxValue = 255; // Maximum dim level
  byte step = 15; // Value change for each encoder detent
  int value = maxValue; // Lamp brightness [0..255] int to handle overshoot calculations
  unsigned int switchPin; // Index of GPI pin connected to button
  unsigned int encoderClkPin; // Index of GPI pin connectd to encoder clock
  unsigned int encoderDataPin; // Index of GPI pin connectd to encoder data
  uint8_t nCount; // Used to filter encoder
  uint8_t nCode; // Used to filter encoder
  const char* name; // Name of lamp, e.g. "kitchen lights"
  // Function to initialise encoder
  void init(unsigned int nSwitch, unsigned int nClk, unsigned int nData) {
    switchPin = nSwitch;
    encoderClkPin = nClk;
    encoderDataPin = nData;
    pinMode(switchPin, INPUT_PULLUP);
    pinMode(encoderClkPin, INPUT_PULLUP);
    pinMode(encoderDataPin, INPUT_PULLUP);
    // Set limits to trigger calculation of step
    setMin(20);
    setMax(255);
  };
  // Function to set brightness [0..255]
  bool setValue(int nValue) {
    if(nValue < minValue)
      nValue = minValue;
    else if(nValue > maxValue)
      nValue = maxValue;
    if(value == nValue)
      return false;
    value = nValue;
    return true;
  };
  // Function to set lamp status [True = on]
  bool setState(bool bState) {
    if(state == bState)
      return false;
    state = bState;
    return true;
  };
  // Function to read switch state - return true if state changed
  bool readSwitch() {
    // Assume debounce performed externally
    bool bState = digitalRead(switchPin);
    if(bState == switchState)
      return false;
    switchState = bState;
    return true;
  };
  // Function to process rotary encoder - returns true if encoder changed
  bool processEncoder() {
    if(!state)
      return false; // Don't adjust level if turned off
    bool bClk = digitalRead(encoderClkPin);
    if(!bClk && !nCode)
      return false; // No clock signal or not mid decode
    bool bData = digitalRead(encoderDataPin);
    nCode <<= 2;
    if(bData)
      nCode |= 0x02;
    if(bClk)
      nCode |= 0x01;
    nCode &= 0x0f;
    // If valid then add to 8-bit history validate rotation codes and process
    if(anValid[nCode]) {
      nCount <<= 4;
      nCount |= nCode;
      if(nCount == 0xd4 && value < 255) {
        // CW
        return setValue(value + step); //!@todo Handle fast rotation
      }
      else if(nCount == 0x17 && value > 0) {
        // CCW
        return setValue(value - step);
      }
    }
    return false;
  };
  // Function to set minimum dim level (based on 20 detents per revolution, 80% rotation for full scale)
  void setMin(byte nValue) {
    if(nValue > maxValue)
      return;
    minValue = nValue;
    step = (maxValue - minValue) / 16;
  }
  // Function to set maximum dim level
  void setMax(byte nValue) {
    if(nValue < minValue)
      return;
    maxValue = nValue;
    step = (maxValue - minValue) / 16;
  }
};

LAMP g_lamps[LAMP_QUANT]; // Create instances of lamp controllers

// HTTP handler for root
void handleWebHome() {
  Serial.println("HTTP request for home page");
  String sHtml = "<!DOCTYPE html><html lang='en'>\
    <head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'/></head>\
    <body><div { background: #006368; }><h1>riban control light dimmer switch</h1><form method='POST'>";
  for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp) {
    sHtml += "<input type='submit' name='toggle' value='";
    sHtml += g_lamps[nLamp].name;
    sHtml+= "'>";
  }
  sHtml += "</form><a href='update'>Firmware update</a></div></body></html>";
  webserver.send(200, "text/html", sHtml);
}

void handleWebHomePost() {
  if (webserver.method() == HTTP_POST) {
    for (uint8_t i = 0; i < webserver.args(); i++) {
      if(webserver.argName(i) == "toggle") {
        for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp) {
          if(webserver.arg(i) == g_lamps[nLamp].name) {
            g_lamps[nLamp].setState(!g_lamps[nLamp].state);
            sendValue(nLamp);
          }
        }
      }
    }
  }
  handleWebHome();
}

// HTTP handler for root
void handleWebUpdate() {
  //!@todo Implement HTTP handler
  Serial.println("HTTP request for firmware update page");
  webserver.send(200, "text/html", "<!DOCTYPE html><html lang='en'> \
    <head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'/></head> \
    <body><h1>riban control light dimmer switch</h1> \
    <form method='POST' action='' enctype='multipart/form-data'>Update:<br><input type='file' accept='.bin' name='update'><input type='submit' value='Update Firmware'></form> \
    </body> \
    </html>");
}

/** Send value to lamp controller
 *  @param  nLamp Value to send [0..255]
 */
void sendValue(unsigned int nLamp) {
  if(nLamp >= LAMP_QUANT)
    return;
  byte nValue = 0;
  if(g_lamps[nLamp].state)
    nValue = g_lamps[nLamp].value;
  Wire.beginTransmission(I2C_SLAVE); //!@todo Define slave address via web interface
  Wire.write(nLamp);
  Wire.write(nValue);
  Wire.endTransmission();
  Serial.printf("Send I2C Address: %d Value: %d\n", nLamp, nValue);
}

void setup() {
    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println("riban light switch v0.1 starting...");

    // Initialise lamps
    g_lamps[0].init(ENCODER1_BUTTON, ENCODER1_CLK, ENCODER1_DATA);
    g_lamps[0].name = "Kitchen lights";
    g_lamps[1].init(ENCODER2_BUTTON, ENCODER2_CLK, ENCODER2_DATA);
    g_lamps[1].name = "Dining lights";

    // Set hostname
    uint32_t chipid = ESP.getChipId();
    
    sprintf(g_hostname, "riban-control-%02x%02x%02x%02x", (chipid >> 24) & 0xFF, (chipid >> 16) & 0xFF, (chipid >> 8) & 0xFF, chipid & 0xFF);

    // If button pressed during boot, start wifiManager
    if(!digitalRead(g_lamps[0].switchPin)) {
        WiFi.disconnect();
        WiFiManager wifiManager;
        wifiManager.startConfigPortal(g_hostname);
    }
    WiFi.begin(); // Attempt to connect to last AP
    WiFi.hostname(g_hostname);
    
    Serial.print("Trying to connect to ");
    Serial.println(WiFi.SSID());
    
    // Configure alexa interface
    fauxmo.createServer(true);
    fauxmo.setPort(80);
    for(unsigned int nLamp = 0; nLamp < LAMP_QUANT; ++nLamp)
      fauxmo.addDevice(g_lamps[nLamp].name); //!@todo Set device names from web interface

    fauxmo.onSetState([](unsigned char nId, const char * sName, bool bState, unsigned char nValue) {
        Serial.printf("Alexa command for device %d (%s) state: %s value: %d\n", nId, sName, bState ? "ON" : "OFF", nValue);
        if(nId < LAMP_QUANT) {
            g_lamps[nId].setState(bState);
            g_lamps[nId].setValue(nValue);
            g_nLampFlag |= 1 << nId;
        }
    });

    Serial.println("Enable I2C Master");
    Wire.begin(SDA_PIN, SCL_PIN, I2C_MASTER);

    webserver.on("/", HTTP_GET, handleWebHome);
    webserver.on("/", HTTP_POST, handleWebHomePost);
    webserver.on("/update", HTTP_GET, handleWebUpdate);
    httpUpdater.setup(&webserver);
}

void loop() {
    static uint32_t nLastCycleTime = millis(); // Timestamp for end of each loop cycle
    static uint32_t nDebounceTime = 0; // Countdown timer to inhibit switch readings after previous change of state
    static bool bStationHasConnected = false; // True if station has connected to AP at least once since boot

    // Start services after WiFi station has connected to AP for first time
    if(!bStationHasConnected) {
      bStationHasConnected = WiFi.isConnected();
      if(bStationHasConnected) {
        Serial.printf("Connected to AP %s, IP: %d.%d.%d.%d", WiFi.SSID().c_str(), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        fauxmo.enable(true);
        Serial.println("Alexa handler started");
        webserver.begin();
        Serial.println("HTTP server started");
        if(MDNS.begin(g_hostname))
          Serial.printf("mDNS started: %s\n", g_hostname);
        MDNS.addService("http", "tcp", 8000);
      }
    }
    
    // Process physical user interface (knobs, switches, etc.)
    for(byte nLamp = 0; nLamp < LAMP_QUANT; ++nLamp) {
      if(g_lamps[nLamp].processEncoder())
        g_nLampFlag |= 1 << nLamp;
      if(nDebounceTime == 0 && g_lamps[nLamp].readSwitch()) {
        nDebounceTime = 50;
        if(g_lamps[nLamp].switchState) {
          g_lamps[nLamp].setState(!g_lamps[nLamp].state);
          g_nLampFlag |= 1 << nLamp;
        }
      }
    }

    // Process alexa commands
    fauxmo.handle();
    
    // Process http requests
    webserver.handleClient();

    // Update mDNS
    MDNS.update();

    // Send pending lamp control messages
    int nLamp = 0;
    while(g_nLampFlag) {
      if(g_nLampFlag & 0x01) {
        sendValue(nLamp);
        fauxmo.setState(g_lamps[nLamp].name, g_lamps[nLamp].state, g_lamps[nLamp].value);
      }
      g_nLampFlag >>= 1;
      ++nLamp;
    }

    // Update clocks, counters, timers, etc.
    uint32_t nNow = millis();
    uint32_t nCycleDuration = nNow - nLastCycleTime;
    if(nDebounceTime < nCycleDuration)
      nDebounceTime = 0;
    else
      nDebounceTime -= nCycleDuration;
    nLastCycleTime = nNow;
}
