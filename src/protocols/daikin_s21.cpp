#include "climate_uart/protocols/daikin_s21.h"

#include "climate_uart/result.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint8_t kS21Stx = 0x02;
constexpr uint8_t kS21Etx = 0x03;
constexpr uint8_t kS21Ack = 0x06;
constexpr uint8_t kS21Nak = 0x15;
constexpr uint32_t kResponseTimeoutMs = 250;
constexpr uint16_t kMaxFrameSize = 64;
constexpr int kMinTemperature = 18;
constexpr int kMaxTemperature = 32;
constexpr int kSetpointOffset = 28;
constexpr int kSetpointStep = 5;

constexpr uint8_t kQueryF1[2] = {'F', '1'};
constexpr uint8_t kQueryF5[2] = {'F', '5'};
constexpr uint8_t kQueryRh[2] = {'R', 'H'};
}  // namespace

DaikinS21::DaikinS21(transport::UartTransport &uart) : uart_(uart) {}

uint8_t DaikinS21::checksum(const uint8_t *bytes, uint16_t len) {
	uint8_t sum = 0;
	for (uint16_t i = 0; i < len; i++) {
		sum += bytes[i];
	}
	return sum;
}

HeatpumpMode DaikinS21::byteToMode(uint8_t mode) {
	switch (mode) {
		case '4':
			return HeatpumpMode::Heat;
		case '3':
			return HeatpumpMode::Cold;
		case '2':
			return HeatpumpMode::Dry;
		case '6':
			return HeatpumpMode::Fan;
		case '1':
			return HeatpumpMode::Auto;
		case '0':
			return HeatpumpMode::None;
		default:
			return HeatpumpMode::Auto;
	}
}

uint8_t DaikinS21::modeToByte(HeatpumpMode mode, HeatpumpAction action) {
	if (action != HeatpumpAction::On || mode == HeatpumpMode::None) {
		return '0';
	}

	switch (mode) {
		case HeatpumpMode::Heat:
			return '4';
		case HeatpumpMode::Cold:
			return '3';
		case HeatpumpMode::Dry:
			return '2';
		case HeatpumpMode::Fan:
			return '6';
		case HeatpumpMode::Auto:
		default:
			return '1';
	}
}

HeatpumpFanSpeed DaikinS21::byteToFan(uint8_t code) {
	switch (code) {
		case 'A':
			return HeatpumpFanSpeed::Auto;
		case 'B':
		case '3':
			return HeatpumpFanSpeed::Low;
		case '4':
		case '5':
			return HeatpumpFanSpeed::Med;
		case '6':
		case '7':
			return HeatpumpFanSpeed::High;
		default:
			return HeatpumpFanSpeed::Auto;
	}
}

uint8_t DaikinS21::fanToByte(HeatpumpFanSpeed fan) {
	switch (fan) {
		case HeatpumpFanSpeed::Low:
			return '3';
		case HeatpumpFanSpeed::Med:
			return '5';
		case HeatpumpFanSpeed::High:
			return '7';
		case HeatpumpFanSpeed::Auto:
		default:
			return 'A';
	}
}

Result DaikinS21::readByte(uint8_t *byte, uint32_t timeoutMs) {
	uint32_t start = time_now_ms();

	while (time_elapsed_ms(start) < timeoutMs) {
		size_t size = 1;
		Result ret = uart_.read(byte, &size);
		if (ret == kSuccess && size == 1) {
			return kSuccess;
		}
	}

	CLIMATE_LOG_DEBUG("Daikin ReadByte Timeout (%u ms)", timeoutMs);
	return kTimeout;
}

Result DaikinS21::waitForAck() {
	uint8_t byte = 0;
	Result ret = readByte(&byte, kResponseTimeoutMs);
	if (ret != kSuccess) {
		CLIMATE_LOG_WARNING("Daikin: Timeout waiting for ACK");
		return ret;
	}

	if (byte == kS21Ack) {
		return kSuccess;
	}

	CLIMATE_LOG_WARNING("Daikin: Unexpected byte waiting for ACK: 0x%02X", byte);
	return kInvalidReply;
}

