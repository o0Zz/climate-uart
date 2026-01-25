# Climate-UART

Climate-UART is a platform-agnostic library that provides a unified API to control and monitor HVAC systems over UART.

It is designed to run on many microcontrollers (ESP32, Arduino, etc.) and can be ported to new platforms by supplying a UART transport implementation.

## Why this project
I started the ha-climate2mqtt project to communicate with many HVAC units, but I quickly ran into a problem: there were plenty of vendor-specific libraries on GitHub, yet no single, consistent API that made it easy to swap brands. Climate-UART solves that by unifying multiple UART protocols behind one shared interface, so applications can support several climates without rewriting control logic.

## ❄️ Supported Climates
- [X] Mitsubishi (UART) [Mitsubishi](doc/Hardware_Mitsubishi.md)
- [X] Toshiba (UART) - (Seiya, Shorai, Yukai, ...) [Toshiba](doc/Hardware_Toshiba.md)
- [X] Daikin S21 (UART)
- [X] LG Aircon (UART)
- [X] Hitachi H-Link (UART)
- [ ] Fujitsu (UART)

## Example
```cpp
#include "climate_uart/protocols/mitsubishi.h"
#include "climate_uart/transport/uart_transport_esp32.h"

using namespace climate_uart;

void main() {
	// 1) Init UART transport (ESP32)
	transport::UartTransportESP32 uart(UART_NUM_1, /*tx=*/17, /*rx=*/16);

	// 2) Create Mitsubishi protocol and use it as ClimateInterface
	protocols::Mitsubishi mitsu(uart);
	ClimateInterface &climate = mitsu;

	// 3) Initialize protocol (opens UART with proper settings)
	if (climate.init() != kSuccess) {
		// handle error
		return;
	}

	// 4) Get current settings
	ClimateSettings settings;
	if (climate.getState(settings) == kSuccess) {
		// use settings.action, settings.mode, settings.temperature, etc.
	}
}
```