#include "global.h"
#include <Arduino.h>

int32_t hx71708_read(int pinDOUT, int pinSCK, uint8_t extra, uint32_t timeout_ms);

QueueHandle_t queue_weigh = xQueueCreate(1, sizeof(weigh_t));
calib_t       calib;



void task_read_weigh(void* pvParameters) {
    int32_t sum = 0;
    int32_t old = 0;
    for (;;) {
        weigh_t w;
        sum = 0;
        for(int i = 0; i < 2; ++i){
            sum += hx71708_read(27, 26, 3, 300);
        }
        if(abs(old - (sum / 2)) > 100){
            continue;
        }
        old = sum / 2;
        w.raw = sum / 2;
        w.val = (int32_t)((w.raw - calib.offset) * calib.scale);
        xQueueOverwrite(queue_weigh, &w);
        vTaskDelay(pdMS_TO_TICKS(3));
    }
}

// extra = 1..4: +1=>10Hz, +2=>20Hz, +3=>80Hz, +4=>320Hz
// Возвращает 32-бит со знаком (sign-extend 24->32). При таймауте — 0x80000000.
int32_t hx71708_read(int pinDOUT, int pinSCK, uint8_t extra, uint32_t timeout_ms) {
    if (extra < 1 || extra > 4) extra = 1;
    static bool init = false;
    if (!init) {
        pinMode(pinSCK, OUTPUT);
        pinMode(pinDOUT, INPUT);
    }
    digitalWrite(pinSCK, LOW);
    init = true;

    // Ждём готовности: DOUT должен упасть в 0
    uint32_t t0 = millis();
    while (digitalRead(pinDOUT) == HIGH) {
        if (millis() - t0 > timeout_ms) return INT32_MIN;  // таймаут
    }

    // Чтение 24 бит MSB->LSB
    uint32_t v = 0;
    for (int i = 0; i < 24; ++i) {
        digitalWrite(pinSCK, HIGH);
        delayMicroseconds(1);  // T2≈0.1us, T3>=0.2us, держим ~1us
        v = (v << 1) | (uint32_t)digitalRead(pinDOUT);
        digitalWrite(pinSCK, LOW);
        delayMicroseconds(1);  // T4>=0.2us
    }

    // Доп. импульсы для выбора частоты следующего цикла
    for (uint8_t i = 0; i < extra; ++i) {
        digitalWrite(pinSCK, HIGH);
        delayMicroseconds(1);
        digitalWrite(pinSCK, LOW);
        delayMicroseconds(1);
    }

    // Знаковое расширение 24-битного 2's complement в int32_t
    if (v & 0x800000UL) v |= 0xFF000000UL;
    return (int32_t)v;
}