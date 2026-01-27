
#include "climate_uart/common.h"
#include "climate_uart/platform.h"

#ifdef ARDUINO

#include <Arduino.h>

namespace climate_uart {
namespace {

uint32_t arduino_time_now_ms() {
    return static_cast<uint32_t>(millis());
}

uint32_t arduino_time_now_us() {
    return static_cast<uint32_t>(micros());
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

void arduino_log_v(LogLevel level, const char *format, va_list args) {
    if (!Serial) {
        return;
    }

    char buffer[256];
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    if (written <= 0) {
        return;
    }

    Serial.print("[climate-uart][");
    Serial.print(level_prefix(level));
    Serial.print("] ");
    Serial.println(buffer);
}

void arduino_log_buffer(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0 || !Serial) {
        return;
    }

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
}

const PlatformUtils kArduinoUtils{
    &arduino_time_now_ms,
    &arduino_time_now_us,
    &arduino_log_v,
    &arduino_log_buffer,
};

struct ArduinoPlatformAutoInit {
    ArduinoPlatformAutoInit() {
        set_platform_utils(&kArduinoUtils);
    }
};

ArduinoPlatformAutoInit g_auto_init;

}  // namespace
}  // namespace climate_uart

#endif  // ARDUINO