#include <Print.h>
#include "main.h"
#include <WiFiClientSecure.h>
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
#include "PageBuilder.h"
#include "utils.h"
#include "oauth.h"

#define TEXT_JSON "text/json"

MDNSResponder mdns;

ESP8266WebServer server(80);

#define NTP_SERVER "ntp2.stratum2.ru"
#define CONFIG_FILE_NAME "/config.json"
#define I2C_ADDR 0x0
#define I2C_LINES 4
#define I2C_COLS 24
#define DEFAULT_API_UPDATE_INTERVAL 5000
#define DEFAULT_NTP_UPDATE_INTEVAL 5000
#define API_HOST "content.googleapis.com"
#define API_URL "/youtube/v3/channels?id=%s&part=statistics&key=%s"
#define SSD_NAME "Youtube dashboard"
#define MDNS_DOMAIN "you-dash"

#define API_REFRESH_INTERVAL 5 * 60 * 1000 // 5 minutes

const size_t configBufferSize = JSON_OBJECT_SIZE(5) + 400;

struct Config
{
    char access_token[150] = "";
    char refresh_token[50] = "";
    int api_update_interval = DEFAULT_API_UPDATE_INTERVAL;
    int timezone = 12;
};

struct Config config;

PageElement IndexBody("file:/main.html");

PageBuilder IndexPage("/", {IndexBody});

bool initializationDone = false;

Ticker apiTimer(getJsonData, config.api_update_interval);
Ticker ntpTimer(ntpTick, DEFAULT_NTP_UPDATE_INTEVAL);
Ticker tokenRefreshTimer(refreshToken, API_REFRESH_INTERVAL);
Ticker rebootTimer([]() {
    ESP.restart();
},
                   1000);

WiFiClientSecure client;
#define MINUTE 1000 * 60

void refreshToken()
{
    Log.trace("Refres token\n");
    JsonObject &refresh_data = refresh(config.refresh_token);
    if (refresh_data.success() && refresh_data.containsKey("access_token"))
    {
        strlcpy(config.access_token, refresh_data["access_token"], sizeof(config.access_token));
        SPIFFSWrite();
    }
    else
    {
        Log.error("Token resresh error\n");
        refresh_data.prettyPrintTo(Serial);
    }
}
void oauthTokenRefresh()
{
    Log.trace("oauthTokenRefresh\n");
    JsonObject &root = info(config.access_token);
    root.prettyPrintTo(Serial);
    if (root.success() && root.containsKey("expires_in"))
    {
        int expires = root["expires_in"].as<int>();
        tokenRefreshTimer.interval(1000 * expires);
    }
    else
    {
        refreshToken();
    }
}
void setupNTP()
{
    NTP.begin();
    NTP.setInterval(63);
    NTP.setTimeZone(config.timezone);
}
// TODO: display time on display
void ntpTick()
{
    // Serial.println (NTP.getTimeDateString ());
    Log.notice("Got NTP time: %s\n", NTP.getTimeDateString().c_str());
    RSCG12864B.print_string_5x7_xy(0, 0, NTP.getTimeDateString().c_str());
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
void getJsonData()
{

    // char api_url[255];

    // Log.notice("Getting json api data... %s\n", api_url);
    // if (!client.connect(API_HOST, 443))
    // {
    //     Log.error("Connection failed\n");
    //     return;
    // }

    // client.print(String("GET ") + api_url + " HTTP/1.1\r\n" +
    //              "Host: " + API_HOST + "\r\n" +
    //              "Connection: close\r\n\r\n");

    // while (client.connected())
    // {
    //     String line = client.readStringUntil('\n');
    //     Log.trace("%s\n", line.c_str());
    //     if (line == "\r")
    //     {
    //         Log.notice("Headers received\n");
    //         break;
    //     }
    // }
    // String payload = "";
    // while (client.connected())
    // {
    //     payload += client.readStringUntil('\n');
    //     payload += '\n';
    // }

    // Log.trace("Payload: %s\n", payload.c_str());

    // const size_t capacity = 1024;
    // DynamicJsonBuffer jsonBuffer(capacity);

    // JsonObject &root = jsonBuffer.parseObject(payload);
    // if (root.success())
    // {
    //     displayData(root);
    //     initializationDone = true;
    // }
    // else
    // {
    //     Log.error("Parsing failed!\n");
    // }
}

void SPIFFSRead()
{
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

            strlcpy(config.access_token, json["access_token"], sizeof(config.access_token));
            strlcpy(config.refresh_token, json["refresh_token"], sizeof(config.refresh_token));
            config.api_update_interval = json["api_update_interval"].as<int>();
            config.timezone = json["timezone"].as<int>();
        }
    }
    else
    {
        Serial.println("config file doesn't exist");
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
    json.prettyPrintTo(Serial);
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

    if (!wifiManager.autoConnect("Youtube dashboard"))
    {
        Log.error("Failed to connect and hit timeout\n");
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

        Log.notice("handleFileUpload Name: %s", filename.c_str());
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
    Log.notice("handleFileList: %s", path.c_str());
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
        JsonObject &root = exchange(authorization_code);

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

            Log.error("authResponse %s", output);
            server.send(500, TEXT_JSON, output);
        }
    }
    else
    {
        server.send(500, TEXT_JSON, "{\"error\": \"authorization_code is requred\"");
    }
}

