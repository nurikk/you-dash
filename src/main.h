#include <ArduinoJson.h>
void setupNTP();
void SPIFFSRead();
void SPIFFSWrite();
void setupWifi();
void validateAccessToken();
void refreshToken();
JsonObject &getJsonConfig();

void parsApi();
void renderScreen();
void displayMetric(size_t idx);
void setupI2C();
void setupHTTPServer();