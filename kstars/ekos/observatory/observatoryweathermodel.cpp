/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatoryweathermodel.h"
#include "Options.h"

namespace Ekos
{

void ObservatoryWeatherModel::initModel(Weather *weather)
{
    mWeather = weather;

    connect(mWeather, &Weather::ready, this, &ObservatoryWeatherModel::ready);
    connect(mWeather, &Weather::newStatus, this, &ObservatoryWeatherModel::newStatus);
    connect(mWeather, &Weather::disconnected, this, &ObservatoryWeatherModel::disconnected);

    // read the default values
    warningActions.closeDome = Options::weatherWarningCloseDome();
    warningActions.closeShutter = Options::weatherWarningCloseShutter();
    warningActions.delay = Options::weatherWarningDelay();
    alertActions.closeDome = Options::weatherAlertCloseDome();
    alertActions.closeShutter = Options::weatherAlertCloseShutter();
    alertActions.delay = Options::weatherAlertDelay();



    if (mWeather->status() != ISD::Weather::WEATHER_IDLE)
        emit ready();
}

ISD::Weather::Status ObservatoryWeatherModel::status()
{
    if (mWeather == nullptr)
        return ISD::Weather::WEATHER_IDLE;

    return mWeather->status();
}

void ObservatoryWeatherModel::setWarningActions(WeatherActions actions) {
    warningActions = actions;
    Options::setWeatherWarningCloseDome(actions.closeDome);
    Options::setWeatherWarningCloseShutter(actions.closeShutter);
    Options::setWeatherWarningDelay(actions.delay);
}

void ObservatoryWeatherModel::setAlertActions(WeatherActions actions) {
    alertActions = actions;
    Options::setWeatherAlertCloseDome(actions.closeDome);
    Options::setWeatherAlertCloseShutter(actions.closeShutter);
    Options::setWeatherAlertDelay(actions.delay);
}

} // Ekos
