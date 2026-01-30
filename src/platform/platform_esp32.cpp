#include "climate_uart/platform.h"

#include "esp_log.h"
#include "esp_timer.h"

namespace climate_uart {

uint32_t time_now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

uint32_t time_elapsed_ms(uint32_t start_ms) {
    return time_now_ms() - start_ms;
}

void log_buffer(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    ESP_LOG_BUFFER_HEX("climate-uart", buffer, size);
}

void log_write(LogLevel level, const char *format, ...) 
{
    esp_log_level_t esp_level = ESP_LOG_INFO;

    switch (level) {
        case LogLevel::kDebug:
            esp_level = ESP_LOG_DEBUG;
            break;
        case LogLevel::kInfo:
            esp_level = ESP_LOG_INFO;
            break;
        case LogLevel::kWarning:
            esp_level = ESP_LOG_WARN;
            break;
        case LogLevel::kError:
            esp_level = ESP_LOG_ERROR;
            break;
        default:
            esp_level = ESP_LOG_INFO;
            break;
    }

    va_list args;
    va_start(args, format);

    esp_log_writev(esp_level, "climate-uart", format, args);

    va_end(args);
}

}  // namespace climate_uart
