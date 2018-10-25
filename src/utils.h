#include <ArduinoJson.h>

String urlencode(String str);
JsonObject &postRequest(const char *server, const int port, String header, String data, DynamicJsonBuffer *jsonBuffer);
JsonObject &getRequest(const char *server, const int port, String request, DynamicJsonBuffer *jsonBuffer);
JsonObject &request(const char *server, const int port, String data, DynamicJsonBuffer *jsonBuffer);

time_t DatePlusDays(time_t startTime, int days);
