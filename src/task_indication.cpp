#include "TM16xxDisplay.h"
#include "global.h"
#include <Arduino.h>
#include <TM16xxIC.h>

TM16xxIC module(IC_TM1629A, 33, 32, 25);
//------------------------------------------------------------------------
struct set_led_t {
    uint8_t pos;
    bool    on;
};
QueueHandle_t queue_set_led = xQueueCreate(1, sizeof(set_led_t));

void setLedStatic(uint8_t led, bool on);
void setLed(uint8_t led, bool on) {
    set_led_t msg = {led, on};
    xQueueOverwrite(queue_set_led, &msg);
}
//------------------------------------------------------------------------
struct set_display_t {
    int32_t val;
    int     pointPos;  // позиция точки 1..6, -1 нет точки
};
QueueHandle_t queue_set_display = xQueueCreate(1, sizeof(set_display_t));
void printDisplayStatic(int32_t v, int pointPos = -1);
void printDisplay(int32_t v, int pointPos) {
    set_display_t msg = {v, pointPos};
    xQueueOverwrite(queue_set_display, &msg);
}




//------------------------------------------------------------------------
void task_indication(void* pvParameters) {
    for (;;) {
        if (set_led_t msg; xQueueReceive(queue_set_led, &msg, pdMS_TO_TICKS(50))) {
            setLedStatic(msg.pos, msg.on);
        }
        if (set_display_t msg; xQueueReceive(queue_set_display, &msg, pdMS_TO_TICKS(50))) {
            printDisplayStatic(msg.val, msg.pointPos);
        }
    }
}

uint8_t point_mask = 0b00000100;  // если здесь 1 значит точка включена
uint8_t digit[12] = {
    0b11011011,  // 0
    0b00010001,  // 1
    0b10111010,  // 2
    0b10110011,  // 3
    0b01110001,  // 4
    0b11100011,  // 5
    0b11101011,  // 6
    0b10010001,  // 7
    0b11111011,  // 8
    0b11110011,  // 9
    0b00000000,  // off
    0b00100000   // '-'
};

void printDisplayStatic(int32_t v, int pointPos) {
    uint8_t pat[6];
    for (int i = 0; i < 6; ++i)
        pat[i] = digit[10];  // off

    // знак и модуль
    bool     neg = (v < 0);
    int64_t  vv = v;
    uint32_t val = neg ? uint32_t(-vv) : uint32_t(vv);

    // цифры в диапазоне индикаторов j=3..7
    int right = 5;  // позиция внутри массива pat[] (0..5)
    int digits_cnt = 0;

    if (val == 0) {
        pat[right] = digit[0];
        digits_cnt = 1;
    } else {
        while (val && right >= 1) {  // не выходим за j=2 (там знак/пусто)
            pat[right] = digit[val % 10];
            val /= 10;
            --right;
            ++digits_cnt;
        }
    }

    // минус в j=2
    if (neg) pat[0] = digit[11];

    // точка как отступ от правого края числа
    if (pointPos >= 0) {
        int dp_idx = 5 - (pointPos - 1);  // pointPos=1 → крайняя справа
        if (dp_idx >= 1 && dp_idx <= 5) pat[dp_idx] |= point_mask;
    }

    // вывод: индикатор j = 2..7
    for (int i = 0; i < 6; ++i) {
        module.setSegments(pat[i], i + 2);
    }
}

void setLedStatic(uint8_t led, bool on) {
    static uint8_t led_0 = 0;
    static uint8_t led_1 = 0;
    if (led == 1) {
        on ? (led_0 |= 0b00000010) : (led_0 &= 0b11111101);
    } else if (led == 2) {
        on ? (led_0 |= 0b00000001) : (led_0 &= 0b11111110);
    } else if (led == 3) {
        on ? (led_1 |= 0b00001000) : (led_1 &= 0b11110111);
    } else if (led == 4) {
        on ? (led_1 |= 0b00000100) : (led_1 &= 0b11111011);
    } else if (led == 5) {
        on ? (led_1 |= 0b00000010) : (led_1 &= 0b11111101);
    } else if (led == 6) {
        on ? (led_1 |= 0b00000001) : (led_1 &= 0b11111110);
    }
    module.setSegments(led_0, 0);
    module.setSegments(led_1, 1);
}

void printLed(int n = -1) {
    module.setSegments(0, 0);
    module.setSegments(0, 1);
    if (n == 1)
        module.setSegments(0b00000010, 0);
    else if (n == 2)
        module.setSegments(0b00000001, 0);
    else if (n == 3)
        module.setSegments(0b00001000, 1);
    else if (n == 4)
        module.setSegments(0b00000100, 1);
    else if (n == 5)
        module.setSegments(0b00000010, 1);
    else if (n == 6)
        module.setSegments(0b00000001, 1);
}