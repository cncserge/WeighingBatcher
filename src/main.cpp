#include <Arduino.h>
#include <EEPROM.h>
#include <global.h>

const int pin_out = 17;
const int pin_zummer = 14;

void task_read_weigh(void* pvParameters);
void task_indication(void* pvParameters);
void taskOta(void* pvParameter);

const int pin_btn_z = 4;   // ноль кнопка
const int pin_btn_o = 19;  // од кнопка
const int pin_btn_t = 18;  // тара кнопка

enum {
    set_calib,
    work,
    set_dose,
    set_const,
    set_tare
};
enum {
    led_calib = 1,
    led_set_zero,
    led_out,
    led_set_dose,
    led_set_const,
    led_set_tare
};

settings_t settings;

int  mode = -1;
void setup() {
    Serial.begin(115200);
    pinMode(pin_btn_z, INPUT_PULLUP);
    pinMode(pin_btn_o, INPUT_PULLUP);
    pinMode(pin_btn_t, INPUT_PULLUP);
    xTaskCreatePinnedToCore(taskOta, "taskOta", 8000, NULL, 1, NULL, 0);
    if (!EEPROM.begin(512)) {
        while (1) {
            delay(1000);
            Serial.println("Failed to initialize EEPROM");
        }
    }
    if (EEPROM.read(0) != 17) {
        calib.offset = 0;
        calib.scale = 1.0;
        settings.weigh_min = 100;
        settings.weigh_dose = 900;
        settings.weigh_const = 50;
        EEPROM.write(0, 17);
        EEPROM.commit();
        saveCalib();
        saveSettings();
    }
    readCalib();
    readSettings();

    pinMode(pin_out, OUTPUT);
    digitalWrite(pin_out, LOW);

    xTaskCreatePinnedToCore(task_read_weigh, "task_read_weigh", 4000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_indication, "task_indication", 4000, NULL, 1, NULL, 1);
    digitalRead(pin_btn_o) == LOW ? mode = set_calib : mode = work;
}

void loop() {
    if (weigh_t msg; xQueueReceive(queue_weigh, &msg, pdMS_TO_TICKS(1000))) {
        if (mode == set_calib) {
            bool on_delay = false;
            if (!digitalRead(pin_btn_z)) {
                on_delay = true;
                calib.offset = msg.raw;
                saveCalib();
            }
            if (!digitalRead(pin_btn_t)) {
                on_delay = true;
                calib.scale = 5000. / (msg.raw - calib.offset);
                saveCalib();
            }
            printDisplay(msg.val);
            if (on_delay) delay(1000);
        }
        if (mode == work) {
            printDisplay(msg.val);
            {
                static uint32_t time_out = 0;
                if (digitalRead(pin_btn_z) == LOW) {
                    if (millis() - time_out > 2000) {
                        calib.offset = msg.raw;
                        saveCalib();
                        setLed(led_set_zero, true);
                        delay(2000);
                        setLed(led_set_zero, false);
                        time_out = millis();
                    }
                } else {
                    time_out = millis();
                }
            }
        }
    }

    {
        static uint32_t time_out = 0;
        if (digitalRead(pin_btn_t) == LOW) {
            if (millis() - time_out > 2000) {
                time_out = millis();
                if (mode == work) {
                    mode = set_dose;
                    setLed(led_set_dose, true);
                    delay(200);
                } else if (mode == set_dose) {
                    mode = set_const;
                    setLed(led_set_dose, false);
                    delay(200);
                    setLed(led_set_const, true);
                    delay(200);
                } else if (mode == set_const) {
                    mode = set_tare;
                    setLed(led_set_const, false);
                    delay(200);
                    setLed(led_set_tare, true);
                    delay(200);
                } else if (mode == set_tare) {
                    mode = work;
                    setLed(led_set_tare, false);
                    delay(200);
                }
            }
        } else {
            time_out = millis();
        }
    }

    if (mode == set_dose) {
        printDisplay(settings.weigh_dose);
        if (!digitalRead(pin_btn_o)) {
            if (settings.weigh_dose > 100) {
                settings.weigh_dose -= 1;
                saveSettings();
                delay(100);
            }
        }
        if (!digitalRead(pin_btn_z)) {
            if (settings.weigh_dose < 5000) {
                settings.weigh_dose += 1;
                saveSettings();
                delay(100);
            }
        }
    }
    if (mode == set_const) {
        printDisplay(settings.weigh_const);
        if (!digitalRead(pin_btn_o)) {
            if (settings.weigh_const > 0) {
                settings.weigh_const -= 1;
                saveSettings();
                delay(100);
            }
        }
        if (!digitalRead(pin_btn_z)) {
            if (settings.weigh_const < 500) {
                settings.weigh_const += 1;
                saveSettings();
                delay(100);
            }
        }
    }
    if (mode == set_tare) {
        printDisplay(settings.weigh_min);
        if (!digitalRead(pin_btn_o)) {
            if (settings.weigh_min > 0) {
                settings.weigh_min -= 1;
                saveSettings();
                delay(100);
            }
        }
        if (!digitalRead(pin_btn_z)) {
            if (settings.weigh_min < 1000) {
                settings.weigh_min += 1;
                saveSettings();
                delay(100);
            }
        }
    }

    if (mode == work) {

    } else {
        digitalWrite(pin_out, LOW);
    }

    if (mode == set_calib) {
        static uint32_t time = 0;
        static bool     en = false;
        if (millis() - time >= 500) {
            time = millis();
            en = !en;
            setLed(led_calib, en);
        }
    }

    if (mode == work) {
        static int old = -1;
        if (old != digitalRead((pin_out))) {
            old = digitalRead((pin_out));
            setLed(led_out, old);
        }
    }

    delay(1);
}

void saveCalib(void) {
    Serial.printf("Save calib: offset=%d scale=%.5f\n", calib.offset, calib.scale);
    EEPROM.put(10, calib);
    EEPROM.commit();
}
void readCalib(void) {
    EEPROM.get(10, calib);
    Serial.printf("Read calib: offset=%d scale=%.5f\n", calib.offset, calib.scale);
}

void saveSettings(void) {
    Serial.printf("Save settings: weigh_min=%d weigh_dose=%d weigh_const=%d\n", settings.weigh_min, settings.weigh_dose, settings.weigh_const);
    EEPROM.put(100, settings);
    EEPROM.commit();
}

void readSettings(void) {
    EEPROM.get(100, settings);
    Serial.printf("Read settings: weigh_min=%d weigh_dose=%d weigh_const=%d\n", settings.weigh_min, settings.weigh_dose, settings.weigh_const);
}
