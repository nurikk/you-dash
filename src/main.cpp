#include "main.h"
#include <FS.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Ticker.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ArduinoLog.h>
#include <RSCG12864B.h>
#include <ESP8266mDNS.h>
#include "utils.h"
#include "oauth.h"

#define TEXT_JSON "text/json"

MDNSResponder mdns;

ESP8266WebServer server(80);

#define CONFIG_FILE_NAME "/config.json"
#define ACCESS_POINT_NAME "Youtube dashboard"
#define MDNS_DOMAIN "you-dash"
#define SSD_NAME "Youtube dashboard"

#define API_REFRESH_INTERVAL 5 * 60 * 1000 // 5 minutes

struct Config
{
    char client_id[100] = "";
    char client_secret[100] = "";

    char access_token[150] = "";
    char refresh_token[50] = "";

    int api_update_interval = API_REFRESH_INTERVAL;
    int timezone = 12;
};

struct Config config;

const size_t mainApiResponseBufferSize =
    31 * JSON_ARRAY_SIZE(5) + JSON_ARRAY_SIZE(30) + JSON_OBJECT_SIZE(2) + 5 * JSON_OBJECT_SIZE(3) + 1320;
DynamicJsonBuffer mainApiResponseBuffer(mainApiResponseBufferSize);
JsonObject *mainApiResponse;

const size_t channelStatsResponseBufferSizes = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 410;
DynamicJsonBuffer channelStatsResponseBuffer(channelStatsResponseBufferSizes);
JsonObject *channelStats;

size_t currentScreen = 0;

Ticker apiTimer(parsApi, config.api_update_interval);
Ticker tokenRefreshTimer(validateAccessToken, API_REFRESH_INTERVAL);
Ticker rebootTimer([]() {
    ESP.restart();
},
                   1000);

Ticker displayTimer(renderScreen, 5000);

void setup()
{
    Serial.begin(9600);
    delay(5000);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    setupI2C();
    Log.notice("Serial ok\n");
    SPIFFSRead();

    setupWifi();
    Log.notice("Connected...yeey :)\n");
    setupNTP();

    setupHTTPServer();
    RSCG12864B.clear();

    if (mdns.begin(MDNS_DOMAIN, WiFi.localIP()))
    {
        mdns.addService("http", "tcp", 80);

        RSCG12864B.print_string_5x7_xy(0, 24, String(String("http://") + MDNS_DOMAIN + ".local").c_str());

        Log.trace("MDNS responder started\n");
    }

    validateAccessToken();
}

void loop()
{
    apiTimer.update();
    tokenRefreshTimer.update();
    rebootTimer.update();
    displayTimer.update();
    server.handleClient();
}


void refreshToken()
{
    Log.trace("Refresh token\n");
    DynamicJsonBuffer jsonBuffer(255);
    JsonObject &refresh_data = refresh(config.refresh_token, config.client_id, config.client_secret, &jsonBuffer);
    if (refresh_data.success() && refresh_data.containsKey("access_token"))
    {
        strlcpy(config.access_token, refresh_data["access_token"], sizeof(config.access_token));
        SPIFFSWrite();
    }
    else
    {
        tokenRefreshTimer.stop();
        Log.error("Token refresh error\n");
        refresh_data.prettyPrintTo(Serial);

        RSCG12864B.clear();
        RSCG12864B.print_string_5x7_xy(0, 0, refresh_data["error"]);
        RSCG12864B.print_string_5x7_xy(0, 20, refresh_data["error_description"]);
    }
}

