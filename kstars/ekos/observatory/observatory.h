/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_observatory.h"
#include "observatorydomemodel.h"
#include "observatoryweathermodel.h"
#include "indiweather.h"

#include <QWidget>
#include <QObject>
#include "klocalizedstring.h"


namespace Ekos
{

class Observatory : public QWidget, public Ui::Observatory
{
    Q_OBJECT

public:
    Observatory();
    ObservatoryDomeModel *getDomeModel() { return mDomeModel; }
    ObservatoryWeatherModel *getWeatherModel() { return mWeatherModel; }

    // Logging
    QString getLogText() { return m_LogText.join("\n"); }

signals:
    Q_SCRIPTABLE void newLog(const QString &text);

private:
    ObservatoryDomeModel *mDomeModel;
    void setDomeModel(ObservatoryDomeModel *model);

    ObservatoryWeatherModel *mWeatherModel;
    void setWeatherModel(ObservatoryWeatherModel *model);

    // Logging
    QStringList m_LogText;
    void appendLogText(const QString &);
    void clearLog();

private slots:
    void initWeather();
    void shutdownWeather();
    void setWeatherStatus(ISD::Weather::Status status);

    void initDome();
    void shutdownDome();

    void setDomeStatus(ISD::Dome::Status status);
    void setShutterStatus(ISD::Dome::ShutterStatus status);
};
}
