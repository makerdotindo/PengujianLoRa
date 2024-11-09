#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <axp20x.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

#define DHTPIN 25     // Pin where the DHT sensor is connected
#define DHTTYPE DHT22 // Change to DHT11 if you are using a DHT11

DHT dht(DHTPIN, DHTTYPE);
bool hasValidPreviousData = false;
float previousTemp = 0.0;

// Constants
const int LAHAN_ID = 1;
const float PROXIMITY_THRESHOLD = 0.8; // Significant difference threshold
const char* SSID = "MAKER 2024";
const char* PASSWORD = "Makerdotindo24";
const String GOOGLE_SCRIPT_ID = "AKfycbxGHNdjSwgf25ju2himYqfTNPLvRr3q6elo4Dy126nzG7861HWMGtzQKY2XG9s5_Y9Y";
float temp;
float humidity;
String previousJsonString;


// AXP20X Battery Management
AXP20X_Class axp;
bool isPowerMonitorFound = false;

void connectToWiFi();
void parseAndSendData();
String constructUrl(float humidity, float temperature, float ec, float ph, float nitrogen, float phosphorus, float potassium, float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel);
void logSensorData(float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel);
float randomFloat(float min, float max);
void initPowerMonitor();
void sendToGoogleSheet(String url);
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);
bool significantChange(float humidity);

void setup() {
    Serial.begin(115200);
    dht.begin(); // Initialize the DHT sensor
    initPowerMonitor();
    connectToWiFi();
    Serial.println("Event-based Sensor Data Generator Started!");
}

void loop() {
    temp = dht.readTemperature();
    humidity = dht.readHumidity();

    // Check for valid readings
    if (isnan(temp) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    Serial.print("Temperature: ");
    Serial.println(temp);
    Serial.print("Humidity: ");
    Serial.println(humidity);

    parseAndSendData();
    
    delay(10000); // Delay between data parsing
}

void connectToWiFi() {
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void parseAndSendData() {
    float ec = randomFloat(0, 100);
    float ph = randomFloat(0, 14);
    float nitrogen = randomFloat(0, 5);
    float phosphorus = randomFloat(0, 10);
    float potassium = randomFloat(0, 15);

    // Battery Stats
    float vbat, batCurrent, batPower;
    int batChargeCurrent, batLevel;
    getBatteryStats(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

    String url = constructUrl(humidity, temp, ec, ph, nitrogen, phosphorus, potassium, vbat, batCurrent, batPower, batChargeCurrent, batLevel);
    logSensorData(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

    if (!hasValidPreviousData || significantChange(temp)) {
        previousJsonString = url;
        previousTemp = temp;
        hasValidPreviousData = true;
        Serial.println("Sending data...");
        sendToGoogleSheet(url);
    } else {
        Serial.printf("No significant change (diff: %.2f). Previous: %.2f, Current: %.2f\n", abs(temp - previousTemp), previousTemp, temp);
    }
}

String constructUrl(float humidity, float temperature, float ec, float ph, float nitrogen, float phosphorus, float potassium, float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel) {
    return String("https://script.google.com/macros/s/") + GOOGLE_SCRIPT_ID +
           "/exec?lahanID=" + String(LAHAN_ID) +
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
}

void logSensorData(float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel) {
    Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temp, humidity);
    if (isPowerMonitorFound) {
        Serial.println("Battery Status:");
        Serial.printf("Voltage: %.2fV\nCurrent: %.2fmA\nPower: %.2fmW\nCharge Current: %dmA\nBattery Level: %d%%\n",
                      vbat, batCurrent, batPower, batChargeCurrent, batLevel);
    }
}

bool significantChange(float currentValue) {
    return abs(currentValue - previousTemp) > PROXIMITY_THRESHOLD;
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
        Serial.println("AXP PASS!");
        isPowerMonitorFound = true;
        return;
    }
    Serial.println("AXP192 Power monitor initialized!");
    isPowerMonitorFound = true;

    // Configure power outputs
    axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
    axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
    axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
    axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
    axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
    axp.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
}

void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel) {
    if (!isPowerMonitorFound) {
        vbat = batCurrent = batPower = 0;
        batChargeCurrent = batLevel = 0;
        return;
    }

    vbat = axp.getBattVoltage() / 1000.0;
    batCurrent = axp.getBattDischargeCurrent();
    batPower = axp.getBattInpower();
    batChargeCurrent = axp.getBattChargeCurrent();
    batLevel = constrain(map(vbat * 1000, 3300, 4200, 0, 100), 0, 100);
}

float randomFloat(float min, float max) {
    return (float)random(min * 100, max * 100) / 100;
}
