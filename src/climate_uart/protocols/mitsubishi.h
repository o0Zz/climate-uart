#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class Mitsubishi : public ClimateInterface {
public:
    explicit Mitsubishi(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    enum class PacketType : uint8_t {
        Unknown = 0x00,
        SetSettingsInformation,
        GetSettingsInformation,
        GetRoomTemperature,
        SetTimer,
        GetTimer,
        GetStatus
    };

    struct Packet {
        uint8_t stx{0x00};
        uint8_t cmd{0x00};
        uint8_t header[2]{};
        uint8_t size{0};
        uint8_t data[16]{};
        uint8_t checksum{0};
    };

    static uint8_t crc(const uint8_t *buffer, uint8_t size);

    Result readByte(uint8_t *byte, uint32_t timeoutMs);
    Result readPacket(Packet &packet);
    Result writePacket(const Packet &packet);
    Result connect();

    transport::UartTransport &uart_;
    bool connected_{false};
};

}  // namespace protocols
}  // namespace climate_uart
