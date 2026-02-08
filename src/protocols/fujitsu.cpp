#include "climate_uart/protocols/fujitsu.h"

#include "climate_uart/result.h"

#include <string.h>

// Ref: https://github.com/unreality/FujiHeatPump
// Ref: https://github.com/FujiHeatPump/esphome-fujitsu/blob/master/src/FujiHeatPump.cpp
// Fujitsu UART protocol: 500 baud, 8E1, 8-byte frames, XOR 0xFF encoded

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint32_t kBaudRate = 500;
constexpr uint32_t kFrameSize = 8;
constexpr uint32_t kReadTimeoutMs = 1000;
constexpr uint32_t kFrameGapMs = 50;

// Byte 3: mode/fan/enabled/error
constexpr uint8_t kModeIndex     = 3;
constexpr uint8_t kModeMask      = 0b00001110;
constexpr uint8_t kModeOffset    = 1;

constexpr uint8_t kFanIndex      = 3;
constexpr uint8_t kFanMask       = 0b01110000;
constexpr uint8_t kFanOffset     = 4;

constexpr uint8_t kEnabledIndex  = 3;
constexpr uint8_t kEnabledMask   = 0b00000001;
constexpr uint8_t kEnabledOffset = 0;

constexpr uint8_t kErrorIndex    = 3;
constexpr uint8_t kErrorMask     = 0b10000000;
constexpr uint8_t kErrorOffset   = 7;

// Byte 4: temperature/economy
constexpr uint8_t kTempIndex     = 4;
constexpr uint8_t kTempMask      = 0b01111111;
constexpr uint8_t kTempOffset    = 0;

constexpr uint8_t kEconomyIndex  = 4;
constexpr uint8_t kEconomyMask   = 0b10000000;
constexpr uint8_t kEconomyOffset = 7;

// Byte 5: updateMagic/swing
constexpr uint8_t kUpdateMagicIndex  = 5;
constexpr uint8_t kUpdateMagicMask   = 0b11110000;
constexpr uint8_t kUpdateMagicOffset = 4;

constexpr uint8_t kSwingIndex        = 5;
constexpr uint8_t kSwingMask         = 0b00000100;
constexpr uint8_t kSwingOffset       = 2;

constexpr uint8_t kSwingStepIndex    = 5;
constexpr uint8_t kSwingStepMask     = 0b00000010;
constexpr uint8_t kSwingStepOffset   = 1;

// Byte 6: controllerPresent/controllerTemp
constexpr uint8_t kCtrlPresentIndex  = 6;
constexpr uint8_t kCtrlPresentMask   = 0b00000001;
constexpr uint8_t kCtrlPresentOffset = 0;

constexpr uint8_t kCtrlTempIndex     = 6;
constexpr uint8_t kCtrlTempMask      = 0b01111110;
constexpr uint8_t kCtrlTempOffset    = 1;

// Fujitsu mode values
constexpr uint8_t kFujiModeFan  = 1;
constexpr uint8_t kFujiModeDry  = 2;
constexpr uint8_t kFujiModeCool = 3;
constexpr uint8_t kFujiModeHeat = 4;
constexpr uint8_t kFujiModeAuto = 5;

// Fujitsu fan values
constexpr uint8_t kFujiFanAuto   = 0;
constexpr uint8_t kFujiFanQuiet  = 1;
constexpr uint8_t kFujiFanLow    = 2;
constexpr uint8_t kFujiFanMedium = 3;
constexpr uint8_t kFujiFanHigh   = 4;
}  // namespace

Fujitsu::Fujitsu(transport::UartTransport &uart, bool secondary)
    : uart_(uart), secondary_(secondary) {}

// --- Mode / Fan conversion helpers ---

uint8_t Fujitsu::modeToByte(HeatpumpMode mode) {
    switch (mode) {
        case HeatpumpMode::Fan:  return kFujiModeFan;
        case HeatpumpMode::Dry:  return kFujiModeDry;
        case HeatpumpMode::Cold: return kFujiModeCool;
        case HeatpumpMode::Heat: return kFujiModeHeat;
        case HeatpumpMode::Auto: return kFujiModeAuto;
        default:                 return kFujiModeAuto;
    }
}

HeatpumpMode Fujitsu::byteToMode(uint8_t val) {
    switch (val) {
        case kFujiModeFan:  return HeatpumpMode::Fan;
        case kFujiModeDry:  return HeatpumpMode::Dry;
        case kFujiModeCool: return HeatpumpMode::Cold;
        case kFujiModeHeat: return HeatpumpMode::Heat;
        case kFujiModeAuto: return HeatpumpMode::Auto;
        default:            return HeatpumpMode::Auto;
    }
}

