/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/weather.h"


namespace Ekos
{

class ObservatoryWeatherModel
{
public:
    ObservatoryWeatherModel() = default;

    void initModel(Weather *weather);

private:
    Weather *mWeather;
};

}
