#include <WString.h>
#include <WiFiClientSecure.h>
#include <ArduinoLog.h>
#include <ArduinoJson.h>

String urlencode(String str)
{
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (unsigned int i = 0; i < str.length(); i++)
    {
        c = str.charAt(i);
        if (c == ' ')
        {
            encodedString += '+';
        }
        else if (isalnum(c))
        {
            encodedString += c;
        }
        else
        {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9)
            {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9)
            {
                code0 = c - 10 + 'A';
            }
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
        yield();
    }
    return encodedString;
}

JsonObject &request(const char *server, const int port, String data)
{

    

    WiFiClientSecure client;
    Log.notice("Connecting to: %s:%d\n", server, port);
    if (!client.connect(server, port))
    {
        Log.error("Connection failed\n");
        const size_t bufferSize = JSON_OBJECT_SIZE(0);
        DynamicJsonBuffer buff(bufferSize);
        return buff.createObject();
    }
    Log.trace("Request ->\n%s\n<-\n", data.c_str());
    client.print(data);

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
    DynamicJsonBuffer jsonBuffer;
    return jsonBuffer.parseObject(client);
}

JsonObject &postRequest(const char *server, const int port, String header, String data)
{
    Log.notice("Calling postRequest\n");
    JsonObject &resp = request(server, port, header + data);
    Log.notice("End calling postRequest\n");
    resp.prettyPrintTo(Serial);
    return resp;
}

JsonObject &getRequest(const char *server, const int port, String data)
{
    JsonObject &resp = request(server, port, data);
    resp.prettyPrintTo(Serial);
    return resp;
}