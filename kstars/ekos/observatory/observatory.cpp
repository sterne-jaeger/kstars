/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatory.h"

namespace Ekos
{
Observatory::Observatory()
{
    setupUi(this);
    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
}

void Observatory::setDomeModel(ObservatoryDomeModel *model)
{
    mDomeModel = model;
}

void Observatory::setWeatherModel(ObservatoryWeatherModel *model)
{
    mWeatherModel = model;

    if (model != nullptr)
    {
        connect(model, &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
        initWeather();
    }
    else
    {
        shutdownWeather();
    }
}

void Observatory::initWeather()
{
    weatherBox->setEnabled(true);
    weatherLabel->setEnabled(true);
    setWeatherStatus(mWeatherModel->status());
}

void Observatory::shutdownWeather()
{
    weatherBox->setEnabled(false);
    weatherLabel->setEnabled(false);
    setWeatherStatus(ISD::Weather::WEATHER_IDLE);
}


void Observatory::setWeatherStatus(ISD::Weather::Status status)
{
    std::string label;
    switch (status) {
    case ISD::Weather::WEATHER_OK:
        label = "security-high";
        break;
    case ISD::Weather::WEATHER_WARNING:
        label = "security-medium";
        break;
    case ISD::Weather::WEATHER_ALERT:
        label = "security-low";
        break;
    default:
        label = "";
        break;
    }

    weatherStatusLabel->setPixmap(QIcon::fromTheme(label.c_str())
                                  .pixmap(QSize(48, 48)));
}

}
