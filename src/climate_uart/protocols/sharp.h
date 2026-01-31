#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class Sharp : public ClimateInterface {
public:
    explicit Sharp(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    struct Frame {
        uint8_t data[18];
        uint8_t size{0};
    };

    Result readByte(uint8_t *byte, uint16_t timeoutMs);
    Result readFrame(Frame &frame);
    Result sendAck();
    Result sendCommand(const ClimateSettings &settings);
    void flushRx();
    Result connect();

    static uint8_t crc(const uint8_t *buffer, size_t size);
    static uint8_t cmdCrc(const uint8_t *buffer);
    static uint8_t modeToByte(HeatpumpMode mode);
    static HeatpumpMode byteToMode(uint8_t val);
    static uint8_t fanToByte(HeatpumpFanSpeed fanSpeed);
    static HeatpumpFanSpeed byteToFan(uint8_t val);
    static uint8_t vaneToByte(HeatpumpVaneMode vaneMode);
    static HeatpumpVaneMode byteToVane(uint8_t val);

    transport::UartTransport &uart_;
    bool connected_{false};
};

}  // namespace protocols
}  // namespace climate_uart