uint8_t Fujitsu::fanToByte(HeatpumpFanSpeed fanSpeed) {
    switch (fanSpeed) {
        case HeatpumpFanSpeed::Auto: return kFujiFanAuto;
        case HeatpumpFanSpeed::Low:  return kFujiFanLow;
        case HeatpumpFanSpeed::Med:  return kFujiFanMedium;
        case HeatpumpFanSpeed::High: return kFujiFanHigh;
        default:                     return kFujiFanAuto;
    }
}

HeatpumpFanSpeed Fujitsu::byteToFan(uint8_t val) {
    switch (val) {
        case kFujiFanAuto:   return HeatpumpFanSpeed::Auto;
        case kFujiFanQuiet:  return HeatpumpFanSpeed::Low;
        case kFujiFanLow:    return HeatpumpFanSpeed::Low;
        case kFujiFanMedium: return HeatpumpFanSpeed::Med;
        case kFujiFanHigh:   return HeatpumpFanSpeed::High;
        default:             return HeatpumpFanSpeed::Auto;
    }
}

// --- Frame encode / decode ---

Fujitsu::Frame Fujitsu::decodeFrame(const uint8_t *buf) {
    Frame f;
    f.source = buf[0];
    f.dest   = buf[1] & 0x7F;
    f.type   = (buf[2] & 0x30) >> 4;

    f.acError          = (buf[kErrorIndex]       & kErrorMask)       >> kErrorOffset;
    f.temperature      = (buf[kTempIndex]        & kTempMask)        >> kTempOffset;
    f.mode             = (buf[kModeIndex]        & kModeMask)        >> kModeOffset;
    f.fanMode          = (buf[kFanIndex]         & kFanMask)         >> kFanOffset;
    f.economyMode      = (buf[kEconomyIndex]     & kEconomyMask)    >> kEconomyOffset;
    f.swingMode        = (buf[kSwingIndex]       & kSwingMask)       >> kSwingOffset;
    f.swingStep        = (buf[kSwingStepIndex]   & kSwingStepMask)  >> kSwingStepOffset;
    f.controllerPresent = (buf[kCtrlPresentIndex] & kCtrlPresentMask) >> kCtrlPresentOffset;
    f.updateMagic      = (buf[kUpdateMagicIndex] & kUpdateMagicMask) >> kUpdateMagicOffset;
    f.onOff            = (buf[kEnabledIndex]     & kEnabledMask)    >> kEnabledOffset;
    f.controllerTemp   = (buf[kCtrlTempIndex]    & kCtrlTempMask)   >> kCtrlTempOffset;

    f.writeBit   = (buf[2] & 0x08) != 0;
    f.loginBit   = (buf[1] & 0x20) != 0;
    f.unknownBit = (buf[1] & 0x80) != 0;

    return f;
}

void Fujitsu::encodeFrame(const Frame &f, uint8_t *buf) {
    memset(buf, 0, kFrameSize);

    buf[0] = f.source;
    buf[1] = f.dest & 0x7F;
    buf[2] = (f.type & 0x03) << 4;

    if (f.writeBit)   buf[2] |= 0x08;
    if (f.unknownBit) buf[1] |= 0x80;
    if (f.loginBit)   buf[1] |= 0x20;

    buf[kModeIndex]        = (buf[kModeIndex]        & ~kModeMask)        | (f.mode << kModeOffset);
    buf[kEnabledIndex]     = (buf[kEnabledIndex]     & ~kEnabledMask)     | (f.onOff << kEnabledOffset);
    buf[kFanIndex]         = (buf[kFanIndex]         & ~kFanMask)         | (f.fanMode << kFanOffset);
    buf[kErrorIndex]       = (buf[kErrorIndex]       & ~kErrorMask)       | (f.acError << kErrorOffset);
    buf[kEconomyIndex]     = (buf[kEconomyIndex]     & ~kEconomyMask)     | (f.economyMode << kEconomyOffset);
    buf[kTempIndex]        = (buf[kTempIndex]        & ~kTempMask)        | (f.temperature << kTempOffset);
    buf[kSwingIndex]       = (buf[kSwingIndex]       & ~kSwingMask)       | (f.swingMode << kSwingOffset);
    buf[kSwingStepIndex]   = (buf[kSwingStepIndex]   & ~kSwingStepMask)   | (f.swingStep << kSwingStepOffset);
    buf[kCtrlPresentIndex] = (buf[kCtrlPresentIndex] & ~kCtrlPresentMask) | (f.controllerPresent << kCtrlPresentOffset);
    buf[kUpdateMagicIndex] = (buf[kUpdateMagicIndex] & ~kUpdateMagicMask) | (f.updateMagic << kUpdateMagicOffset);
    buf[kCtrlTempIndex]    = (buf[kCtrlTempIndex]    & ~kCtrlTempMask)    | (f.controllerTemp << kCtrlTempOffset);
}

