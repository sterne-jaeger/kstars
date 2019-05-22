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
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Observatory")
    Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

public:
    Observatory();
    ObservatoryDomeModel *getDomeModel() { return mDomeModel; }
    ObservatoryWeatherModel *getWeatherModel() { return mWeatherModel; }

    // Logging
    QStringList logText() { return m_LogText; }
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

    // timer for refreshing the observatory status
    QTimer weatherStatusTimer;

    // reacting on weather changes
    void setWarningActions(WeatherActions actions);
    void setAlertActions(WeatherActions actions);

private slots:
    void initWeather();
    void shutdownWeather();
    void setWeatherStatus(ISD::Weather::Status status);

    // reacting on weather changes
    void weatherWarningSettingsChanged();
    void weatherAlertSettingsChanged();

    void initDome();
    void shutdownDome();

    void setDomeStatus(ISD::Dome::Status status);
    void setShutterStatus(ISD::Dome::ShutterStatus status);
};
}
