#include "counter.h"
#include "store.h" // For EEPROM operations like readOverallCount, storeOverallCount, storeTodayCount, getDaylightRecordings, startANewDay

// Global variables for counts
static int todayCount = 0;
static long overallCount = 0;
static int currentPage = 0;
static short pageContent[4] = { 0, 0, 0, 0 }; // Buffer for daylight recordings

void initCounts() {
  overallCount = readOverallCount(); // Initialize overall count from EEPROM
  todayCount = 0; // Today's count always starts at 0 at the beginning of a day or reset
  startANewDay(); // Determine EEPROM address for today's count
  storeTodayCount(todayCount); // Store initial todayCount (0) in EEPROM
}

void incrementCounts() {
  todayCount++;
  overallCount++;
  storeTodayCount(todayCount);
  storeOverallCount(overallCount);
}

int getTodayCount() {
  return todayCount;
}

long getOverallCount() {
  return overallCount;
}

void resetOverallCount() {
  storeOverallCount(0); // Reset overall count to 0 in EEPROM
  overallCount = 0; // Reset in RAM as well
  // For safety, also reset today's count and start a new day entry
  todayCount = 0;
  startANewDay();
  storeTodayCount(todayCount);
}


void resetPage() {
  currentPage = 0; // Reset pagination
}