Result DaikinS21::sendFrame(const uint8_t *frame, uint16_t frameLen) {
	if (!frame || frameLen == 0) {
		return kInvalidParameters;
	}

	Result ret = uart_.write(&kS21Stx, 1);
	if (ret != kSuccess) {
		return ret;
	}

	ret = uart_.write(frame, frameLen);
	if (ret != kSuccess) {
		return ret;
	}

	uint8_t cs = checksum(frame, frameLen);
	ret = uart_.write(&cs, 1);
	if (ret != kSuccess) {
		return ret;
	}

	return uart_.write(&kS21Etx, 1);
}

Result DaikinS21::readFrame(uint8_t *buffer, uint16_t *outSize) {
	if (!buffer || !outSize || *outSize == 0) {
		return kInvalidParameters;
	}

	uint16_t idx = 0;
	memset(buffer, 0x00, *outSize);

	while (buffer[idx] != kS21Stx) {
		if (readByte(&buffer[idx], kResponseTimeoutMs) == kTimeout) {
			return kTimeout;
		}

		if (buffer[idx] != kS21Stx) {
			CLIMATE_LOG_WARNING("Daikin: Discarded byte: 0x%02X", buffer[idx]);
		}
	}

	while ((buffer[++idx] != kS21Etx) && (idx < *outSize - 1)) {
		if (readByte(&buffer[idx], kResponseTimeoutMs) != kSuccess) {
			CLIMATE_LOG_WARNING("Daikin: Timeout reading frame");
			return kTimeout;
		}
	}

	uint8_t calculated = checksum(buffer, static_cast<uint16_t>(idx - 1));
	if (buffer[idx - 1] != calculated) {
		CLIMATE_LOG_ERROR("Daikin: Checksum mismatch %02X != %02X", buffer[idx - 1], calculated);
		CLIMATE_LOG_BUFFER(buffer, idx);
		return kInvalidCrc;
	}

	*outSize = static_cast<uint16_t>(idx + 1);
	return kSuccess;
}

Result DaikinS21::query(const uint8_t *frame, uint16_t frameLen, uint8_t *payload, uint16_t *payloadLen) {
	if (!frame || !payload || !payloadLen) {
		return kInvalidParameters;
	}

	Result ret = sendFrame(frame, frameLen);
	if (ret != kSuccess) {
		return ret;
	}

	ret = waitForAck();
	if (ret != kSuccess) {
		return ret;
	}

	ret = readFrame(payload, payloadLen);
	if (ret != kSuccess) {
		return ret;
	}

	uint8_t ack = kS21Ack;
	ret = uart_.write(&ack, 1);
	if (ret != kSuccess) {
		return ret;
	}

	return kSuccess;
}

Result DaikinS21::sendCmd(const uint8_t *frame, uint16_t frameLen) {
	if (!frame) {
		return kInvalidParameters;
	}

	Result ret = sendFrame(frame, frameLen);
	if (ret != kSuccess) {
		return ret;
	}

	return waitForAck();
}

Result DaikinS21::setSwingSettings(bool swingV, bool swingH) {
	uint8_t command[6];
	command[0] = 'D';
	command[1] = '5';
	uint8_t bits = static_cast<uint8_t>((swingH ? 2 : 0) + (swingV ? 1 : 0));
	if (swingV && swingH) {
		bits = static_cast<uint8_t>(bits + 4);
	}
	command[2] = static_cast<uint8_t>('0' + bits);
	command[3] = (swingV || swingH) ? '?' : '0';
	command[4] = '0';
	command[5] = '0';

	return sendCmd(command, sizeof(command));
}

