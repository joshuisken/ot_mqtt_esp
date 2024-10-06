/*
OpenTherm Gateway/Monitor Example
By: Ihor Melnyk
Date: May 1st, 2019
http://ihormelnyk.com

Moving to MQTT monitoring
By: Jos Huisken
Date: Aug, 2024
*/

#include <Arduino.h>
#include <Hashtable.h>
#include <OpenTherm.h>
#include <DS18B20.h>
#include <EspMQTTClient.h>
#include "secrets.h"

#ifndef VERSION
#define VERSION "unknown"
#endif


// MQTT client

#ifndef CLIENT_NAME
#define CLIENT_NAME      "opentherm"     // MQTT client name to identify the device
#endif
#define BROKER_PORT      1883            // MQTT Port. No "" needed
#define lastwill_text    "offline"       // MQTT message to report lastwill and testament.

String  client_name    = CLIENT_NAME;                    // MQTT Topic to report initial value
String  topic_state    = "esp/"+client_name+"/state";    // MQTT Topic to report status
String  topic_command  = "esp/"+client_name+"/cmd";      // MQTT Topic to execute command
String  topic_active   = "esp/"+client_name+"/active";   // MQTT Topic to report active state of OT
String  topic_master   = "esp/"+client_name+"/master";   // MQTT topic to report master request
String  topic_slave    = "esp/"+client_name+"/slave";    // MQTT topic to report slave request
String  topic_temp     = "esp/"+client_name+"/temp";     // MQTT topic to report slave request

EspMQTTClient client(WIFI_SSID, WIFI_PASS, BROKER_IP,
		     BROKER_USERNAME, BROKER_PASSWORD,
		     CLIENT_NAME, BROKER_PORT);

// OpenTherm gateway

const int mInPin = D2;   // 2 for Arduino,  4 for ESP8266 (D2), 21 for ESP32
const int mOutPin = D1;  // 4 for Arduino,  5 for ESP8266 (D1), 22 for ESP32

const int sInPin = D6;  // 3 for Arduino, 12 for ESP8266 (D6), 19 for ESP32
const int sOutPin = D7; // 5 for Arduino, 13 for ESP8266 (D7), 23 for ESP32

OpenTherm mOT(mInPin, mOutPin);
OpenTherm sOT(sInPin, sOutPin, true);


// Temperature sensor

const int tInPin = D5;  // 14 for ESP8266 (D5)

DS18B20 tSensor(tInPin);

float ptemp = 0.0, temp = 0.0;


// OpenTherm Timeout signalling

const unsigned long timeout = 5000; // Timeout of 5 seconds
unsigned long last_event = 0;
bool ot_active = false;


// Avoid sending superflous mqtt messages, by maintaining previous
// states of all OT transfers from master and slave

typedef uint8_t K;

Hashtable<K, uint16_t> otRegsMaster;
Hashtable<K, uint16_t> otRegsSlave;

bool updated(unsigned long frame, Hashtable<K, uint16_t> &d)
{
  uint32_t v = frame & 0xffffffff;
  uint16_t data_value = v & 0xffff;
  const K data_id = (v >> 16) & 0xff;
  // uint8_t spare = (v >> 24) & 0xf;
  // assert spare == 0, "Spare must be all zero.";
  uint8_t msg_type = (v >> 28) & 0x7;
  // bool parity = (v >> 31) & 0x1;
  // assert self._parity() == 0, "Parity check fails.";
  if (d.containsKey(data_id)) {
    uint16_t *prev_value = d.get(data_id);
    if (*prev_value == data_value)
      return false;
    *prev_value = data_value;
    return true;
  }
  d.put(data_id, data_value);
  return true;
}


// Interrupt routines

void IRAM_ATTR mHandleInterrupt()
{
  mOT.handleInterrupt();
}

void IRAM_ATTR sHandleInterrupt()
{
  sOT.handleInterrupt();
}

void processRequest(unsigned long request, OpenThermResponseStatus status)
{
  last_event = millis();  // Timeout reset
  Serial.println("M" + String(request, HEX)); // master/thermostat request
  if (updated(request, otRegsMaster))
    client.publish(topic_master, String(request, HEX));
  /* else */
  /*   client.publish(topic_master, String(request+1, HEX)); */
  unsigned long response = mOT.sendRequest(request);
  if (response)  {
    sOT.sendResponse(response);
    Serial.println("S" + String(response, HEX)); // slave/boiler response
    if (updated(response, otRegsSlave))
      client.publish(topic_slave, String(response, HEX));
    /* else */
    /*   client.publish(topic_master, String(response+1, HEX)); */
  }
}


static void executeCommand(const String payload)
{
  if (strncmp(payload.c_str(), "clear", 3) == 0) {
    // Clear the hashtables, such that clients can reinitialize
    otRegsMaster.clear();
    otRegsSlave.clear();
  }
  if (strncmp(payload.c_str(), "reset", 3) == 0) {
    // Reboot, such that OT is restarted
    client.publish(topic_state, "resetting", true);
    delay(500);   // Wait a little
    noInterrupts();
    wifi_station_disconnect();
    delay(20000); // Hopefully timeout for thermostat control unit
    ESP.restart();
  }
}


void onConnectionEstablished() {
  // MQTT Initial Connection
  String online = String("online, version ") + String(VERSION);
  client.publish(topic_state, online, true);
  Serial.println(online);
  Serial.println("Master pins: " + String(mInPin) + " " + String(mOutPin));
  Serial.println("Slave  pins: " + String(sInPin) + " " + String(sOutPin));  
  Serial.println("DallasT pin: " + String(tInPin));
  ptemp = 0.0;
  client.subscribe(topic_command, [] (const String &payload)  {
      Serial.print("\nReceived command ");
      Serial.println(payload);
      executeCommand(payload);
    });
}


void setup()
{
  Serial.begin(115200);        // 9600 supported by OpenTherm Monitor App
  // OpenTherm
  mOT.begin(mHandleInterrupt); // for ESP ot.begin(); without interrupt handler can be used
  sOT.begin(sHandleInterrupt, processRequest);
  // Enable the web updater.
  client.enableHTTPWebUpdater();
  // Enable the OTA update.
  client.enableOTA();
  // MQTT Last Will & Testament
  const char* lastwill = "offline";
  client.enableLastWillMessage(topic_state.c_str(), lastwill, true);
  client.enableLastWillMessage(topic_active.c_str(), "0", true);
}


void loop()
{
  client.loop();   // MQTT Loop: Must be called once per loop.
  sOT.process();
  if (client.isConnected()) {
    // Temperature sensor
    temp = tSensor.getTempC();
    if ((temp > ptemp + 0.3) || (temp < ptemp - 0.3)) {
      ptemp = temp;
      Serial.println("T" + String(temp, 2));
      client.publish(topic_temp, String(temp, 1));
    }
    // Timeout
    if (ot_active) {
      if (millis() - last_event > timeout) {
	ot_active = false;
	client.publish(topic_active, "0");
	last_event = 0;
      }
    } else {
      if (last_event) {
	ot_active = true;
	client.publish(topic_active, "1");
      }
    }
  }
}
