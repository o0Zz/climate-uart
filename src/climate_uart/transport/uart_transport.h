#pragma once

#include "climate_uart/result.h"

namespace climate_uart {
namespace transport {

enum class UartParity : uint8_t {
    None = 0,
    Even,
    Odd
};

class UartTransport {
public:
    virtual ~UartTransport() = default;

    virtual Result open(uint32_t baudrate, UartParity parity, uint8_t stopBits) = 0;
    virtual Result close() = 0;
    virtual size_t available() = 0;
    virtual Result read(uint8_t *buffer, size_t *size) = 0;
    virtual Result write(const uint8_t *buffer, size_t size) = 0;
};

}  // namespace transport
}  // namespace climate_uart
