//ttgo 
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <axp20x.h>
#include <Wire.h>

// Data settings
const int LAHAN_ID = 1;
// const float HUMIDITY_THRESHOLD = 30.0;     // Threshold for sending data
const float PROXIMITY_THRESHOLD = 1.0; // If new value is within this range of previous, use previous
unsigned long lastSendTime = 0;
const long sendInterval = 120000; // Ganti jadi 2 menit (120000)

// Store previous valid data
String previousJsonString = "";
float previousHumidity = 0;
bool hasValidPreviousData = false;

// WiFi credentials
const char* ssid = "MAKER 2024";
const char* password = "Makerdotindo24";

// AXP20X BATTERY MANAGEMENT 
AXP20X_Class axp;
bool isPowerMonitorFound = false;

// Google Sheet configuration
String GOOGLE_SCRIPT_ID = "AKfycbxGHNdjSwgf25ju2himYqfTNPLvRr3q6elo4Dy126nzG7861HWMGtzQKY2XG9s5_Y9Y";

float randomFloat(float min, float max)
{
  return (float)random(min * 100, max * 100) / 100;
}

void generateAndSendData();
void sendToGoogleSheet(String jsonString);
void parseAndSendData();
void initPowerMonitor();
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);

void setup()
{
  Serial.begin(115200);
  // AXP20X INIT
  initPowerMonitor();
    // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to WiFi");
  
  Serial.println("Event-based Sensor Data Generator Started!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Kirim data jika interval waktu telah berlalu
  if (currentMillis - lastSendTime >= sendInterval) {
    parseAndSendData();
    lastSendTime = currentMillis; // Perbarui waktu pengiriman terakhir
  }
}

void parseAndSendData() {
  // Data dummy sensor
  int lahanID = 1;
  float humidity = randomFloat(20, 35);
  float temperature = randomFloat(20, 35);
  float ec = randomFloat(0, 100);
  float ph = randomFloat(0, 14);
  float nitrogen = randomFloat(0, 5);
  float phosphorus = randomFloat(0, 10);
  float potassium = randomFloat(0, 15);

  // Data baterai
  float vbat = 0, batCurrent = 0, batPower = 0;
  int batChargeCurrent = 0, batLevel = 0;
  getBatteryStats(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

  // Buat URL dengan parameter termasuk data baterai
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

  // Print battery info to Serial
  if (isPowerMonitorFound) {
    Serial.println("Battery Status:");
    Serial.printf("Voltage: %.2fV\n", vbat);
    Serial.printf("Current: %.2fmA\n", batCurrent);
    Serial.printf("Power: %.2fmW\n", batPower);
    Serial.printf("Charge Current: %dmA\n", batChargeCurrent);
    Serial.printf("Battery Level: %d%%\n", batLevel);
  }

  // Cek proximity dengan data sebelumnya
  bool isNearPrevious = hasValidPreviousData &&
                        abs(humidity - previousHumidity) <= PROXIMITY_THRESHOLD;

  if (humidity > 0) {
    if (isNearPrevious) {
      Serial.printf("Humidity (%.2f) near previous value (%.2f), sending previous data\n",
                    humidity, previousHumidity);
      sendToGoogleSheet(previousJsonString);
    } else {
      previousJsonString = url;
      previousHumidity = humidity;
      hasValidPreviousData = true;
      
      Serial.printf("New humidity (%.2f) differs significantly, sending new data\n", humidity);
      sendToGoogleSheet(url);
    }
  } else {
    Serial.printf("Humidity below threshold (%.2f), no data sent\n", humidity);
  }
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