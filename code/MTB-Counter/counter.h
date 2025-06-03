#ifndef COUNTER_H
#define COUNTER_H

#include <Arduino.h>

void initCounts();
void incrementCounts();
int getTodayCount();
long getOverallCount();
void resetOverallCount();
void resetPage(); // Function to reset the pagination for display


#endif