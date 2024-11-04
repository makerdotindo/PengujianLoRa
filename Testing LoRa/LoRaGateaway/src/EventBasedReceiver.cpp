#include <Arduino.h>
#include <WiFi.h>
#include <LoRa.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <axp20x.h>

AXP20X_Class axp;

// WiFi credentials
const char* ssid = "MAKER 2024";
const char* password = "Makerdotindo24";

// Google Sheet configuration
String GOOGLE_SCRIPT_ID = "AKfycbx8IIsDeHu9XjGmfTL1yWaWqkSl28-4nY11GByRAMe-zrF0nje-RX9wd2QwpMJRew8a";

// LoRa pins configuration
#define SS 18
#define RST 14
#define DIO0 26

void setupLoRa();
void sendToGoogleSheet(String jsonData);
void parseAndSendData(String jsonData);
void getBatteryInfo(JsonObject& battery);

void setup() {
  Serial.begin(115200);

  // Initialize I2C for AXP192
  Wire.begin(21, 22);
  if(axp.begin(Wire, AXP192_SLAVE_ADDRESS) == AXP_FAIL) {
    Serial.println(F("failed to initialize communication with AXP192"));
  }

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Setup LoRa
  setupLoRa();
  Serial.println("LoRa Receiver Ready!");
}

void getBatteryInfo(JsonObject& battery) {
  // Get battery voltage in mV
  float batteryVoltage = axp.getBattVoltage();
  
  // Get battery percentage (approximate calculation)
  float batteryPercentage = (batteryVoltage - 3300) / (4200 - 3300) * 100;
  if (batteryPercentage > 100) batteryPercentage = 100;
  if (batteryPercentage < 0) batteryPercentage = 0;
  
  // Get charge and discharge current in mA
  float chargeCurrent = axp.getBattChargeCurrent();
  float dischargeCurrent = axp.getBattDischargeCurrent();

  // Store battery information in JSON
  battery["voltage"] = batteryVoltage;
  battery["dischargeCurrent"] = dischargeCurrent;
  battery["percentage"] = batteryPercentage;
  battery["chargeCurrent"] = chargeCurrent;

  // Print battery information to Serial
  Serial.println("Receiver Battery Information:");
  Serial.printf("Voltage: %.2f mV\n", batteryVoltage);
  Serial.printf("Percentage: %.2f%%\n", batteryPercentage);
  Serial.printf("Charge Current: %.2f mA\n", chargeCurrent);
  Serial.printf("Discharge Current: %.2f mA\n", dischargeCurrent);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // Read received LoRa packet
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }
    
    Serial.println("Received LoRa data: " + receivedData);
    
    // Parse and send to Google Sheet
    parseAndSendData(receivedData);
  }
}

void setupLoRa() {
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa initialized successfully");
}

void parseAndSendData(String jsonData) {
  StaticJsonDocument<300> doc;
  StaticJsonDocument<200> batteryDoc;
  
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }

  // Extract sensor values
  int lahanID = doc["lahanID"];
  JsonObject sensor = doc["sensor"];
  float humidity = sensor["Humidity"];
  float temperature = sensor["Temperature"];
  float ec = sensor["Ec"];
  float ph = sensor["Ph"];
  float nitrogen = sensor["Nitrogen"];
  float phosphorus = sensor["Phosporus"];
  float potassium = sensor["Kalium"];

  // Get receiver's battery information
  JsonObject battery = batteryDoc.createNestedObject("battery");
  getBatteryInfo(battery);

  // Create URL for Google Sheet with added battery parameters
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + 
              "/exec?lahanID=" + String(lahanID) +
              "&humidity=" + String(humidity) +
              "&temperature=" + String(temperature) +
              "&ec=" + String(ec) +
              "&ph=" + String(ph) +
              "&nitrogen=" + String(nitrogen) +
              "&phosphorus=" + String(phosphorus) +
              "&potassium=" + String(potassium) +
              "&batteryVoltage=" + String(battery["voltage"].as<float>()) +
              "&batteryPercentage=" + String(battery["percentage"].as<float>()) +
              "&batteryChargeCurrent=" + String(battery["chargeCurrent"].as<float>()) +
              "&batteryDischargeCurrent=" + String(battery["dischargeCurrent"].as<float>());

  sendToGoogleSheet(url);
}

void sendToGoogleSheet(String url) {
  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Data sent to Google Sheet: " + payload);
    } else {
      Serial.printf("HTTP Error: %d\n", httpCode);
    }
  } else {
    Serial.println("Failed to send data to Google Sheet");
  }
  
  http.end();
}