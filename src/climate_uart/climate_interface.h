#pragma once

#include "climate_uart/result.h"
#include "climate_uart/climate_types.h"

namespace climate_uart {

class ClimateInterface {
public:
    virtual ~ClimateInterface() = default;

    virtual Result init() = 0;
    virtual Result setState(const ClimateSettings &settings) = 0;
    virtual Result getState(ClimateSettings &settings) = 0;
    virtual Result getRoomTemperature(float &temperature) = 0;
};

}  // namespace climate_uart
