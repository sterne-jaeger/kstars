/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indi/indiweather.h"
#include "observatoryweathermodel.h"
#include "observatorydomemodel.h"
#include "../auxiliary/weather.h"
#include "../auxiliary/dome.h"

namespace Ekos
{

typedef enum {
    OBSERVATORY_IDLE,
    OBSERVATORY_OPENING,
    OBSERVATORY_CLOSING,
    OBSERVATORY_READY,
    OBSERVATORY_CLOSED
} ObservatoryState;

class ObservatoryModel
{

public:
    ObservatoryModel() = default;
    ~ObservatoryModel() = default;

    void initModel(Weather * weather);
    void initModel(Dome * dome);

signals:
    void newLog(const QString &text);
    void newStatus(Ekos::ObservatoryState state);
    void weatherChanged(ISD::Weather::Status state);

private:
    ObservatoryWeatherModel *mWeatherModel;
    ObservatoryDomeModel *mDomeModel;

};
}
