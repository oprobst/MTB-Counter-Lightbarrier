#ifndef STORE_H
#define STORE_H

#include <Arduino.h>
#include <EEPROM.h>

/*
 * Search for the first free EEPROM Address to store today's count later there.
 */
int startANewDay();

/*
 * Read a long (4byte) value from EEPROM
 */
long EEPROMReadlong(long address);

/*
 * Write a long (4byte) value to EEPROM
 */
void EEPROMWritelong(int address, long value);

/*
 * Store the count for today only
 */
void storeTodayCount(int count);

/*
 * Read the fist 3 long value from EEPROM. Check if at least 2 of 3 have the same value and return it. If they have completely different values, return the value of address 0.
 * This is implemented to mitigate an EEPROM failure.
 */
long readOverallCount();

/*
 * Store the current overall count into the first 12 (3x4) byte of the EEPROM.
 */
void storeOverallCount(long value);

/*
 * Get last measurements during past daycycles based on page
 */
short* getDaylightRecordings(short page);

/*
 * Clears the relevant parts of EEPROM memory.
 */
void clearEEPROM();

#endif