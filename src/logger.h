#include <Arduino.h>

class Logger : public Print
{
  private:
    String *data;

  public:
    Logger();
    ~Logger();

    String *results();
    void clear();

    size_t write(uint8_t) = 0;
    size_t write(const uint8_t *buffer, size_t size);
};
