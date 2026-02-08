#include <ClimateUART.h>

climate_uart::transport::UartTransportArduino uartArduino(Serial);

climate_uart::protocols::Mitsubishi my_climate(uartArduino);
//climate_uart::protocols::DaikinS21 my_climate(uartArduino);
//climate_uart::protocols::HitachiHLink my_climate(uartArduino);
//climate_uart::protocols::LgAircon my_climate(uartArduino);
//climate_uart::protocols::Sharp my_climate(uartArduino);
//climate_uart::protocols::Toshiba my_climate(uartArduino);

void setup() {

  /*Important note:
      Do NOT add `Serial.begin(9600)` and do NOT use `Serial.print(...)`.
      Most climate units require Even or Odd parity, so `climate_uart` must use a hardware serial interface.
      As a result, this hardware serial cannot be used for debugging or other purposes.
      Use a software serial for debug output, or use a serial implementation that supports parity.
  */

  my_climate.init();
}

void loop() {
  /*
    This loop will power off the climate every 5s
  */
  climate_uart::ClimateSettings settings;

  settings.action      = climate_uart::HeatpumpAction::Off;
  settings.temperature = 21;
  settings.fanSpeed    = climate_uart::HeatpumpFanSpeed::Auto;
  settings.mode        = climate_uart::HeatpumpMode::Auto;
  settings.vaneMode    = climate_uart::HeatpumpVaneMode::Auto;

  my_climate.setState(settings);

  delay(5000);
}
