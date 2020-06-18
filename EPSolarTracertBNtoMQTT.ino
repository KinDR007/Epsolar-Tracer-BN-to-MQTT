
#include "config.h"
//#include <WiFi.h>/*ESP32WiFi*/
//#include <WiFiClient.h>/*ESP32WiFi*/
//#include <WebServer.h>/*ESP32WiFi*/
//#include <ESPmDNS.h> /*ESP32WiFi*/
#include <WiFiUdp.h> /*ESP8266WiFi*/
#include <ESP8266WiFi.h> /*ESP8266WiFi*/
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
/*
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
*/
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

int timerTask2, timerTask3;
float ctemp, bvoltage, battChargeCurrent, btemp, bremaining, lpower, lcurrent, pvvoltage, pvcurrent, pvpower;
float batt_type, batt_cap, batt_highdisc, batt_chargelimit, batt_overvoltrecon, batt_equalvolt, batt_boostvolt, batt_floatvolt, batt_boostrecon;
float batt_lowvoltrecon, batt_undervoltrecon, batt_undervoltwarn, batt_lowvoltdisc;
float stats_today_pv_volt_min, stats_today_pv_volt_max, battChargePower, Generated_energy_today, Generated_energy_monthly,ambient_temp;
uint8_t result;

WiFiClient espClient;
PubSubClient client(espClient);

// this is to check if we can write since rs485 is half duplex
bool rs485DataReceived = true;

ModbusMaster node;
SimpleTimer timer;

char buf[10];
String value;
char mptt_location[16];

// tracer requires no handshaking
void preTransmission() {}
void postTransmission() {}

// a list of the regisities to query in order
typedef void (*RegistryList[])();
RegistryList Registries = {
  AddressRegistry_3100,
  AddressRegistry_3106,
  AddressRegistry_311A,
  AddressRegistry_3300,
  AddressRegistry_330C,
  AddressRegistry_330E,
//  AddressRegistry_331E,
  AddressRegistry_9000,
};
// keep log of where we are
uint8_t currentRegistryNumber = 0;