Result DaikinS21::init() {
	Result ret = uart_.open(2400, transport::UartParity::Even, 2);
	if (ret != kSuccess) {
		return ret;
	}

	connected_ = true;
	return kSuccess;
}

Result DaikinS21::setState(const ClimateSettings &settings) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	int target = settings.temperature;
	if (target < kMinTemperature) {
		target = kMinTemperature;
	} else if (target > kMaxTemperature) {
		target = kMaxTemperature;
	}

	int16_t c10 = static_cast<int16_t>(target * 10);
	uint8_t command[6];
	command[0] = 'D';
	command[1] = '1';
	command[2] = (settings.action == HeatpumpAction::On) ? '1' : '0';
	command[3] = modeToByte(settings.mode, settings.action);
	command[4] = static_cast<uint8_t>((c10 + 3) / kSetpointStep + kSetpointOffset);
	command[5] = fanToByte(settings.fanSpeed);

	Result ret = sendCmd(command, sizeof(command));
	if (ret != kSuccess) {
		return ret;
	}

	bool swing = (settings.vaneMode == HeatpumpVaneMode::Swing);
	ret = setSwingSettings(swing, swing);
	if (ret != kSuccess) {
		CLIMATE_LOG_WARNING("S21: Swing update failed (%d)", ret);
	}

	return kSuccess;
}

Result DaikinS21::getState(ClimateSettings &settings) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	settings = ClimateSettings{};
	settings.action = HeatpumpAction::Off;
	settings.mode = HeatpumpMode::None;
	settings.fanSpeed = HeatpumpFanSpeed::Auto;
	settings.vaneMode = HeatpumpVaneMode::Auto;
	settings.temperature = kMinTemperature;

	uint8_t payload[kMaxFrameSize];
	uint16_t payloadLen = kMaxFrameSize;

	Result ret = query(kQueryF1, sizeof(kQueryF1), payload, &payloadLen);
	if (ret != kSuccess) {
		CLIMATE_LOG_ERROR("Daikin: Failed to query basic state (%d)", ret);
		return ret;
	}

	if (payload[0] != 'G' || payload[1] != '1') {
		return kInvalidData;
	}

	settings.action = (payload[2] == '1') ? HeatpumpAction::On : HeatpumpAction::Off;
	settings.mode = byteToMode(payload[3]);
	settings.temperature = static_cast<int>(((payload[4] - kSetpointOffset) * kSetpointStep) / 10);
	settings.fanSpeed = byteToFan(payload[5]);
	if (settings.action != HeatpumpAction::On) {
		settings.mode = HeatpumpMode::None;
	}

	payloadLen = kMaxFrameSize;
	ret = query(kQueryF5, sizeof(kQueryF5), payload, &payloadLen);
	if (ret == kSuccess && payload[0] == 'G' && payload[1] == '5') {
		bool swingV = (payload[2] & 1) != 0;
		bool swingH = (payload[2] & 2) != 0;
		settings.vaneMode = (swingV || swingH) ? HeatpumpVaneMode::Swing : HeatpumpVaneMode::Auto;
	}

	connected_ = true;
	return kSuccess;
}

Result DaikinS21::getRoomTemperature(float &temperature) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	uint8_t payload[kMaxFrameSize];
	uint16_t payloadLen = kMaxFrameSize;

	Result ret = query(kQueryRh, sizeof(kQueryRh), payload, &payloadLen);
	if (ret != kSuccess) {
		CLIMATE_LOG_WARNING("Daikin: Failed to query room temp (%d)", ret);
		return ret;
	}

	if (payloadLen < 3 || payload[0] != 'S' || payload[1] != 'H') {
		return kInvalidData;
	}

	if (payloadLen < kMaxFrameSize) {
		payload[payloadLen] = 0;
	} else {
		payload[kMaxFrameSize - 1] = 0;
	}

	temperature = static_cast<float>(atoi(reinterpret_cast<char *>(&payload[2]))) / 10.0f;
	return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
