#include <WString.h>
#include "utils.h"
#include <ArduinoJson.h>

// OAUTH2 Basics
String access_type = "offline";
String redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
String response_type = "code";
String auth_uri = "https://accounts.google.com/o/oauth2/auth";
String info_uri = "/oauth2/v3/tokeninfo";
String token_uri = "/oauth2/v4/token";

String scope = "https://www.googleapis.com/auth/yt-analytics.readonly https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube";

// OAUTH2 Client credentials
String client_id = "363468596642-s7qhf77ud5hjc5mn3r2b9fllouuu4epp.apps.googleusercontent.com";
String client_secret = "2n2y0mVLt-tgr-tLrsjpqE0W";

const char *host = "www.googleapis.com";
const int httpsPort = 443;

String authUrl()
{
    String URL;
    URL = auth_uri + "?";
    URL += "scope=" + urlencode(scope);
    URL += "&redirect_uri=" + urlencode(redirect_uri);
    URL += "&response_type=" + urlencode(response_type);
    URL += "&client_id=" + urlencode(client_id);
    URL += "&access_type=" + urlencode(access_type);
    return URL;
}

JsonObject& exchange(String authorization_code)
{

    String postData = "";
    postData += "code=" + authorization_code;
    postData += "&client_id=" + client_id;
    postData += "&client_secret=" + client_secret;
    postData += "&redirect_uri=" + redirect_uri;
    postData += "&grant_type=" + String("authorization_code");

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    return postRequest(host, httpsPort, postHeader, postData);
}

JsonObject& refresh(String refresh_token)
{

    String postData = "";
    postData += "refresh_token=" + refresh_token;
    postData += "&client_id=" + client_id;
    postData += "&client_secret=" + client_secret;
    postData += "&grant_type=" + String("refresh_token");

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    return postRequest(host, httpsPort, postHeader, postData);
}

JsonObject& info(String access_token)
{
    String reqHeader = "";
    reqHeader += ("GET " + info_uri + "?access_token=" + urlencode(access_token) + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
    return getRequest(host, httpsPort, reqHeader);
}

// JsonObject& callApi(String access_token)
// {
//     // Serial.println("access_token");
//     // Serial.println(access_token);
//     //   String postData = "";
//     //   postData += "{\n  \"values\": [[\"brasel\",\"fink\"]]\n}";

//     //   String postHeader = "";
//     //   postHeader += ("POST /v4/spreadsheets/" + sheet_id + "/values/" + sheet_range + ":append" + "?valueInputOption=raw" + " HTTP/1.1\r\n");
//     //   postHeader += ("Host: " + String(sheetsHost) + ":" + String(httpsPort) + "\r\n");
//     //   postHeader += ("Connection: close\r\n");
//     //   postHeader += ("Authorization: Bearer " + access_token + "\r\n");
//     //   postHeader += ("Content-Type: application/json; charset=UTF-8\r\n");
//     //   postHeader += ("Content-Length: ");
//     //   postHeader += (postData.length());
//     //   postHeader += ("\r\n\r\n");

//     //   String result = postRequest(sheetsHost, postHeader, postData);

//     return NULL;
// }

