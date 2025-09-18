#ifndef GLOBAL_H
#define GLOBAL_H
#include <Arduino.h>

struct weigh_t {
    int32_t raw;
    int32_t val;
};

extern QueueHandle_t queue_weigh;  // = xQueueCreate(10, sizeof(weigh_t));

struct calib_t {
    int32_t offset = 0;
    double  scale = 1.0;
};

extern calib_t calib;

void setLed(uint8_t led, bool on);
void printDisplay(int32_t v, int pointPos = -1);
void readCalib(void);
void saveCalib(void);
#endif  // GLOBAL_H