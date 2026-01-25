#include "climate_uart/protocols/lg_aircon.h"

#include <cstring>

#include "climate_uart/common.h"

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint8_t kMsgLen = 13;
constexpr uint32_t kTimeoutMs = 500;

constexpr uint8_t kMsgTypeStatusMaster = 0xA8;
constexpr uint8_t kMsgTypeStatusUnit = 0xC8;

constexpr uint8_t kModeCool = 0;
constexpr uint8_t kModeDry = 1;
constexpr uint8_t kModeFan = 2;
constexpr uint8_t kModeAuto = 3;
constexpr uint8_t kModeHeat = 4;

constexpr uint8_t kFanLow = 0;
constexpr uint8_t kFanMed = 1;
constexpr uint8_t kFanHigh = 2;
constexpr uint8_t kFanAuto = 3;
constexpr uint8_t kFanQuiet = 4;

constexpr uint8_t kPowerOn = 0x02;
constexpr uint8_t kPowerOff = 0x00;

constexpr uint8_t kSwingHorizontal = 0x40;
constexpr uint8_t kSwingVertical = 0x80;
constexpr uint8_t kSwingBoth = kSwingHorizontal | kSwingVertical;
constexpr uint8_t kSwingOff = 0x00;

constexpr uint8_t kHeatpumpModeLg[] = {
	0xFF,  // None
	kModeCool,
	kModeDry,
	kModeFan,
	kModeAuto,
	kModeHeat
};

constexpr uint8_t kFanSpeedLg[] = {
	0xFF,  // None
	kFanAuto,
	kFanHigh,
	kFanMed,
	kFanLow
};
}  // namespace

LgAircon::LgAircon(transport::UartTransport &uart) : uart_(uart) {
	std::memset(lastRecvStatus_, 0x00, sizeof(lastRecvStatus_));
}

uint8_t LgAircon::crc(const uint8_t *buffer, size_t size) {
	uint32_t result = 0;
	for (size_t i = 0; i < size; i++) {
		result += buffer[i];
	}
	return static_cast<uint8_t>((result & 0xFF) ^ 0x55);
}

Result LgAircon::readByte(uint8_t *byte, uint32_t timeoutMs) {
	if (!byte) {
		return kInvalidParameters;
	}

	uint32_t start = time_now_ms();
	while (timer_elapsed_ms(start) < timeoutMs) {
		size_t size = 1;
		Result ret = uart_.read(byte, &size);
		if (ret == kSuccess && size == 1) {
			return kSuccess;
		}
	}

	CLIMATE_LOG_DEBUG("LG: ReadByte Timeout (%u ms)", timeoutMs);
	return kTimeout;
}

Result LgAircon::readMsg(uint8_t *buffer, size_t bufferSize) {
	if (!buffer || bufferSize < kMsgLen) {
		return kInvalidParameters;
	}

	for (size_t i = 0; i < bufferSize; i++) {
		if (readByte(&buffer[i], kTimeoutMs) != kSuccess) {
			return kTimeout;
		}
	}

	CLIMATE_LOG_DEBUG("LG Read:");
	CLIMATE_LOG_BUFFER(buffer, bufferSize);

	uint8_t expected = crc(buffer, bufferSize - 1);
	if (buffer[12] != expected) {
		CLIMATE_LOG_ERROR("LG: Invalid checksum rcv=0x%02X, calc=0x%02X", buffer[12], expected);
		return kInvalidCrc;
	}

	return kSuccess;
}

Result LgAircon::writeMsg(uint8_t *buffer, size_t bufferSize) {
	if (!buffer || bufferSize == 0) {
		return kInvalidParameters;
	}

	CLIMATE_LOG_DEBUG("LG Write:");
	CLIMATE_LOG_BUFFER(buffer, bufferSize);

	return uart_.write(buffer, bufferSize);
}

Result LgAircon::readStatus(uint8_t *buffer, size_t bufferSize) {
	uint32_t start = time_now_ms();
	while (timer_elapsed_ms(start) < 2000) {
		Result ret = readMsg(buffer, bufferSize);
		if (ret == kSuccess) {
			uint8_t msgType = buffer[0];
			if ((msgType & 0xF8) == kMsgTypeStatusUnit) {
				switch (msgType & 0x07) {
					case 0:
						roomTemperature_ = static_cast<float>(buffer[7] & 0x3F) / 2.0f + 10.0f;
						return kSuccess;
					default:
						break;
				}
			}
		}
	}
	return kTimeout;
}

