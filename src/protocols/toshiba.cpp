#include "climate_uart/protocols/toshiba.h"

#include "climate_uart/result.h"

#include <string.h>
#include <stdlib.h>

//Ref: https://github.com/ormsport/ToshibaCarrierHvac/blob/main/src/ToshibaCarrierHvac.cpp
//Ref: https://github.com/IntExCZ/Toshiba-HVAC/blob/main/HomeAssistant/AppDaemon/apps/toshiba-hvac.py

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint16_t kMaxPacketSize = 0xFF;
constexpr uint32_t kPacketReadTimeoutMs = 250;
constexpr uint8_t kPacketStx = 0x02;

constexpr uint8_t kPacketTypeReplyMask = 0x80;
constexpr uint8_t kPacketTypeCommand = 0x10;

constexpr uint8_t kFunctionPowerState = 0x80;
constexpr uint8_t kFunctionPowerSelection = 0x87;
constexpr uint8_t kFunctionStatus = 0x88;
constexpr uint8_t kFunctionFanMode = 0xA0;
constexpr uint8_t kFunctionSwing = 0xA3;
constexpr uint8_t kFunctionUnitMode = 0xB0;
constexpr uint8_t kFunctionSetpoint = 0xB3;
constexpr uint8_t kFunctionRoomTemp = 0xBB;
constexpr uint8_t kFunctionGroup1 = 0xF8;

constexpr uint8_t kPowerStateOn = 0x30;
constexpr uint8_t kPowerStateOff = 0x31;

constexpr uint8_t kModeAuto = 0x41;
constexpr uint8_t kModeCool = 0x42;
constexpr uint8_t kModeHeat = 0x43;
constexpr uint8_t kModeDry = 0x44;
constexpr uint8_t kModeFanOnly = 0x45;

constexpr uint8_t kFanQuiet = 0x31;
constexpr uint8_t kFanLvl1 = 0x32;
constexpr uint8_t kFanLvl2 = 0x33;
constexpr uint8_t kFanLvl3 = 0x34;
constexpr uint8_t kFanLvl4 = 0x35;
constexpr uint8_t kFanLvl5 = 0x36;
constexpr uint8_t kFanAuto = 0x41;

constexpr uint8_t kSwingFix = 0x31;
constexpr uint8_t kSwingVertical = 0x41;
constexpr uint8_t kSwingHorizontal = 0x42;
constexpr uint8_t kSwingBoth = 0x43;
constexpr uint8_t kSwingPos1 = 0x50;
constexpr uint8_t kSwingPos2 = 0x51;
constexpr uint8_t kSwingPos3 = 0x52;
constexpr uint8_t kSwingPos4 = 0x53;
constexpr uint8_t kSwingPos5 = 0x54;
}  // namespace

Toshiba::Toshiba(transport::UartTransport &uart) : uart_(uart) {}

uint8_t Toshiba::crc(const uint8_t *data, uint8_t dataLen) {
	uint32_t sum = 0x00;
	for (uint8_t i = 1; i < dataLen; i++) {
		sum += data[i];
	}
	return static_cast<uint8_t>(-static_cast<uint8_t>(sum));
}

uint8_t Toshiba::modeToByte(HeatpumpMode mode) {
	switch (mode) {
		case HeatpumpMode::Auto:
			return kModeAuto;
		case HeatpumpMode::Cold:
			return kModeCool;
		case HeatpumpMode::Heat:
			return kModeHeat;
		case HeatpumpMode::Dry:
			return kModeDry;
		case HeatpumpMode::Fan:
			return kModeFanOnly;
		default:
			return kModeAuto;
	}
}

HeatpumpMode Toshiba::byteToMode(uint8_t val) {
	switch (val) {
		case kModeAuto:
			return HeatpumpMode::Auto;
		case kModeCool:
			return HeatpumpMode::Cold;
		case kModeHeat:
			return HeatpumpMode::Heat;
		case kModeDry:
			return HeatpumpMode::Dry;
		case kModeFanOnly:
			return HeatpumpMode::Fan;
		default:
			return HeatpumpMode::Auto;
	}
}

