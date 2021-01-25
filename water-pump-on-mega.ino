#include <OneWire.h>

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

bool isWaterPump1Enabled = false;
bool isWaterPump2Enabled = false;
bool ventEnabled = false;
const int MIN_VOLTS_VALUE = 100;

OneWire ds(12);
int temperature = 0; // Глобальная переменная для хранения значение температуры с датчика DS18B20

long lastUpdateTime = 0; // Переменная для хранения времени последнего считывания с датчика
const int TEMP_UPDATE_TIME = 1000; // Определяем периодичность проверок
const int MAX_TEMP_VALUE = 35; // Максимально допустима температура, без активного охлаждения
const int MIN_TEMP_VALUE = 25; // Минимально допустимая температура при которой возможно активное охлаждение

void setup() {
    Serial.begin(9600);

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
}

void loop() {
    debug();
    handlerPump();
//    handleVentilation();
    delay(300);
}

void handlerPump() {
    bool minWaterLevel1 = analogRead(MIN_WATER_LEVEL_1) > MIN_VOLTS_VALUE;
    bool maxWaterLevel1 = analogRead(MAX_WATER_LEVEL_1) > MIN_VOLTS_VALUE;
    bool minWaterLevel2 = analogRead(MIN_WATER_LEVEL_2) > MIN_VOLTS_VALUE;
    bool maxWaterLevel2 = analogRead(MAX_WATER_LEVEL_2) > MIN_VOLTS_VALUE;

    if (!minWaterLevel1 && !isWaterPump1Enabled) {
        isWaterPump1Enabled = true;
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

/**
 * Определяем температуру от датчика DS18b20
 */
void handleVentilation() {
    byte data[2];
    ds.reset();
    ds.write(0xCC);
    ds.write(0x44);

    if (millis() - lastUpdateTime > TEMP_UPDATE_TIME) {
        lastUpdateTime = millis();
        ds.reset();
        ds.write(0xCC);
        ds.write(0xBE);
        data[0] = ds.read();
        data[1] = ds.read();

        // Формируем значение
        temperature = (data[1] << 8) + data[0];
        temperature = temperature >> 4;
    }
    Serial.print("temperature: ");
    Serial.println(temperature); // Выводим полученное значение температуры
    // Т.к. переменная temperature имеет тип int, дробная часть будет просто отбрасываться

    if (temperature > MAX_TEMP_VALUE && !ventEnabled) {
        ventEnabled = true;
        digitalWrite(5, !ventEnabled);
    } else if (temperature < MIN_TEMP_VALUE && ventEnabled) {
        ventEnabled = false;
        digitalWrite(5, !ventEnabled);
    }
}

void debug() {
    Serial.print("voltage\tMIN_WATER_LEVEL_1: ");
    Serial.print(analogRead(MIN_WATER_LEVEL_1));
    Serial.print(", MAX_WATER_LEVEL_1: ");
    Serial.print(analogRead(MAX_WATER_LEVEL_1));
    Serial.print(", MIN_WATER_LEVEL_2: ");
    Serial.print(analogRead(MIN_WATER_LEVEL_2));
    Serial.print(", MAX_WATER_LEVEL_2: ");
    Serial.println(analogRead(MAX_WATER_LEVEL_2));
}
