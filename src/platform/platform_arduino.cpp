
#include "climate_uart/platform.h"

#ifdef ARDUINO

// Do not enable ARDUINO log when you are using Hardware Serial as it interfer with climate UART's UART communication.
// You can enable it if you are using SoftwareSerial or other non-Hardware Serial transport.
#define ARDUINO_LOG_ENABLED 0

#include <Arduino.h>

namespace climate_uart {

uint32_t time_now_ms() {
    return static_cast<uint32_t>(millis());
}

uint32_t time_elapsed_ms(uint32_t start_ms) {
    return time_now_ms() - start_ms;
}

const char *level_prefix(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug:
            return "D";
        case LogLevel::kInfo:
            return "I";
        case LogLevel::kWarning:
            return "W";
        case LogLevel::kError:
            return "E";
        default:
            return "?";
    }
}

void log_buffer(const uint8_t *buffer, size_t size) 
{
    if (!buffer || size == 0 || !Serial) {
        return;
    }

#if ARDUINO_LOG_ENABLED
    Serial.print("[climate-uart][B] ");
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(buffer[i], HEX);
        if (i + 1 < size) {
            Serial.print(' ');
        }
    }
    Serial.println();
#endif
}

void log_write(LogLevel level, const char *format, ...) 
{
#if ARDUINO_LOG_ENABLED
    va_list args;
    va_start(args, format);

    char buffer[128];
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    if (written <= 0) {
        va_end(args);
        return;
    }

    Serial.print("[climate-uart][");
    Serial.print(level_prefix(level));
    Serial.print("] ");
    Serial.println(buffer);

    va_end(args);
#endif
}

}  // namespace climate_uart

#endif  // ARDUINO