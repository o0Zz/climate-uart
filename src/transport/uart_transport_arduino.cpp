#ifdef ARDUINO
#include "climate_uart/transport/uart_transport_arduino.h"

#include "climate_uart/result.h"

#include <stdio.h>
#include <stdlib.h>

namespace climate_uart {
namespace transport {

UartTransportArduino::UartTransportArduino(HardwareSerial &serial, int txPin, int rxPin)
    : serial_(serial), txPin_(txPin), rxPin_(rxPin) {}

static uint32_t toArduinoConfig(UartParity parity, uint8_t stopBits) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    const bool twoStop = (stopBits == 2);
    switch (parity) {
        case UartParity::Even:
            return twoStop ? SERIAL_8E2 : SERIAL_8E1;
        case UartParity::Odd:
            return twoStop ? SERIAL_8O2 : SERIAL_8O1;
        case UartParity::None:
        default:
            return twoStop ? SERIAL_8N2 : SERIAL_8N1;
    }
#else
    (void)parity;
    (void)stopBits;
    return SERIAL_8N1;
#endif
}

Result UartTransportArduino::open(uint32_t baudrate, UartParity parity, uint8_t stopBits) {
#ifdef ARDUINO
    if (opened_) {
        close();
    }

    const uint32_t config = toArduinoConfig(parity, stopBits);

#if defined(ARDUINO_ARCH_ESP32)
    if (txPin_ >= 0 && rxPin_ >= 0) {
        serial_.begin(baudrate, config, rxPin_, txPin_);
    } else {
        serial_.begin(baudrate, config);
    }
#else
    serial_.begin(baudrate);
#endif

    opened_ = true;
    return kSuccess;
#else
    (void)baudrate;
    (void)parity;
    (void)stopBits;
    return kNotSupported;
#endif
}

Result UartTransportArduino::close() {
#ifdef ARDUINO
    if (!opened_) {
        return kSuccess;
    }

    serial_.end();
    opened_ = false;
    return kSuccess;
#else
    return kNotSupported;
#endif
}

size_t UartTransportArduino::available() {
#ifdef ARDUINO
    return static_cast<size_t>(serial_.available());
#else
    return 0;
#endif
}

Result UartTransportArduino::read(uint8_t *buffer, size_t *size) {
    if (!buffer || !size || *size == 0) {
        return kInvalidParameters;
    }

#ifdef ARDUINO
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
#else
    *size = 0;
    return kNotSupported;
#endif
}

Result UartTransportArduino::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) {
        return kInvalidParameters;
    }

#ifdef ARDUINO
    size_t written = serial_.write(buffer, size);
    if (written != size) {
        return kWriteError;
    }
    return kSuccess;
#else
    return kNotSupported;
#endif
}

}  // namespace transport
}  // namespace climate_uart
#endif  // ARDUINO