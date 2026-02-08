#pragma once

// ClimateUART.h
// Main header file for the ClimateUART library for Arduino

#include "climate_uart/transport/uart_transport_arduino.h"
#include "climate_uart/transport/uart_transport_esp32.h"

#include "climate_uart/protocols/daikin_s21.h"
#include "climate_uart/protocols/fujitsu.h"
#include "climate_uart/protocols/hitachi_hlink.h"
#include "climate_uart/protocols/lg_aircon.h"
#include "climate_uart/protocols/mitsubishi.h"
#include "climate_uart/protocols/sharp.h"
#include "climate_uart/protocols/toshiba.h"

#include "climate_uart/platform.h"