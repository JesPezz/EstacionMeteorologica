#ifndef GOOGLE_SHEET_MANAGER_H
#define GOOGLE_SHEET_MANAGER_H

#include <HTTPClient.h>
#include <vector>
#include <Preferences.h>

extern std::vector<String> storedReadings;
extern Preferences preferences;
class GoogleSheetManager {
    public:
        static String url; // Declaración de la variable
    };

void googlesheet();
void saveAndSendData();
void sendReadingToGoogleSheet();
void sendAllReadingsToGoogleSheet();

#endif