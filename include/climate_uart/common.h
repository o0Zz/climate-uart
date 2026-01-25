#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "climate_uart/platform.h"

#ifndef CLIMATE_UART_LOG_TAG
#define CLIMATE_UART_LOG_TAG "climate-uart"
#endif

namespace climate_uart {
using Result = int;

constexpr Result kSuccess = 0;
constexpr Result kDeviceNotFound = -1;
constexpr Result kDeviceInitFailed = -2;
constexpr Result kFailSetPin = -3;
constexpr Result kReadError = -4;
constexpr Result kWriteError = -5;
constexpr Result kFailAck = -6;
constexpr Result kFailDeviceTooSlow = -7;
constexpr Result kFailWrongMode = -8;
constexpr Result kTimeout = -9;
constexpr Result kInvalidCrc = -10;
constexpr Result kInvalidData = -11;
constexpr Result kNotSupported = -12;
constexpr Result kInvalidParameters = -13;
constexpr Result kInvalidState = -14;
constexpr Result kInvalidNotConnected = -15;
constexpr Result kInvalidReply = -16;

inline uint32_t time_now_ms() {
    const auto *utils = get_platform_utils();
    return utils->time_now_ms ? utils->time_now_ms() : 0U;
}

inline uint32_t time_now_us() {
    const auto *utils = get_platform_utils();
    return utils->time_now_us ? utils->time_now_us() : 0U;
}

inline uint32_t timer_elapsed_ms(uint32_t start_ms) {
    return time_now_ms() - start_ms;
}

inline void log_buffer(const uint8_t *buffer, size_t size) {
    const auto *utils = get_platform_utils();
    if (!utils->log_buffer) {
        return;
    }
    utils->log_buffer(buffer, size);
}

inline void log_write(LogLevel level, const char *format, ...) {
    const auto *utils = get_platform_utils();
    if (!utils->log_v) {
        return;
    }
    va_list args;
    va_start(args, format);
    utils->log_v(level, format, args);
    va_end(args);
}
}  // namespace climate_uart

#define CLIMATE_LOG_DEBUG(...) ::climate_uart::log_write(::climate_uart::LogLevel::kDebug, __VA_ARGS__)
#define CLIMATE_LOG_INFO(...) ::climate_uart::log_write(::climate_uart::LogLevel::kInfo, __VA_ARGS__)
#define CLIMATE_LOG_WARNING(...) ::climate_uart::log_write(::climate_uart::LogLevel::kWarning, __VA_ARGS__)
#define CLIMATE_LOG_ERROR(...) ::climate_uart::log_write(::climate_uart::LogLevel::kError, __VA_ARGS__)
#define CLIMATE_LOG_BUFFER(buffer, size) ::climate_uart::log_buffer(buffer, size)