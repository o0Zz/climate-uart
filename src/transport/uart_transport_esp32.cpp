#include "climate_uart/transport/uart_transport_esp32.h"

#include "climate_uart/result.h"

namespace climate_uart {
namespace transport {

UartTransportESP32::UartTransportESP32(uart_port_t port,
                                       int txPin,
                                       int rxPin,
                                       int rtsPin,
                                       int ctsPin,
                                       int rxBufferSize,
                                       int txBufferSize)
    : port_(port),
      txPin_(txPin),
      rxPin_(rxPin),
      rtsPin_(rtsPin),
      ctsPin_(ctsPin),
      rxBufferSize_(rxBufferSize),
      txBufferSize_(txBufferSize) {}

Result UartTransportESP32::open(uint32_t baudrate, UartParity parity, uint8_t stopBits) {
    if (installed_) {
        close();
    }

    uart_config_t config{};
    config.baud_rate = static_cast<int>(baudrate);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    if (parity == UartParity::Even) {
        config.parity = UART_PARITY_EVEN;
    } else if (parity == UartParity::Odd) {
        config.parity = UART_PARITY_ODD;
    }
    config.stop_bits = (stopBits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    if (uart_param_config(port_, &config) != ESP_OK) {
        return kDeviceInitFailed;
    }

    if (uart_set_pin(port_, txPin_, rxPin_, rtsPin_, ctsPin_) != ESP_OK) {
        return kFailSetPin;
    }

    if (uart_driver_install(port_, rxBufferSize_, txBufferSize_, 0, nullptr, 0) != ESP_OK) {
        return kDeviceInitFailed;
    }

    installed_ = true;
    return kSuccess;
}

Result UartTransportESP32::close() {
    if (!installed_) {
        return kSuccess;
    }

    if (uart_driver_delete(port_) != ESP_OK) {
        return kDeviceInitFailed;
    }

    installed_ = false;
    return kSuccess;
}

size_t UartTransportESP32::available() {
    size_t size = 0;
    if (uart_get_buffered_data_len(port_, &size) != ESP_OK) {
        return 0;
    }
    return size;
}

Result UartTransportESP32::read(uint8_t *buffer, size_t *size) {
    if (!buffer || !size || *size == 0) {
        return kInvalidParameters;
    }

    int readBytes = uart_read_bytes(port_, buffer, *size, 0);
    if (readBytes < 0) {
        *size = 0;
        return kReadError;
    }

    *size = static_cast<size_t>(readBytes);
    return kSuccess;
}

Result UartTransportESP32::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return kInvalidParameters;
    }

    int written = uart_write_bytes(port_, reinterpret_cast<const char *>(buffer), size);
    if (written < 0 || static_cast<size_t>(written) != size) {
        return kWriteError;
    }

    return kSuccess;
}

}  // namespace transport
}  // namespace climate_uart
