#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>

namespace climate_uart {

enum class LogLevel {
    kDebug,
    kInfo,
    kWarning,
    kError,
};

struct PlatformUtils {
    uint32_t (*time_now_ms)();
    uint32_t (*time_now_us)();
    void (*log_v)(LogLevel level, const char *format, va_list args);
    void (*log_buffer)(const uint8_t *buffer, size_t size);
};

void set_platform_utils(const PlatformUtils *utils);
const PlatformUtils *get_platform_utils();

}  // namespace climate_uart
