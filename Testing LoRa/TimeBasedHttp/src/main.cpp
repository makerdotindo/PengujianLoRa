// TTGO (KABEL KANTOR)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <axp20x.h>
#include <Wire.h>

// Data settings
const int LAHAN_ID = 1;
unsigned long lastSendTime = 0;
const long sendInterval = 120000;  //2 detik

// WiFi credentials
const char* ssid = "MAKER 2024";
const char* password = "Makerdotindo24";

// AXP20X BATTERY MANAGEMENT 
AXP20X_Class axp;
bool isPowerMonitorFound = false;

// Google Sheet configuration
String GOOGLE_SCRIPT_ID = "AKfycbztOhKDKJz0Q9EaVcqEx7dqX5m0mj0s5er7XacNxK4_M9C0akVOfOD7J6ZeKj8U9AgN";

void sendToGoogleSheet(String url);
void parseAndSendData();
void initPowerMonitor();
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);

void setup() {
  Serial.begin(115200);
  
  // AXP20X INIT
  initPowerMonitor();
  
  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to WiFi");
  
  Serial.println("Time-based Sensor Data Generator Started!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Send data every 5 seconds
  if (currentMillis - lastSendTime >= sendInterval) {
    parseAndSendData();
    lastSendTime = currentMillis;
  }
}

void parseAndSendData() {
  // Generate sensor data
  int lahanID = 1;
  float humidity = random(20, 35);
  float temperature = random(20, 35);
  float ec = random(0, 100);
  float ph = random(0, 14);
  float nitrogen = random(0, 5);
  float phosphorus = random(0, 10);
  float potassium = random(0, 15);

  // Get battery data
  float vbat = 0, batCurrent = 0, batPower = 0;
  int batChargeCurrent = 0, batLevel = 0;
  getBatteryStats(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

  // Create URL with all parameters
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + 
                "/exec?lahanID=" + String(lahanID) +
                "&humidity=" + String(humidity) +
                "&temperature=" + String(temperature) +
                "&ec=" + String(ec) +
                "&ph=" + String(ph) +
                "&nitrogen=" + String(nitrogen) +
                "&phosphorus=" + String(phosphorus) +
                "&potassium=" + String(potassium) +
                "&batteryVoltage=" + String(vbat) +
                "&batteryCurrent=" + String(batCurrent) +
                "&batteryPower=" + String(batPower) +
                "&batteryChargeCurrent=" + String(batChargeCurrent) +
                "&batteryLevel=" + String(batLevel);

  // Print sensor and battery info to Serial
  Serial.println("\n=== Sending Data ===");
  Serial.printf("Humidity: %.2f%%\n", humidity);
  Serial.printf("Temperature: %.2f°C\n", temperature);
  Serial.printf("EC: %.2f\n", ec);
  Serial.printf("pH: %.2f\n", ph);
  Serial.printf("NPK: %.2f, %.2f, %.2f\n", nitrogen, phosphorus, potassium);
  
  if (isPowerMonitorFound) {
    Serial.println("Battery Status:");
    Serial.printf("Voltage: %.2fV\n", vbat);
    Serial.printf("Current: %.2fmA\n", batCurrent);
    Serial.printf("Power: %.2fmW\n", batPower);
    Serial.printf("Charge Current: %dmA\n", batChargeCurrent);
    Serial.printf("Battery Level: %d%%\n", batLevel);
  }

  // Send data regardless of values
  sendToGoogleSheet(url);
}

void sendToGoogleSheet(String url) {
  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  Serial.println("Sending URL: " + url);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Data sent successfully: " + payload);
    } else {
      Serial.printf("HTTP Error: %d\n", httpCode);
    }
  } else {
    Serial.println("Failed to send data");
  }
  
  http.end();
}

void initPowerMonitor() {
    Wire.begin(21, 22); // SDA, SCL
    if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
        Serial.println("AXP192 Power monitor initialized!");
        isPowerMonitorFound = true;
        
        // Configure power output
        axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
        axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
        axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
        axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
        axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
        axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
    } else {
        Serial.println("AXP192 Power monitor not found!");
        isPowerMonitorFound = false;
    }
}

void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel) {
    if (!isPowerMonitorFound) {
        vbat = 0;
        batCurrent = 0;
        batPower = 0;
        batChargeCurrent = 0;
        batLevel = 0;
        return;
    }
    
    vbat = axp.getBattVoltage() / 1000.0;
    batCurrent = axp.getBattDischargeCurrent();
    batPower = axp.getBattInpower();
    batChargeCurrent = axp.getBattChargeCurrent();
    
    // Calculate battery level (4.2V = 100%, 3.3V = 0%)
    batLevel = map(vbat * 1000, 3300, 4200, 0, 100);
    batLevel = constrain(batLevel, 0, 100);
}