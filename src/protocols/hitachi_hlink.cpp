#include "climate_uart/protocols/hitachi_hlink.h"

#include "climate_uart/result.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// H-Link protocol based on:
// https://github.com/lumixen/esphome-hlink-ac/blob/main/components/hlink_ac/hlink_ac.cpp
// https://github.com/lumixen/esphome-hlink-ac/blob/main/components/hlink_ac/hlink_ac.h
// https://hackaday.io/project/168959/logs?sort=newest&page=1

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint32_t kHlinkBaudrate = 9600;
constexpr uint32_t kReadTimeoutMs = 300;
constexpr uint16_t kMsgBufferSize = 64;
constexpr uint8_t kDataMaxLen = 8;
constexpr uint8_t kAsciiCr = 0x0D;

constexpr uint16_t kFeaturePowerState = 0x0000;
constexpr uint16_t kFeatureMode = 0x0001;
constexpr uint16_t kFeatureFanMode = 0x0002;
constexpr uint16_t kFeatureTargetTemp = 0x0003;
constexpr uint16_t kFeatureSwingMode = 0x0014;
constexpr uint16_t kFeatureCurrentIndoorTemp = 0x0100;

constexpr uint16_t kModeHeat = 0x0010;
constexpr uint16_t kModeHeatAuto = 0x8010;
constexpr uint16_t kModeCool = 0x0040;
constexpr uint16_t kModeCoolAuto = 0x8040;
constexpr uint16_t kModeDry = 0x0020;
constexpr uint16_t kModeDryAuto = 0x8020;
constexpr uint16_t kModeFan = 0x0050;
constexpr uint16_t kModeAuto = 0x8000;

constexpr uint8_t kFanAuto = 0x00;
constexpr uint8_t kFanHigh = 0x01;
constexpr uint8_t kFanMed = 0x02;
constexpr uint8_t kFanLow = 0x03;
constexpr uint8_t kFanQuiet = 0x04;

constexpr uint8_t kSwingOff = 0x00;
constexpr uint8_t kSwingVertical = 0x01;
constexpr uint8_t kSwingHorizontal = 0x02;
constexpr uint8_t kSwingBoth = 0x03;

constexpr uint8_t kPowerOn = 0x01;
constexpr uint8_t kPowerOff = 0x00;
}  // namespace

HitachiHLink::HitachiHLink(transport::UartTransport &uart) : uart_(uart) {}

uint16_t HitachiHLink::crc(uint16_t address, const uint8_t *data, uint8_t dataLen) {
	uint16_t sum = 0xFFFF;
	sum -= (address >> 8) & 0xFF;
	sum -= address & 0xFF;
	for (uint8_t i = 0; i < dataLen; i++)
		sum -= data[i];
		
	return sum;
}

uint16_t HitachiHLink::modeToWord(HeatpumpMode mode) {
	switch (mode) {
		case HeatpumpMode::Cold:
			return kModeCool;
		case HeatpumpMode::Heat:
			return kModeHeat;
		case HeatpumpMode::Dry:
			return kModeDry;
		case HeatpumpMode::Fan:
			return kModeFan;
		case HeatpumpMode::Auto:
		default:
			return kModeAuto;
	}
}

HeatpumpMode HitachiHLink::wordToMode(uint16_t val) {
	switch (val) {
		case kModeCool:
			return HeatpumpMode::Cold;
		case kModeHeat:
			return HeatpumpMode::Heat;
		case kModeDry:
			return HeatpumpMode::Dry;
		case kModeFan:
			return HeatpumpMode::Fan;
		case kModeHeatAuto:
		case kModeCoolAuto:
		case kModeDryAuto:
		case kModeAuto:
		default:
			return HeatpumpMode::Auto;
	}
}

uint8_t HitachiHLink::fanToByte(HeatpumpFanSpeed fanSpeed) {
	switch (fanSpeed) {
		case HeatpumpFanSpeed::High:
			return kFanHigh;
		case HeatpumpFanSpeed::Med:
			return kFanMed;
		case HeatpumpFanSpeed::Low:
			return kFanLow;
		case HeatpumpFanSpeed::Auto:
		default:
			return kFanAuto;
	}
}

