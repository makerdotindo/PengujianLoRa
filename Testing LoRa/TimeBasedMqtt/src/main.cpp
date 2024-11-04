#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <axp20x.h>
#include <Wire.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "MAKER 2024";
const char* password = "Makerdotindo24";

// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// MQTT Topic
const char *topic = "TimeBasedMqtt";

// Data settings
const int LAHAN_ID = 1;
unsigned long lastSendTime = 0;
const long sendInterval = 120000;  // 5 seconds interval

// WiFi reconnect settings
unsigned long lastWiFiCheckTime = 0;
const long wifiCheckInterval = 30000;  // Check WiFi every 30 seconds

// AXP20X BATTERY MANAGEMENT 
AXP20X_Class axp;
bool isPowerMonitorFound = false;

WiFiClient espClient;
PubSubClient client(espClient);

// Function declarations remain the same
void setup_wifi();
void reconnect();
void sendSensorData();
void initPowerMonitor();
void getBatteryStats(float &vbat, float &batCurrent, float &batPower, int &batChargeCurrent, int &batLevel);

void setup() {
  Serial.begin(115200);
  
  // AXP20X INIT
  initPowerMonitor();
  
  setup_wifi();
  client.setServer(mqtt_broker, mqtt_port);
  
  Serial.println("MQTT-based Sensor Data Generator Started!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Check WiFi connection
  if (currentMillis - lastWiFiCheckTime >= wifiCheckInterval) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      setup_wifi();
    }
    lastWiFiCheckTime = currentMillis;
  }

  // Check MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Handle MQTT keep-alive

  // Send data every 5 seconds
  if (currentMillis - lastSendTime >= sendInterval) {
    sendSensorData();
    lastSendTime = currentMillis;
  }
}

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi..");
  WiFi.begin(ssid, password);
  
  // Wait up to 20 seconds for connection
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 40) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void reconnect() {
  int attempts = 0;
  while (!client.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Try to connect with username and password
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
    attempts++;
  }
}


void sendSensorData() {
  // Generate sensor data
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

  // Create JSON object
  StaticJsonDocument<256> doc;
  doc["lahanID"] = LAHAN_ID;
  doc["humidity"] = humidity;
  doc["temperature"] = temperature;
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

  // Publish the message to MQTT broker
  if (client.connected()) {
    if (client.publish(topic, jsonString.c_str())) {
      Serial.println("Message published successfully");
    } else {
      Serial.println("Failed to publish message");
    }
  } else {
    Serial.println("Client not connected. Cannot publish message.");
  }
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