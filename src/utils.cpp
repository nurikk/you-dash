#include <WString.h>
#include <WiFiClientSecure.h>
#include <ArduinoLog.h>
#include <ArduinoJson.h>
#include <TimeLib.h>


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

JsonObject &request(const char *server, const int port, String data, DynamicJsonBuffer *jsonBuffer)
{
    jsonBuffer->clear();
    WiFiClientSecure client;
    Log.notice("Connecting to: %s:%d\n", server, port);
    if (!client.connect(server, port))
    {
        Log.error("Connection failed\n");
        return jsonBuffer->createObject();
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

    return jsonBuffer->parseObject(client);
}

JsonObject &postRequest(const char *server, const int port, String header, String data, DynamicJsonBuffer *jsonBuffer)
{
    return request(server, port, header + data, jsonBuffer);
}

JsonObject &getRequest(const char *server, const int port, String data, DynamicJsonBuffer *jsonBuffer)
{
    return request(server, port, data, jsonBuffer);
}

time_t DatePlusDays(time_t startTime, int days)
{
    const time_t ONE_DAY = 24 * 60 * 60;

    return startTime + (days * ONE_DAY);
}

char *ultos_recursive(unsigned long val, char *s, unsigned radix, int pos)
{
  int c;

  if (val >= radix)
    s = ultos_recursive(val / radix, s, radix, pos+1);
  c = val % radix;
  c += (c < 10 ? '0' : 'a' - 10);
  *s++ = c;
  if (pos % 3 == 0) *s++ = ',';
  return s;
}

char *ltos(long val, char *s, int radix)
{
  if (radix < 2 || radix > 36) {
    s[0] = 0;
  } else {
    char *p = s;
    if (radix == 10 && val < 0) {
      val = -val;
      *p++ = '-';
    }
    p = ultos_recursive(val, p, radix, 0) - 1;
    *p = 0;
  }
  return s;
}



time_t parseTime(const char *str)
{
    tmElements_t tm;
int Year, Month, Day, Hour, Minute, Second, TzHourOffset, TzMinOffset ;
    // 2019-01-29T23:29:56+08:00
  sscanf(str, "%d-%d-%dT%d:%d:%d+%d:%d", &Year, &Month, &Day, &Hour, &Minute, &Second, &TzHourOffset, &TzMinOffset);
  tm.Year = CalendarYrToTm(Year);
  tm.Month = Month;
  tm.Day = Day;
  tm.Hour = Hour;
  tm.Minute = Minute;
  tm.Second = Second;
  return makeTime(tm);
}