void setupHTTPServer()
{

    server.on("/token", []() {
        JsonObject &root = info(config.access_token);
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

    server.on("/config", []() {
        JsonObject &root = getJsonConfig();
        char output[root.measureLength() + 1];
        root.printTo(output, sizeof(output));
        server.send(200, TEXT_JSON, output);
    });

    server.on("/config", HTTP_POST, []() {
        config.api_update_interval = atoi(server.arg("api_update_interval").c_str());
        config.timezone = atoi(server.arg("timezone").c_str());

        NTP.setTimeZone(config.timezone);
        apiTimer.interval(config.api_update_interval);
        SPIFFSWrite();
        server.sendHeader("Location", "/config", true);
        server.send(302, "text/plain", "");
    });

    server.on("/config", HTTP_DELETE, []() {
        SPIFFS.remove(CONFIG_FILE_NAME);
    });

    server.on("/auth", []() {
        server.sendHeader("Location", authUrl(), true);
        server.send(302, "text/plain", "");
    });

    server.on("/exchange", HTTP_POST, handleExchange);

    IndexPage.insert(server);
    server.serveStatic(CONFIG_FILE_NAME, SPIFFS, CONFIG_FILE_NAME);

    server.begin();
}

void setup()
{
    Serial.begin(9600);
    delay(5000);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);

    Log.notice("Serial ok\n");

    if (SPIFFS.begin())
    {
        Log.notice("SPIFFS.begin\n");
    }
    else
    {
        Log.error("SPIFFS error\n");
    }

    SPIFFSRead();
    setupI2C();
    setupWifi();
    Log.notice("Connected...yeey :)\n");
    setupNTP();
    setupHTTPServer();
    RSCG12864B.clear();

    RSCG12864B.print_string_5x7_xy(0, 0, "open");
    RSCG12864B.print_string_5x7_xy(0, 12, String(String("http://") + WiFi.localIP().toString()).c_str());

    if (mdns.begin(MDNS_DOMAIN, WiFi.localIP()))
    {
        mdns.addService("http", "tcp", 80);

        RSCG12864B.print_string_5x7_xy(0, 24, String(String("http://") + MDNS_DOMAIN + ".local").c_str());

        Log.trace("MDNS responder started\n");
    }

    // apiTimer.start();
    // ntpTimer.start();
    // tokenRefreshTimer.start();
    // oauthTokenRefresh();
}

void loop()
{
    apiTimer.update();
    ntpTimer.update();
    tokenRefreshTimer.update();
    rebootTimer.update();

    server.handleClient();
}
