/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatorymodel.h"
#include "Options.h"

namespace Ekos
{

QPointer<QDBusInterface> schedulerInterface { nullptr };


ObservatoryModel::ObservatoryModel()
{

    mStatusControl.useDome = Options::observatoryStatusUseDome();
    mStatusControl.useShutter = Options::observatoryStatusUseShutter();
    mStatusControl.useWeather = Options::observatoryStatusUseWeather();

    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
    // Set up DBus interfaces
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Observatory", this);
    // create D-Bus interface to the Scheduler module
    schedulerInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Scheduler", "org.kde.kstars.Ekos.Scheduler",
                                       QDBusConnection::sessionBus(), this);
    // listen on D-Bus for newStatus messages from the Scheduler module
    QDBusConnection::sessionBus().connect("org.kde.kstars", "/KStars/Ekos/Scheduler", "org.kde.kstars.Ekos.Scheduler",
                                          "newStatus", this, SLOT(setSchedulerStatus(Ekos::SchedulerState)));
}

void ObservatoryModel::setDomeModel(ObservatoryDomeModel *model) {
    mDomeModel = model;
    if (model != nullptr)
    {
        // listen to status changes for dome and shutter
        connect(mDomeModel, &ObservatoryDomeModel::newStatus, [this](ISD::Dome::Status s) { Q_UNUSED(s); updateStatus(); });
        connect(mDomeModel, &ObservatoryDomeModel::newShutterStatus, [this](ISD::Dome::ShutterStatus s) { Q_UNUSED(s); updateStatus(); });

        // listen to weather status changes
        if (mWeatherModel != nullptr)
            connect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }

    updateStatus();
}

void ObservatoryModel::setWeatherModel(ObservatoryWeatherModel *model) {
    mWeatherModel = model;
    if (model != nullptr)
    {
        connect(mWeatherModel, &ObservatoryWeatherModel::newStatus, [this](ISD::Weather::Status s) { Q_UNUSED(s); updateStatus(); });
        connect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }
    else
    {
        disconnect(mWeatherModel, &ObservatoryWeatherModel::execute, this, &ObservatoryModel::execute);
    }
    updateStatus();
}


void ObservatoryModel::setStatusControl(ObservatoryStatusControl control)
{
    mStatusControl = control;
    Options::setObservatoryStatusUseDome(control.useDome);
    Options::setObservatoryStatusUseShutter(control.useShutter);
    Options::setObservatoryStatusUseWeather(control.useWeather);
    updateStatus();
}

bool ObservatoryModel::isReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->status() != ISD::Dome::DOME_IDLE))
        return false;

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        return false;

    // weather relevant for the status and weather is OK
    if (mStatusControl.useWeather && (getWeatherModel() == nullptr || getWeatherModel()->status() != ISD::Weather::WEATHER_OK))
        return false;

    return true;
}

ObservatoryModel::Status ObservatoryModel::getStatus()
{
    if (isReady())
        return STATUS_READY;

    else if (mStatusControl.useWeather)
    {
        if (getWeatherModel() == nullptr)
            return STATUS_IDLE;
        else if (getWeatherModel()->status() == ISD::Weather::WEATHER_WARNING)
            return STATUS_WARNING;
        else if (getWeatherModel()->status() == ISD::Weather::WEATHER_ALERT)
            return STATUS_ALERT;
    }
    // default case
    return STATUS_IDLE;

}

void ObservatoryModel::execute(ISD::Weather::Status status)
{
    switch (status) {
    case ISD::Weather::WEATHER_WARNING:
        // call back via setSchedulerStatus()
        schedulerInterface->call(QDBus::AutoDetect, "pause");
        mExecStatus = EXEC_RUNNING;
        break;
    case ISD::Weather::WEATHER_ALERT:
        // call back via setSchedulerStatus()
        schedulerInterface->call(QDBus::AutoDetect, "stop");
        mExecStatus = EXEC_RUNNING;
    default:
        break;
    }
}

void ObservatoryModel::setSchedulerStatus(SchedulerState state)
{
    // do nothing if no warning or alert action has been triggered
    if (mExecStatus == EXEC_IDLE)
        return;

    // handle the edge cases
    if (getWeatherModel() == nullptr || getDomeModel() == nullptr)
        return;

    // scheduler has reached the paused stage
    // now its time to call the respective actions of the dome
    switch (getStatus()) {
    case STATUS_WARNING:
        // for warnings the scheduler should be paused
        if (state == SCHEDULER_PAUSED)
        {
            getDomeModel()->execute(getWeatherModel()->getWarningActions());
            // executions completed
            mExecStatus = EXEC_IDLE;
        }
        break;
    case STATUS_ALERT:
        // for alerts the scheduler should be shut down
        if (state == SCHEDULER_IDLE || state == SCHEDULER_ABORTED)
        {
            getDomeModel()->execute(getWeatherModel()->getAlertActions());
            // executions completed
            mExecStatus = EXEC_IDLE;
        }
    default:
        // in all other cases: do nothing
        break;
    }

}


void ObservatoryModel::updateStatus()
{
    emit newStatus(getStatus());
}

void ObservatoryModel::makeReady()
{
    // dome relevant for the status and dome is ready
    if (mStatusControl.useDome && (getDomeModel() == nullptr || getDomeModel()->status() != ISD::Dome::DOME_IDLE))
        getDomeModel()->unpark();

    // shutter relevant for the status and shutter open
    if (mStatusControl.useShutter && (getDomeModel() == nullptr ||
                                      (getDomeModel()->hasShutter() && getDomeModel()->shutterStatus() != ISD::Dome::SHUTTER_OPEN)))
        getDomeModel()->openShutter();

    // weather relevant for the status and weather is OK
    // Haha, weather we can't change
}

}
