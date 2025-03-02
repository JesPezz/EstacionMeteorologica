#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <time.h>

void syncClock();
void getCurrentTime(struct tm* timeinfo);
bool isHourOnTheDot();
bool isFiveMinutes();
bool isTenMinutes();
bool isMinuteOnTheDot();
#endif