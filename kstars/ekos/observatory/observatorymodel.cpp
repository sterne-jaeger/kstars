/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatorymodel.h"
#include "Options.h"

namespace Ekos
{

ObservatoryModel::ObservatoryModel()
{

    mStatusControl.useDome = Options::observatoryStatusUseDome();
    mStatusControl.useShutter = Options::observatoryStatusUseShutter();
    mStatusControl.useWeather = Options::observatoryStatusUseWeather();

    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
}

void ObservatoryModel::setDomeModel(ObservatoryDomeModel *model) {
    mDomeModel = model;
    if (model != nullptr)
    {
        connect(mDomeModel, &ObservatoryDomeModel::newStatus, [this](ISD::Dome::Status s) { Q_UNUSED(s); updateStatus(); });
        connect(mDomeModel, &ObservatoryDomeModel::newShutterStatus, [this](ISD::Dome::ShutterStatus s) { Q_UNUSED(s); updateStatus(); });
        if (mWeatherModel != nullptr)
            connect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }
    else
    {
        if (mWeatherModel != nullptr)
            disconnect(mWeatherModel, &ObservatoryWeatherModel::execute, mDomeModel, &ObservatoryDomeModel::execute);
    }

    updateStatus();
}

void ObservatoryModel::setWeatherModel(ObservatoryWeatherModel *model) {
    mWeatherModel = model;
    if (model != nullptr)
    {
        connect(mWeatherModel, &ObservatoryWeatherModel::newStatus, [this](ISD::Weather::Status s) { Q_UNUSED(s); updateStatus(); });
        connect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }
    else
    {
        disconnect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }
    updateStatus();
}


void ObservatoryModel::setStatusControl(ObservatoryStatusControl control)
{
    mStatusControl = control;
    Options::setObservatoryStatusUseDome(control.useDome);
    Options::setObservatoryStatusUseShutter(control.useShutter);
    Options::setObservatoryStatusUseWeather(control.useWeather);
    updateStatus();
}

bool ObservatoryModel::isReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->status() != ISD::Dome::DOME_IDLE))
        return false;

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        return false;

    // weather relevant for the status and weather is OK
    if (mStatusControl.useWeather && (getWeatherModel() == nullptr || getWeatherModel()->status() != ISD::Weather::WEATHER_OK))
        return false;

    return true;
}

ObservatoryModel::Status ObservatoryModel::getStatus()
{
    if (isReady())
        return STATUS_READY;

    else if (mStatusControl.useWeather)
    {
        if (getWeatherModel() == nullptr)
            return STATUS_IDLE;
        else if (getWeatherModel()->status() == ISD::Weather::WEATHER_WARNING)
            return STATUS_WARNING;
        else if (getWeatherModel()->status() == ISD::Weather::WEATHER_ALERT)
            return STATUS_ALERT;
    }
    // default case
    return STATUS_IDLE;

}

void ObservatoryModel::execute(WeatherActions actions)
{
    // pas through the dome model
    if (getDomeModel() != nullptr)
        getDomeModel()->execute(actions);
}

void ObservatoryModel::updateStatus()
{
    emit newStatus(getStatus());
}

void ObservatoryModel::makeReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->status() != ISD::Dome::DOME_IDLE))
        getDomeModel()->unpark();

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        getDomeModel()->openShutter();

    // weather relevant for the status and weather is OK
    // Haha, weather we can't change
}

}
