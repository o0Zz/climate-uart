#include "climate_uart/platform.h"

#ifdef ESP_PLATFORM

#include "esp_log.h"
#include "esp_timer.h"

namespace climate_uart {

uint32_t time_now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

uint32_t time_elapsed_ms(uint32_t start_ms) {
    return time_now_ms() - start_ms;
}

}  // namespace climate_uart

#endif