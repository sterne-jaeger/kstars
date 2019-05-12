/*  Ekos Observatory Module

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_observatory.h"
#include "observatorymodel.h"

#include <QWidget>


namespace Ekos
{

class Observatory : public QWidget, public Ui::Observatory
{
public:
    Observatory();
    ObservatoryModel *getModel() { return mModel; }

private:
    ObservatoryModel *mModel;
    void setModel(ObservatoryModel * model);
};
}
