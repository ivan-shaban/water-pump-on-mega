#include "credentials.h"
#include <WiFiEspAT.h>
#include <PubSubClient.h>

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1

#include "SoftwareSerial.h"

SoftwareSerial Serial1(6, 7); // RX, TX
#endif

#define STRING_LEN 128

#define MQTT_RECONNECT_TIMEOUT 5000
#define MQTT_UPDATE_TIMEOUT 1000
#define MQTT_PING_TIMEOUT 500
//#define MQTT_PING_TIMEOUT 30000
#define WATER_SENSOR_TIMEOUT 300

#define MIN_WATER_LEVEL_1 12
#define MAX_WATER_LEVEL_1 13

#define MIN_WATER_LEVEL_2 14
#define MAX_WATER_LEVEL_2 15

#define RELAY_PORT_1 A7
#define RELAY_PORT_2 A6
#define RELAY_PORT_3 A5
#define RELAY_PORT_4 A4
#define RELAY_PORT_5 A3
#define RELAY_PORT_6 A2
#define RELAY_PORT_7 A1
#define RELAY_PORT_8 A0

// network section
int status = WL_IDLE_STATUS;     // the Wifi radio's status

// mqtt section
unsigned long mqttReconnectTimestamp = 0;
unsigned long mqttUpdateTimestamp = 0;
unsigned long mqttPingTimestamp = 0;
char mqttPingTopic[STRING_LEN];
char mqttWaterPumpActionTopic[STRING_LEN];
char mqttWaterPumpMinStatusTopic[STRING_LEN];
char mqttWaterPumpMaxStatusTopic[STRING_LEN];
char mqttWaterPumpStatusTopic[STRING_LEN];
char mqttWaterPumpWorkTimeTopic[STRING_LEN];

const int MIN_VOLTS_VALUE = 100;
bool isWaterPump1Enabled = false;
bool isWaterPump2Enabled = false;
long waterSensorTimestamp = 0;
long lastWaterPumpStartTimestamp = 0;


void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
}


WiFiClient espClient;
PubSubClient mqttClient(MQTT_SERVER_IP, 1883, callback, espClient);

void setupWifi() {
    // initialize ESP module
    WiFi.init(&Serial3);

    // check for the presence of the shield
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
        // don't continue
        while (true);
    }

    WiFi.disconnect();                  // to clear the way. not persistent
    WiFi.setPersistent();               // set the following WiFi connection as persistent
    WiFi.endAP();                       // to disable default automatic start of persistent AP at startup

    WiFi.hostname(DEVICE_PUBLIC_NAME);
    // setting up static ip for our device
    WiFi.config(DEVICE_IP, GATEWAY_IP, GATEWAY_IP, NETWORK_MASK);

    // attempt to connect to WiFi network
    while (status != WL_CONNECTED) {
        Serial.println();
        Serial.print("Attempting to connect to Wifi: ");
        Serial.print(WIFI_SSID);
        Serial.print(" \\ ");
        Serial.println(WIFI_PASS);
        // Connect to WPA/WPA2 network
        status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    // you're connected now, so print out the data
    Serial.println("You're connected to the network");

    IPAddress localIP = WiFi.localIP();
    Serial.print("\tIP Address: ");
    Serial.println(localIP);
}

boolean reconnectMQTT() {
    Serial.println("\tAttempting MQTT connection...");

    if (mqttClient.connect(MQTT_CLIENT_NAME)) {
        // create all topics to prevent construction in runtime
        String temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/ping");
        temp.toCharArray(mqttPingTopic, STRING_LEN);

        temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/wp1");
        temp.toCharArray(mqttWaterPumpActionTopic, STRING_LEN);

        temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/wp1/min");
        temp.toCharArray(mqttWaterPumpMinStatusTopic, STRING_LEN);

        temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/wp1/max");
        temp.toCharArray(mqttWaterPumpMaxStatusTopic, STRING_LEN);

        temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/wp1/status");
        temp.toCharArray(mqttWaterPumpStatusTopic, STRING_LEN);

        temp = String(MQTT_TOPIC_PREFIX) + String(MQTT_CLIENT_NAME) + String("/wp1/worktime");
        temp.toCharArray(mqttWaterPumpWorkTimeTopic, STRING_LEN);

        Serial.println("MQTT connected");

        // Once connected, publish an announcement...
        mqttClient.publish(mqttPingTopic, 1, true);
        mqttClient.subscribe(mqttWaterPumpActionTopic);
    }
    return mqttClient.connected();
}