HeatpumpFanSpeed HitachiHLink::byteToFan(uint8_t val) {
	switch (val) {
		case kFanHigh:
			return HeatpumpFanSpeed::High;
		case kFanMed:
			return HeatpumpFanSpeed::Med;
		case kFanLow:
			return HeatpumpFanSpeed::Low;
		case kFanAuto:
		case kFanQuiet:
		default:
			return HeatpumpFanSpeed::Auto;
	}
}

uint8_t HitachiHLink::vaneToByte(HeatpumpVaneMode vaneMode) {
	switch (vaneMode) {
		case HeatpumpVaneMode::Swing:
			return kSwingBoth;
		case HeatpumpVaneMode::V1:
		case HeatpumpVaneMode::V2:
		case HeatpumpVaneMode::V3:
		case HeatpumpVaneMode::V4:
		case HeatpumpVaneMode::V5:
			return kSwingVertical;
		case HeatpumpVaneMode::Auto:
		default:
			return kSwingOff;
	}
}

HeatpumpVaneMode HitachiHLink::byteToVane(uint8_t val) {
	switch (val) {
		case kSwingVertical:
		case kSwingHorizontal:
		case kSwingBoth:
			return HeatpumpVaneMode::Swing;
		case kSwingOff:
		default:
			return HeatpumpVaneMode::Auto;
	}
}

Result HitachiHLink::readByte(uint8_t *byte, uint32_t timeoutMs) {
	if (!byte) {
		return kInvalidParameters;
	}

	uint32_t start = time_now_ms();
	while (time_elapsed_ms(start) < timeoutMs) {
		size_t size = 1;
		Result ret = uart_.read(byte, &size);
		if (ret == kSuccess && size == 1) {
			return kSuccess;
		}
	}

	return kTimeout;
}

Result HitachiHLink::readLine(char *buffer, uint16_t bufferSize, uint32_t timeoutMs) {
	if (!buffer || bufferSize == 0) {
		return kInvalidParameters;
	}

	uint32_t start = time_now_ms();
	uint16_t index = 0;
	while (time_elapsed_ms(start) < timeoutMs) {
		uint8_t byte = 0;
		Result ret = readByte(&byte, timeoutMs);
		if (ret == kSuccess) {
			if (byte == kAsciiCr) {
				if (index < bufferSize) {
					buffer[index] = '\0';
					return kSuccess;
				}
				return kInvalidData;
			}
			if (index + 1 >= bufferSize) {
				buffer[bufferSize - 1] = '\0';
				return kInvalidData;
			}
			buffer[index++] = static_cast<char>(byte);
		}
	}

	return kTimeout;
}

