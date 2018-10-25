#include <ArduinoJson.h>

String authUrl(String client_id, String client_secret);
JsonObject &exchange(String authorization_code, String client_id, String client_secret, DynamicJsonBuffer *jsonBuffer);
JsonObject &info(String access_token, DynamicJsonBuffer *jsonBuffer);
JsonObject &refresh(String refresh_token, String client_id, String client_secret, DynamicJsonBuffer *jsonBuffer);
JsonObject &callApi(String access_token, String start_date, String end_date, DynamicJsonBuffer *jsonBuffer);
JsonObject &getChannelStats(String access_token, DynamicJsonBuffer *jsonBuffer);