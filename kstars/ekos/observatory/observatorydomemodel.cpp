/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatorydomemodel.h"

namespace Ekos
{

void ObservatoryDomeModel::initModel(Dome *dome)
{
    mDome = dome;

    connect(mDome, &Dome::ready, this, &ObservatoryDomeModel::ready);
    connect(mDome, &Dome::newStatus, this, &ObservatoryDomeModel::newStatus);

}


ISD::Dome::Status ObservatoryDomeModel::status()
{
    if (mDome == nullptr)
        return ISD::Dome::DOME_IDLE;

    return mDome->status();
}

}
