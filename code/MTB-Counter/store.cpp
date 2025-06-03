#include "store.h"

// EEPROM Location to store todays count.
static int todaysEepromByteAddress = 0;

// last measurements during past daycycles
static short pageContent[4] = { 0, 0, 0, 0 }; // Made static

/*
 * Read a long (4byte) value from EEPROM
 */
long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

/*
 * Write a long (4byte) value to EEPROM
 */
void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

/*
 * Search for the first free EEPROM Address to store today's count later there.
 */
int startANewDay() {
  todaysEepromByteAddress = 12;
  short value = 1;
  while (value != 254 && todaysEepromByteAddress++ < EEPROM.length() - 1) {
    value = EEPROM.read(todaysEepromByteAddress);
  }
  if (todaysEepromByteAddress >= EEPROM.length() - 1) {
    todaysEepromByteAddress = 12; // Wrap around if memory is full
  }
  return todaysEepromByteAddress;
}

short * getDaylightRecordings(short page) { // page is now parameter
  long address = todaysEepromByteAddress - 4 * page;
  for (short i = 3; i >= 0; i--) {
    if (address - i > 12) { // Ensure we don't read from overall count area
      pageContent[i] = EEPROM.read(address - i);
    } else {
      pageContent[i] = 255; // Mark as invalid/out of range
    }
  }
  return pageContent;
  // The original check for pageContent[0] == 255 to reset page to 0 should be handled by the caller (e.g., UI or Counter)
  // as it depends on the current page context which is outside the responsibility of 'store'.
}

/*
 * Store the count for today only
 */
void storeTodayCount(int count) {
  if (count > 253) { // 253 is max valid count, 254 is empty, 255 invalid, 252 is max.
    EEPROM.write(todaysEepromByteAddress, 253); // Store max if exceeding
  } else {
    EEPROM.write(todaysEepromByteAddress, count);
  }
  EEPROM.update(todaysEepromByteAddress + 1, 254); // Mark next byte as empty
}

/*
 * Read the fist 3 long value from EEPROM. Check if at least 2 of 3 have the same value and return it. If they have completely different values, return the value of address 0.
 * This is implemented to mitigate an EEPROM failure.
 */
long readOverallCount() {
  long l1 = EEPROMReadlong(0);
  long l2 = EEPROMReadlong(4);
  long l3 = EEPROMReadlong(8);

  if (l1 != l2) {
    if (l1 == l3) {
      return l1;
    } else if (l2 == l3) {
      return l2;
    } else {
      return l1; // Default to first value if all are different
    }
  } else {
    return l1;
  }
}

/*
 * Store the current overall count into the first 12 (3x4) byte of the EEPROM.
 */
void storeOverallCount(long value) {
  EEPROMWritelong(0, value);
  EEPROMWritelong(4, value);
  EEPROMWritelong(8, value);
}

/*
 * Clears the relevant parts of EEPROM memory.
 */
void clearEEPROM() {
  for (int i = 13; i < EEPROM.length(); i++) { // Clear daily counts
    EEPROM.write(i, 254); // Set to "empty"
  }
  for (int i = 0; i < 13; i++) { // Clear overall count (first 12 bytes)
    EEPROM.write(i, 0);
  }
}