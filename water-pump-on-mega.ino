#include "WiFiEsp.h"
#include <PubSubClient.h>
#include "Data.h"
#include "credentials.h"

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1

#include "SoftwareSerial.h"

SoftwareSerial Serial1(6, 7); // RX, TX
#endif

#define MQTT_RECONNECT_TIMEOUT 5000
#define MQTT_UPDATE_TIMEOUT 1000
#define MQTT_PING_TIMEOUT 30000
#define MQTT_MAX_MESSAGES_COUNT 100
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
long mqttReconnectTimestamp = 0;
long mqttUpdateTimestamp = 0;
long mqttPingTimestamp = 0;
long mqttMessagesToSendCount = 0;
char *mqttTopics[MQTT_MAX_MESSAGES_COUNT];
char *mqttPayloads[MQTT_MAX_MESSAGES_COUNT];

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

// Initialize the Ethernet client object
WiFiEspClient espClient;
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
    // Create a random client ID
//    String clientId = "ESP8266Client-";
//    clientId += String(random(0xffff), HEX);
//    if (mqttClient.connect(clientId.c_str())) {
    if (mqttClient.connect(MQTT_CLIENT_NAME)) {
        Serial.println("MQTT connected");
        // Once connected, publish an announcement...
        mqttClient.publish("kaz9/mega/ping", 1);
        mqttClient.subscribe("kaz9/mega/water-pump");
    }
    return mqttClient.connected();
}

void logToMQTT(char *topic, char *payload) {
    mqttClient.publish(topic, payload);
//    if (mqttMessagesToSendCount < MQTT_MAX_MESSAGES_COUNT) {
//        mqttMessagesToSendCount++;
//        mqttTopics[mqttMessagesToSendCount] = topic;
//        mqttPayloads[mqttMessagesToSendCount] = payload;
//    }
}

void handleMQTT(long now) {
    if (now - mqttUpdateTimestamp > MQTT_UPDATE_TIMEOUT) {
        if (!mqttClient.connected()) {
            Serial.println("\tdisconnected from mqtt");
            if (now - mqttReconnectTimestamp > MQTT_RECONNECT_TIMEOUT) {
                mqttReconnectTimestamp = now;
                mqttUpdateTimestamp = 0;
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
                logToMQTT("kaz9/mega/ping", "1");
            }
            mqttUpdateTimestamp = now;
            mqttClient.loop();

            // send messages to mqtt
//            for (int i = 0; i < mqttMessagesToSendCount; i++) {
//                Serial.print("\tpublish to '");
//                Serial.print(mqttTopics[mqttMessagesToSendCount]);
//                Serial.print("' with payload '");
//                Serial.print(mqttPayloads[mqttMessagesToSendCount]);
//                Serial.println("'");
//
//                mqttClient.publish(mqttTopics[mqttMessagesToSendCount], mqttPayloads[mqttMessagesToSendCount]);
//            }
//            mqttMessagesToSendCount = 0;
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

        logToMQTT("kaz9/mega/water-pump1/min", minWaterLevel1 ? 1 : 0);
        logToMQTT("kaz9/mega/water-pump1/max", maxWaterLevel1 ? 1 : 0);

        if (!minWaterLevel1 && !isWaterPump1Enabled) {
            isWaterPump1Enabled = true;
            lastWaterPumpStartTimestamp = now;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);

            Serial.println("\tpump is ON");

            logToMQTT("kaz9/mega/water-pump1/on", now);
        } else if (maxWaterLevel1 && isWaterPump1Enabled) {
            isWaterPump1Enabled = false;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);

            Serial.print("\tpump is OFF");
            Serial.print(", work time: ");
            Serial.println(now - lastWaterPumpStartTimestamp);

            logToMQTT("kaz9/mega/water-pump1/off", now - lastWaterPumpStartTimestamp);
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
