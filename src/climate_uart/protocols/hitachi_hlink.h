#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class HitachiHLink : public ClimateInterface {
public:
    explicit HitachiHLink(transport::UartTransport &uart);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    enum class ResponseStatus : uint8_t {
        Ok = 0,
        Ng,
        Invalid
    };

    struct Response {
        ResponseStatus status{ResponseStatus::Invalid};
        uint8_t data[32]{};
        uint8_t dataLen{0};
    };

    static uint16_t checksum(uint16_t address, const uint8_t *data, uint8_t dataLen);
    static uint16_t modeToWord(HeatpumpMode mode);
    static HeatpumpMode wordToMode(uint16_t val);
    static uint8_t fanToByte(HeatpumpFanSpeed fanSpeed);
    static HeatpumpFanSpeed byteToFan(uint8_t val);
    static uint8_t vaneToByte(HeatpumpVaneMode vaneMode);
    static HeatpumpVaneMode byteToVane(uint8_t val);

    Result readByte(uint8_t *byte, uint32_t timeoutMs);
    Result readLine(char *buffer, uint16_t bufferSize, uint32_t timeoutMs);
    Result parseResponse(const char *line, Response &response);
    Result sendFrame(const char *type, uint16_t address, const uint8_t *data, uint8_t dataLen);
    Result query(uint16_t address, Response &response);
    Result command(uint16_t address, const uint8_t *data, uint8_t dataLen);

    transport::UartTransport &uart_;
    bool connected_{false};
};

}  // namespace protocols
}  // namespace climate_uart
