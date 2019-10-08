/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "kstarsdata.h"

#include "observatory.h"

#include "ekos_observatory_debug.h"

namespace Ekos
{
Observatory::Observatory()
{
    setupUi(this);

    // status control
    mObservatoryModel = new ObservatoryModel();
    setObseratoryStatusControl(mObservatoryModel->statusControl());
    // update UI for status control
    connect(useDomeCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useShutterCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useWeatherCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(mObservatoryModel, &Ekos::ObservatoryModel::newStatus, this, &Ekos::Observatory::observatoryStatusChanged);
    // ready button deactivated
    // connect(statusReadyButton, &QPushButton::clicked, mObservatoryModel, &Ekos::ObservatoryModel::makeReady);
    statusReadyButton->setEnabled(false);

    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
    statusDefinitionBox->setVisible(true);
    statusDefinitionBox->setEnabled(true);
    // make invisible, since not implemented yet
    weatherWarningSchedulerCB->setVisible(false);
    weatherAlertSchedulerCB->setVisible(false);
    // initialize the weather sensor data group box
    sensorDataBoxLayout = new QFormLayout();
    sensorData->setLayout(sensorDataBoxLayout);
    initSensorGraphs();
}

void Observatory::setObseratoryStatusControl(ObservatoryStatusControl control)
{
    if (mObservatoryModel != nullptr)
    {
        useDomeCB->setChecked(control.useDome);
        useShutterCB->setChecked(control.useShutter);
        useWeatherCB->setChecked(control.useWeather);
    }
}


void Observatory::setDomeModel(ObservatoryDomeModel *model)
{
    mObservatoryModel->setDomeModel(model);
    if (model != nullptr)
    {
        connect(model, &Ekos::ObservatoryDomeModel::ready, this, &Ekos::Observatory::initDome);
        connect(model, &Ekos::ObservatoryDomeModel::disconnected, this, &Ekos::Observatory::shutdownDome);
        connect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        connect(model, &Ekos::ObservatoryDomeModel::newParkStatus, this, &Ekos::Observatory::setDomeParkStatus);
        connect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
        connect(model, &Ekos::ObservatoryDomeModel::azimuthPositionChanged, this, &Ekos::Observatory::domeAzimuthChanged);
        connect(model, &Ekos::ObservatoryDomeModel::newAutoSyncStatus, this, &Ekos::Observatory::showAutoSync);

        // motion controls
        connect(motionMoveAbsButton, &QCheckBox::clicked, [this]()
        {
            mObservatoryModel->getDomeModel()->setAzimuthPosition(absoluteMotionSB->value());
        });

        connect(motionMoveRelButton, &QCheckBox::clicked, [this]()
        {
            mObservatoryModel->getDomeModel()->setRelativePosition(relativeMotionSB->value());
        });

        // abort button
        connect(motionAbortButton, &QPushButton::clicked, model, &ObservatoryDomeModel::abort);

        // weather controls
        connect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i);
            weatherWarningSettingsChanged();
        });

        connect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i);
            weatherAlertSettingsChanged();
        });
    }
    else
    {
        shutdownDome();


        disconnect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        disconnect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i);
            weatherWarningSettingsChanged();
        });

        disconnect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        disconnect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i);
            weatherWarningSettingsChanged();
        });
    }
}

