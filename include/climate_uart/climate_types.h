#pragma once

#include <cstdint>

namespace climate_uart {

enum class HeatpumpMode : uint8_t {
    None = 0,
    Cold,
    Dry,
    Fan,
    Auto,
    Heat,
    Count
};

enum class HeatpumpFanSpeed : uint8_t {
    None = 0,
    Auto,
    High,
    Med,
    Low,
    Count
};

enum class HeatpumpAction : uint8_t {
    Off = 0,
    On,
    Direction
};

enum class HeatpumpVaneMode : uint8_t {
    Auto = 0,
    V1,
    V2,
    V3,
    V4,
    V5,
    Swing,
    Count
};

struct ClimateSettings {
    HeatpumpAction action{HeatpumpAction::Off};
    int temperature{0};
    HeatpumpFanSpeed fanSpeed{HeatpumpFanSpeed::Auto};
    HeatpumpMode mode{HeatpumpMode::Auto};
    HeatpumpVaneMode vaneMode{HeatpumpVaneMode::Auto};
};

}  // namespace climate_uart
