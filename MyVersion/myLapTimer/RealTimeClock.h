
#ifndef _RealTimeClock_H
#define _RealTimeClock_H
#include <Arduino.h>

void initRTC();
void updateRTC();
uint32_t getCurrentTime();
void synchRTC();
void loopNTP();
#endif