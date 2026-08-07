#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <time.h>
#include <Arduino.h>

void syncClock();
void getCurrentTime(struct tm* timeinfo);
bool isHourOnTheDot();
bool isFiveMinutes();
bool isTenMinutes();
bool isMinuteOnTheDot();
String getFormattedDateTime();
#endif