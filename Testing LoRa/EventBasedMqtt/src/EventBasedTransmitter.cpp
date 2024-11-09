#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <axp20x.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

// Pin Definitions
#define DHTPIN 25     // Pin where the DHT sensor is connected
#define DHTTYPE DHT22 // DHT22 (AM2302) sensor type

// Constants
const int LAHAN_ID = 1;
const float PROXIMITY_THRESHOLD = 0.8;

// WiFi settings
const char* SSID = "MAKER 2024";
const char* PASSWORD = "Makerdotindo24";

// MQTT Settings
const char* MQTT_BROKER = "broker.emqx.io";
const char* MQTT_USERNAME = "emqx";
const char* MQTT_PASSWORD = "public";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "EventBasedMqtt";

// Global variables
DHT dht(DHTPIN, DHTTYPE);
bool hasValidPreviousData = false;
float previousTemp = 0.0;
float temp;
float humidity;

// AXP20X Battery Management
AXP20X_Class axp;
bool isPowerMonitorFound = false;

// Initialize clients
WiFiClient espClient;
PubSubClient client(espClient);

// Function declarations
void connectToWiFi();
void parseAndSendData();
void initPowerMonitor();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);
float randomFloat(float min, float max);
bool significantChange(float currentValue);
void logSensorData(float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel);

void setup() {
    Serial.begin(115200);
    dht.begin();
    initPowerMonitor();
    connectToWiFi();
    
    // Setup MQTT
    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setCallback(callback);
    
    Serial.println("Event-based MQTT Transmitter Started!");
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
    
    temp = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    parseAndSendData();
    delay(10000);
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
    // Generate random sensor data (similar to main.cpp)
    float ec = randomFloat(0, 100);
    float ph = randomFloat(0, 14);
    float nitrogen = randomFloat(0, 5);
    float phosphorus = randomFloat(0, 10);
    float potassium = randomFloat(0, 15);

    // Get battery stats
    float vbat, batCurrent, batPower;
    int batChargeCurrent, batLevel;
    getBatteryStats(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

    // Log sensor data
    logSensorData(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

    if (!hasValidPreviousData || significantChange(temp)) {
        // Create JSON document
        StaticJsonDocument<512> doc;
        doc["lahanID"] = LAHAN_ID;
        doc["humidity"] = humidity;
        doc["temperature"] = temp;
        doc["ec"] = ec;
        doc["ph"] = ph;
        doc["nitrogen"] = nitrogen;
        doc["phosphorus"] = phosphorus;
        doc["potassium"] = potassium;
        doc["batteryVoltage"] = vbat;
        doc["batteryCurrent"] = batCurrent;
        doc["batteryPower"] = batPower;
        doc["batteryChargeCurrent"] = batChargeCurrent;
        doc["batteryLevel"] = batLevel;

        // Serialize JSON to string
        String jsonString;
        serializeJson(doc, jsonString);

        // Publish to MQTT
        if (client.publish(MQTT_TOPIC, jsonString.c_str())) {
            Serial.println("Data published successfully");
            previousTemp = temp;
            hasValidPreviousData = true;
        } else {
            Serial.println("Failed to publish data");
        }
    } else {
        Serial.printf("No significant change (diff: %.2f). Previous: %.2f, Current: %.2f\n", 
                     abs(temp - previousTemp), previousTemp, temp);
    }
}

void reconnectMQTT() {
    while (!client.connected()) {
        String clientId = "esp32-client-";
        clientId += String(WiFi.macAddress());
        
        Serial.printf("Attempting MQTT connection as %s...\n", clientId.c_str());
        
        if (client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe(MQTT_TOPIC);
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(client.state());
            Serial.println(" Retrying in 2 seconds");
            delay(2000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void initPowerMonitor() {
    Wire.begin(21, 22);
    if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
        Serial.println("AXP PASS!");
        isPowerMonitorFound = true;

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

void logSensorData(float vbat, float batCurrent, float batPower, int batChargeCurrent, int batLevel) {
    Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temp, humidity);
    if (isPowerMonitorFound) {
        Serial.println("Battery Status:");
        Serial.printf("Voltage: %.2fV\nCurrent: %.2fmA\nPower: %.2fmW\nCharge Current: %dmA\nBattery Level: %d%%\n",
                     vbat, batCurrent, batPower, batChargeCurrent, batLevel);
    }
}

float randomFloat(float min, float max) {
    return (float)random(min * 100, max * 100) / 100;
}

bool significantChange(float currentValue) {
    return abs(currentValue - previousTemp) > PROXIMITY_THRESHOLD;
}