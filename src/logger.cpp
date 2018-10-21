#include "logger.h"

Logger::Logger()
{
    data = new String();
}

Logger::~Logger()
{
    delete data;
    data = NULL;
}

String *Logger::results()
{
    return data;
}

void Logger::clear()
{
    delete data;
    data = new String();
}

size_t Logger::write(const uint8_t *buffer, size_t size)
{
    size_t n = 0;

    while (size--)
    {
        if (data->concat(*buffer++))
            n++;
        else
            break;
    }

    return n;
}