void validateAccessToken()
{
    Log.trace("validateAccessToken\n");

    if (strlen(config.access_token) == 0)
    {
        RSCG12864B.clear();
        RSCG12864B.print_string_5x7_xy(0, 0, "no token info");
        RSCG12864B.print_string_5x7_xy(0, 12, String(String("http://") + WiFi.localIP().toString()).c_str());
        return;
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(6) + 340;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = info(config.access_token, &jsonBuffer);
    root.prettyPrintTo(Serial);
    if (root.success() && root.containsKey("expires_in"))
    {
        int expires = root["expires_in"].as<int>();
        tokenRefreshTimer.interval(1000 * expires);
        tokenRefreshTimer.start();

        parsApi();
    }
    else
    {
        refreshToken();
    }
}

void setupNTP()
{
    NTP.begin();
    NTP.setInterval(61);
    NTP.setTimeZone(config.timezone);
}

void displayChannelStats()
{
    RSCG12864B.clear();
    if (channelStats->success() && channelStats->containsKey("items"))
    {
        JsonArray &items = channelStats->get<JsonArray>("items");
        if (items.size() > 0)
        {
            JsonObject &stats = items[0]["statistics"];

            char viewCount[50];
            sprintf(viewCount, "views: %d", stats["viewCount"].as<int>());
            RSCG12864B.print_string_5x7_xy(0, 10, viewCount);

            char subscriberCount[50];
            sprintf(subscriberCount, "subscribers: %d", stats["subscriberCount"].as<int>());
            RSCG12864B.print_string_5x7_xy(0, 30, subscriberCount);

            char videoCount[50];
            sprintf(videoCount, "videos: %d", stats["videoCount"].as<int>());
            RSCG12864B.print_string_5x7_xy(0, 40, videoCount);

            Log.notice("viewCount %l\n", stats["viewCount"].as<int>());
            Log.notice("commentCount %l\n", stats["commentCount"].as<int>());
            Log.notice("subscriberCount %l\n", stats["subscriberCount"].as<int>());
            Log.notice("videoCount %l\n", stats["videoCount"].as<int>());
        }
    }
}

void renderScreen()
{
    if (mainApiResponse->get<JsonArray>("rows").size() == 0)
    {
        return Log.error("nothing to render\n");
    }
    JsonArray &columnHeaders = mainApiResponse->get<JsonArray>("columnHeaders");
    if (currentScreen >= columnHeaders.size())
    {
        currentScreen = 0;
    }
    Log.notice("renderScreen %d metricsCount %d\n", currentScreen, columnHeaders.size());

    const char *columnType = columnHeaders[currentScreen]["columnType"];

    if (strcmp(columnType, "METRIC") == 0)
    {
        displayMetric(currentScreen);
    }
    else
    {
        displayChannelStats();
    }
    currentScreen++;
}

void displayData(JsonObject &data)
{
    JsonObject &stats = data["items"][0]["statistics"];
    char viewCount[50];
    sprintf(viewCount, "views: %d", stats["viewCount"].as<int>());
    RSCG12864B.print_string_5x7_xy(0, 10, viewCount);

    char commentCount[50];
    sprintf(commentCount, "comments: %d", stats["commentCount"].as<int>());
    RSCG12864B.print_string_5x7_xy(0, 20, commentCount);

    char subscriberCount[50];
    sprintf(subscriberCount, "subscribers: %d", stats["subscriberCount"].as<int>());
    RSCG12864B.print_string_5x7_xy(0, 30, subscriberCount);

    char videoCount[50];
    sprintf(videoCount, "videos: %d", stats["videoCount"].as<int>());
    RSCG12864B.print_string_5x7_xy(0, 40, videoCount);

    Log.notice("viewCount %l\n", stats["viewCount"].as<int>());
    Log.notice("commentCount %l\n", stats["commentCount"].as<int>());
    Log.notice("subscriberCount %l\n", stats["subscriberCount"].as<int>());
    Log.notice("videoCount %l\n", stats["videoCount"].as<int>());
}

void SPIFFSRead()
{
    const size_t configBufferSize = JSON_OBJECT_SIZE(5) + 400;

    if (!SPIFFS.begin())
    {
        return Log.error("SPIFFS.begin error\n");
    }

    if (SPIFFS.exists(CONFIG_FILE_NAME))
    {
        Log.notice("Reading config file\n");
        File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");

        Log.notice("Opened config file\n");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();
        DynamicJsonBuffer jsonBuffer(configBufferSize);
        JsonObject &json = jsonBuffer.parseObject(buf.get());

        if (json.success())
        {

            Log.notice("Parsed json\n");

            json.prettyPrintTo(Serial);

            if (json.containsKey("access_token"))
            {
                strlcpy(config.access_token, json["access_token"], sizeof(config.access_token));
            }
            if (json.containsKey("refresh_token"))
            {
                strlcpy(config.refresh_token, json["refresh_token"], sizeof(config.refresh_token));
            }

            if (json.containsKey("client_id"))
            {
                strlcpy(config.client_id, json["client_id"], sizeof(config.client_id));
            }

            if (json.containsKey("client_secret"))
            {
                strlcpy(config.client_secret, json["client_secret"], sizeof(config.client_secret));
            }

            if (json.containsKey("api_update_interval"))
            {
                config.api_update_interval = json["api_update_interval"].as<int>();
                apiTimer.interval(config.api_update_interval);
            }

            if (json.containsKey("timezone"))
            {
                config.timezone = json["timezone"].as<int>();
            }
        }
    }
    else
    {
        Log.notice("Config file doesn't exist\n");
    }
}

JsonObject &getJsonConfig()
{
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(4));
    JsonObject &json = jsonBuffer.createObject();
    json["access_token"] = config.access_token;
    json["refresh_token"] = config.refresh_token;
    json["api_update_interval"] = config.api_update_interval;
    json["timezone"] = config.timezone;

    json["client_id"] = config.client_id;
    json["client_secret"] = config.client_secret;
    return json;
}

