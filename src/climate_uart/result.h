#pragma once

#include "climate_uart/platform.h"

namespace climate_uart {
using Result = int;

constexpr Result kSuccess = 0;
constexpr Result kDeviceNotFound = -1;
constexpr Result kDeviceInitFailed = -2;
constexpr Result kFailSetPin = -3;
constexpr Result kReadError = -4;
constexpr Result kWriteError = -5;
constexpr Result kFailAck = -6;
constexpr Result kFailDeviceTooSlow = -7;
constexpr Result kFailWrongMode = -8;
constexpr Result kTimeout = -9;
constexpr Result kInvalidCrc = -10;
constexpr Result kInvalidData = -11;
constexpr Result kNotSupported = -12;
constexpr Result kInvalidParameters = -13;
constexpr Result kInvalidState = -14;
constexpr Result kInvalidNotConnected = -15;
constexpr Result kInvalidReply = -16;

}  // namespace climate_uart
