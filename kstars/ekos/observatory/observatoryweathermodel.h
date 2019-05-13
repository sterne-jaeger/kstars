/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/weather.h"
#include "indiweather.h"

#include <QObject>

namespace Ekos
{

class ObservatoryWeatherModel : public QObject
{

    Q_OBJECT

public:
    ObservatoryWeatherModel() = default;

    void initModel(Weather *weather);
    ISD::Weather::Status status();

private:
    Weather *mWeather;

signals:
    void newStatus(ISD::Weather::Status state);
    void ready();

};

}