// --- UART read / write ---

Result Fujitsu::readFrame(Frame &frame) {
    uint8_t buf[kFrameSize];
    size_t totalRead = 0;

    uint32_t start = time_now_ms();
    while (totalRead < kFrameSize) {
        if (time_elapsed_ms(start) > kReadTimeoutMs) {
            CLIMATE_LOG_DEBUG("Fujitsu: readFrame timeout");
            return kTimeout;
        }

        size_t chunkSize = 1;
        Result ret = uart_.read(&buf[totalRead], &chunkSize);
        if (ret == kSuccess && chunkSize == 1) {
            totalRead++;
        }
    }

    // XOR decode (Fujitsu protocol inverts all bytes on the wire)
    for (size_t i = 0; i < kFrameSize; i++) {
        buf[i] ^= 0xFF;
    }

    CLIMATE_LOG_DEBUG("Fujitsu RX:");
    CLIMATE_LOG_BUFFER(buf, kFrameSize);

    frame = decodeFrame(buf);
    return kSuccess;
}

Result Fujitsu::writeFrame(const Frame &frame) {
    uint8_t buf[kFrameSize];
    encodeFrame(frame, buf);

    CLIMATE_LOG_DEBUG("Fujitsu TX:");
    CLIMATE_LOG_BUFFER(buf, kFrameSize);

    // XOR encode
    for (size_t i = 0; i < kFrameSize; i++) {
        buf[i] ^= 0xFF;
    }

    Result ret = uart_.write(buf, kFrameSize);
    if (ret != kSuccess) {
        CLIMATE_LOG_ERROR("Fujitsu: writeFrame failed: %d", ret);
        return ret;
    }

    // Read back our own frame (half-duplex bus)
    uint8_t echo[kFrameSize];
    size_t echoRead = 0;
    uint32_t start = time_now_ms();
    while (echoRead < kFrameSize) {
        if (time_elapsed_ms(start) > kReadTimeoutMs) {
            break;
        }
        size_t chunkSize = 1;
        uart_.read(&echo[echoRead], &chunkSize);
        if (chunkSize == 1) {
            echoRead++;
        }
    }

    return kSuccess;
}

// --- Protocol state machine ---

Result Fujitsu::processStatusFrame(Frame &rx, Frame &tx) {
    tx = rx;
    tx.source = controllerAddress_;
    tx.type = static_cast<uint8_t>(MessageType::Status);
    tx.unknownBit = true;
    tx.writeBit = false;

    if (rx.controllerPresent == 1) {
        // We are logged in - normal operation
        loggedIn_ = true;

        if (seenSecondary_) {
            tx.dest = static_cast<uint8_t>(Address::Secondary);
            tx.loginBit = true;
            tx.controllerPresent = 0;
        } else {
            tx.dest = static_cast<uint8_t>(Address::Unit);
            tx.loginBit = false;
            tx.controllerPresent = 1;
        }
        tx.updateMagic = 0;

        // Apply pending settings if any
        if (hasPendingUpdate_) {
            tx.writeBit = true;
            tx.onOff = (pendingUpdate_.action == HeatpumpAction::On) ? 1 : 0;
            tx.temperature = static_cast<uint8_t>(pendingUpdate_.temperature);
            tx.mode = modeToByte(pendingUpdate_.mode);
            tx.fanMode = fanToByte(pendingUpdate_.fanSpeed);
            tx.swingMode = (pendingUpdate_.vaneMode == HeatpumpVaneMode::Swing) ? 1 : 0;
            hasPendingUpdate_ = false;
        }

        // Save current state from the received frame
        currentState_ = rx;
    } else {
        // Not yet logged in
        if (!secondary_) {
            // Primary: send login request
            tx.dest = static_cast<uint8_t>(Address::Unit);
            tx.loginBit = false;
            tx.controllerPresent = 0;
            tx.updateMagic = 0;
            tx.type = static_cast<uint8_t>(MessageType::Login);

            tx.onOff = 0;
            tx.temperature = 0;
            tx.mode = 0;
            tx.fanMode = 0;
            tx.swingMode = 0;
            tx.swingStep = 0;
            tx.acError = 0;
        } else {
            // Secondary: reply with present flag
            tx.dest = static_cast<uint8_t>(Address::Unit);
            tx.loginBit = false;
            tx.controllerPresent = 1;
            tx.updateMagic = 2;
        }
    }

    return kSuccess;
}

