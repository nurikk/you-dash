#include <ArduinoJson.h>

JsonObject& exchange(String authorization_code);
String authUrl();
JsonObject& info(String access_token);
JsonObject& refresh(String refresh_token);