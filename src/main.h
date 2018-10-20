

void getJsonData();
void ntpTick();
void log(const char *logString);
void setupNTP();
void saveConfigCallback();
void getJsonData();
void SPIFFSRead();
void SPIFFSWrite();
void setupWifi();



class CustomConsole: public Print {
    public:
        virtual size_t write(uint8_t);
        virtual size_t write(const char *str);
};