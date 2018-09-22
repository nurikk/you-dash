#include "main.h"
#include <FS.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Ticker.h>
#include <TimeLib.h>
#include <NtpClientLib.h>

#define NTP_SERVER "ntp2.stratum2.ru"
#define CONFIG_FILE_NAME "/config.json"
#define I2C_ADDR 0x27
#define I2C_LINES 4
#define I2C_COLS 24
#define DEFAULT_API_UPDATE_INTERVAL 5000
#define DEFAULT_NTP_UPDATE_INTEVAL 1000
#define API_URL "http://content.googleapis.com/youtube/v3/channels?id=%s&part=statistics&key=%s"

struct Config
{
    char api_token[40];
    char channel_id[25];
    int api_update_interval = DEFAULT_API_UPDATE_INTERVAL;
    int ntp_update_interval = DEFAULT_NTP_UPDATE_INTEVAL;
};

Config config;

bool shouldSaveConfig = false;
bool initializationDone = false;

Ticker apiTimer(getJsonData, config.api_update_interval);
Ticker ntpTimer(ntpTick, config.ntp_update_interval);

LiquidCrystal_I2C lcd(I2C_ADDR, I2C_COLS, I2C_LINES);

void setupNTP()
{
    NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
        if (error)
        {
            log("Time Sync error: ");
            if (error == noResponse)
            {
                log("NTP server not reachable");
            }
            else if (error == invalidAddress)
            {
                log("Invalid NTP server address");
            }
        }
        else
        {

            log(("Got NTP time: " + NTP.getTimeDateString(NTP.getLastNTPSync())).c_str());
        }
    });
    NTP.begin(NTP_SERVER, 1, true);
    NTP.setInterval(63);
}
// TODO: display time on display
void ntpTick()
{
    String msg = "Got NTP time: " + NTP.getTimeDateString(NTP.getLastNTPSync());
    log(msg.c_str());
}

void log(const char *logString)
{
    if (!initializationDone)
    {
        Serial.println(logString);
    }
}
void saveConfigCallback()
{
    log("Should save config");
    shouldSaveConfig = true;
}

// TODO: display data
void displayData(JsonObject &data)
{
}
void getJsonData()
{

    char api_url[255];
    sprintf(api_url, API_URL, config.channel_id, config.api_token);
    HTTPClient http;
    http.begin(api_url);
    int httpCode = http.GET();
    if (httpCode > 0)
    {
        log("[HTTP] GET... code: " + httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();

            log(payload.c_str());

            const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
            DynamicJsonBuffer jsonBuffer(capacity);

            JsonObject &root = jsonBuffer.parseObject(payload);
            if (!root.success())
            {
                log("Parsing failed!");
            }
            else
            {
                displayData(root);
                // seems all ok
                initializationDone = true;
            }
        }
    }
    else
    {
        log(http.errorToString(httpCode).c_str());
    }

    http.end();
}

void SPIFFSRead()
{
    if (SPIFFS.begin() && SPIFFS.exists(CONFIG_FILE_NAME))
    {
        log("reading config file");
        File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
        if (configFile)
        {
            log("opened config file");
            size_t size = configFile.size();
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(buf.get());

            if (json.success())
            {
                log("parsed json");
                strlcpy(config.api_token, json["api_token"], sizeof(config.api_token));
                strlcpy(config.channel_id, json["channel_id"], sizeof(config.channel_id));
                config.api_update_interval = json["api_update_interval"] || DEFAULT_API_UPDATE_INTERVAL;
                config.ntp_update_interval = json["ntp_update_interval"] || DEFAULT_NTP_UPDATE_INTEVAL;
            }
        }
    }
}

void SPIFFSWrite()
{
    log("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["api_token"] = config.api_token;
    json["channel_id"] = config.channel_id;
    json["api_update_interval"] = config.api_update_interval;
    json["ntp_update_interval"] = config.ntp_update_interval;

    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
    if (configFile)
    {
        json.printTo(configFile);
        configFile.close();
    }
    else
    {
        log("failed to open config file for writing");
    }
}
void setupWifi()
{
    WiFiManager wifiManager;
    wifiManager.setMinimumSignalQuality();

    WiFiManagerParameter custom_api_token("api_token", "api token", config.api_token, 40);
    WiFiManagerParameter custom_channel_id("channel_id", "channel id", config.channel_id, 25);
    // TODO: add api_update_interval and ntp_update_interval params to setup

    wifiManager.addParameter(&custom_api_token);
    wifiManager.addParameter(&custom_channel_id);

    wifiManager.setSaveConfigCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("Youtube dashboard"))
    {
        log("failed to connect and hit timeout");
        delay(3000);
        ESP.reset();
        delay(5000);
    }

    strlcpy(config.api_token, custom_api_token.getValue(), sizeof(config.api_token));
    strlcpy(config.channel_id, custom_channel_id.getValue(), sizeof(config.channel_id));
}
void setup()
{

    Serial.begin(115200);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(3, 0);
    log("Hello, world!");

    SPIFFSRead();
    setupWifi();

    if (shouldSaveConfig)
    {
        SPIFFSWrite();
    }
    log("connected...yeey :)");

    apiTimer.start();
    ntpTimer.start();
}

void loop()
{
    apiTimer.update();
    ntpTimer.update();
}