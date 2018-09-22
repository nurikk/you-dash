#include <FS.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Ticker.h"
#include <TimeLib.h>
#include <NtpClientLib.h>

#define NTP_SERVER "ntp2.stratum2.ru"
#define CONFIG_FILE_NAME "/config.json"
#define I2C_ADDR 0x27
#define I2C_LINES 4
#define I2C_COLS 24

int api_update_interval = 5000;
char api_token[40];
char channel_id[25];
bool shouldSaveConfig = false;
bool initializationDone = false;
void getJsonData();
void ntpTick();
void log(const char *logString);

Ticker apiTimer(getJsonData, api_update_interval);
Ticker ntpTimer(ntpTick, 1000);

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
    log((const char *) msg.c_str());
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
    sprintf(api_url, "http://content.googleapis.com/youtube/v3/channels?id=%s&part=statistics&key=%s", channel_id, api_token);
    HTTPClient http;
    http.begin(api_url);
    int httpCode = http.GET();
    if (httpCode > 0)
    {
        // HTTP header has been send and Server response header has been handled
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
    if (SPIFFS.begin())
    {
        log("mounted file system");
        if (SPIFFS.exists(CONFIG_FILE_NAME))
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
                    strcpy(api_token, json["api_token"]);
                }
            }
        }
    }
}

void SPIFFSWrite()
{
    log("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["api_token"] = api_token;
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
    if (!configFile)
    {
        log("failed to open config file for writing");
    }
    json.printTo(configFile);
    configFile.close();
}
void setupWifi()
{
    WiFiManager wifiManager;
    wifiManager.setMinimumSignalQuality();
    WiFiManagerParameter custom_api_token("api_token", "api token", api_token, 40);
    wifiManager.addParameter(&custom_api_token);
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("AutoConnectAP"))
    {
        log("failed to connect and hit timeout");
        delay(3000);
        ESP.reset();
        delay(5000);
    }

    strcpy(api_token, custom_api_token.getValue());
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