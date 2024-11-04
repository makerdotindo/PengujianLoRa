#include <Arduino.h>
#include <axp20x.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LoRa.h>

// Data settings
const int LAHAN_ID = 1;
const unsigned long SEND_INTERVAL = 120000; // 2 minutes in milliseconds
unsigned long lastSendTime = 0;

// LoRa settings
#define SS 18
#define RST 14
#define DIO0 26
#define SCK 5
#define MISO 19
#define MOSI 27

AXP20X_Class axp;

void setupLoRa();
void sendLoRaMessage(String message);
void generateAndSendData();
void getBatteryInfo(JsonObject& battery);

float randomFloat(float min, float max) {
  return (float)random(min * 100, max * 100) / 100;
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  // Initialize I2C for AXP192
  Wire.begin(21, 22);
  if (axp.begin(Wire, AXP192_SLAVE_ADDRESS) == AXP_FAIL) {
    Serial.println(F("Failed to initialize communication with AXP192"));
  }

  // Setup LoRa
  setupLoRa();

  Serial.println("Periodic Sensor Data Transmitter Started!");
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    generateAndSendData();
    lastSendTime = currentTime;
  }
}

void setupLoRa() {
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa initialized");
}

void getBatteryInfo(JsonObject& battery) {
  float batteryVoltage = axp.getBattVoltage();
  float batteryPercentage = (batteryVoltage - 3300) / (4200 - 3300) * 100;
  if (batteryPercentage > 100) batteryPercentage = 100;
  if (batteryPercentage < 0) batteryPercentage = 0;
  
  float chargeCurrent = axp.getBattChargeCurrent();
  float dischargeCurrent = axp.getBattDischargeCurrent();

  battery["voltage"] = batteryVoltage;
  battery["dischargeCurrent"] = dischargeCurrent;
  battery["percentage"] = batteryPercentage;
  battery["chargeCurrent"] = chargeCurrent;
}

void sendLoRaMessage(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println("LoRa message sent: " + message);
}

void generateAndSendData() {
  StaticJsonDocument<300> doc;

  // Generate sensor data
  float humidity = randomFloat(20, 35);
  float temperature = randomFloat(20, 35);
  float ec = randomFloat(0, 100);
  float ph = randomFloat(0, 14);
  float nitrogen = randomFloat(0, 5);
  float phosphorus = randomFloat(0, 10);
  float potassium = randomFloat(0, 15);

  doc["type"] = "sensor";
  doc["lahanID"] = LAHAN_ID;

  JsonObject sensor = doc.createNestedObject("sensor");
  sensor["Humidity"] = humidity;
  sensor["Temperature"] = temperature;
  sensor["Ec"] = ec;
  sensor["Ph"] = ph;
  sensor["Nitrogen"] = nitrogen;
  sensor["Phosporus"] = phosphorus;
  sensor["Kalium"] = potassium;

  // Add battery information
  JsonObject battery = doc.createNestedObject("battery");
  getBatteryInfo(battery);

  String jsonString;
  serializeJson(doc, jsonString);
  sendLoRaMessage(jsonString);

  // Print the data to Serial for debugging
  serializeJsonPretty(doc, Serial);
  Serial.println();
  Serial.printf("Data sent at: %lu ms\n", millis());
}