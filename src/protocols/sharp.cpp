#include "climate_uart/protocols/sharp.h"

#include "climate_uart/result.h"

#include <stdlib.h>

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint16_t kPacketReadTimeoutMs = 500;
constexpr uint8_t kCommandFrameSize = 14;
constexpr uint8_t kModeFrameSize = 14;
constexpr uint8_t kStatusFrameSize = 18;
constexpr uint8_t kAckByte = 0x06;

constexpr uint8_t kFrameStartTx = 0xDD;
constexpr uint8_t kFrameStartRx = 0xDC;
constexpr uint8_t kFrameTypeCommand = 0xFB;
constexpr uint8_t kFrameTypeResponse = 0xFC;

constexpr uint8_t kPowerHeat = 0x01;
constexpr uint8_t kPowerCool = 0x02;
constexpr uint8_t kPowerDry = 0x03;
constexpr uint8_t kPowerFan = 0x04;

constexpr uint8_t kFanAuto = 0x02;
constexpr uint8_t kFanMid = 0x03;
constexpr uint8_t kFanLow = 0x04;
constexpr uint8_t kFanHigh = 0x05;
constexpr uint8_t kFanHighest = 0x07;

constexpr uint8_t kSwingVSwng = 0x0F;
constexpr uint8_t kSwingVAuto = 0x08;
constexpr uint8_t kSwingVHighest = 0x09;
constexpr uint8_t kSwingVHigh = 0x0A;
constexpr uint8_t kSwingVMid = 0x0B;
constexpr uint8_t kSwingVLow = 0x0C;
constexpr uint8_t kSwingVLowest = 0x0D;

constexpr uint8_t kSwingHSwng = 0x0F;
constexpr uint8_t kSwingHMiddle = 0x01;

constexpr uint8_t kMsgInit1[7] = {0x02, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kMsgInit2[8] = {0x02, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0x01, 0x00};
constexpr uint8_t kMsgSubscribe1[7] = {0x03, 0xFF, 0xA0, 0x01, 0x00, 0x00, 0x00};
constexpr uint8_t kMsgSubscribe2[7] = {0x03, 0xFE, 0xA0, 0x01, 0x00, 0x00, 0x00};
constexpr uint8_t kMsgGetState[4] = {0xDD, 0x02, 0xFC, 0x62};
constexpr uint8_t kMsgGetStatus[4] = {0xDD, 0x02, 0xFD, 0x62};
constexpr uint8_t kMsgConnected[7] = {0x03, 0x05, 0xB0, 0x00, 0x10, 0x00, 0x00};
}  // namespace

Sharp::Sharp(transport::UartTransport &uart) : uart_(uart) {}

uint8_t Sharp::crc(const uint8_t *buffer, size_t size) {
    uint32_t crcValue = 0;
    for (size_t i = 1; i < size; i++) {
        crcValue += buffer[i];
    }
    return static_cast<uint8_t>(0 - static_cast<uint8_t>(crcValue));
}

uint8_t Sharp::cmdCrc(const uint8_t *buffer) {
    uint8_t checksum = 0x03;
    for (int i = 4; i < 12; i++) {
        checksum ^= buffer[i] & 0x0F;
        checksum ^= (buffer[i] >> 4) & 0x0F;
    }
    checksum = 0x0F - (checksum & 0x0F);
    return static_cast<uint8_t>((checksum << 4) | 0x01);
}

uint8_t Sharp::modeToByte(HeatpumpMode mode) {
    switch (mode) {
        case HeatpumpMode::Heat:
            return kPowerHeat;
        case HeatpumpMode::Cold:
            return kPowerCool;
        case HeatpumpMode::Dry:
            return kPowerDry;
        case HeatpumpMode::Fan:
            return kPowerFan;
        case HeatpumpMode::Auto:
            return kPowerCool;
        default:
            return kPowerCool;
    }
}

