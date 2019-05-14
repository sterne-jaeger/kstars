/*  Ekos Observatory Module

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


namespace Ekos
{

class Observatory : public QWidget, public Ui::Observatory
{
public:
    Observatory();
    ObservatoryDomeModel *getDomeModel() { return mDomeModel; }
    ObservatoryWeatherModel *getWeatherModel() { return mWeatherModel; }

private:
    ObservatoryDomeModel *mDomeModel;
    void setDomeModel(ObservatoryDomeModel *model);

    ObservatoryWeatherModel *mWeatherModel;
    void setWeatherModel(ObservatoryWeatherModel *model);

private slots:
    void initWeather();
    void shutdownWeather();
    void setWeatherStatus(ISD::Weather::Status status);

    void initDome();
    void shutdownDome();
    void setDomeStatus(ISD::Dome::Status status);
};
}