Result HitachiHLink::parseResponse(const char *line, Response &response) {
	if (!line) {
		return kInvalidParameters;
	}

	response.dataLen = 0;
	if (strcmp(line, "OK") == 0) {
		response.status = ResponseStatus::Ok;
		return kSuccess;
	}
	if (strcmp(line, "NG") == 0) {
		response.status = ResponseStatus::Ng;
		return kSuccess;
	}

	char temp[kMsgBufferSize];
	strncpy(temp, line, sizeof(temp));
	temp[sizeof(temp) - 1] = '\0';

	char *token1 = strtok(temp, " ");
	char *token2 = strtok(nullptr, " ");
	char *token3 = strtok(nullptr, " ");
	if (!token1 || !token2 || !token3) {
		return kInvalidData;
	}

	if (strcmp(token1, "OK") == 0) {
		response.status = ResponseStatus::Ok;
	} else if (strcmp(token1, "NG") == 0) {
		response.status = ResponseStatus::Ng;
	} else {
		response.status = ResponseStatus::Invalid;
		return kInvalidData;
	}

	if (strncmp(token2, "P=", 2) != 0 || strncmp(token3, "C=", 2) != 0) {
		return kInvalidData;
	}

	const char *pStr = token2 + 2;
	const char *cStr = token3 + 2;
	int pLen = static_cast<int>(strlen(pStr));
	if ((pLen % 2) != 0) {
		return kInvalidData;
	}

	response.dataLen = static_cast<uint8_t>(pLen / 2);
	if (response.dataLen > sizeof(response.data)) {
		return kInvalidData;
	}

	for (uint8_t i = 0; i < response.dataLen; i++) {
		char byteStr[3] = {pStr[i * 2], pStr[i * 2 + 1], '\0'};
		response.data[i] = static_cast<uint8_t>(strtoul(byteStr, nullptr, 16));
	}

	uint16_t receivedChecksum = static_cast<uint16_t>(strtoul(cStr, nullptr, 16));
	uint16_t calculatedChecksum = crc(0, response.data, response.dataLen);
	if (calculatedChecksum != receivedChecksum) {
		CLIMATE_LOG_WARNING("Hitachi H-Link: Invalid checksum recv=0x%04X, calc=0x%04X", receivedChecksum,
					 calculatedChecksum);
		return kInvalidCrc;
	}

	return kSuccess;
}

Result HitachiHLink::sendFrame(const char *type, uint16_t address, const uint8_t *data, uint8_t dataLen) {
	if (!type) {
		return kInvalidParameters;
	}

	char dataStr[kDataMaxLen * 2 + 1] = {0};
	for (uint8_t i = 0; i < dataLen && i < kDataMaxLen; i++) {
		snprintf(&dataStr[i * 2], 3, "%02X", data[i]);
	}

	uint16_t cs = crc(address, data, dataLen);
	char message[kMsgBufferSize] = {0};
	if (dataLen > 0) {
		snprintf(message, sizeof(message), "%s P=%04X,%s C=%04X\r", type, address, dataStr, cs);
	} else {
		snprintf(message, sizeof(message), "%s P=%04X C=%04X\r", type, address, cs);
	}

	CLIMATE_LOG_DEBUG("Hitachi H-Link Send: %s", message);
	return uart_.write(reinterpret_cast<const uint8_t *>(message), strlen(message));
}

Result HitachiHLink::query(uint16_t address, Response &response) {
	Result ret = sendFrame("MT", address, nullptr, 0);
	if (ret != kSuccess) {
		return ret;
	}

	char line[kMsgBufferSize] = {0};
	ret = readLine(line, sizeof(line), kReadTimeoutMs);
	if (ret != kSuccess) {
		CLIMATE_LOG_ERROR("Hitachi H-Link: Failed to read response line for address 0x%04X", address);
		return ret;
	}

	CLIMATE_LOG_DEBUG("Hitachi H-Link Read: %s", line);
	ret = parseResponse(line, response);
	if (ret != kSuccess) {
		return ret;
	}

	return (response.status == ResponseStatus::Ok) ? kSuccess : kInvalidReply;
}

Result HitachiHLink::command(uint16_t address, const uint8_t *data, uint8_t dataLen) {
	Response response;
	Result ret = sendFrame("ST", address, data, dataLen);
	if (ret != kSuccess) {
		return ret;
	}

	char line[kMsgBufferSize] = {0};
	ret = readLine(line, sizeof(line), kReadTimeoutMs);
	if (ret != kSuccess) {
		return ret;
	}

	CLIMATE_LOG_DEBUG("Hitachi H-Link Read: %s", line);
	ret = parseResponse(line, response);
	if (ret != kSuccess) {
		return ret;
	}

	return (response.status == ResponseStatus::Ok) ? kSuccess : kInvalidReply;
}

Result HitachiHLink::init() {
	Result ret = uart_.open(kHlinkBaudrate, transport::UartParity::Odd, 1);
	if (ret != kSuccess) {
		return ret;
	}

	connected_ = true;
	return kSuccess;
}

