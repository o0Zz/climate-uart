#pragma once

#include <stdint.h>
#include <stddef.h>
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

#ifdef ESP_PLATFORM
    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
    #include "esp_log.h"
    #define CLIMATE_LOG_DEBUG(...)			    ESP_LOGD("climate-uart", __VA_ARGS__)
    #define CLIMATE_LOG_INFO(...)				ESP_LOGI("climate-uart", __VA_ARGS__)
    #define CLIMATE_LOG_WARNING(...)			ESP_LOGW("climate-uart", __VA_ARGS__)
    #define CLIMATE_LOG_ERROR(...)		        ESP_LOGE("climate-uart", __VA_ARGS__)
    #define CLIMATE_LOG_BUFFER(buf, size)	    ESP_LOG_BUFFER_HEXDUMP("climate-uart", buf, size, ESP_LOG_DEBUG)
#else
    #define CLIMATE_LOG_DEBUG(...) ::climate_uart::log_write(::climate_uart::LogLevel::kDebug, __VA_ARGS__)
    #define CLIMATE_LOG_INFO(...) ::climate_uart::log_write(::climate_uart::LogLevel::kInfo, __VA_ARGS__)
    #define CLIMATE_LOG_WARNING(...) ::climate_uart::log_write(::climate_uart::LogLevel::kWarning, __VA_ARGS__)
    #define CLIMATE_LOG_ERROR(...) ::climate_uart::log_write(::climate_uart::LogLevel::kError, __VA_ARGS__)
    #define CLIMATE_LOG_BUFFER(buffer, size) ::climate_uart::log_buffer(buffer, size)
#endif