Result Fujitsu::processLoginFrame(Frame &rx, Frame &tx) {
    tx = rx;
    tx.source = controllerAddress_;
    tx.dest = static_cast<uint8_t>(Address::Secondary);
    tx.loginBit = true;
    tx.controllerPresent = 1;
    tx.updateMagic = 0;
    tx.unknownBit = true;
    tx.writeBit = false;

    // Preserve current state in the response
    tx.onOff = currentState_.onOff;
    tx.temperature = currentState_.temperature;
    tx.mode = currentState_.mode;
    tx.fanMode = currentState_.fanMode;
    tx.swingMode = currentState_.swingMode;
    tx.swingStep = currentState_.swingStep;
    tx.acError = currentState_.acError;

    return kSuccess;
}

Result Fujitsu::poll() {
    Frame rx;
    Result ret = readFrame(rx);
    if (ret != kSuccess) {
        return ret;
    }

    // Only process frames addressed to us
    if (rx.dest != controllerAddress_) {
        // If the frame is going to the secondary, note it exists and grab its temp
        if (rx.dest == static_cast<uint8_t>(Address::Secondary)) {
            seenSecondary_ = true;
            currentState_.controllerTemp = rx.controllerTemp;
        }
        return kSuccess;
    }

    lastFrameMs_ = time_now_ms();

    Frame tx{};
    if (rx.type == static_cast<uint8_t>(MessageType::Status)) {
        processStatusFrame(rx, tx);
    } else if (rx.type == static_cast<uint8_t>(MessageType::Login)) {
        processLoginFrame(rx, tx);
    } else if (rx.type == static_cast<uint8_t>(MessageType::Error)) {
        CLIMATE_LOG_ERROR("Fujitsu: AC error received, error=%u", rx.acError);
        return kReadError;
    } else {
        CLIMATE_LOG_WARNING("Fujitsu: Unknown message type %u", rx.type);
        return kInvalidData;
    }

    // Wait for frame gap before replying
    while (time_elapsed_ms(lastFrameMs_) < kFrameGapMs) {
        // busy wait
    }

    return writeFrame(tx);
}

// --- ClimateInterface implementation ---

Result Fujitsu::init() {
    Result ret = uart_.open(kBaudRate, transport::UartParity::Even, 1);
    if (ret != kSuccess) {
        CLIMATE_LOG_ERROR("Fujitsu: UART open failed: %d", ret);
        return ret;
    }

    controllerAddress_ = secondary_
        ? static_cast<uint8_t>(Address::Secondary)
        : static_cast<uint8_t>(Address::Primary);

    CLIMATE_LOG_INFO("Fujitsu: init as %s controller (addr=%u)",
                     secondary_ ? "secondary" : "primary", controllerAddress_);

    // Poll a few times to establish connection with the indoor unit
    for (int i = 0; i < 10; i++) {
        poll();
    }

    return kSuccess;
}

Result Fujitsu::setState(const ClimateSettings &settings) {
    pendingUpdate_ = settings;
    hasPendingUpdate_ = true;

    // Poll until the update is applied
    for (int attempt = 0; attempt < 20 && hasPendingUpdate_; attempt++) {
        Result ret = poll();
        if (ret != kSuccess && ret != kTimeout) {
            return ret;
        }
    }

    return hasPendingUpdate_ ? kTimeout : kSuccess;
}

Result Fujitsu::getState(ClimateSettings &settings) {
    // Poll to get fresh state
    for (int attempt = 0; attempt < 10; attempt++) {
        Result ret = poll();
        if (ret != kSuccess && ret != kTimeout) {
            return ret;
        }
    }

    if (!loggedIn_) {
        return kInvalidNotConnected;
    }

    settings.action = (currentState_.onOff == 1) ? HeatpumpAction::On : HeatpumpAction::Off;
    settings.temperature = static_cast<int>(currentState_.temperature);
    settings.mode = byteToMode(currentState_.mode);
    settings.fanSpeed = byteToFan(currentState_.fanMode);
    settings.vaneMode = (currentState_.swingMode != 0) ? HeatpumpVaneMode::Swing : HeatpumpVaneMode::Auto;

    return kSuccess;
}

Result Fujitsu::getRoomTemperature(float &temperature) {
    // Poll to get fresh state
    for (int attempt = 0; attempt < 10; attempt++) {
        Result ret = poll();
        if (ret != kSuccess && ret != kTimeout) {
            return ret;
        }
    }

    if (!loggedIn_) {
        return kInvalidNotConnected;
    }

    temperature = static_cast<float>(currentState_.controllerTemp);
    return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
