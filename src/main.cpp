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


#define NTP_SERVER "ntp2.stratum2.ru"
#define CONFIG_FILE_NAME "/config.json"
#define I2C_ADDR 0x0
#define I2C_LINES 4
#define I2C_COLS 24
#define DEFAULT_API_UPDATE_INTERVAL 5000
#define DEFAULT_NTP_UPDATE_INTEVAL 1000
#define API_HOST "content.googleapis.com"
#define API_URL "/youtube/v3/channels?id=%s&part=statistics&key=%s"

struct Config
{
    char api_token[40] = "AIzaSyAquyQRuQrBnatHDoJEh-PR8egKtxZQ3Sk";
    char channel_id[25] = "UCJvx6WSYs463NNJZr1MaLkw";
    int api_update_interval = DEFAULT_API_UPDATE_INTERVAL;
    int ntp_update_interval = DEFAULT_NTP_UPDATE_INTEVAL;
    int timezone = 12;
};

Config config;

bool shouldSaveConfig = false;
bool initializationDone = false;

Ticker apiTimer(getJsonData, config.api_update_interval);
Ticker ntpTimer(ntpTick, config.ntp_update_interval);



WiFiClientSecure client;

size_t CustomConsole::write(uint8_t character)
{
    return Serial.print(character);
}

size_t CustomConsole::write(const char *str)
{
    return Serial.println(str);
}

void setupNTP()
{
    NTP.begin();

    NTP.setInterval(63);
    NTP.setTimeZone(12);
}
// TODO: display time on display
void ntpTick()
{
    // Serial.println (NTP.getTimeDateString ());
    Log.notice("Got NTP time: %s\n", NTP.getTimeDateString().c_str());
    RSCG12864B.print_string_5x7_xy(0,0, NTP.getTimeDateString().c_str());
}

void saveConfigCallback()
{
    Log.notice("Should save config\n");
    shouldSaveConfig = true;
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

    char api_url[255];

    sprintf(api_url, API_URL, config.channel_id, config.api_token);

    Log.notice("Getting json api data... %s\n", api_url);
    if (!client.connect(API_HOST, 443))
    {
        Log.error("Connection failed\n");
        return;
    }

    client.print(String("GET ") + api_url + " HTTP/1.1\r\n" +
                 "Host: " + API_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    while (client.connected())
    {
        String line = client.readStringUntil('\n');
        Log.trace("%s\n", line.c_str());
        if (line == "\r")
        {
            Log.notice("Headers received\n");
            break;
        }
    }
    String payload = "";
    while (client.connected())
    {
        payload += client.readStringUntil('\n');
        payload += '\n';
    }

    Log.trace("Payload: %s\n", payload.c_str());

    const size_t capacity = 1024;
    DynamicJsonBuffer jsonBuffer(capacity);

    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
    {
        displayData(root);
        initializationDone = true;
    }
    else
    {
        Log.error("Parsing failed!\n");
    }
}

void SPIFFSRead()
{
    if (SPIFFS.begin() && SPIFFS.exists(CONFIG_FILE_NAME))
    {
        Log.notice("Reading config file\n");
        File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
        if (configFile)
        {
            Log.notice("Opened config file\n");
            size_t size = configFile.size();
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(buf.get());

            if (json.success())
            {
                Log.notice("Parsed json\n");
                strlcpy(config.api_token, json["api_token"], sizeof(config.api_token));
                strlcpy(config.channel_id, json["channel_id"], sizeof(config.channel_id));
                config.api_update_interval = json["api_update_interval"] || DEFAULT_API_UPDATE_INTERVAL;
                config.ntp_update_interval = json["ntp_update_interval"] || DEFAULT_NTP_UPDATE_INTEVAL;
                config.timezone = json["timezone"] || 0;
            }
        }
    }
}

void SPIFFSWrite()
{
    Log.notice("Saving config\n");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["api_token"] = config.api_token;
    json["channel_id"] = config.channel_id;
    json["api_update_interval"] = config.api_update_interval;
    json["ntp_update_interval"] = config.ntp_update_interval;
    json["timezone"] = config.timezone;

    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
    if (configFile)
    {
        json.printTo(configFile);
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

    WiFiManagerParameter custom_api_token("api_token", "api token", config.api_token, 40);
    WiFiManagerParameter custom_channel_id("channel_id", "channel id", config.channel_id, 25);
    // TODO: add api_update_interval, ntp_update_interval and timezone params to setup

    wifiManager.addParameter(&custom_api_token);
    wifiManager.addParameter(&custom_channel_id);

    wifiManager.setSaveConfigCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("Youtube dashboard"))
    {
        Log.error("Failed to connect and hit timeout\n");
        delay(3000);
        ESP.reset();
        delay(5000);
    }

    strlcpy(config.api_token, custom_api_token.getValue(), sizeof(config.api_token));
    strlcpy(config.channel_id, custom_channel_id.getValue(), sizeof(config.channel_id));
}



void setupI2C(){
    RSCG12864B.begin();
    RSCG12864B.brightness(200);
    RSCG12864B.print_string_12_xy(20,35, "LCD init ok");
}

void setup()
{

    delay(5000);
    setupI2C();

    Serial.begin(9600);
    Serial.println("Serial ok");

    Log.begin(LOG_LEVEL_VERBOSE, &Serial);

    Log.notice("Hello, world!\n");

    // SPIFFSRead();
    setupWifi();

    if (shouldSaveConfig)
    {
        SPIFFSWrite();
    }
    Log.notice("Connected...yeey :)\n");

    apiTimer.start();
    ntpTimer.start();

    setupNTP();


    RSCG12864B.print_string_12_xy(0,0, "Setup done");
    delay(3000);
    RSCG12864B.clear();
}

void loop()
{
    apiTimer.update();
    ntpTimer.update();
}
