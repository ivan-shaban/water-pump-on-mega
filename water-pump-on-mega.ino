#include "WiFiEsp.h"
#include <PubSubClient.h>

// TODO: отправлять среднее вреся работы насоса в mqtt
// TODO: добавить проверку что если работа насоса превышает среднее время работы насоса, то сбрасывать его и слать лог с ошибкой
// TODO: добавить очередь команды которые будут посылаться в mqtt
// TODO: добавить поддержку датчика протечки и прерывать работу насоса и слать сообщение с ошибкой в mqtt

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif

#define MQTT_RECONNECT_TIMEOUT 5000
#define MQTT_UPDATE_TIMEOUT 1000
#define MQTT_PING_TIMEOUT 30000
#define WATER_SENSOR_TIMEOUT 3000

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
char ssid[] = "Lords-Network-2.4Ghz";
char pass[] = "qBM029qBM029";
int status = WL_IDLE_STATUS;     // the Wifi radio's status

// mqtt section
IPAddress mqttServerIP(192, 168, 31, 211);
long mqttReconnectTimestamp = 0;
long mqttUpdateTimestamp = 0;
long mqttPingTimestamp = 0;

const int MIN_VOLTS_VALUE = 100;
bool isWaterPump1Enabled = false;
bool isWaterPump2Enabled = false;
long waterSensorTimestamp = 0;
long lastWaterPumpStartTimestamp = 0;


void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}
// Initialize the Ethernet client object
WiFiEspClient espClient;
PubSubClient mqttClient(mqttServerIP, 1883, callback, espClient);

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
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network
        status = WiFi.begin(ssid, pass);
    }
    // you're connected now, so print out the data
    Serial.println("You're connected to the network");

    IPAddress localIP = WiFi.localIP();
    Serial.print("\tIP Address: ");
    Serial.println(localIP);
}

boolean reconnectMQTT() {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
        Serial.println("MQTT connected");
        // Once connected, publish an announcement...
        mqttClient.publish("kaz9/mega/ping", 1);
        mqttClient.subscribe("kaz9/mega/water-pump");
    }
    return mqttClient.connected();
}
void logToMQTT(char* topic, char* payload) {
    mqttClient.publish(topic, payload);

}

void handleMQTT(long now) {
    if (now - mqttUpdateTimestamp > MQTT_UPDATE_TIMEOUT) {
        if (!mqttClient.connected()) {
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
                mqttClient.publish("kaz9/mega/ping", 1);
            }
            mqttUpdateTimestamp = now;
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

        if (!minWaterLevel1 && !isWaterPump1Enabled) {
            isWaterPump1Enabled = true;
            lastWaterPumpStartTimestamp = now;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);
        } else if (maxWaterLevel1 && isWaterPump1Enabled) {
            isWaterPump1Enabled = false;
            digitalWrite(RELAY_PORT_1, !isWaterPump1Enabled);
        }

        if (!minWaterLevel2 && !isWaterPump2Enabled) {
            isWaterPump2Enabled = true;
            digitalWrite(RELAY_PORT_2, !isWaterPump2Enabled);
        } else if (maxWaterLevel2 && isWaterPump2Enabled) {
            isWaterPump2Enabled = false;
            digitalWrite(RELAY_PORT_2, !isWaterPump2Enabled);
        }
    }
}

void setup() {
    // initialize serial for debugging
    Serial.begin(115200);
    // initialize serial for ESP module
    Serial3.begin(115200);

    pinMode(RELAY_PORT_1, OUTPUT);        // 8 relay output
    pinMode(RELAY_PORT_2, OUTPUT);        // 7 relay output
    pinMode(RELAY_PORT_3, OUTPUT);        // 6 relay output
    pinMode(RELAY_PORT_4, OUTPUT);        // 5 relay output
    pinMode(RELAY_PORT_5, OUTPUT);        // 4 relay output
    pinMode(RELAY_PORT_6, OUTPUT);        // 3 relay output
    pinMode(RELAY_PORT_7, OUTPUT);        // 2 relay output
    pinMode(RELAY_PORT_8, OUTPUT);        // 1 relay output

    pinMode(MIN_WATER_LEVEL_1, INPUT);    // water sensor
    pinMode(MAX_WATER_LEVEL_1, INPUT);    // water sensor
    pinMode(MIN_WATER_LEVEL_2, INPUT);    // water sensor
    pinMode(MAX_WATER_LEVEL_2, INPUT);    // water sensor

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
