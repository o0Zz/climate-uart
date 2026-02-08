# Climate-UART

Climate-UART is a platform-agnostic library that provides a unified API to control and monitor HVAC systems over UART.

It is designed to run on many microcontrollers (ESP32, Arduino, etc.) and can be ported to new platforms by supplying a UART transport implementation.

## Why this project
I started the ha-climate2mqtt project to communicate with many HVAC units, but I quickly ran into a problem: there were plenty of vendor-specific libraries on GitHub, yet no single, consistent API that made it easy to swap brands. Climate-UART solves that by unifying multiple UART protocols behind one shared interface, so applications can support several climates without rewriting control logic.

## ❄️ Supported Climates
- [X] Mitsubishi (UART)
- [X] Toshiba (UART) - (Seiya, Shorai, Yukai, ...)
- [X] Daikin S21 (UART)
- [X] Sharp (UART)
- [X] LG Aircon (UART)
- [X] Hitachi H-Link (UART)
- [X] Fujitsu (UART)

## Example Arduino
```cpp
#include <ClimateUART.h>

climate_uart::transport::UartTransportArduino uartArduino(Serial);
climate_uart::protocols::DaikinS21 daikin(uartArduino);

void setup() {
  daikin.init();
}

void loop() {
  climate_uart::ClimateSettings settings;
  daikin.getState(settings);
}
```

## Example ESP32
```cpp
#include "climate_uart/protocols/mitsubishi.h"
#include "climate_uart/transport/uart_transport_esp32.h"

using namespace climate_uart;

void main() {
	transport::UartTransportESP32 uart(UART_NUM_1, /*tx=*/17, /*rx=*/16);

	protocols::Mitsubishi mitsu(uart);
	ClimateInterface &climate = mitsu;

	if (climate.init() != kSuccess) {
		return;
	}

	ClimateSettings settings;
	if (climate.getState(settings) == kSuccess) {
		// use settings.action, settings.mode, settings.temperature, etc.
	}
}
```