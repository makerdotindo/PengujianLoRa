#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <axp20x.h>
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi settings
const char *ssid = "MAKER 2024";
const char *password = "Makerdotindo24";

// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// MQTT Topic
const char *topic = "EventBasedMqtt";

// Data settings
const int LAHAN_ID = 1;
const float PROXIMITY_THRESHOLD = 1.0;
unsigned long lastSendTime = 0;
const long sendInterval = 120000;

// Store previous valid data
float previousHumidity = 0;
bool hasValidPreviousData = false;

// AXP20X BATTERY MANAGEMENT 
AXP20X_Class axp;
bool isPowerMonitorFound = false;

// Initialize clients
WiFiClient espClient;
PubSubClient client(espClient);
    // Flag untuk mengecek apakah ada data sebelumnya

void publishData();
void initPowerMonitor();
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);


void setup() {
    Serial.begin(115200);
    
    // Initialize power monitor
    initPowerMonitor();
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the Wi-Fi network");
    
    // Setup MQTT
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    
    // Connect to MQTT broker
    reconnect();
    
    Serial.println("MQTT Sensor Publisher Started!");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= sendInterval) {
        publishData();
        lastSendTime = currentMillis;
    }
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

void initPowerMonitor() {
    Wire.begin(21, 22);
    if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
        Serial.println("AXP192 Power monitor initialized!");
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
    
    batLevel = map(vbat * 1000, 3300, 4200, 0, 100);
    batLevel = constrain(batLevel, 0, 100);
}

void publishData() {
    StaticJsonDocument<512> doc;
    
    // Simulate humidity reading (replace with actual sensor reading)
    float humidity = random(20, 35);
    
    // Cek proximity dengan data sebelumnya
    bool isNearPrevious = hasValidPreviousData &&
                         abs(humidity - previousHumidity) <= PROXIMITY_THRESHOLD;

    if (humidity > 0) {
        if (isNearPrevious) {
            Serial.printf("Humidity (%.2f) near previous value (%.2f), skipping publish\n",
                        humidity, previousHumidity);
            return; // Skip publishing if value is too close to previous
        } else {
            // Update previous values
            previousHumidity = humidity;
            hasValidPreviousData = true;
            
            // Prepare new data to publish
            doc["lahanID"] = LAHAN_ID;
            doc["humidity"] = humidity;
            doc["temperature"] = random(20, 35);
            doc["ec"] = random(0, 100);
            doc["ph"] = random(0, 14);
            doc["nitrogen"] = random(0, 5);
            doc["phosphorus"] = random(0, 10);
            doc["potassium"] = random(0, 15);

            // Battery data
            float vbat = 0, batCurrent = 0, batPower = 0;
            int batChargeCurrent = 0, batLevel = 0;
            getBatteryStats(vbat, batCurrent, batPower, batChargeCurrent, batLevel);

            doc["batteryVoltage"] = vbat;
            doc["batteryCurrent"] = batCurrent;
            doc["batteryPower"] = batPower;
            doc["batteryChargeCurrent"] = batChargeCurrent;
            doc["batteryLevel"] = batLevel;

            char json[512];
            serializeJson(doc, json);
            
            client.publish(topic, json);
            Serial.printf("New humidity (%.2f) differs significantly, publishing new data: %s\n", 
                        humidity, json);
        }
    } else {
        Serial.printf("Humidity below threshold (%.2f), no data published\n", humidity);
    }
}

void reconnect() {
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
            client.subscribe(topic);
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }
}
