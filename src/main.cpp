#include "TM16xxDisplay.h"
#include <Arduino.h>
#include <TM16xxIC.h>

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

int32_t hx71708_read(int pinDOUT, int pinSCK, uint8_t extra, uint32_t timeout_ms);

TM16xxIC      module(IC_TM1629A, 33, 32, 25);
TM16xxDisplay display(&module, 8);

void printDisplay(int32_t v, int pointPos = -1) {
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

void setup() {
    for (int i = 0; i < 10; i++) {
        printLed(i);
        delay(1000);
    }
    // display.print(23);
    Serial.begin(115200);
}

int  k = 0;
int  d = 0;
void loop() {
    // int i = (1 << d++);
    // module.setSegments(i, k);
    // if(d > 8){
    //     d = 0;
    //     module.setSegments(0, k);
    //     k++;
    // }
    // Serial.printf("k=%d i=%02X\n", k, i);
    // display.setDisplayToDecNumber(i++, 0);
    delay(100);

    int32_t v = hx71708_read(27, 26, 3, 300);
    if (v == INT32_MIN) {
        Serial.println("Timeout");
    } else {
        printDisplay(v);
        Serial.println(v);
    }
    delay(1);
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
