/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

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

struct WeatherActions {
    bool closeDome, closeShutter;
    int delay;
};

class ObservatoryWeatherModel : public QObject
{

    Q_OBJECT

public:
    ObservatoryWeatherModel() = default;

    void initModel(Weather *weather);
    ISD::Weather::Status status();

    WeatherActions getWarningActions() { return warningActions; }
    void setWarningActions(WeatherActions actions);
    WeatherActions getAlertActions() { return alertActions; }
    void setAlertActions(WeatherActions actions);

private:
    Weather *mWeather;
    struct WeatherActions warningActions, alertActions;

signals:
    void newStatus(ISD::Weather::Status state);
    void ready();
    void disconnected();

};

}