uint8_t Toshiba::fanToByte(HeatpumpFanSpeed fanSpeed) {
	switch (fanSpeed) {
		case HeatpumpFanSpeed::Auto:
			return kFanAuto;
		case HeatpumpFanSpeed::Low:
			return kFanLvl1;
		case HeatpumpFanSpeed::Med:
			return kFanLvl3;
		case HeatpumpFanSpeed::High:
			return kFanLvl5;
		default:
			return kFanAuto;
	}
}

HeatpumpFanSpeed Toshiba::byteToFan(uint8_t val) {
	switch (val) {
		case kFanAuto:
			return HeatpumpFanSpeed::Auto;
		case kFanQuiet:
		case kFanLvl1:
			return HeatpumpFanSpeed::Low;
		case kFanLvl2:
		case kFanLvl3:
			return HeatpumpFanSpeed::Med;
		case kFanLvl4:
		case kFanLvl5:
			return HeatpumpFanSpeed::High;
		default:
			return HeatpumpFanSpeed::Auto;
	}
}

uint8_t Toshiba::vaneToByte(HeatpumpVaneMode vaneMode) {
	switch (vaneMode) {
		case HeatpumpVaneMode::Auto:
			return kSwingFix;
		case HeatpumpVaneMode::Swing:
			return kSwingVertical;
		case HeatpumpVaneMode::V1:
			return kSwingPos1;
		case HeatpumpVaneMode::V2:
			return kSwingPos2;
		case HeatpumpVaneMode::V3:
			return kSwingPos3;
		case HeatpumpVaneMode::V4:
			return kSwingPos4;
		case HeatpumpVaneMode::V5:
			return kSwingPos5;
		default:
			return kSwingFix;
	}
}

HeatpumpVaneMode Toshiba::byteToVane(uint8_t val) {
	switch (val) {
		case kSwingFix:
			return HeatpumpVaneMode::Auto;
		case kSwingVertical:
		case kSwingHorizontal:
		case kSwingBoth:
			return HeatpumpVaneMode::Swing;
		case kSwingPos1:
			return HeatpumpVaneMode::V1;
		case kSwingPos2:
			return HeatpumpVaneMode::V2;
		case kSwingPos3:
			return HeatpumpVaneMode::V3;
		case kSwingPos4:
			return HeatpumpVaneMode::V4;
		case kSwingPos5:
			return HeatpumpVaneMode::V5;
		default:
			return HeatpumpVaneMode::Auto;
	}
}

Result Toshiba::readByte(uint8_t *byte, uint32_t timeoutMs) {
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

	CLIMATE_LOG_DEBUG("Toshiba ReadByte Timeout (%u ms)", timeoutMs);
	return kTimeout;
}

