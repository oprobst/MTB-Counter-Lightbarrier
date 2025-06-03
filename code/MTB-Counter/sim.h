#ifndef SIM_H
#define SIM_H

#include <Arduino.h>
#include <Adafruit_FONA.h> // Include FONA library header
#include <TimeLib.h>        // Include TimeLib for time_t object

// Function to initialize the FONA module
void initFona();

// Function to get general FONA information (like IMEI, CCID, RSSI)
void getFonaInfo();

// Function to get the current SIM signal strength (RSSI)
int8_t getSimRSSI();

// Function to get the millis timestamp of the last successful POST request
uint32_t getLastPostMillis();

// Function to connect to GPRS
bool connectGPRS();

// Function to disconnect from GPRS
void disconnectGPRS();

// Function to send a POST request to a specified URL with given data
bool sendPostRequest(const char* url, const char* data);

// Function to synchronize the RTC time via NTP
bool syncTimeNTP();

// Function to get the current time from FONA's RTC as time_t
time_t getFonaTime();

#endif