Result LgAircon::connect() {
	uint8_t buffer[kMsgLen];
	connected_ = false;

	std::memset(buffer, 0x00, sizeof(buffer));
	buffer[0] = kMsgTypeStatusMaster;
	buffer[1] = 0x00;
	buffer[8] |= 0x40;
	buffer[10] = 0x80;
	buffer[12] = crc(buffer, kMsgLen);

	Result ret = writeMsg(buffer, kMsgLen);
	if (ret != kSuccess) {
		return ret;
	}

	ret = readStatus(lastRecvStatus_, kMsgLen);
	if (ret == kSuccess) {
		connected_ = true;
	}

	return ret;
}

Result LgAircon::init() {
	std::memset(lastRecvStatus_, 0x00, sizeof(lastRecvStatus_));
	roomTemperature_ = 20.0f;

	Result ret = uart_.open(104, transport::UartParity::None, 1);
	if (ret != kSuccess) {
		return ret;
	}

	return connect();
}

Result LgAircon::setState(const ClimateSettings &settings) {
	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	uint8_t buffer[kMsgLen];
	std::memset(buffer, 0x00, sizeof(buffer));
	buffer[0] = kMsgTypeStatusMaster;
	buffer[1] = 0x01;
	if (settings.action == HeatpumpAction::On) {
		buffer[1] |= kPowerOn;
	}
	buffer[1] |= (kHeatpumpModeLg[static_cast<uint8_t>(settings.mode)] & 0x07) << 2;
	buffer[1] |= (kFanSpeedLg[static_cast<uint8_t>(settings.fanSpeed)] & 0x07) << 5;
	buffer[2] = lastRecvStatus_[2] & ~(kSwingBoth);
	if (settings.vaneMode == HeatpumpVaneMode::Swing) {
		buffer[2] |= kSwingVertical;
	}
	buffer[3] = lastRecvStatus_[3];
	buffer[4] = lastRecvStatus_[4];
	buffer[5] = lastRecvStatus_[5] & ~0x01;
	buffer[6] = static_cast<uint8_t>(0x10 | ((settings.temperature - 15) & 0x0F));
	buffer[7] = lastRecvStatus_[7];
	buffer[8] = lastRecvStatus_[8];
	buffer[9] = lastRecvStatus_[9];
	buffer[10] = lastRecvStatus_[10];
	buffer[11] = lastRecvStatus_[11];
	buffer[12] = crc(buffer, kMsgLen);

	Result ret = writeMsg(buffer, kMsgLen);
	if (ret != kSuccess) {
		return ret;
	}

	ret = readStatus(lastRecvStatus_, kMsgLen);
	if (ret != kSuccess) {
		CLIMATE_LOG_WARNING("LG: No response after set_state");
	}

	return ret;
}

Result LgAircon::getState(ClimateSettings &settings) {
	settings = ClimateSettings{};

	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	Result ret = readStatus(lastRecvStatus_, kMsgLen);
	if (ret != kSuccess) {
		return ret;
	}

	uint8_t *status = lastRecvStatus_;

	settings.action = ((status[1] & kPowerOn) == 0) ? HeatpumpAction::Off : HeatpumpAction::On;

	uint8_t modeVal = static_cast<uint8_t>((status[1] >> 2) & 0x07);
	switch (modeVal) {
		case kModeCool:
			settings.mode = HeatpumpMode::Cold;
			break;
		case kModeDry:
			settings.mode = HeatpumpMode::Dry;
			break;
		case kModeFan:
			settings.mode = HeatpumpMode::Fan;
			break;
		case kModeAuto:
			settings.mode = HeatpumpMode::Auto;
			break;
		case kModeHeat:
			settings.mode = HeatpumpMode::Heat;
			break;
		default:
			settings.mode = HeatpumpMode::Auto;
			break;
	}

	uint8_t fanVal = static_cast<uint8_t>((status[1] >> 5) & 0x07);
	switch (fanVal) {
		case kFanLow:
			settings.fanSpeed = HeatpumpFanSpeed::Low;
			break;
		case kFanMed:
			settings.fanSpeed = HeatpumpFanSpeed::Med;
			break;
		case kFanHigh:
			settings.fanSpeed = HeatpumpFanSpeed::High;
			break;
		case kFanAuto:
		case kFanQuiet:
		default:
			settings.fanSpeed = HeatpumpFanSpeed::Auto;
			break;
	}

	float target = static_cast<float>((status[6] & 0x0F) + 15);
	if (status[5] & 0x01) {
		target += 0.5f;
	}
	settings.temperature = static_cast<int>(target + 0.5f);

	bool vertSwing = (status[2] & kSwingVertical) != 0;
	settings.vaneMode = vertSwing ? HeatpumpVaneMode::Swing : HeatpumpVaneMode::Auto;

	while (readStatus(lastRecvStatus_, kMsgLen) == kSuccess) {
		// flush pending status
	}

	return kSuccess;
}

Result LgAircon::getRoomTemperature(float &temperature) {
	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	temperature = roomTemperature_;
	return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