HeatpumpMode Sharp::byteToMode(uint8_t val) {
    switch (val) {
        case kPowerHeat:
            return HeatpumpMode::Heat;
        case kPowerCool:
            return HeatpumpMode::Cold;
        case kPowerDry:
            return HeatpumpMode::Dry;
        case kPowerFan:
            return HeatpumpMode::Fan;
        default:
            return HeatpumpMode::Cold;
    }
}

uint8_t Sharp::fanToByte(HeatpumpFanSpeed fanSpeed) {
    switch (fanSpeed) {
        case HeatpumpFanSpeed::Auto:
            return kFanAuto;
        case HeatpumpFanSpeed::Low:
            return kFanLow;
        case HeatpumpFanSpeed::Med:
            return kFanMid;
        case HeatpumpFanSpeed::High:
            return kFanHigh;
        default:
            return kFanAuto;
    }
}

HeatpumpFanSpeed Sharp::byteToFan(uint8_t val) {
    switch (val) {
        case kFanAuto:
            return HeatpumpFanSpeed::Auto;
        case kFanLow:
            return HeatpumpFanSpeed::Low;
        case kFanMid:
            return HeatpumpFanSpeed::Med;
        case kFanHigh:
        case kFanHighest:
            return HeatpumpFanSpeed::High;
        default:
            return HeatpumpFanSpeed::Auto;
    }
}

uint8_t Sharp::vaneToByte(HeatpumpVaneMode vaneMode) {
    switch (vaneMode) {
        case HeatpumpVaneMode::Auto:
            return kSwingVAuto;
        case HeatpumpVaneMode::Swing:
            return kSwingVSwng;
        case HeatpumpVaneMode::V1:
            return kSwingVHighest;
        case HeatpumpVaneMode::V2:
            return kSwingVHigh;
        case HeatpumpVaneMode::V3:
            return kSwingVMid;
        case HeatpumpVaneMode::V4:
            return kSwingVLow;
        case HeatpumpVaneMode::V5:
            return kSwingVLowest;
        default:
            return kSwingVAuto;
    }
}

HeatpumpVaneMode Sharp::byteToVane(uint8_t val) {
    switch (val) {
        case kSwingVAuto:
            return HeatpumpVaneMode::Auto;
        case kSwingVSwng:
            return HeatpumpVaneMode::Swing;
        case kSwingVHighest:
            return HeatpumpVaneMode::V1;
        case kSwingVHigh:
            return HeatpumpVaneMode::V2;
        case kSwingVMid:
            return HeatpumpVaneMode::V3;
        case kSwingVLow:
            return HeatpumpVaneMode::V4;
        case kSwingVLowest:
            return HeatpumpVaneMode::V5;
        default:
            return HeatpumpVaneMode::Auto;
    }
}

Result Sharp::readByte(uint8_t *byte, uint16_t timeoutMs) {
    uint32_t start = time_now_ms();

    while (time_elapsed_ms(start) < timeoutMs) {
        size_t size = 1;
        Result ret = uart_.read(byte, &size);
        if (ret == kSuccess && size == 1) {
            return kSuccess;
        }
    }

    CLIMATE_LOG_DEBUG("Sharp ReadByte Timeout (%u ms)", timeoutMs);
    return kTimeout;
}

