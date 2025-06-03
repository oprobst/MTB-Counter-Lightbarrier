#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

void initSensors();
bool checkBarrier();
bool isPermanentBlocked();
void handleSleepAndDaylight();
void handleButtonPresses();
int getBrightness();
bool getIsDay();
int getBatteryLevel(); // Function to get battery level for UI

#endif