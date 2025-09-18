#include <Arduino.h>
#include <EEPROM.h>
#include <global.h>

const int pin_out = 17;
void      task_read_weigh(void* pvParameters);
void      task_indication(void* pvParameters);
void      taskOta(void* pvParameter);

const int pin_btn_z = 4;   // ноль кнопка
const int pin_btn_o = 19;  // од кнопка
const int pin_btn_t = 18;  // тара кнопка

enum {
    set_calib,
    work,
};

int  mode = -1;
void setup() {
    pinMode(pin_btn_z, INPUT_PULLUP);
    pinMode(pin_btn_o, INPUT_PULLUP);
    pinMode(pin_btn_t, INPUT_PULLUP);
    EEPROM.begin(512);
    if (!EEPROM.read(0) != 7) {
        calib.offset = 0;
        calib.scale = 1.0;
        EEPROM.write(0, 7);
        saveCalib();
    }
    readCalib();
    Serial.begin(115200);
    pinMode(pin_out, OUTPUT);
    digitalWrite(pin_out, LOW);

    xTaskCreatePinnedToCore(task_read_weigh, "task_read_weigh", 4000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_indication, "task_indication", 4000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskOta, "taskOta", 8000, NULL, 1, NULL, 0);
    digitalRead(pin_btn_o) == LOW ? mode = set_calib : mode = work;
}

void loop() {
    if (weigh_t msg; xQueueReceive(queue_weigh, &msg, pdMS_TO_TICKS(1000))) {
        if (mode == set_calib) {
            if (!digitalRead(pin_btn_z)) {
                calib.offset = msg.raw;
            }
            if(digitalRead(pin_btn_t)){
                calib.scale = 5000 / 
            }
            printDisplay(msg.val);
        }
    }

    delay(1);
}

void saveCalib(void) {
    EEPROM.put(10, calib);
    EEPROM.commit();
}
void readCalib(void) {
    EEPROM.get(10, calib);
}
