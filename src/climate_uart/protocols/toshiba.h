#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class Toshiba : public ClimateInterface {
public:
    explicit Toshiba(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    struct Packet {
        uint8_t stx{0x00};
        uint8_t header[2]{};
        uint8_t type{0x00};
        uint8_t unknown1{0x00};
        uint8_t unknown2{0x00};
        uint8_t size{0};
        uint8_t data[0xFF]{};
        uint8_t checksum{0x00};
    };

    static uint8_t crc(const uint8_t *data, uint8_t dataLen);
    static uint8_t modeToByte(HeatpumpMode mode);
    static HeatpumpMode byteToMode(uint8_t val);
    static uint8_t fanToByte(HeatpumpFanSpeed fanSpeed);
    static HeatpumpFanSpeed byteToFan(uint8_t val);
    static uint8_t vaneToByte(HeatpumpVaneMode vaneMode);
    static HeatpumpVaneMode byteToVane(uint8_t val);

    Result readByte(uint8_t *byte, uint32_t timeoutMs);
    Result readPacket(Packet &packet);
    Result sendCommand(uint8_t *data, uint16_t dataSize);
    Result query(uint8_t function, Packet &result);
    void flushRx();
    Result connect();
    Result command(uint8_t function, uint8_t value);

    transport::UartTransport &uart_;
    bool connected_{false};
};

}  // namespace protocols
}  // namespace climate_uart
