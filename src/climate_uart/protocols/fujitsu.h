#pragma once

#include "climate_uart/climate_interface.h"
#include "climate_uart/transport/uart_transport.h"

namespace climate_uart {
namespace protocols {

class Fujitsu : public ClimateInterface {
public:
    explicit Fujitsu(transport::UartTransport &uart, bool secondary = false);

    Result init() override;
    Result setState(const ClimateSettings &settings) override;
    Result getState(ClimateSettings &settings) override;
    Result getRoomTemperature(float &temperature) override;

private:
    enum class Address : uint8_t {
        Start     = 0,
        Unit      = 1,
        Primary   = 32,
        Secondary = 33,
    };

    enum class MessageType : uint8_t {
        Status  = 0,
        Error   = 1,
        Login   = 2,
        Unknown = 3,
    };

    struct Frame {
        uint8_t source{0};
        uint8_t dest{0};
        uint8_t type{0};

        uint8_t onOff{0};
        uint8_t mode{0};
        uint8_t fanMode{0};
        uint8_t temperature{16};
        uint8_t economyMode{0};
        uint8_t swingMode{0};
        uint8_t swingStep{0};
        uint8_t controllerPresent{0};
        uint8_t updateMagic{0};
        uint8_t controllerTemp{16};
        uint8_t acError{0};

        bool writeBit{false};
        bool loginBit{false};
        bool unknownBit{false};
    };

    static uint8_t modeToByte(HeatpumpMode mode);
    static HeatpumpMode byteToMode(uint8_t val);
    static uint8_t fanToByte(HeatpumpFanSpeed fanSpeed);
    static HeatpumpFanSpeed byteToFan(uint8_t val);

    Frame decodeFrame(const uint8_t *buf);
    void encodeFrame(const Frame &frame, uint8_t *buf);

    Result readFrame(Frame &frame);
    Result writeFrame(const Frame &frame);

    Result processStatusFrame(Frame &rx, Frame &tx);
    Result processLoginFrame(Frame &rx, Frame &tx);
    Result poll();

    transport::UartTransport &uart_;
    bool secondary_{false};
    uint8_t controllerAddress_{0};
    bool loggedIn_{false};
    bool seenSecondary_{false};
    uint32_t lastFrameMs_{0};

    ClimateSettings pendingUpdate_{};
    bool hasPendingUpdate_{false};

    Frame currentState_{};
    float roomTemperature_{0.0f};
};

}  // namespace protocols
}  // namespace climate_uart
