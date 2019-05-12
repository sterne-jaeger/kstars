/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/dome.h"


namespace Ekos
{

class ObservatoryDomeModel
{
public:
    ObservatoryDomeModel() = default;

    void initModel(Dome *dome);

private:
    Dome *mDome;
};

}