Result Sharp::readFrame(Frame &frame) {
    frame.size = 0;
    frame.data[0] = 0x00;

    while (frame.data[0] != kFrameStartRx) {
        if (readByte(&frame.data[0], kPacketReadTimeoutMs) == kTimeout) {
            return kTimeout;
        }

        if (frame.data[0] != kFrameStartRx) {
            CLIMATE_LOG_WARNING("Sharp: Discarded byte: 0x%02X", frame.data[0]);
        }
    }

    if (readByte(&frame.data[1], kPacketReadTimeoutMs) != kSuccess) {
        return kTimeout;
    }

    frame.size = static_cast<uint8_t>(frame.data[1] + 3);

    for (uint8_t i = 2; i < frame.size; i++) {
        if (readByte(&frame.data[i], kPacketReadTimeoutMs) != kSuccess) {
            return kTimeout;
        }
    }

    uint8_t calculated = crc(frame.data, frame.size - 1);
    if (frame.data[frame.size - 1] != calculated) {
        CLIMATE_LOG_ERROR("Sharp: Invalid CRC");
        CLIMATE_LOG_BUFFER(frame.data, frame.size);
        return kInvalidCrc;
    }

    CLIMATE_LOG_DEBUG("Received frame: size=%u, type=0x%02X", frame.size, frame.data[2]);
    CLIMATE_LOG_BUFFER(frame.data, frame.size);

    return kSuccess;
}

Result Sharp::sendAck() {
    uint8_t ack = kAckByte;
    return uart_.write(&ack, 1);
}

Result Sharp::sendCommand(const ClimateSettings &settings) {
    uint8_t buffer[kCommandFrameSize];
    int pos = 0;

    buffer[pos++] = kFrameStartTx;
    buffer[pos++] = 0x0B;
    buffer[pos++] = kFrameTypeCommand;
    buffer[pos++] = 0x60;

    if (settings.mode == HeatpumpMode::Fan) {
        buffer[pos++] = 0x01;
    } else if (settings.mode == HeatpumpMode::Dry) {
        buffer[pos++] = 0x00;
    } else {
        buffer[pos++] = static_cast<uint8_t>(0xC0 | (settings.temperature - 16));
    }

    buffer[pos++] = (settings.action == HeatpumpAction::On) ? 0x31 : 0x21;

    uint8_t mode = modeToByte(settings.mode);
    uint8_t fan = fanToByte(settings.fanSpeed);
    if (mode == kPowerFan && fan == kFanAuto) {
        fan = kFanLow;
    }
    buffer[pos++] = static_cast<uint8_t>(mode | (fan << 4));

    buffer[pos++] = 0x00;

    uint8_t swingV = vaneToByte(settings.vaneMode);
    uint8_t swingH = kSwingHMiddle;
    buffer[pos++] = static_cast<uint8_t>((swingH << 4) | swingV);

    buffer[pos++] = (settings.action == HeatpumpAction::On) ? 0x80 : 0x00;
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x10;

    buffer[pos++] = cmdCrc(buffer);
    buffer[pos++] = crc(buffer, kCommandFrameSize);

    CLIMATE_LOG_DEBUG("Sending command:");
    CLIMATE_LOG_BUFFER(buffer, kCommandFrameSize);

    return uart_.write(buffer, kCommandFrameSize);
}

void Sharp::flushRx() {
    Frame frame;
    while (readFrame(frame) == kSuccess) {
    }
}

Result Sharp::connect() {
    connected_ = false;

    const struct SyncPacket {
        const uint8_t *data;
        size_t size;
    } packets[] = {
        {kMsgInit1, sizeof(kMsgInit1)},
        {kMsgInit2, sizeof(kMsgInit2)},
        {kMsgSubscribe1, sizeof(kMsgSubscribe1)},
        {kMsgSubscribe2, sizeof(kMsgSubscribe2)},
        {kMsgGetState, sizeof(kMsgGetState)},
        {kMsgGetStatus, sizeof(kMsgGetStatus)},
        {kMsgConnected, sizeof(kMsgConnected)},
    };

    CLIMATE_LOG_DEBUG("Sharp: Starting handshake sequence ...");

    for (size_t i = 0; i < sizeof(packets) / sizeof(packets[0]); i++) {
        CLIMATE_LOG_DEBUG("Sending handshake SYN%u packet ...", static_cast<unsigned>(i + 1));
        CLIMATE_LOG_BUFFER(packets[i].data, packets[i].size);

        Result ret = uart_.write(packets[i].data, packets[i].size);
        if (ret != kSuccess) {
            CLIMATE_LOG_ERROR("Sharp: Handshake SYN%u write failed: %d", static_cast<unsigned>(i + 1), ret);
            return ret;
        }

        flushRx();
    }

    CLIMATE_LOG_INFO("Sharp: Heatpump connected successfully!");
    connected_ = true;
    return kSuccess;
}