void handleMQTT(long now) {
    if (now - mqttUpdateTimestamp > MQTT_UPDATE_TIMEOUT) {
        mqttUpdateTimestamp = now;

        if (!mqttClient.connected()) {
            Serial.println("\tdisconnected from mqtt");
            if (now - mqttReconnectTimestamp > MQTT_RECONNECT_TIMEOUT) {
                mqttReconnectTimestamp = now;
                // Attempt to reconnect
                if (reconnectMQTT()) {
                    mqttReconnectTimestamp = 0;
                } else {
                    Serial.print("failed, rc=");
                    Serial.print(mqttClient.state());
                    Serial.println(" try again in 5 seconds");
                }
            }
        } else {
            if (now - mqttPingTimestamp > MQTT_PING_TIMEOUT) {
                mqttPingTimestamp = now;
                Serial.println("\tping");
                mqttClient.publish(mqttPingTopic, 1, true);
            }
            mqttClient.loop();
        }
    }
}

void handlerPump(long now) {
    if (now - waterSensorTimestamp > WATER_SENSOR_TIMEOUT) {
        waterSensorTimestamp = now;

        bool minWaterLevel1 = analogRead(MIN_WATER_LEVEL_1) > MIN_VOLTS_VALUE;
        bool maxWaterLevel1 = analogRead(MAX_WATER_LEVEL_1) > MIN_VOLTS_VALUE;
        bool minWaterLevel2 = analogRead(MIN_WATER_LEVEL_2) > MIN_VOLTS_VALUE;
        bool maxWaterLevel2 = analogRead(MAX_WATER_LEVEL_2) > MIN_VOLTS_VALUE;
        Serial.print("min: ");
        Serial.print(minWaterLevel1);
        Serial.print(", max: ");
        Serial.println(maxWaterLevel1);

        mqttClient.publish(mqttWaterPumpMinStatusTopic, minWaterLevel1 ? 1 : 0, true);
        mqttClient.publish(mqttWaterPumpMaxStatusTopic, maxWaterLevel1 ? 1 : 0, true);

        if (!minWaterLevel1 && !isWaterPump1Enabled) {
            isWaterPump1Enabled = true;
            lastWaterPumpStartTimestamp = now;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);

            Serial.println("\tpump is ON");

            mqttClient.publish(mqttWaterPumpStatusTopic, 1, true);
        } else if (maxWaterLevel1 && isWaterPump1Enabled) {
            isWaterPump1Enabled = false;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);

            Serial.print("\tpump is OFF");
            Serial.print(", work time: ");
            Serial.println(now - lastWaterPumpStartTimestamp);

            mqttClient.publish(mqttWaterPumpStatusTopic, 0, true);
            mqttClient.publish(mqttWaterPumpWorkTimeTopic, now - lastWaterPumpStartTimestamp, true);
        }
    }
}

void setup() {
    Serial.begin(115200);                   // initialize serial for debugging
    Serial3.begin(115200);                  // initialize serial for ESP module

    pinMode(RELAY_PORT_1, OUTPUT);          // 8 relay output
    pinMode(RELAY_PORT_2, OUTPUT);          // 7 relay output
    pinMode(RELAY_PORT_3, OUTPUT);          // 6 relay output
    pinMode(RELAY_PORT_4, OUTPUT);          // 5 relay output
    pinMode(RELAY_PORT_5, OUTPUT);          // 4 relay output
    pinMode(RELAY_PORT_6, OUTPUT);          // 3 relay output
    pinMode(RELAY_PORT_7, OUTPUT);          // 2 relay output
    pinMode(RELAY_PORT_8, OUTPUT);          // 1 relay output

    pinMode(MIN_WATER_LEVEL_1, INPUT);      // water sensor
    pinMode(MAX_WATER_LEVEL_1, INPUT);      // water sensor
    pinMode(MIN_WATER_LEVEL_2, INPUT);      // water sensor
    pinMode(MAX_WATER_LEVEL_2, INPUT);      // water sensor

    // отключаем порты реле
    digitalWrite(RELAY_PORT_1, HIGH);
    digitalWrite(RELAY_PORT_2, HIGH);
    digitalWrite(RELAY_PORT_3, HIGH);
    digitalWrite(RELAY_PORT_4, HIGH);
    digitalWrite(RELAY_PORT_5, HIGH);
    digitalWrite(RELAY_PORT_6, HIGH);
    digitalWrite(RELAY_PORT_7, HIGH);
    digitalWrite(RELAY_PORT_8, HIGH);

    setupWifi();
}

void loop() {
    long now = millis();

    handleMQTT(now);
    handlerPump(now);
}
