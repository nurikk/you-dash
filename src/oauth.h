#include <ArduinoJson.h>

String authUrl();
JsonObject& exchange(String authorization_code, DynamicJsonBuffer *jsonBuffer);
JsonObject& info(String access_token, DynamicJsonBuffer *jsonBuffer);
JsonObject& refresh(String refresh_token, DynamicJsonBuffer *jsonBuffer);
JsonObject& callApi(String access_token, String start_date, String end_date, DynamicJsonBuffer *jsonBuffer);