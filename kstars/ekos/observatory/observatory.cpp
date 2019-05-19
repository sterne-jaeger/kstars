/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatory.h"

#include "ekos_observatory_debug.h"

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
        connect(model, &Ekos::ObservatoryDomeModel::ready, this, &Ekos::Observatory::initDome);
        connect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        connect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
    }
    else
    {
        shutdownDome();
        disconnect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
        disconnect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        disconnect(model, &Ekos::ObservatoryDomeModel::ready, this, &Ekos::Observatory::initDome);
    }
}

void Observatory::initDome()
{
    domeBox->setEnabled(true);

    if (mDomeModel != nullptr)
    {
        connect(mDomeModel, &Ekos::ObservatoryDomeModel::newLog, this, &Ekos::Observatory::appendLogText);

        switch (mDomeModel->status()) {
        case ISD::Dome::DOME_PARKED:
            showDomeParked(true);
            break;
        case ISD::Dome::DOME_IDLE:
            showDomeParked(false);
            break;
        default:
            break;
        }

        if (mDomeModel->canPark())
        {
            connect(domePark, &QPushButton::clicked, mDomeModel, &Ekos::ObservatoryDomeModel::park);
            connect(domeUnpark, &QPushButton::clicked, mDomeModel, &Ekos::ObservatoryDomeModel::unpark);
            domePark->setEnabled(true);
            domeUnpark->setEnabled(true);
        }
        else
        {
            domePark->setEnabled(false);
            domeUnpark->setEnabled(false);
        }

        if (mDomeModel->hasShutter())
        {
            shutterBox->setVisible(true);
        }
        else
        {
            shutterBox->setVisible(false);
        }

        setShutterStatus(mDomeModel->shutterStatus());

        // shutter control not implemented yet
        shutterClosed->setEnabled(false);
        shutterOpen->setEnabled(false);
    }

    // make invisible, since not implemented yet
    angleLabel->setVisible(false);
    domeAngleSpinBox->setVisible(false);
    setDomeAngleButton->setVisible(false);
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

    disconnect(domePark, &QPushButton::clicked, mDomeModel, &Ekos::ObservatoryDomeModel::park);
    disconnect(domeUnpark, &QPushButton::clicked, mDomeModel, &Ekos::ObservatoryDomeModel::unpark);
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{

    switch (status) {
    case ISD::Dome::DOME_ERROR:
        break;
    case ISD::Dome::DOME_IDLE:
        appendLogText("Dome is unparked.");
        showDomeParked(false);
        break;
    case ISD::Dome::DOME_MOVING:
        appendLogText("Dome is moving...");
        break;
    case ISD::Dome::DOME_PARKED:
        appendLogText("Dome is parked.");
        showDomeParked(true);
        break;
    case ISD::Dome::DOME_PARKING:
        appendLogText("Dome is parking...");
        break;
    case ISD::Dome::DOME_UNPARKING:
        appendLogText("Dome is unparking...");
        break;
    case ISD::Dome::DOME_TRACKING:
        appendLogText("Dome is tracking.");
        break;
    default:
        break;
    }
}

void Observatory::showDomeParked(bool parked)
{
    domePark->setChecked(parked);
    domePark->setText(parked ? "PARKED" : "PARK");
    domeUnpark->setChecked(!parked);
    domeUnpark->setText(parked ? "UNPARK" : "UNPARKED");
}

void Observatory::setDomeParked(bool parked)
{
    showDomeParked(parked);

}

void Observatory::setShutterStatus(ISD::Dome::ShutterStatus status)
{
    switch (status) {
    case ISD::Dome::SHUTTER_OPEN:
        shutterOpen->setChecked(true);
        shutterClosed->setChecked(false);
        shutterOpen->setText("OPEN");
        appendLogText("Shutter is open.");
        break;
    case ISD::Dome::SHUTTER_OPENING:
        shutterOpen->setText("OPENING");
        appendLogText("Shutter is opening...");
        break;
    case ISD::Dome::SHUTTER_CLOSED:
        shutterOpen->setChecked(false);
        shutterClosed->setChecked(true);
        shutterClosed->setText("CLOSED");
        appendLogText("Shutter is closed.");
        break;
    case ISD::Dome::SHUTTER_CLOSING:
        shutterClosed->setText("CLOSING");
        appendLogText("Shutter is closing...");
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
        appendLogText("Weather is OK.");
        break;
    case ISD::Weather::WEATHER_WARNING:
        label = "security-medium";
        appendLogText("Weather WARNING!");
        break;
    case ISD::Weather::WEATHER_ALERT:
        label = "security-low";
        appendLogText("!! WEATHER ALERT !!");
        break;
    default:
        label = "";
        break;
    }

    weatherStatusLabel->setPixmap(QIcon::fromTheme(label.c_str())
                                  .pixmap(QSize(48, 48)));
}


void Observatory::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_OBSERVATORY) << text;

    emit newLog(text);
}

void Observatory::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

}
