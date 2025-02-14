#ifndef GOOGLE_SHEET_MANAGER_H
#define GOOGLE_SHEET_MANAGER_H

#include <HTTPClient.h>
#include <vector>
#include <Preferences.h>

extern std::vector<String> storedReadings;
extern Preferences preferences;

void googlesheet();
void saveAndSendData();
void sendReadingToGoogleSheet(const String &reading);
void sendAllReadingsToGoogleSheet();

#endif