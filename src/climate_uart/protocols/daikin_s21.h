#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class DaikinS21 : public ClimateInterface {
public:
    explicit DaikinS21(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    static uint8_t checksum(const uint8_t *bytes, uint16_t len);
    static HeatpumpMode byteToMode(uint8_t mode);
    static uint8_t modeToByte(HeatpumpMode mode, HeatpumpAction action);
    static HeatpumpFanSpeed byteToFan(uint8_t code);
    static uint8_t fanToByte(HeatpumpFanSpeed fan);

    Result readByte(uint8_t *byte, uint32_t timeoutMs);
    Result waitForAck();
    Result sendFrame(const uint8_t *frame, uint16_t frameLen);
    Result readFrame(uint8_t *buffer, uint16_t *outSize);
    Result query(const uint8_t *frame, uint16_t frameLen, uint8_t *payload, uint16_t *payloadLen);
    Result sendCmd(const uint8_t *frame, uint16_t frameLen);
    Result setSwingSettings(bool swingV, bool swingH);

    transport::UartTransport &uart_;
    bool connected_{false};
};

}  // namespace protocols
}  // namespace climate_uart
