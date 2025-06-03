#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <XPT2046_Touchscreen.h> 
#include <SPI.h>

extern Adafruit_ST7789 tft;

struct DateCount {
    char date[8];  // z.B. "01.05.25"
    short count;
};


void turnDisplay(bool status);
void initScreen();
void showSplash();
void showBatteryStatus (int level);
void showSimSignal(int level);
void showTime();

void showToday (int count);
void show4Weeks (int count);
void showAllCount (long count);
void showStatus (const char * status, short severity);
void showLastTransmission (const char * lastTransmission);


void showCountPerDate(const DateCount (&data)[36]);


#endif