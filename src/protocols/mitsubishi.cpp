#include "climate_uart/protocols/mitsubishi.h"

#include "climate_uart/result.h"

#include <string.h>
#include <stdlib.h>

namespace climate_uart {
namespace protocols {

namespace {
constexpr uint8_t kStx = 0xFC;
constexpr uint32_t kTimeoutMs = 1000;
constexpr uint8_t kProtoReply = 0x20;

constexpr uint8_t kHeatpumpModeMitsubishi[] = {
	0xFF,  // None
	0x03,  // Cold
	0x02,  // Dry
	0x07,  // Fan
	0x08,  // Auto
	0x01   // Heat
};

constexpr uint8_t kFanSpeedMitsubishi[] = {
	0xFF,  // None
	0x00,  // Auto
	0x06,  // High
	0x03,  // Med
	0x01   // Low
};

constexpr uint8_t kVaneModeMitsubishi[] = {
	0x00,  // Auto
	0x01,  // 1
	0x02,  // 2
	0x03,  // 3
	0x04,  // 4
	0x05,  // 5
	0x07   // Swing
};
}  // namespace

Mitsubishi::Mitsubishi(transport::UartTransport &uart) : uart_(uart) {}

uint8_t Mitsubishi::crc(const uint8_t *buffer, uint8_t size) {
	uint32_t sum = 0;
	for (uint8_t i = 1; i < size; i++) {
		sum += buffer[i];
	}
	return static_cast<uint8_t>(-static_cast<int8_t>(sum));
}

Result Mitsubishi::readByte(uint8_t *byte, uint32_t timeoutMs) {
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

	CLIMATE_LOG_DEBUG("Mitsu: ReadByte Timeout (%u ms)", timeoutMs);
	return kTimeout;
}

Result Mitsubishi::readPacket(Packet &packet) {
	packet.stx = 0x00;
	while (packet.stx != kStx) {
		if (readByte(&packet.stx, kTimeoutMs) == kTimeout) 
			return kTimeout;
		if (packet.stx != kStx)
			CLIMATE_LOG_WARNING("Mitsu: Discarded byte: 0x%02X", packet.stx);
	}

	if (readByte(&packet.cmd, kTimeoutMs) != kSuccess)
		return kTimeout;
	if (readByte(&packet.header[0], kTimeoutMs) != kSuccess)
		return kTimeout;
	if (readByte(&packet.header[1], kTimeoutMs) != kSuccess)
		return kTimeout;
	if (readByte(&packet.size, kTimeoutMs) != kSuccess) 
		return kTimeout;

	for (int i = 0; i < packet.size; i++) {
		if (readByte(&packet.data[i], kTimeoutMs) != kSuccess) {
			return kTimeout;
		}
	}

	if (readByte(&packet.checksum, kTimeoutMs) != kSuccess) {
		return kTimeout;
	}

	uint8_t calculated = crc(reinterpret_cast<uint8_t *>(&packet), static_cast<uint8_t>(packet.size + 5));
	if (calculated != packet.checksum) {
		CLIMATE_LOG_ERROR("Mitsu: Invalid crc rcv=0x%02X, calc=0x%02X continue...", packet.checksum, calculated);
	}

	CLIMATE_LOG_DEBUG("Mitsu Read Packet: cmd=0x%02X, size=%u", packet.cmd, packet.size);
	CLIMATE_LOG_BUFFER(reinterpret_cast<const uint8_t *>(&packet), packet.size + 6);
	return kSuccess;
}

Result Mitsubishi::writePacket(const Packet &packet) {
	uint8_t buffer[32] = {kStx, packet.cmd, 0x01, 0x30, packet.size};
	memcpy(&buffer[5], packet.data, packet.size);
	buffer[5 + packet.size] = crc(buffer, static_cast<uint8_t>(packet.size + 5));

	CLIMATE_LOG_DEBUG("Mitsu Write:");
	CLIMATE_LOG_BUFFER(buffer, packet.size + 6);

	Result ret = uart_.write(buffer, packet.size + 6);
	if (ret != kSuccess) {
		CLIMATE_LOG_ERROR("Mitsu: WritePacket failed: %d", ret);
	}
	return ret;
}

Result Mitsubishi::connect() {
	Packet packet{};
	packet.cmd = 0x5A;
	packet.size = 0x02;
	packet.data[0] = 0xCA;
	packet.data[1] = 0x01;

	CLIMATE_LOG_DEBUG("Mitsubishi connecting ...");

	connected_ = false;
	Result ret = writePacket(packet);
	while (ret == kSuccess) {
		ret = readPacket(packet);
		if (ret == kSuccess) {
			if (packet.cmd == static_cast<uint8_t>(0x5A | kProtoReply) || packet.cmd == 0x5A) {
				CLIMATE_LOG_INFO("Mitsubishi connected !");
				connected_ = true;
				return kSuccess;
			}
		}
	}

	CLIMATE_LOG_ERROR("Mitsubishi not connected");
	return kInvalidNotConnected;
}

Result Mitsubishi::init() {
	Result ret = uart_.open(2400, transport::UartParity::Even, 1);
	if (ret != kSuccess) {
		return ret;
	}

	return connect();
}

Result Mitsubishi::setState(const ClimateSettings &settings) {
	Packet pkt{};

	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	pkt.cmd = 0x41;
	pkt.size = 16;
	pkt.data[0] = static_cast<uint8_t>(PacketType::SetSettingsInformation);
	pkt.data[1] |= 0x1F;
	pkt.data[3] = (settings.action == HeatpumpAction::On) ? 0x01 : 0x00;
	pkt.data[4] = kHeatpumpModeMitsubishi[static_cast<uint8_t>(settings.mode)];
	pkt.data[5] = static_cast<uint8_t>(0x0F - (settings.temperature - 16));
	pkt.data[6] = kFanSpeedMitsubishi[static_cast<uint8_t>(settings.fanSpeed)];
	pkt.data[7] = kVaneModeMitsubishi[static_cast<uint8_t>(settings.vaneMode)];
	pkt.data[10] = 0x00;

	Result ret = writePacket(pkt);
	if (ret != kSuccess) {
		return ret;
	}

	Packet reply{};
	ret = readPacket(reply);
	if (ret != kSuccess) {
		return ret;
	}

	return (reply.cmd == static_cast<uint8_t>(0x41 | kProtoReply)) ? kSuccess : kInvalidReply;
}

Result Mitsubishi::getState(ClimateSettings &settings) {
	Packet pkt{};

	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	settings = ClimateSettings{};

	pkt.cmd = 0x42;
	pkt.size = 16;
	pkt.data[0] = static_cast<uint8_t>(PacketType::GetSettingsInformation);

	Result ret = writePacket(pkt);
	if (ret != kSuccess) {
		return ret;
	}

	Packet reply{};
	ret = readPacket(reply);
	if (ret != kSuccess) {
		CLIMATE_LOG_ERROR("Mitsu: Failed to get state, marking as disconnected...");
		connected_ = false;
		return ret;
	}

	if (reply.data[0] != static_cast<uint8_t>(PacketType::GetSettingsInformation)) {
		CLIMATE_LOG_ERROR("Mitsu: Invalid reply packet type: 0x%02X", reply.data[0]);
		return kInvalidData;
	}

	settings.action = (reply.data[3] == 0x01) ? HeatpumpAction::On : HeatpumpAction::Off;

	for (uint8_t mode = static_cast<uint8_t>(HeatpumpMode::None);
		 mode < static_cast<uint8_t>(HeatpumpMode::Count);
		 mode++) {
		if (reply.data[4] == kHeatpumpModeMitsubishi[mode]) {
			settings.mode = static_cast<HeatpumpMode>(mode);
			break;
		}
	}

	settings.temperature = (0x0F - reply.data[5]) + 16;

	if (reply.data[6] == 0x01 || reply.data[6] == 0x02) {
		settings.fanSpeed = HeatpumpFanSpeed::Low;
	} else if (reply.data[6] == 0x03 || reply.data[6] == 0x04) {
		settings.fanSpeed = HeatpumpFanSpeed::Med;
	} else if (reply.data[6] == 0x05 || reply.data[6] == 0x06) {
		settings.fanSpeed = HeatpumpFanSpeed::High;
	} else {
		settings.fanSpeed = HeatpumpFanSpeed::Auto;
	}

	for (uint8_t vane = static_cast<uint8_t>(HeatpumpVaneMode::Auto);
		 vane < static_cast<uint8_t>(HeatpumpVaneMode::Count);
		 vane++) {
		if (reply.data[7] == kVaneModeMitsubishi[vane]) {
			settings.vaneMode = static_cast<HeatpumpVaneMode>(vane);
			break;
		}
	}

	return kSuccess;
}

Result Mitsubishi::getRoomTemperature(float &temperature) {
	Packet pkt{};

	if (!connected_) {
		Result ret = connect();
		if (ret != kSuccess) {
			return ret;
		}
	}

	pkt.cmd = 0x42;
	pkt.size = 16;
	pkt.data[0] = static_cast<uint8_t>(PacketType::GetRoomTemperature);

	Result ret = writePacket(pkt);
	if (ret != kSuccess) {
		return ret;
	}

	Packet reply{};
	ret = readPacket(reply);
	if (ret != kSuccess) {
		return ret;
	}

	if (reply.data[0] != static_cast<uint8_t>(PacketType::GetRoomTemperature)) {
		CLIMATE_LOG_ERROR("Mitsu: Invalid reply packet type: 0x%02X", reply.data[0]);
		return kInvalidData;
	}

	temperature = reply.data[3] + 10.0f;
	if (reply.data[6] != 0x00) {
		temperature = (reply.data[6] & 0x7F) / 2.0f;
	}

	return kSuccess;
}

}  // namespace protocols
}  // namespace climate_uart
