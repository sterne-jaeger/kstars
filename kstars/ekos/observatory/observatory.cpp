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
    if (model != nullptr)
    {
        connect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        initDome();
    }
    else
    {
        shutdownDome();
        disconnect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
    }
}

void Observatory::initDome()
{
    domeBox->setEnabled(true);
    shutterBox->setEnabled(true);
    /*
    shutterClosed->setEnabled(true);
    shutterOpen->setEnabled(true);
    angleLabel->setEnabled(true);
    domeAngleSpinBox->setEnabled(true);
    setDomeAngleButton->setEnabled(true);
    */

}

void Observatory::shutdownDome()
{
    domeBox->setEnabled(false);
    shutterBox->setEnabled(false);
    domePark->setEnabled(false);
    domeUnpark->setEnabled(false);
    shutterClosed->setEnabled(false);
    shutterOpen->setEnabled(false);
    angleLabel->setEnabled(false);
    domeAngleSpinBox->setEnabled(false);
    setDomeAngleButton->setEnabled(false);
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{

    switch (status) {
    case ISD::Dome::DOME_ERROR:
        break;
    case ISD::Dome::DOME_IDLE:
        domePark->setChecked(false);
        domeUnpark->setChecked(true);
        break;
    case ISD::Dome::DOME_MOVING:
        break;
    case ISD::Dome::DOME_PARKED:
        domePark->setChecked(true);
        domeUnpark->setChecked(false);
        break;
    case ISD::Dome::DOME_PARKING:
        break;
    case ISD::Dome::DOME_UNPARKING:
        break;
    case ISD::Dome::DOME_TRACKING:
        break;
    default:
        break;
    }
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
        disconnect(model, &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
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