// function to switch to next registry
void nextRegistryNumber() {
  currentRegistryNumber = (currentRegistryNumber + 1) % ARRAY_SIZE( Registries);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.hostname(OTA_HOSTNAME);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  // Modbus slave ID 1
  node.begin(EPSOLAR_DEVICE_ID, Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  client.setServer(mqtt_server, 1883);


  timerTask2 = timer.setInterval(10000, doRegistryNumber);
  timerTask3 = timer.setInterval(10000, nextRegistryNumber);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("EPSolar1", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void doRegistryNumber() {
  Registries[currentRegistryNumber]();
}

void AddressRegistry_3100() {
  result = node.readInputRegisters(0x3100, 10);
  if (result == node.ku8MBSuccess)
  {
    ctemp = (long)node.getResponseBuffer(0x11) / 100.0f;
    dtostrf(ctemp, 2, 3, buf );
    //mqtt_location = MQTT_ROOT + "/" + EPSOLAR_DEVICE_ID + "/ctemp";
    client.publish("EPSolar/1/ctemp", buf);// temp inside of box

    bvoltage = (long)node.getResponseBuffer(0x04) / 100.0f;
    dtostrf(bvoltage, 2, 3, buf );
    client.publish("EPSolar/1/bvoltage", buf);

    lpower = ((long)node.getResponseBuffer(0x0F) << 16 | node.getResponseBuffer(0x0E)) / 100.0f;
    dtostrf(lpower, 2, 3, buf);
    client.publish("EPSolar/1/lpower", buf);

    lcurrent = (long)node.getResponseBuffer(0x0D) / 100.0f;
    dtostrf(lcurrent, 2, 3, buf );
    client.publish("EPSolar/1/lcurrent", buf);

    pvvoltage = (long)node.getResponseBuffer(0x00) / 100.0f;
    dtostrf(pvvoltage, 2, 3, buf );
    client.publish("EPSolar/1/pvvoltage", buf);

    pvcurrent = (long)node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(pvcurrent, 2, 3, buf );
    client.publish("EPSolar/1/pvcurrent", buf);

    pvpower = ((long)node.getResponseBuffer(0x03) << 16 | node.getResponseBuffer(0x02)) / 100.0f;
    dtostrf(pvpower, 2, 3, buf );
    client.publish("EPSolar/1/pvpower", buf);

    battChargeCurrent = (long)node.getResponseBuffer(0x05) / 100.0f;
    dtostrf(battChargeCurrent, 2, 3, buf );
    client.publish("EPSolar/1/battChargeCurrent", buf);

  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_311A() {
  result = node.readInputRegisters(0x311A, 2);
  if (result == node.ku8MBSuccess)
  {
    bremaining = node.getResponseBuffer(0x00) / 1.0f;
    dtostrf(bremaining, 2, 3, buf );
    client.publish("EPSolar/1/bremaining", buf);//SOC

    btemp = node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(btemp, 2, 3, buf );
    client.publish("EPSolar/1/btemp", buf);

  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_9000() {
  result = node.readHoldingRegisters(0x9000, 14);
  client.publish("EPSolar/1/loop1", "9000");
  if (result == node.ku8MBSuccess)
  {
    client.publish("EPSolar/1/loop1", "9000 result");
    batt_cap = node.getResponseBuffer(0x01) / 1.0f;
    dtostrf(batt_cap, 2, 3, buf);
    client.publish("EPSolar/1/batt_cap", buf);

    batt_highdisc = node.getResponseBuffer(0x03) / 100.0f;
    dtostrf(batt_highdisc, 2, 3, buf);
    client.publish("EPSolar/1/batt_highdisc", buf);

    batt_chargelimit = node.getResponseBuffer(0x04) / 100.0f;
    dtostrf(batt_chargelimit, 2, 3, buf);
    client.publish("EPSolar/1/batt_chargelimit", buf);

    batt_overvoltrecon = node.getResponseBuffer(0x05) / 100.0f;
    dtostrf(batt_overvoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_overvoltrecon", buf);

    batt_equalvolt = node.getResponseBuffer(0x06) / 100.0f;
    dtostrf(batt_equalvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_equalvolt", buf);

    batt_boostvolt = node.getResponseBuffer(0x07) / 100.0f;
    dtostrf(batt_boostvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_boostvolt", buf);

    batt_floatvolt = node.getResponseBuffer(0x08) / 100.0f;
    dtostrf(batt_floatvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_floatvolt", buf);

    batt_boostrecon = node.getResponseBuffer(0x09) / 100.0f;
    dtostrf(batt_boostrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_boostrecon", buf);

    batt_lowvoltrecon = node.getResponseBuffer(0x0A) / 100.0f;
    dtostrf(batt_lowvoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_lowvoltrecon", buf);

    batt_undervoltrecon = node.getResponseBuffer(0x0B) / 100.0f;
    dtostrf(batt_undervoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_undervoltrecon", buf);

    batt_undervoltwarn = node.getResponseBuffer(0x0C) / 100.0f;
    dtostrf(batt_undervoltwarn, 2, 3, buf);
    client.publish("EPSolar/1/batt_undervoltwarn", buf);

    batt_lowvoltdisc = node.getResponseBuffer(0x0D) / 100.0f;
    dtostrf(batt_lowvoltdisc, 2, 3, buf);
    client.publish("EPSolar/1/batt_lowvoltdisc", buf);
  }
}

void AddressRegistry_3300() {
  result = node.readInputRegisters(0x3300, 2);
  if (result == node.ku8MBSuccess)
  {
    stats_today_pv_volt_max = node.getResponseBuffer(0x00) / 100.0f;
    dtostrf(stats_today_pv_volt_max, 2, 3, buf);
    client.publish("EPSolar/1/pv_max_volt", buf);
    //Serial.print("Stats Today PV Voltage MAX: ");
    //Serial.println(stats_today_pv_volt_max);

    stats_today_pv_volt_min = node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(stats_today_pv_volt_min, 2, 3, buf);
    client.publish("EPSolar/1/pv_min_volt", buf);
    //Serial.print("Stats Today PV Voltage MIN: ");
    //Serial.println(stats_today_pv_volt_min);

  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_3106()
{
  result = node.readInputRegisters(0x3106, 2);

  if (result == node.ku8MBSuccess) {
    battChargePower = (node.getResponseBuffer(0x00) | node.getResponseBuffer(0x01) << 16)  / 100.0f;
    dtostrf(battChargePower, 2, 3, buf);
    client.publish("EPSolar/1/batt_charge_power", buf);
    // Serial.print("Battery Charge Power: ");
    //Serial.println(battChargePower);
  }
}


void AddressRegistry_330C() {
  result = node.readInputRegisters(0x330C, 2);
  if (result == node.ku8MBSuccess) {
    Generated_energy_today = (node.getResponseBuffer(0x00) | node.getResponseBuffer(0x01) << 16)  / 100.0f;//node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(Generated_energy_today, 2, 3, buf);
    client.publish("EPSolar/1/Generated_energy_today", buf);

  }
}

void AddressRegistry_330E() {
  result = node.readInputRegisters(0x330E, 2);
  if (result == node.ku8MBSuccess) {
    Generated_energy_monthly = (node.getResponseBuffer(0x00) | node.getResponseBuffer(0x01) << 16)  / 100.0f;//node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(Generated_energy_monthly, 2, 3, buf);
    client.publish("EPSolar/1/Generated_energy_monthly", buf);

  }
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  timer.run();
}
