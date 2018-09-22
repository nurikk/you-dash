#include <ArduinoJson.h>

void getJsonData();
void ntpTick();
void log(const char *logString);
void setupNTP();
void saveConfigCallback();
void displayData(JsonObject &data);
void getJsonData();
void SPIFFSRead();
void SPIFFSWrite();
void setupWifi();