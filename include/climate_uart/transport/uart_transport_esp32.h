#pragma once

#include "climate_uart/transport/uart_transport.h"

#ifdef ESP_PLATFORM

#include "driver/uart.h"

namespace climate_uart {
namespace transport {

class UartTransportESP32 : public UartTransport {
public:
    UartTransportESP32(uart_port_t port,
                       int txPin,
                       int rxPin,
                       int rtsPin = UART_PIN_NO_CHANGE,
                       int ctsPin = UART_PIN_NO_CHANGE,
                       int rxBufferSize = 256,
                       int txBufferSize = 256);

    Result open(uint32_t baudrate, UartParity parity, uint8_t stopBits) override;
    Result close() override;
    size_t available() override;
    Result read(uint8_t *buffer, size_t *size) override;
    Result write(const uint8_t *buffer, size_t size) override;

private:
    uart_port_t port_;
    int txPin_;
    int rxPin_;
    int rtsPin_;
    int ctsPin_;
    int rxBufferSize_;
    int txBufferSize_;
    bool installed_{false};
};

}  // namespace transport
}  // namespace climate_uart

#endif