Result Toshiba::readPacket(Packet &packet) {
	packet.stx = 0x00;
	while (packet.stx != kPacketStx) {
		if (readByte(&packet.stx, kPacketReadTimeoutMs) == kTimeout) {
			return kTimeout;
		}
		if (packet.stx != kPacketStx) {
			CLIMATE_LOG_WARNING("Toshiba: Discarded byte: 0x%02X", packet.stx);
		}
	}

	if (readByte(&packet.header[0], kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}
	if (readByte(&packet.header[1], kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}
	if (readByte(&packet.type, kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}
	if (readByte(&packet.unknown1, kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}
	if (readByte(&packet.unknown2, kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}
	if (readByte(&packet.size, kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}

	if (packet.size > kMaxPacketSize) {
		return kInvalidData;
	}

	for (uint16_t i = 0; i < packet.size; i++) {
		if (readByte(&packet.data[i], kPacketReadTimeoutMs) != kSuccess) {
			return kTimeout;
		}
	}

	if (readByte(&packet.checksum, kPacketReadTimeoutMs) != kSuccess) {
		return kTimeout;
	}

	uint8_t calculated = crc(reinterpret_cast<uint8_t *>(&packet), static_cast<uint8_t>(packet.size + 7));
	if (calculated != packet.checksum) {
		CLIMATE_LOG_ERROR("Toshiba: Invalid crc recv=0x%02X, calc=0x%02X", packet.checksum, calculated);
	}

	CLIMATE_LOG_DEBUG("Received packet: type=0x%02X, size=%u", packet.type, packet.size);
	CLIMATE_LOG_BUFFER(reinterpret_cast<const uint8_t *>(&packet), packet.size + 8);
	return kSuccess;
}

Result Toshiba::sendCommand(uint8_t *data, uint16_t dataSize) {
	if (!data && dataSize > 0) {
		return kInvalidParameters;
	}

	uint8_t buffer[64];
	uint16_t pos = 0;
	buffer[pos++] = kPacketStx;
	buffer[pos++] = 0x00;
	buffer[pos++] = 0x03;
	buffer[pos++] = kPacketTypeCommand;
	buffer[pos++] = 0x00;
	buffer[pos++] = 0x00;
	buffer[pos++] = static_cast<uint8_t>(dataSize + 5);
	buffer[pos++] = 0x01;
	buffer[pos++] = 0x30;
	buffer[pos++] = 0x01;
	buffer[pos++] = 0x00;
	buffer[pos++] = static_cast<uint8_t>(dataSize);
	if (dataSize > 0) {
		memcpy(&buffer[pos], data, dataSize);
		pos += dataSize;
	}
	buffer[pos] = crc(buffer, static_cast<uint8_t>(pos));
	pos++;

	CLIMATE_LOG_DEBUG("Sending command size=%u", pos);
	CLIMATE_LOG_BUFFER(buffer, pos);

	return uart_.write(buffer, pos);
}

Result Toshiba::query(uint8_t function, Packet &result) {
	uint8_t buffer[] = {function};
	Result ret = sendCommand(buffer, sizeof(buffer));
	if (ret != kSuccess) {
		return ret;
	}

	while (readPacket(result) == kSuccess) {
		if (result.type == static_cast<uint8_t>(kPacketTypeCommand | kPacketTypeReplyMask)) {
			return kSuccess;
		}
	}

	return kTimeout;
}

void Toshiba::flushRx() {
	Packet packet{};
	while (readPacket(packet) == kSuccess) {
	}
}

Result Toshiba::connect() {
	static const uint8_t kSyn1[] = {0x02, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02};
	static const uint8_t kSyn2[] = {0x02, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x01, 0x02, 0xFE};
	static const uint8_t kSyn3[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0xFA};
	static const uint8_t kSyn4[] = {0x02, 0x00, 0x01, 0x81, 0x01, 0x00, 0x02, 0x00, 0x00, 0x7B};
	static const uint8_t kSyn5[] = {0x02, 0x00, 0x01, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0xFB};
	static const uint8_t kSyn6[] = {0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFE};
	static const uint8_t kSyn7[] = {0x02, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0xFB};
	static const uint8_t kSyn8[] = {0x02, 0x00, 0x02, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0xFA};

	struct SyncPkt {
		const uint8_t *data;
		size_t size;
	};
	const SyncPkt syncPkts[] = {
		{kSyn1, sizeof(kSyn1)},
		{kSyn2, sizeof(kSyn2)},
		{kSyn3, sizeof(kSyn3)},
		{kSyn4, sizeof(kSyn4)},
		{kSyn5, sizeof(kSyn5)},
		{kSyn6, sizeof(kSyn6)},
		{kSyn7, sizeof(kSyn7)},
		{kSyn8, sizeof(kSyn8)}
	};

	connected_ = false;
	CLIMATE_LOG_DEBUG("Toshiba: Starting handshake sequence ...");
	for (size_t i = 0; i < sizeof(syncPkts) / sizeof(syncPkts[0]); i++) {
		CLIMATE_LOG_DEBUG("Sending handshake SYN%u packet ...", static_cast<unsigned>(i + 1));
		CLIMATE_LOG_BUFFER(syncPkts[i].data, syncPkts[i].size);

		Result ret = uart_.write(syncPkts[i].data, syncPkts[i].size);
		if (ret != kSuccess) {
			CLIMATE_LOG_ERROR("Toshiba: Handshake SYN%u write failed: %d", static_cast<unsigned>(i + 1), ret);
			return ret;
		}
		flushRx();
	}

	Packet packet{};
	if (query(kFunctionStatus, packet) != kSuccess) {
		CLIMATE_LOG_ERROR("Toshiba: Handshake failed - no response to status query");
		return kTimeout;
	}

	CLIMATE_LOG_INFO("Toshiba Heatpump connected successfully");
	connected_ = true;
	return kSuccess;
}

Result Toshiba::command(uint8_t function, uint8_t value) {
	Packet result{};
	uint8_t buffer[] = {function, value};
	Result ret = sendCommand(buffer, sizeof(buffer));
	if (ret != kSuccess) {
		return ret;
	}

	while (readPacket(result) == kSuccess) {
		if (result.type == static_cast<uint8_t>(kPacketTypeCommand | kPacketTypeReplyMask)) {
			CLIMATE_LOG_DEBUG("Command response received for function: '0x%X' (Size=%u)", function, result.size);
			return kSuccess;
		}
	}

	return kTimeout;
}

Result Toshiba::init() {
	connected_ = false;
	CLIMATE_LOG_DEBUG("Opening UART for Toshiba Heatpump ...");
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

Result Toshiba::setState(const ClimateSettings &settings) {
	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return kInvalidNotConnected;
		}
	}

	Result ret = command(kFunctionPowerState,
						 static_cast<uint8_t>((settings.action == HeatpumpAction::On) ? kPowerStateOn : kPowerStateOff));
	if (ret != kSuccess) {
		return ret;
	}

	if (settings.action == HeatpumpAction::On) {
		ret = command(kFunctionSetpoint, static_cast<uint8_t>(settings.temperature));
		if (ret != kSuccess) {
			return ret;
		}

		ret = command(kFunctionUnitMode, modeToByte(settings.mode));
		if (ret != kSuccess) {
			return ret;
		}

		ret = command(kFunctionFanMode, fanToByte(settings.fanSpeed));
		if (ret != kSuccess) {
			return ret;
		}

		ret = command(kFunctionSwing, vaneToByte(settings.vaneMode));
		if (ret != kSuccess) {
			return ret;
		}
	}

	flushRx();
	CLIMATE_LOG_DEBUG("Settings sent successfully");
	return kSuccess;
}

Result Toshiba::getState(ClimateSettings &settings) {
	Packet response{};
	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return kInvalidNotConnected;
		}
	}

	settings = ClimateSettings{};

	Result ret = query(kFunctionGroup1, response);
	if (ret != kSuccess || response.size < 12 || response.data[7] != kFunctionGroup1) {
		CLIMATE_LOG_ERROR("Toshiba: Failed to get state Group1, marking as disconnected...");
		connected_ = false;
		return kTimeout;
	}

	settings.mode = byteToMode(response.data[8]);
	settings.temperature = response.data[9];
	settings.fanSpeed = byteToFan(response.data[10]);

	ret = query(kFunctionPowerState, response);
	if (ret != kSuccess || response.size < 9 || response.data[7] != kFunctionPowerState) {
		CLIMATE_LOG_ERROR("Toshiba: Failed to get state power");
		return kInvalidData;
	}

	settings.action = (response.data[8] == kPowerStateOn) ? HeatpumpAction::On : HeatpumpAction::Off;

	ret = query(kFunctionSwing, response);
	if (ret != kSuccess || response.size < 9 || response.data[7] != kFunctionSwing) {
		CLIMATE_LOG_ERROR("Toshiba: Failed to get state swing");
		return kInvalidData;
	}

	settings.vaneMode = byteToVane(response.data[8]);
	flushRx();

	CLIMATE_LOG_DEBUG("Toshiba state: mode=%u, temp=%d, fan=%u, action=%u, vane=%u",
					 static_cast<uint8_t>(settings.mode), settings.temperature,
					 static_cast<uint8_t>(settings.fanSpeed), static_cast<uint8_t>(settings.action),
					 static_cast<uint8_t>(settings.vaneMode));

	return kSuccess;
}

Result Toshiba::getRoomTemperature(float &temperature) {
	Packet response{};
	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return kInvalidNotConnected;
		}
	}

	Result ret = query(kFunctionRoomTemp, response);
	if (ret == kSuccess && response.size >= 9 && response.data[7] == kFunctionRoomTemp) {
		temperature = static_cast<float>(static_cast<int8_t>(response.data[1]));
		CLIMATE_LOG_DEBUG("Room temperature: %.1f°C", temperature);
	}

	return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