void Observatory::initDome()
{
    domeBox->setEnabled(true);

    if (getDomeModel() != nullptr)
    {
        connect(getDomeModel(), &Ekos::ObservatoryDomeModel::newLog, this, &Ekos::Observatory::appendLogText);

        // dome motion buttons
        connect(motionCWButton, &QPushButton::clicked, [ = ](bool checked)
        {
            getDomeModel()->moveDome(true, checked);
        });
        connect(motionCCWButton, &QPushButton::clicked, [ = ](bool checked)
        {
            getDomeModel()->moveDome(false, checked);
        });

        if (getDomeModel()->canPark())
        {
            connect(domePark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::park);
            connect(domeUnpark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::unpark);
            domePark->setEnabled(true);
            domeUnpark->setEnabled(true);
        }
        else
        {
            domePark->setEnabled(false);
            domeUnpark->setEnabled(false);
        }

        if (getDomeModel()->isRolloffRoof())
        {
            SlavingBox->setVisible(false);
            domeAzimuthPosition->setText(i18nc("Not Applicable", "N/A"));
            enableMotionControl(true);
        }
        else
        {
            // initialize the dome motion controls
            domeAzimuthChanged(getDomeModel()->azimuthPosition());

            // slaving
            showAutoSync(getDomeModel()->isAutoSync());
            connect(slavingEnableButton, &QPushButton::clicked, this, [this]()
            {
                enableAutoSync(true);
            });
            connect(slavingDisableButton, &QPushButton::clicked, this, [this]()
            {
                enableAutoSync(false);
            });
        }

        // shutter handling
        if (getDomeModel()->hasShutter())
        {
            shutterBox->setVisible(true);
            shutterBox->setEnabled(true);
            connect(shutterOpen, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::openShutter);
            connect(shutterClosed, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::closeShutter);
            shutterClosed->setEnabled(true);
            shutterOpen->setEnabled(true);
            setShutterStatus(getDomeModel()->shutterStatus());
            useShutterCB->setVisible(true);
        }
        else
        {
            shutterBox->setVisible(false);
            weatherWarningShutterCB->setVisible(false);
            weatherAlertShutterCB->setVisible(false);
            useShutterCB->setVisible(false);
        }

        // abort button should always be available
        motionAbortButton->setEnabled(true);
        // update the dome parking status
        setDomeParkStatus(getDomeModel()->parkStatus());
    }

}

