#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class LgAircon : public ClimateInterface {
public:
    explicit LgAircon(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    static uint8_t crc(const uint8_t *buffer, size_t size);

    Result readByte(uint8_t *byte, uint32_t timeoutMs);
    Result readMsg(uint8_t *buffer, size_t bufferSize);
    Result writeMsg(uint8_t *buffer, size_t bufferSize);
    Result readStatus(uint8_t *buffer, size_t bufferSize);
    Result connect();

    transport::UartTransport &uart_;
    bool connected_{false};
    float roomTemperature_{20.0f};
    uint8_t lastRecvStatus_[13]{};
};

}  // namespace protocols
}  // namespace climate_uart
