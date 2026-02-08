#ifdef ARDUINO
#include "climate_uart/transport/uart_transport_arduino.h"

#include "climate_uart/result.h"

#include <stdio.h>
#include <stdlib.h>

namespace climate_uart {
namespace transport {

static uint32_t toArduinoConfig(UartParity parity, uint8_t stopBits) 
{
    if (parity == UartParity::Even)
        return (stopBits == 2) ? SERIAL_8E2 : SERIAL_8E1;
    if (parity == UartParity::Odd)
        return (stopBits == 2) ? SERIAL_8O2 : SERIAL_8O1;
        
    return (stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;
}

UartTransportArduino::UartTransportArduino(HardwareSerial &serial, int txPin, int rxPin)
    : serial_(serial), txPin_(txPin), rxPin_(rxPin) {}


Result UartTransportArduino::open(uint32_t baudrate, UartParity parity, uint8_t stopBits) {
    if (opened_) {
        close();
    }

    const uint32_t config = toArduinoConfig(parity, stopBits);

#if defined(ARDUINO_ARCH_ESP32) 
    if (txPin_ >= 0 && rxPin_ >= 0)
    {
        serial_.begin(baudrate, config, rxPin_, txPin_);
        opened_ = true;
        return kSuccess;
    }
#endif

    serial_.begin(baudrate, config);
    opened_ = true;
    return kSuccess;
}

Result UartTransportArduino::close() {
    if (!opened_) {
        return kSuccess;
    }

    serial_.end();
    opened_ = false;
    return kSuccess;
}

size_t UartTransportArduino::available() {
    return static_cast<size_t>(serial_.available());
}

Result UartTransportArduino::read(uint8_t *buffer, size_t *size) {
    if (!buffer || !size || *size == 0) {
        return kInvalidParameters;
    }

    size_t toRead = *size;
    size_t readCount = 0;
    while (readCount < toRead && serial_.available() > 0) {
        int val = serial_.read();
        if (val < 0) {
            break;
        }
        buffer[readCount++] = static_cast<uint8_t>(val);
    }

    *size = readCount;
    return kSuccess;
}

Result UartTransportArduino::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return kInvalidParameters;
    }

    size_t written = serial_.write(buffer, size);
    if (written != size) {
        return kWriteError;
    }
    return kSuccess;
}

}  // namespace transport
}  // namespace climate_uart
#endif  // ARDUINO