void Observatory::shutdownDome()
{
    domeBox->setEnabled(false);
    shutterBox->setEnabled(false);
    shutterBox->setVisible(false);
    domePark->setEnabled(false);
    domeUnpark->setEnabled(false);
    shutterClosed->setEnabled(false);
    shutterOpen->setEnabled(false);

    disconnect(domePark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::park);
    disconnect(domeUnpark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::unpark);
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting dome status to " << status;

    switch (status)
    {
        case ISD::Dome::DOME_ERROR:
            appendLogText(i18n("%1 error. See INDI log for details.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(false);
            break;

    case ISD::Dome::DOME_IDLE:
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);

            appendLogText(i18n("%1 is idle.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_MOVING_CW:
            motionCWButton->setChecked(true);
            motionCWButton->setEnabled(false);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);
            if (getDomeModel()->isRolloffRoof())
            {
                domeAzimuthPosition->setText(i18n("Opening"));
                toggleButtons(domeUnpark, i18n("Unparking"), domePark, i18n("Park"));
                appendLogText(i18n("Rolloff roof opening..."));
            }
            else
            {
                appendLogText(i18n("Dome is moving clockwise..."));
            }
            break;

        case ISD::Dome::DOME_MOVING_CCW:
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(true);
            motionCCWButton->setEnabled(false);
            if (getDomeModel()->isRolloffRoof())
            {
                domeAzimuthPosition->setText(i18n("Closing"));
                toggleButtons(domePark, i18n("Parking"), domeUnpark, i18n("Unpark"));
                appendLogText(i18n("Rolloff roof is closing..."));
            }
            else
            {
                appendLogText(i18n("Dome is moving counter clockwise..."));
            }
            break;

        case ISD::Dome::DOME_PARKED:
            setDomeParkStatus(ISD::PARK_PARKED);

            appendLogText(i18n("%1 is parked.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_PARKING:
            toggleButtons(domePark, i18n("Parking"), domeUnpark, i18n("Unpark"));
            motionCWButton->setEnabled(true);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Closing"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(true);

            appendLogText(i18n("%1 is parking...", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_UNPARKING:
            toggleButtons(domeUnpark, i18n("Unparking"), domePark, i18n("Park"));
            motionCCWButton->setEnabled(true);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Opening"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(true);
            motionCCWButton->setChecked(false);

            appendLogText(i18n("%1 is unparking...", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_TRACKING:
            enableMotionControl(true);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(true);
            appendLogText(i18n("%1 is tracking.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;
    }
}

void Observatory::setDomeParkStatus(ISD::ParkStatus status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting dome park status to " << status;
    switch (status)
    {
        case ISD::PARK_UNPARKED:
            activateButton(domePark, i18n("Park"));
            buttonPressed(domeUnpark, i18n("Unparked"));
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(false);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Open"));
            else
                enableMotionControl(true);
            break;

        case ISD::PARK_PARKED:
            buttonPressed(domePark, i18n("Parked"));
            activateButton(domeUnpark, i18n("Unpark"));
            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(false);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Closed"));
            else
                enableMotionControl(false);
            break;

        default:
            break;
    }
}


void Observatory::setShutterStatus(ISD::Dome::ShutterStatus status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting shutter status to " << status;

    switch (status)
    {
        case ISD::Dome::SHUTTER_OPEN:
            buttonPressed(shutterOpen, i18n("Opened"));
            activateButton(shutterClosed, i18n("Close"));
            appendLogText(i18n("Shutter is open."));
            break;

        case ISD::Dome::SHUTTER_OPENING:
            toggleButtons(shutterOpen, i18n("Opening"), shutterClosed, i18n("Close"));
            appendLogText(i18n("Shutter is opening..."));
            break;

        case ISD::Dome::SHUTTER_CLOSED:
            buttonPressed(shutterClosed, i18n("Closed"));
            activateButton(shutterOpen, i18n("Open"));
            appendLogText(i18n("Shutter is closed."));
            break;
        case ISD::Dome::SHUTTER_CLOSING:
            toggleButtons(shutterClosed, i18n("Closing"), shutterOpen, i18n("Open"));
            appendLogText(i18n("Shutter is closing..."));
            break;
        default:
            break;
    }
}




void Observatory::setWeatherModel(ObservatoryWeatherModel *model)
{
    mObservatoryModel->setWeatherModel(model);

    if (model != nullptr)
    {
        connect(weatherWarningBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setWarningActionsActive);
        connect(weatherAlertBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setAlertActionsActive);

        connect(model, &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);
        connect(model, &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
        connect(model, &Ekos::ObservatoryWeatherModel::disconnected, this, &Ekos::Observatory::shutdownWeather);
        connect(&weatherStatusTimer, &QTimer::timeout, [this]()
        {
            weatherWarningStatusLabel->setText(getWeatherModel()->getWarningActionsStatus());
            weatherAlertStatusLabel->setText(getWeatherModel()->getAlertActionsStatus());
        });
    }
    else
        shutdownWeather();
}

void Observatory::enableMotionControl(bool enabled)
{
    MotionBox->setEnabled(enabled);

    // absolute motion controls
    if (getDomeModel()->canAbsoluteMove())
    {
        motionMoveAbsButton->setEnabled(enabled);
        absoluteMotionSB->setEnabled(enabled);
    }
    else
    {
        motionMoveAbsButton->setEnabled(false);
        absoluteMotionSB->setEnabled(false);
    }

    // relative motion controls
    if (getDomeModel()->canRelativeMove())
    {
        motionMoveRelButton->setEnabled(enabled);
        relativeMotionSB->setEnabled(enabled);
        motionCWButton->setEnabled(enabled);
        motionCCWButton->setEnabled(enabled);
    }
    else
    {
        motionMoveRelButton->setEnabled(false);
        relativeMotionSB->setEnabled(false);
        motionCWButton->setEnabled(false);
        motionCCWButton->setEnabled(false);
    }

    // special case for rolloff roofs
    if (getDomeModel()->isRolloffRoof())
    {
        motionCWButton->setText(i18n("Open"));
        motionCCWButton->setText(i18n("Close"));
        motionCWButton->setEnabled(enabled);
        motionCCWButton->setEnabled(enabled);
        motionMoveAbsButton->setVisible(false);
        motionMoveRelButton->setVisible(false);
        absoluteMotionSB->setVisible(false);
        relativeMotionSB->setVisible(false);
    }
}

void Observatory::enableAutoSync(bool enabled)
{
    if (getDomeModel() == nullptr)
        showAutoSync(false);
    else
    {
        getDomeModel()->setAutoSync(enabled);
        showAutoSync(enabled);
    }
}

void Observatory::showAutoSync(bool enabled)
{
    slavingEnableButton->setChecked(enabled);
    slavingDisableButton->setChecked(! enabled);
}

void Observatory::initWeather()
{
    weatherBox->setEnabled(true);
    weatherLabel->setEnabled(true);
    weatherActionsBox->setVisible(true);
    weatherActionsBox->setEnabled(true);
    weatherWarningBox->setChecked(getWeatherModel()->getWarningActionsActive());
    weatherAlertBox->setChecked(getWeatherModel()->getAlertActionsActive());
    setWeatherStatus(getWeatherModel()->status());
    setWarningActions(getWeatherModel()->getWarningActions());
    setAlertActions(getWeatherModel()->getAlertActions());
    weatherStatusTimer.start(1000);
}

void Observatory::shutdownWeather()
{
    weatherBox->setEnabled(false);
    weatherLabel->setEnabled(false);
    setWeatherStatus(ISD::Weather::WEATHER_IDLE);
    weatherStatusTimer.stop();
}


void Observatory::updateSensorGraphs(QString label, QDateTime now, double value)
{
    // we assume that labels are unique and use the full label as identifier
    QString id = label;

    if (sensorDataUnits.count(label) == 0)
    {
        QString unit = "none";
        int brac_open  = label.lastIndexOf("(");
        int brac_close = label.lastIndexOf(")");
        if (brac_open+1 < brac_close && brac_open > 0)
            unit = label.mid(brac_open+1, brac_close-brac_open-1);
        sensorDataUnits[id] = unit;
        appendLogText(i18n("New sensor '%1' detected - unit = '%2'.", label, sensorDataUnits[id]));

        // create a new temperature graph
        if (sensorDataUnits[id] == "C")
        {
            QCPGraph *graph = sensorGraphs->addGraph();
            if (brac_open > 0)
                graph->setName(label.left(brac_open-1).trimmed());
            else
                graph->setName(label);

            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::black, 1.5), QBrush(Qt::white), 9));
            graph->setPen(QPen(QColor(120, 120, 120), 2));
            sensorDataGraphs[id] = graph;

            sensorGraphs->xAxis->setRange(now.toTime_t(), now.toTime_t()+120);
            appendLogText(i18n("New temperature sensor '%1' detected.", label));
        }
    }

    // add data for the graphs we display
    if (sensorDataGraphs[id] != nullptr)
    {
        sensorDataGraphs[id]->addData(static_cast<double>(now.toTime_t()), value);
        sensorGraphs->replot();
        sensorGraphs->rescaleAxes();
    }
}

void Observatory::updateSensorData(std::vector<ISD::Weather::WeatherData> weatherData)
{
    std::vector<ISD::Weather::WeatherData>::iterator it;
    QDateTime now = KStarsData::Instance()->lt();

    for (it=weatherData.begin(); it != weatherData.end(); ++it)
    {
        QString id = it->label;

        if (sensorDataWidgets[id] == nullptr)
        {
            QLabel* labelWidget = new QLabel(it->label);
            QLineEdit* valueWidget = new QLineEdit(QString().setNum(it->value, 'f', 2));
            valueWidget->setReadOnly(false);
            valueWidget->setAlignment(Qt::AlignRight);

            sensorDataWidgets[id] = new QPair<QLabel*, QLineEdit*>(labelWidget, valueWidget);

            sensorDataBoxLayout->addRow(labelWidget, valueWidget);
         }
        else
        {
            sensorDataWidgets[id]->first->setText(QString(it->label));
            sensorDataWidgets[id]->second->setText(QString().setNum(it->value, 'f', 2));
        }

        // store sensor data unit if necessary
        updateSensorGraphs(it->label, now, it->value);
    }
}

void Observatory::setWeatherStatus(ISD::Weather::Status status)
{
    std::string label;
    switch (status)
    {
        case ISD::Weather::WEATHER_OK:
            label = "security-high";
            appendLogText(i18n("Weather is OK"));
            break;
        case ISD::Weather::WEATHER_WARNING:
            label = "security-medium";
            appendLogText(i18n("Weather Warning"));
            break;
        case ISD::Weather::WEATHER_ALERT:
            label = "security-low";
            appendLogText(i18n("Weather Alert"));
            break;
        default:
            label = "";
            break;
    }

    weatherStatusLabel->setPixmap(QIcon::fromTheme(label.c_str()).pixmap(QSize(48, 48)));

    std::vector<ISD::Weather::WeatherData> weatherData = getWeatherModel()->getWeatherData();

    // update weather sensor data
    updateSensorData(weatherData);

}

void Observatory::initSensorGraphs()
{
    // set some pens, brushes and backgrounds:
    sensorGraphs->xAxis->setBasePen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setBasePen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setTickPen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setTickPen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setSubTickPen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setSubTickPen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setTickLabelColor(Qt::white);
    sensorGraphs->yAxis->setTickLabelColor(Qt::white);
    sensorGraphs->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    sensorGraphs->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    sensorGraphs->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    sensorGraphs->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    sensorGraphs->xAxis->grid()->setSubGridVisible(true);
    sensorGraphs->yAxis->grid()->setSubGridVisible(true);
    sensorGraphs->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    sensorGraphs->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    sensorGraphs->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    sensorGraphs->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(80, 80, 80));
    plotGradient.setColorAt(1, QColor(50, 50, 50));
    sensorGraphs->setBackground(plotGradient);
    QLinearGradient axisRectGradient;
    axisRectGradient.setStart(0, 0);
    axisRectGradient.setFinalStop(0, 350);
    axisRectGradient.setColorAt(0, QColor(80, 80, 80));
    axisRectGradient.setColorAt(1, QColor(30, 30, 30));
    sensorGraphs->axisRect()->setBackground(axisRectGradient);

    sensorGraphs->yAxis->setLabel("C");
    sensorGraphs->yAxis->setLabelColor(Qt::white);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm");
    dateTicker->setTickCount(2);
    sensorGraphs->xAxis->setTicker(dateTicker);

    // allow dragging in all directions
    sensorGraphs->setInteraction(QCP::iRangeDrag, true);
    sensorGraphs->setInteraction(QCP::iRangeZoom);
}


void Observatory::weatherWarningSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherWarningDomeCB->isChecked();
    actions.closeShutter = weatherWarningShutterCB->isChecked();
    actions.delay = static_cast<unsigned int>(weatherWarningDelaySB->value());

    getWeatherModel()->setWarningActions(actions);
}

void Observatory::weatherAlertSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherAlertDomeCB->isChecked();
    actions.closeShutter = weatherAlertShutterCB->isChecked();
    actions.delay = static_cast<unsigned int>(weatherAlertDelaySB->value());

    getWeatherModel()->setAlertActions(actions);
}

void Observatory::observatoryStatusChanged(bool ready)
{
    // statusReadyButton->setEnabled(!ready);
    statusReadyButton->setChecked(ready);
    emit newStatus(ready);
}

void Observatory::domeAzimuthChanged(double position)
{
    domeAzimuthPosition->setText(QString::number(position, 'f', 2));
}


void Observatory::setWarningActions(WeatherActions actions)
{
    weatherWarningDomeCB->setChecked(actions.parkDome);
    weatherWarningShutterCB->setChecked(actions.closeShutter);
    weatherWarningDelaySB->setValue(static_cast<int>(actions.delay));
}


void Observatory::setAlertActions(WeatherActions actions)
{
    weatherAlertDomeCB->setChecked(actions.parkDome);
    weatherAlertShutterCB->setChecked(actions.closeShutter);
    weatherAlertDelaySB->setValue(static_cast<int>(actions.delay));
}

void Observatory::toggleButtons(QPushButton *buttonPressed, QString titlePressed, QPushButton *buttonCounterpart, QString titleCounterpart)
{
    buttonPressed->setEnabled(false);
    buttonPressed->setText(titlePressed);

    buttonCounterpart->setEnabled(true);
    buttonCounterpart->setChecked(false);
    buttonCounterpart->setCheckable(false);
    buttonCounterpart->setText(titleCounterpart);
}

void Observatory::activateButton(QPushButton *button, QString title)
{
    button->setEnabled(true);
    button->setCheckable(false);
    button->setText(title);
}

void Observatory::buttonPressed(QPushButton *button, QString title)
{
    button->setEnabled(false);
    button->setCheckable(true);
    button->setChecked(true);
    button->setText(title);

}


void Observatory::statusControlSettingsChanged()
{
    ObservatoryStatusControl control;
    control.useDome = useDomeCB->isChecked();
    control.useShutter = useShutterCB->isChecked();
    control.useWeather = useWeatherCB->isChecked();
    mObservatoryModel->setStatusControl(control);
}


void Observatory::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_OBSERVATORY) << text;

    emit newLog(text);
}

void Observatory::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

}