void SPIFFSWrite()
{
    Log.notice("Saving config\n");
    SPIFFS.remove(CONFIG_FILE_NAME);
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
    if (configFile)
    {
        getJsonConfig().printTo(configFile);
        configFile.close();
    }
    else
    {
        Log.error("Failed to open config file for writing\n");
    }
}

void setupWifi()
{
    WiFiManager wifiManager;
    wifiManager.setMinimumSignalQuality();
    RSCG12864B.clear();
    RSCG12864B.print_string_5x7_xy(0, 35, "Connecting to WiFi...");
    if (!wifiManager.autoConnect(ACCESS_POINT_NAME))
    {
        Log.error("Failed to connect and hit timeout\n");
        RSCG12864B.print_string_5x7_xy(0, 0, "Please connect to wifi");
        RSCG12864B.print_string_5x7_xy(0, 20, ACCESS_POINT_NAME);
        delay(3000);
        ESP.reset();
        delay(5000);
    }
}

void setupI2C()
{
    RSCG12864B.begin();
    RSCG12864B.brightness(200);
    RSCG12864B.print_string_12_xy(20, 35, "LCD init ok");
}

File fsUploadFile;

void handleFileUpload()
{ // upload a new file to the SPIFFS
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        String filename = upload.filename;
        if (!filename.startsWith("/"))
        {
            filename = "/" + filename;
        }

        Log.notice("handleFileUpload Name: %s\n", filename.c_str());
        fsUploadFile = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
        filename = String();
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (fsUploadFile)
        {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (fsUploadFile)
        {                         // If the file was successfully created
            fsUploadFile.close(); // Close the file again
            Log.notice("handleFileUpload Size: %n\n", upload.totalSize);
            server.sendHeader("Location", "/dir"); // Redirect the client to the success page
            server.send(303);
        }
        else
        {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void handleFileList()
{
    String path = server.hasArg("dir") ? server.arg("dir") : "/";
    Log.notice("handleFileList: %s\n", path.c_str());
    Dir dir = SPIFFS.openDir(path);
    path = String();

    String output = "[";
    while (dir.next())
    {
        File entry = dir.openFile("r");
        if (output != "[")
        {
            output += ',';
        }
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\"}";
        entry.close();
    }

    output += "]";
    server.send(200, TEXT_JSON, output);
}

void handleExchange()
{
    if (server.hasArg("authorization_code"))
    {
        String authorization_code = server.arg("authorization_code");
        Log.notice("authorization_code: %s\n", authorization_code.c_str());
        DynamicJsonBuffer jsonBuffer(255);
        JsonObject &root = exchange(authorization_code, config.client_id, config.client_secret, &jsonBuffer);

        if (root.success() && !root.containsKey("error"))
        {
            strlcpy(config.access_token, root["access_token"], sizeof(config.access_token));
            strlcpy(config.refresh_token, root["refresh_token"], sizeof(config.refresh_token));
            SPIFFSWrite();
            server.sendHeader("Location", "/config", true);
            server.send(302, "text/plain", "");
        }
        else
        {

            char output[root.measureLength() + 1];
            root.printTo(output, sizeof(output));

            Log.error("authResponse %s\n", output);
            server.send(500, TEXT_JSON, output);
        }
    }
    else
    {
        server.send(500, TEXT_JSON, "{\"error\": \"authorization_code is requred\"}");
    }
}

void setupHTTPServer()
{

    server.on("/token", []() {
        DynamicJsonBuffer jsonBuffer(255);
        JsonObject &root = info(config.access_token, &jsonBuffer);
        char output[root.measureLength() + 1];
        root.printTo(output, sizeof(output));
        server.send(200, TEXT_JSON, output);
    });
    server.on("/reboot", HTTP_POST, []() {
        server.send(200, "text/plain", "rebooting....");
        rebootTimer.start();
    });

    server.on("/upload", HTTP_POST, []() { server.send(200); }, handleFileUpload);

    server.on("/dir", handleFileList);

    server.on("/config", HTTP_POST, []() {
        Log.notice("Upload new config\n");
        if (server.hasArg("api_update_interval"))
        {
            config.api_update_interval = atoi(server.arg("api_update_interval").c_str());
            apiTimer.interval(config.api_update_interval);
        }

        if (server.hasArg("api_update_interval"))
        {
            config.timezone = atoi(server.arg("timezone").c_str());
            NTP.setTimeZone(config.timezone);
        }

        if (server.hasArg("client_id"))
        {
            strlcpy(config.client_id, server.arg("client_id").c_str(), sizeof(config.client_id));
        }

        if (server.hasArg("client_secret"))
        {
            strlcpy(config.client_secret, server.arg("client_secret").c_str(), sizeof(config.client_secret));
        }
        SPIFFSWrite();
        server.sendHeader("Location", "/config", true);
        server.send(302, "text/plain", "");
    });

    server.on("/config", HTTP_DELETE, []() {
        Log.notice("Remove config\n");
        SPIFFS.remove(CONFIG_FILE_NAME);
    });

    server.on("/config", []() {
        Log.notice("Get config from memory\n");
        JsonObject &root = getJsonConfig();
        char output[root.measureLength() + 1];
        root.printTo(output, sizeof(output));
        server.send(200, TEXT_JSON, output);
    });

    server.on("/auth", []() {
        server.sendHeader("Location", authUrl(config.client_id, config.client_secret), true);
        server.send(302, "text/plain", "");
    });

    server.on("/exchange", HTTP_POST, handleExchange);

    server.serveStatic(CONFIG_FILE_NAME, SPIFFS, CONFIG_FILE_NAME);
    server.serveStatic("/", SPIFFS, "/main.html");

    server.begin();
}



void displayMetric(size_t idx)
{
    RSCG12864B.clear();
    JsonArray &rows = mainApiResponse->get<JsonArray>("rows");
    const char *name = mainApiResponse->get<JsonArray>("columnHeaders")[idx]["name"];
    Log.notice("Display metric: %s points: %d\n", name, rows.size());
    const int width = 128;
    const int height = 47;
    const int barWidth = width / rows.size();
    const int startHeight = 8;

    int minValue = rows[0][idx];
    int maxValue = rows[0][idx];

    for (size_t row = 0; row < rows.size(); row++)
    {
        minValue = min((int)rows[row][idx], minValue);
        maxValue = max((int)rows[row][idx], maxValue);
    }

    int preX, preY, x, y;
    preX = preY = 0;
    size_t rowsCount = rows.size();

    String maxStr = "max:" + String(maxValue);
    int maxYpostion = width - 6 * maxStr.length();

    RSCG12864B.draw_fill_rectangle(0, 0, 127, 7);
    RSCG12864B.draw_fill_rectangle(0, 56, 127, 63);
    RSCG12864B.font_revers_on();
    RSCG12864B.print_string_5x7_xy(0, 0, name);
    RSCG12864B.print_string_5x7_xy(0, 56, String("min:" + String(minValue)).c_str());

    time_t moment = NTP.getTime();
    char timeStr[5];
    sprintf(timeStr, "%02d:%02d", hour(moment), minute(moment));

    RSCG12864B.print_string_5x7_xy(95, 56, timeStr);

    RSCG12864B.print_string_5x7_xy(maxYpostion, 0, maxStr.c_str());
    RSCG12864B.font_revers_off();

    for (size_t row = 0; row < rowsCount; row++)
    {
        int mappedValue = map(rows[row][idx], minValue, maxValue, 0, height);
        x = row * barWidth + row;
        y = startHeight + height - mappedValue;

        RSCG12864B.draw_fill_circle(x, y, 1);
        if (row == 0)
        {
            RSCG12864B.draw_pixel(x, y);
        }
        else
        {
            RSCG12864B.draw_line(preX, preY, x, y);
        }

        preX = x;
        preY = y;
    }
}

void parsApi()
{

    time_t currentTime = NTP.getTime();

    time_t monthAgo = DatePlusDays(currentTime, -32);
    char todayStr[12];
    char monthAgoStr[12];

    sprintf(todayStr, "%4d-%02d-%02d", year(currentTime), month(currentTime), day(currentTime));
    sprintf(monthAgoStr, "%4d-%02d-%02d", year(monthAgo), month(monthAgo), day(monthAgo));

    Log.notice("TOday=%s, monthAgoStr=%s\n", todayStr, monthAgoStr);
    mainApiResponse = &callApi(config.access_token, String(monthAgoStr), String(todayStr), &mainApiResponseBuffer);
    channelStats = &getChannelStats(config.access_token, &channelStatsResponseBuffer);

    if (mainApiResponse->success())
    {
        displayTimer.start();
        apiTimer.start();
    }
    else
    {
        displayTimer.stop();
        mainApiResponse->prettyPrintTo(Serial);
    }
}

