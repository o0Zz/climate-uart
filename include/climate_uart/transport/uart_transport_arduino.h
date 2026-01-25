#pragma once

#include <stdint.h>
#include <stddef.h>

#include "climate_uart/transport/uart_transport.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace climate_uart {
namespace transport {

class UartTransportArduino : public UartTransport {
public:
    explicit UartTransportArduino(HardwareSerial &serial, int txPin = -1, int rxPin = -1);

    Result open(uint32_t baudrate, UartParity parity, uint8_t stopBits) override;
    Result close() override;
    size_t available() override;
    Result read(uint8_t *buffer, size_t *size) override;
    Result write(const uint8_t *buffer, size_t size) override;

private:
    HardwareSerial &serial_;
    int txPin_;
    int rxPin_;
    bool opened_{false};
};

}  // namespace transport
}  // namespace climate_uart
