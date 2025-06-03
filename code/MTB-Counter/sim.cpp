#include "sim.h"
#include "ui.h" // For display feedback. Assuming ui.h declares showStatus(const char * status, short severity);

// Define the FONA shield serial. On Uno it's Serial (HardwareSerial)
HardwareSerial* fonaSerial = &Serial;

// FONA is a bit tricky, needs a reset line
#define FONA_RST 8

// Create the FONA object
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Global variables for SIM status
static uint32_t lastPostMillis = 0;

void initFona() {
  // Power up FONA
  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH);
  delay(10);
  digitalWrite(FONA_RST, LOW);
  delay(100);
  digitalWrite(FONA_RST, HIGH);
  delay(2000); // Give FONA time to boot

  // Ensure hardware serial is ready for FONA
  fonaSerial->begin(9600); // Changed baud rate to 9600

  if (!fona.begin(*fonaSerial)) {
    showStatus(F("FONA Error"), 3); // Severity 3 for error, duration removed
    while (1); // Halt if FONA not found
  }
  
  showStatus(F("FONA OK"), 1); // Severity 1 for OK, duration removed
  // Set FONA to minimal functionality to save power
  fona.setPowerMode(FONA_PM_MINIMUM);
}

void getFonaInfo() {
  char fonaBuf[64];

  // Get IMEI
  fona.getIMEI(fonaBuf);
  
  // Get CCID
  fona.getCCID(fonaBuf);

}

int8_t getSimRSSI() {
  return fona.getRSSI();
}

uint32_t getLastPostMillis() {
  return lastPostMillis;
}

bool connectGPRS() {
  showStatus(F("Connecting GPRS"), 2); // Severity 2 for warning/info during process, duration removed

  if (!fona.enableGPRS(true)) {
    showStatus(F("GPRS Fail"), 3); // Severity 3 for error, duration removed
    return false;
  }
  showStatus(F("GPRS OK"), 1); // Severity 1 for OK, duration removed
  return true;
}

void disconnectGPRS() {
  if (!fona.enableGPRS(false)) {
    // Handle error if GPRS cannot be disabled
  }
}

bool sendPostRequest(const char* url, const char* data) {
  showStatus(F("Sending POST"), 2); // Severity 2 for warning/info during process, duration removed

  if (!fona.HTTP_POST_start(url, F("application/x-www-form-urlencoded"), (uint8_t*)data, strlen(data))) {
    showStatus(F("POST Failed"), 3); // Severity 3 for error, duration removed
    return false;
  }

  uint16_t statuscode;
  int16_t length;
  while (!fona.HTTP_POST_complete(&statuscode, &length)) {
    delay(100);
  }

  if (statuscode == 200) {
    showStatus(F("POST Sent OK"), 1); // Severity 1 for OK, duration removed
    lastPostMillis = millis(); // Update last successful post time
    return true;
  } else {
    showStatus(F("POST Error!"), 3); // Severity 3 for error, duration removed
    return false;
  }
}

bool syncTimeNTP() {
  showStatus(F("Syncing Time"), 2); // Severity 2 for warning/info during process, duration removed

  if (!fona.enableNTP(true)) {
    showStatus(F("NTP Failed!"), 3); // Severity 3 for error, duration removed
    return false;
  }

  // FONA's updateRTC reads time from NTP server into FONA's internal RTC
  if (!fona.updateRTC()) {
    showStatus(F("RTC Update Fail"), 3); // Severity 3 for error, duration removed
    fona.enableNTP(false); // Disable NTP if update fails
    return false;
  }
  fona.enableNTP(false); // Disable NTP after successful update to save power

  showStatus(F("Time Synced"), 1); // Severity 1 for OK, duration removed
  return true;
}

time_t getFonaTime() {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  fona.RTCtime(&year, &month, &day, &hour, &minute, &second);

  tmElements_t tm;
  tm.Year = CalendarYrToTm(year); // Convert full year to year since 1970
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  return makeTime(tm); // Convert tmElements_t to time_t
}