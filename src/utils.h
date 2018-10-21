#include <ArduinoJson.h>

String urlencode(String str);
JsonObject& postRequest(const char *server, const int port, String header, String data);
JsonObject& getRequest(const char *server, const int port, String request);
JsonObject& request(const char *server, const int port, String data);
