#include <ArduinoJson.h>
void setupNTP();
void SPIFFSRead();
void SPIFFSWrite();
void setupWifi();


JsonObject &getJsonConfig();

void parsApi();
void setupI2C();
void setupHTTPServer();