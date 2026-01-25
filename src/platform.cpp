#include "climate_uart/platform.h"

#include <chrono>
#include <cstdio>

namespace climate_uart {
namespace {

uint32_t default_time_now_us() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

uint32_t default_time_now_ms() {
    return default_time_now_us() / 1000U;
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

void default_log_v(LogLevel level, const char *format, va_list args) {
    std::fprintf(stderr, "[climate-uart][%s] ", level_prefix(level));
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
}

void default_log_buffer(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }

    std::fprintf(stderr, "[climate-uart][B] ");
    for (size_t i = 0; i < size; ++i) {
        std::fprintf(stderr, "%02X", buffer[i]);
        if (i + 1 < size) {
            std::fprintf(stderr, " ");
        }
    }
    std::fprintf(stderr, "\n");
}

const PlatformUtils kDefaultUtils{
    &default_time_now_ms,
    &default_time_now_us,
    &default_log_v,
    &default_log_buffer,
};

const PlatformUtils *g_platform_utils = &kDefaultUtils;

}  // namespace

void set_platform_utils(const PlatformUtils *utils) {
    g_platform_utils = utils ? utils : &kDefaultUtils;
}

const PlatformUtils *get_platform_utils() {
    return g_platform_utils;
}

}  // namespace climate_uart
