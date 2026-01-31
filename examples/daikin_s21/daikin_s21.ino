#include <ClimateUART.h>

climate_uart::transport::UartTransportArduino uartArduino(Serial, 0, 0);
climate_uart::protocols::DaikinS21 daikin(uartArduino);

void setup() {
  daikin.init();
}

void loop() {
  climate_uart::ClimateSettings settings;
  daikin.getState(settings);
}
