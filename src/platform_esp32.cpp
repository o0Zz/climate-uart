#include "climate_uart/common.h"
#include "climate_uart/platform.h"

#include "esp_log.h"
#include "esp_timer.h"

namespace climate_uart {
namespace {

uint32_t esp_time_now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

uint32_t esp_time_now_us() {
    return static_cast<uint32_t>(esp_timer_get_time());
}

void esp_log_v(LogLevel level, const char *format, va_list args) {
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

    esp_log_writev(esp_level, CLIMATE_UART_LOG_TAG, format, args);
}

void esp_log_buffer(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    ESP_LOG_BUFFER_HEX(CLIMATE_UART_LOG_TAG, buffer, size);
}

const PlatformUtils kEsp32Utils{
    &esp_time_now_ms,
    &esp_time_now_us,
    &esp_log_v,
    &esp_log_buffer,
};

struct Esp32PlatformAutoInit {
    Esp32PlatformAutoInit() {
        set_platform_utils(&kEsp32Utils);
    }
};

Esp32PlatformAutoInit g_auto_init;

}  // namespace
}  // namespace climate_uart