Result HitachiHLink::setState(const ClimateSettings &settings) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	uint8_t power = (settings.action == HeatpumpAction::On) ? kPowerOn : kPowerOff;
	Result ret = command(kFeaturePowerState, &power, 1);
	if (ret != kSuccess) {
		return ret;
	}

	if (settings.action == HeatpumpAction::On) {
		uint16_t mode = modeToWord(settings.mode);
		uint8_t modeBytes[2] = {static_cast<uint8_t>((mode >> 8) & 0xFF), static_cast<uint8_t>(mode & 0xFF)};
		ret = command(kFeatureMode, modeBytes, 2);
		if (ret != kSuccess) {
			return ret;
		}

		uint16_t temp = static_cast<uint16_t>(settings.temperature);
		uint8_t tempBytes[2] = {static_cast<uint8_t>((temp >> 8) & 0xFF), static_cast<uint8_t>(temp & 0xFF)};
		ret = command(kFeatureTargetTemp, tempBytes, 2);
		if (ret != kSuccess) {
			return ret;
		}

		uint8_t fan = fanToByte(settings.fanSpeed);
		ret = command(kFeatureFanMode, &fan, 1);
		if (ret != kSuccess) {
			return ret;
		}

		uint8_t swing = vaneToByte(settings.vaneMode);
		ret = command(kFeatureSwingMode, &swing, 1);
		if (ret != kSuccess) {
			return ret;
		}
	}

	return kSuccess;
}

Result HitachiHLink::getState(ClimateSettings &settings) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	settings = ClimateSettings{};

	//Order is important

	Response response;
	Result ret = query(kFeaturePowerState, response);
	if (ret != kSuccess || response.dataLen < 1) {
		return kInvalidData;
	}
	settings.action = (response.data[0] == kPowerOn) ? HeatpumpAction::On : HeatpumpAction::Off;

	ret = query(kFeatureMode, response);
	if (ret != kSuccess || response.dataLen < 2) {
		return kInvalidData;
	}
	uint16_t modeVal = static_cast<uint16_t>((response.data[0] << 8) | response.data[1]);
	settings.mode = wordToMode(modeVal);

	ret = query(kFeatureTargetTemp, response);
	if (ret != kSuccess || response.dataLen < 1) {
		return kInvalidData;
	}
	if (response.dataLen == 1) {
		settings.temperature = response.data[0];
	} else {
		settings.temperature = static_cast<int>((response.data[0] << 8) | response.data[1]);
	}

	ret = query(kFeatureSwingMode, response);
	if (ret != kSuccess || response.dataLen < 1) {
		return kInvalidData;
	}
	settings.vaneMode = byteToVane(response.data[0]);

	ret = query(kFeatureFanMode, response);
	if (ret != kSuccess || response.dataLen < 1) {
		return kInvalidData;
	}
	settings.fanSpeed = byteToFan(response.data[0]);

	CLIMATE_LOG_DEBUG("Hitachi H-Link state: mode=%u, temp=%d, fan=%u, action=%u, vane=%u",
					 static_cast<uint8_t>(settings.mode), settings.temperature,
					 static_cast<uint8_t>(settings.fanSpeed), static_cast<uint8_t>(settings.action),
					 static_cast<uint8_t>(settings.vaneMode));

	return kSuccess;
}

Result HitachiHLink::getRoomTemperature(float &temperature) {
	if (!connected_) {
		return kInvalidNotConnected;
	}

	Response response;
	Result ret = query(kFeatureCurrentIndoorTemp, response);
	if (ret != kSuccess || response.dataLen < 1) {
		return kInvalidData;
	}
	if (response.dataLen == 1) {
		temperature = static_cast<float>(response.data[0]);
	} else {
		int16_t temp = static_cast<int16_t>((response.data[0] << 8) | response.data[1]);
		temperature = static_cast<float>(temp);
	}

	CLIMATE_LOG_DEBUG("Hitachi H-Link room temperature: %.1f°C", temperature);
	return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