Result Sharp::init() {
    Result ret = uart_.open(9600, transport::UartParity::Even, 1);
    if (ret != kSuccess) {
        CLIMATE_LOG_DEBUG("Failed to open UART: %d", ret);
        return ret;
    }

    ret = connect();
    if (ret != kSuccess) {
        CLIMATE_LOG_DEBUG("Initial connection failed: %d", ret);
    }

    return kSuccess;
}

Result Sharp::setState(const ClimateSettings &settings) {
    Frame response;

    if (!connected_) {
        Result ret = connect();
        if (ret != kSuccess) {
            return kInvalidNotConnected;
        }
    }

    Result ret = sendCommand(settings);
    if (ret != kSuccess) {
        return ret;
    }

    ret = readFrame(response);
    if (ret == kSuccess) {
        if (response.size > 1) {
            sendAck();
        }
        CLIMATE_LOG_DEBUG("Settings sent successfully");
        return kSuccess;
    }

    flushRx();
    return ret;
}

Result Sharp::getState(ClimateSettings &settings) {
    Frame frame;

    if (!connected_) {
        Result ret = connect();
        if (ret != kSuccess) {
            return kInvalidNotConnected;
        }
    }

    settings = ClimateSettings{};

    Result ret = readFrame(frame);
    if (ret != kSuccess) {
        CLIMATE_LOG_ERROR("Sharp: Failed to read state frame");
        return ret;
    }

    if (frame.size > 1) {
        sendAck();
    }

    if (frame.size == kModeFrameSize && frame.data[2] == kFrameTypeResponse) {
        settings.temperature = static_cast<int>((frame.data[4] & 0x0F) + 16);
        settings.action = (frame.data[8] & 0x80) ? HeatpumpAction::On : HeatpumpAction::Off;

        uint8_t mode = frame.data[5] & 0x0F;
        settings.mode = byteToMode(mode);

        uint8_t fan = (frame.data[5] & 0xF0) >> 4;
        settings.fanSpeed = byteToFan(fan);

        uint8_t swingV = frame.data[6] & 0x0F;
        settings.vaneMode = byteToVane(swingV);

        CLIMATE_LOG_DEBUG("Sharp state: mode=%u, temp=%d, fan=%u, action=%u, vane=%u",
                     static_cast<unsigned>(settings.mode),
                     settings.temperature,
                     static_cast<unsigned>(settings.fanSpeed),
                     static_cast<unsigned>(settings.action),
                     static_cast<unsigned>(settings.vaneMode));
    } else if (frame.size == kStatusFrameSize) {
        settings.temperature = static_cast<int>(frame.data[7]);
        CLIMATE_LOG_DEBUG("Sharp status frame: temp=%d", settings.temperature);
    } else {
        CLIMATE_LOG_WARNING("Sharp: Unexpected frame type");
        return kInvalidData;
    }

    flushRx();
    return kSuccess;
}

Result Sharp::getRoomTemperature(float &temperature) {
    Frame frame;

    if (!connected_) {
        Result ret = connect();
        if (ret != kSuccess) {
            return kInvalidNotConnected;
        }
    }

    Result ret = readFrame(frame);
    if (ret != kSuccess) {
        return ret;
    }

    if (frame.size > 1) {
        sendAck();
    }

    if (frame.size == kStatusFrameSize) {
        temperature = static_cast<float>(frame.data[7]);
        CLIMATE_LOG_DEBUG("Room temperature: %.1f°C", static_cast<double>(temperature));
        return kSuccess;
    }

    return kInvalidData;
}

}  // namespace protocols
}  // namespace climate_uart
