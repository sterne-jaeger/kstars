/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */
#include "observatorymodel.h"

namespace Ekos
{

void ObservatoryModel::initModel(Weather *weather)
{
    mWeatherModel = new ObservatoryWeatherModel();
    mWeatherModel->initModel(weather);
}

void ObservatoryModel::initModel(Dome *dome)
{
    mDomeModel = new ObservatoryDomeModel();
    mDomeModel->initModel(dome);
}

}
