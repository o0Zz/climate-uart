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

uint32_t time_now_ms();
uint32_t time_elapsed_ms(uint32_t start_ms);

void log_buffer(const uint8_t *buffer, size_t size);
void log_write(LogLevel level, const char *format, ...);

}  // namespace climate_uart

#define CLIMATE_LOG_DEBUG(...) ::climate_uart::log_write(::climate_uart::LogLevel::kDebug, __VA_ARGS__)
#define CLIMATE_LOG_INFO(...) ::climate_uart::log_write(::climate_uart::LogLevel::kInfo, __VA_ARGS__)
#define CLIMATE_LOG_WARNING(...) ::climate_uart::log_write(::climate_uart::LogLevel::kWarning, __VA_ARGS__)
#define CLIMATE_LOG_ERROR(...) ::climate_uart::log_write(::climate_uart::LogLevel::kError, __VA_ARGS__)
#define CLIMATE_LOG_BUFFER(buffer, size) ::climate_uart::log_buffer(buffer, size)