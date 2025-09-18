#include "global.h"
#include <Arduino.h>

int32_t               hx71708_read(int pinDOUT, int pinSCK, uint8_t extra, uint32_t timeout_ms);
static inline int32_t weigh_filter(int32_t x);
QueueHandle_t         queue_weigh = xQueueCreate(1, sizeof(weigh_t));
calib_t               calib;

void task_read_weigh(void* pvParameters) {
    for (;;) {
        weigh_t w;
        w.raw = weigh_filter(hx71708_read(27, 26, 3, 300));
        w.val = (int32_t)((w.raw - calib.offset) * calib.scale);
        xQueueOverwrite(queue_weigh, &w);
        vTaskDelay(pdMS_TO_TICKS(3));
    }
}

// Возврат: отфильтрованное значение.
static inline int32_t weigh_filter(int32_t x) {
    // ===== НАСТРОЙКИ =====
    // EMA: out = out + alpha * (med3 - out), где alpha = ALPHA_NUM/ALPHA_DEN
    // Поставь ALPHA_NUM=1, ALPHA_DEN=1, чтобы EMA отключить (моментальный отклик).
    enum { ALPHA_NUM = 1,
           ALPHA_DEN = 1 };  // 1/1 => EMA выкл; 1/2 или 1/4 — лёгкое сглаживание
    // Кламп «иголок» (0 = отключить)
    static const int32_t SPIKE = 0;  // было 300 — давило отклик

    // ===== СОСТОЯНИЕ =====
    static int32_t last_out = 0;
    static int32_t r0 = 0, r1 = 0, r2 = 0;  // кольцо сырых для медианы
    static uint8_t inited = 0;

    if (!inited) {
        r0 = r1 = r2 = last_out = 0;
        inited = 1;
    }

    // Таймаут → вернуть последнее
    if (x == INT32_MIN) return last_out;

    // Анти-спайк (если включён)
    if (SPIKE > 0) {
        int32_t d = x - r2;
        if (d > SPIKE) x = r2 + SPIKE;
        if (d < -SPIKE) x = r2 - SPIKE;
    }

    // Обновить историю для медианы-из-3
    r0 = r1;
    r1 = r2;
    r2 = x;

    // Медиана из (r0,r1,r2) — минимальная задержка, хорошо режет одиночные выбросы
    int32_t a = r0, b = r1, c = r2;
    // median(a,b,c) без ветвлений почти: пару сравнений
    if (a > b) {
        int32_t t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        int32_t t = b;
        b = c;
        c = t;
    }
    if (a > b) {
        int32_t t = a;
        a = b;
        b = t;
    }
    int32_t med = b;

    // EMA (можно отключить ALPHA_NUM=ALPHA_DEN=1)
    if (ALPHA_NUM == ALPHA_DEN) {
        last_out = med;  // EMA off → мгновенно
    } else {
        // last_out += alpha*(med - last_out)
        int64_t diff = (int64_t)med - (int64_t)last_out;
        last_out = (int32_t)((int64_t)last_out + (diff * ALPHA_NUM) / ALPHA_DEN);
    }
    return last_out;
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