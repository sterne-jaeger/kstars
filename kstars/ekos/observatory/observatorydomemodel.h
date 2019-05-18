/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "../auxiliary/dome.h"

#include <QObject>


namespace Ekos
{

class ObservatoryDomeModel: public QObject
{

    Q_OBJECT

public:
    ObservatoryDomeModel() = default;

    void initModel(Dome *dome);

    ISD::Dome::Status status();

    // proxies to the underlying dome object
    bool canPark() { return (mDome != nullptr && mDome->canPark()); }
    void park();
    void unpark();

private:
    Dome *mDome;

signals:
    void newStatus(ISD::Dome::Status state);
    void ready();
    void newLog(const QString &text);
};

}
