#include "main.h"
#include <Arduino.h>

#include <Time.h>
#include <FS.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <time.h>
#include <sys/time.h> // struct timeval
// #include <coredecls.h>                  // settimeofday_cb()

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Ticker.h>
// #include <TimeLib.h>
#include <NtpClientLib.h>
#include <ArduinoLog.h>
#include <RSCG12864B.h>
#include <ESP8266mDNS.h>
#include "utils.h"

#define TEXT_JSON "text/json"

MDNSResponder mdns;

ESP8266WebServer server(80);

#define CONFIG_FILE_NAME "/config.json"
#define ACCESS_POINT_NAME "Youtube dashboard"
#define MDNS_DOMAIN "you-dash"
#define SSD_NAME "Youtube dashboard"

#define API_REFRESH_INTERVAL 30 * 1000 // 1 minute

struct Config
{
    char AccountKey[100] = "";
    char BusStopCode[100] = "";
};

struct Config config;

Ticker apiTimer(parsApi, API_REFRESH_INTERVAL);

Ticker rebootTimer([]() {
    ESP.restart();
},
                   1000);

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

    setupHTTPServer();
    RSCG12864B.clear();

    if (mdns.begin(MDNS_DOMAIN, WiFi.localIP()))
    {
        mdns.addService("http", "tcp", 80);

        RSCG12864B.print_string_5x7_xy(0, 24, String(String("http://") + MDNS_DOMAIN + ".local").c_str());

        Log.trace("MDNS responder started\n");
    }

    setupNTP();
}

void loop()
{
    apiTimer.update();
    rebootTimer.update();
    server.handleClient();
}

#define TZ 8 // (utc+) TZ in hours

bool ntpSynced = false;
void setupNTP()
{
    NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
        Log.notice("NTP: error %d %d\n", error, NTP.getLastNTPSync());

        if (!error && !ntpSynced)
        {
            ntpSynced = true;
            apiTimer.start();
            // parsApi();
        }
    });
    NTP.setInterval(61);
    NTP.begin("pool.ntp.org", 8, false);
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

            if (json.containsKey("BusStopCode"))
            {
                strlcpy(config.BusStopCode, json["BusStopCode"], sizeof(config.BusStopCode));
            }

            if (json.containsKey("AccountKey"))
            {
                strlcpy(config.AccountKey, json["AccountKey"], sizeof(config.AccountKey));
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
    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(2));
    JsonObject &json = jsonBuffer.createObject();
    json["AccountKey"] = config.AccountKey;
    json["BusStopCode"] = config.BusStopCode;
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

void setupHTTPServer()
{

    server.on("/reboot", HTTP_POST, []() {
        server.send(200, "text/plain", "rebooting....");
        rebootTimer.start();
    });

    server.on("/upload", HTTP_POST, []() { server.send(200); }, handleFileUpload);

    server.on("/dir", handleFileList);

    server.on("/config", HTTP_POST, []() {
        Log.notice("Upload new config\n");

        if (server.hasArg("AccountKey"))
        {
            strlcpy(config.AccountKey, server.arg("AccountKey").c_str(), sizeof(config.AccountKey));
        }

        if (server.hasArg("BusStopCode"))
        {
            strlcpy(config.BusStopCode, server.arg("BusStopCode").c_str(), sizeof(config.BusStopCode));
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

    server.serveStatic(CONFIG_FILE_NAME, SPIFFS, CONFIG_FILE_NAME);
    server.serveStatic("/", SPIFFS, "/main.html");

    server.begin();
}

JsonObject &getSchedule(char *BusStopCode, char *AccountKey, DynamicJsonBuffer *jsonBuffer)
{
    String host = String("api.mytransport.sg");
    int httpsPort = 443;
    String path = String("/ltaodataservice/BusArrivalv2?BusStopCode=") + urlencode(BusStopCode);

    String reqHeader = "";
    reqHeader += ("GET " + path + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + host + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("AccountKey: " + String(AccountKey) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
    return getRequest(host.c_str(), httpsPort, reqHeader, jsonBuffer);
}

String busKeys[3] = {"NextBus", "NextBus2", "NextBus3"};

void displayRow(int rowId, JsonObject *service)
{
    String logKey = "";

    int fontSize = 16;
    String ServiceNo = service->get<String>("ServiceNo");
    RSCG12864B.print_string_16_xy(0, fontSize * rowId, ServiceNo.c_str());
    time_t currentTime = NTP.getTime();
    // RSCG12864B.font_revers_on();

    // RSCG12864B.font_revers_off();
    logKey += ServiceNo + ": ";

    for (size_t i = 0; i < sizeof(busKeys) / sizeof(busKeys[0]); i++)
    {
        String busKey = busKeys[i];
        Log.notice("%s\n", busKey.c_str());
        if (service->containsKey(busKey))
        {

            JsonObject &bus = service->get<JsonObject>(busKey.c_str());

            String timeText = "--";
            const char *EstimatedArrival = bus.get<char *>("EstimatedArrival");
            if (strlen(EstimatedArrival) > 0)
            {

                time_t est = parseTime(EstimatedArrival);
                long waitMinutes = round((est - currentTime) / 60);
                Log.notice("waitMinutes %s %d\n", EstimatedArrival, waitMinutes);

                if (waitMinutes < 0)
                {
                    timeText = "Left";
                }
                else if (waitMinutes < 100)
                {
                    timeText = String(waitMinutes);
                }
            }
            logKey += " " + timeText;
            // timeText = "Left";

            RSCG12864B.print_string_5x7_xy(30 + 30 * i, 4 + fontSize * rowId, timeText.c_str());
        }
        Log.notice("%s\n", logKey.c_str());
    }

    // service->prettyPrintTo(Serial);
    Log.notice("Render %s\n", ServiceNo.c_str());
}
void parsApi()
{

    const size_t capacity = JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(3) + 4 * JSON_OBJECT_SIZE(5) + 12 * JSON_OBJECT_SIZE(9) + 7029;

    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject &root = getSchedule(config.BusStopCode, config.AccountKey, &jsonBuffer);
    if (root.success())
    {
        RSCG12864B.clear();
        JsonArray &Services = root["Services"];
        if (Services.size() > 0)
        {
            for (size_t i = 0; i < Services.size(); i++)
            {
                displayRow(i, &Services.get<JsonObject>(i));
            }
        }
        else
        {
            RSCG12864B.print_string_5x7_xy(0, 35, "No buses, too late :(");
        }
    }
    RSCG12864B.print_string_5x7_xy(5, 56, NTP.getTimeDateString().c_str());
    Log.notice("Current time: %s\n", NTP.getTimeDateString().c_str());
    Log.notice("parse api\n");
}
