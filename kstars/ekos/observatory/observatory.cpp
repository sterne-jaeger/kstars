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
    setModel(new ObservatoryModel());
}

void Observatory::setModel(ObservatoryModel *model)
{
    mModel = model;
}

}
