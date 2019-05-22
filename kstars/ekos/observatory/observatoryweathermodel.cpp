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
    connect(mWeather, &Weather::newStatus, this, &ObservatoryWeatherModel::weatherChanged);
    connect(mWeather, &Weather::disconnected, this, &ObservatoryWeatherModel::disconnected);

    // read the default values
    warningActions.parkDome = Options::weatherWarningCloseDome();
    warningActions.closeShutter = Options::weatherWarningCloseShutter();
    warningActions.delay = Options::weatherWarningDelay();
    alertActions.parkDome = Options::weatherAlertCloseDome();
    alertActions.closeShutter = Options::weatherAlertCloseShutter();
    alertActions.delay = Options::weatherAlertDelay();

    warningTimer.setInterval(warningActions.delay * 60 * 1000);
    warningTimer.setSingleShot(true);
    alertTimer.setInterval(alertActions.delay * 60 * 1000);
    alertTimer.setSingleShot(true);

    connect(&warningTimer, &QTimer::timeout, [this]() { execute(warningActions); });
    connect(&alertTimer, &QTimer::timeout, [this]() { execute(alertActions); });

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
    Options::setWeatherWarningCloseDome(actions.parkDome);
    Options::setWeatherWarningCloseShutter(actions.closeShutter);
    Options::setWeatherWarningDelay(actions.delay);
    warningTimer.setInterval(actions.delay * 60 * 1000);
}


QString ObservatoryWeatherModel::getWarningActionsStatus()
{
    if (warningTimer.isActive())
    {
        QString status;
        int remaining = warningTimer.remainingTime()/1000;
        return status.sprintf("%02d:%02d remaining", remaining/60, remaining%60);
    }

    return "Status: inactive";
}

QString ObservatoryWeatherModel::getAlertActionsStatus()
{
    if (alertTimer.isActive())
    {
        QString status;
        int remaining = alertTimer.remainingTime()/1000;
        return status.sprintf("%02d:%02d remaining", remaining/60, remaining%60);
    }

    return "Status: inactive";
}

void ObservatoryWeatherModel::setAlertActions(WeatherActions actions) {
    alertActions = actions;
    Options::setWeatherAlertCloseDome(actions.parkDome);
    Options::setWeatherAlertCloseShutter(actions.closeShutter);
    Options::setWeatherAlertDelay(actions.delay);
    alertTimer.setInterval(actions.delay * 60 * 1000);
}


void ObservatoryWeatherModel::weatherChanged(ISD::Weather::Status status)
{
    switch (status) {
    case ISD::Weather::WEATHER_OK:
        warningTimer.stop();
        alertTimer.stop();
        break;
    case ISD::Weather::WEATHER_WARNING:
        warningTimer.start();
        alertTimer.stop();
        break;
    case ISD::Weather::WEATHER_ALERT:
        warningTimer.stop();
        alertTimer.start();
        break;
    default:
        break;
    }
    emit newStatus(status);
}


} // Ekos
