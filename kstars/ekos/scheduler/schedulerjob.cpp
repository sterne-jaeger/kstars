/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "schedulerjob.h"

#include "dms.h"
#include "kstarsdata.h"
#include "scheduler.h"
#include "Options.h"
#include "ekos/capture/sequencejob.h"

#include <ekos_scheduler_debug.h>

#include <knotification.h>

#include <QTableWidgetItem>

namespace Ekos
{
void SchedulerJob::setName(const QString &value)
{
    name = value;
    updateJobCell();
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;
    if (value == START_ASAP)
        startupTime = QDateTime();
    updateJobCell();
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;

    if (value.isValid())
        startupCondition = START_AT;

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);
}

void SchedulerJob::setSequenceFile(const QUrl &value)
{
    sequenceFile = value;
}

void SchedulerJob::setFITSFile(const QUrl &value)
{
    fitsFile = value;
}

void SchedulerJob::setMinAltitude(const double &value)
{
    minAltitude = value;
}

void SchedulerJob::setMinMoonSeparation(const double &value)
{
    minMoonSeparation = value;
}

void SchedulerJob::setEnforceWeather(bool value)
{
    enforceWeather = value;
}

void SchedulerJob::setCompletionTime(const QDateTime &value)
{
    /* If argument completion time is valid, automatically switch condition to FINISH_AT */
    if (value.isValid())
    {
        setCompletionCondition(FINISH_AT);
        completionTime = value;
    }
    /* If completion time is not valid, but startup time is, deduce completion from startup and duration */
    else if (startupTime.isValid())
    {
        completionTime = startupTime.addSecs(estimatedTime);
    }

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;
    updateJobCell();
}

void SchedulerJob::setStepPipeline(const StepPipeline &value)
{
    stepPipeline = value;
}

void SchedulerJob::setState(const JOBStatus &value)
{
    state = value;

    /* FIXME: move this to Scheduler, SchedulerJob is mostly a model */
    if (JOB_ERROR == state)
        KNotification::event(QLatin1String("EkosSchedulerJobFail"), i18n("Ekos job failed (%1)", getName()));

    /* If job becomes invalid, automatically reset its startup characteristics, and force its duration to be reestimated */
    if (JOB_INVALID == value)
    {
        setStartupCondition(fileStartupCondition);
        setStartupTime(fileStartupTime);
        setEstimatedTime(-1);
    }

    /* If job is aborted, automatically reset its startup characteristics */
    if (JOB_ABORTED == value)
    {
        setStartupCondition(fileStartupCondition);
        /* setStartupTime(fileStartupTime); */
    }

    updateJobCell();
}

void SchedulerJob::setScore(int value)
{
    score = value;
    updateJobCell();
}

void SchedulerJob::setCulminationOffset(const int16_t &value)
{
    culminationOffset = value;
}

void SchedulerJob::setSequenceCount(const int count)
{
    sequenceCount = count;
    updateJobCell();
}

void SchedulerJob::setNameCell(QTableWidgetItem *value)
{
    nameCell = value;
    updateJobCell();
}

void SchedulerJob::setCompletedCount(const int count)
{
    completedCount = count;
    updateJobCell();
}

void SchedulerJob::setStatusCell(QTableWidgetItem *value)
{
    statusCell = value;
    updateJobCell();
    if (statusCell)
        statusCell->setToolTip(i18n("Current status of job '%1', managed by the Scheduler.\n"
                                    "If invalid, the Scheduler was not able to find a proper observation time for the target.\n"
                                    "If aborted, the Scheduler missed the scheduled time or encountered transitory issues and will reschedule the job.\n"
                                    "If complete, the Scheduler verified that all sequence captures requested were stored, including repeats.",
                                    name));
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
    updateJobCell();
    if (startupCell)
        startupCell->setToolTip(i18n("Startup time for job '%1', as estimated by the Scheduler.\n"
                                     "Fixed time from user or culmination time is marked with a chronometer symbol. ",
                                     name));
}

void SchedulerJob::setCompletionCell(QTableWidgetItem *value)
{
    completionCell = value;
    updateJobCell();
    if (completionCell)
        completionCell->setToolTip(i18n("Completion time for job '%1', as estimated by the Scheduler.\n"
                                        "Can be specified by the user to limit duration of looping jobs.\n"
                                        "Fixed time from user is marked with a chronometer symbol. ",
                                        name));
}

void SchedulerJob::setCaptureCountCell(QTableWidgetItem *value)
{
    captureCountCell = value;
    updateJobCell();
    if (captureCountCell)
        captureCountCell->setToolTip(i18n("Count of captures stored for job '%1', based on its sequence job.\n"
                                          "This is a summary, additional specific frame types may be required to complete the job.",
                                          name));
}

void SchedulerJob::setScoreCell(QTableWidgetItem *value)
{
    scoreCell = value;
    updateJobCell();
    if (scoreCell)
        scoreCell->setToolTip(i18n("Current score for job '%1', from its altitude, moon separation and sky darkness.\n"
                                   "Negative if adequate altitude is not achieved yet or if there is no proper observation time today.\n"
                                   "The Scheduler will refresh scores when picking a new candidate job.",
                                   name));
}

void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
    updateJobCell();
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;
    updateJobCell();
}

void SchedulerJob::setStageCell(QTableWidgetItem *cell)
{
    stageCell = cell;
    updateJobCell();
}

void SchedulerJob::setStageLabel(QLabel *label)
{
    stageLabel = label;
    updateJobCell();
}

void SchedulerJob::setFileStartupCondition(const StartupCondition &value)
{
    fileStartupCondition = value;
}

void SchedulerJob::setFileStartupTime(const QDateTime &value)
{
    fileStartupTime = value;
}

void SchedulerJob::setEstimatedTime(const int64_t &value)
{
    /* If startup and completion times are fixed, estimated time cannot change */
    if (START_AT == startupCondition && FINISH_AT == completionCondition)
    {
        estimatedTime = startupTime.secsTo(completionTime);
    }
    /* If startup time is fixed but not completion time, estimated time adjusts completion time */
    else if (START_AT == startupCondition)
    {
        estimatedTime = value;
        completionTime = startupTime.addSecs(value);
    }
    /* If completion time is fixed but not startup time, estimated time adjusts startup time */
    /* FIXME: adjusting startup time will probably not work, because jobs are scheduled from first available altitude */
    else if (FINISH_AT == completionCondition)
    {
        estimatedTime = value;
        startupTime = completionTime.addSecs(-value);
    }
    /* Else estimated time is simply stored as is */
    else estimatedTime = value;

    updateJobCell();
}

void SchedulerJob::setInSequenceFocus(bool value)
{
    inSequenceFocus = value;
}

void SchedulerJob::setPriority(const uint8_t &value)
{
    priority = value;
}

void SchedulerJob::setEnforceTwilight(bool value)
{
    enforceTwilight = value;
}

void SchedulerJob::setEstimatedTimeCell(QTableWidgetItem *value)
{
    estimatedTimeCell = value;
    updateJobCell();
    if (estimatedTimeCell)
        estimatedTimeCell->setToolTip(i18n("Duration job '%1' will take to complete when started, as estimated by the Scheduler.\n"
                                       "Depends on the actions to be run, and the sequence job to be processed.",
                                       name));
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}

void SchedulerJob::setRepeatsRequired(const uint16_t &value)
{
    repeatsRequired = value;
    updateJobCell();
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
    updateJobCell();
}

void SchedulerJob::setCapturedFramesMap(const QMap<QString, uint16_t> &value)
{
    capturedFramesMap = value;
}

void SchedulerJob::setTargetCoords(dms& ra, dms& dec)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.updateCoordsNow(KStarsData::Instance()->updateNum());
}

void SchedulerJob::updateJobCell()
{
    if (nameCell)
    {
        nameCell->setText(name);
        nameCell->tableWidget()->resizeColumnToContents(nameCell->column());
    }

    if (nameLabel)
    {
        nameLabel->setText(name + QString(":"));
    }

    if (statusCell)
    {
        static QMap<JOBStatus, QString> stateStrings;
        static QString stateStringUnknown;
        if (stateStrings.isEmpty())
        {
            stateStrings[JOB_IDLE] = i18n("Idle");
            stateStrings[JOB_EVALUATION] = i18n("Evaluating");
            stateStrings[JOB_SCHEDULED] = i18n("Scheduled");
            stateStrings[JOB_BUSY] = i18n("Running");
            stateStrings[JOB_INVALID] = i18n("Invalid");
            stateStrings[JOB_COMPLETE] = i18n("Complete");
            stateStrings[JOB_ABORTED] = i18n("Aborted");
            stateStrings[JOB_ERROR] =  i18n("Error");
            stateStringUnknown = i18n("Unknown");
        }
        statusCell->setText(stateStrings.value(state, stateStringUnknown));
        statusCell->tableWidget()->resizeColumnToContents(statusCell->column());
    }

    if (stageCell || stageLabel)
    {
        /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case should ; also, not thread-safe */
        /* FIXME: this should work with a static initializer in C++11, but QT versions are touchy on this, and perhaps i18n can't be used? */
        static QMap<JOBStage, QString> stageStrings;
        static QString stageStringUnknown;
        if (stageStrings.isEmpty())
        {
            stageStrings[STAGE_IDLE] = i18n("Idle");
            stageStrings[STAGE_SLEWING] = i18n("Slewing");
            stageStrings[STAGE_SLEW_COMPLETE] = i18n("Slew complete");
            stageStrings[STAGE_FOCUSING] =
                    stageStrings[STAGE_POSTALIGN_FOCUSING] = i18n("Focusing");
            stageStrings[STAGE_FOCUS_COMPLETE] =
                    stageStrings[STAGE_POSTALIGN_FOCUSING_COMPLETE ] = i18n("Focus complete");
            stageStrings[STAGE_ALIGNING] = i18n("Aligning");
            stageStrings[STAGE_ALIGN_COMPLETE] = i18n("Align complete");
            stageStrings[STAGE_RESLEWING] = i18n("Repositioning");
            stageStrings[STAGE_RESLEWING_COMPLETE] = i18n("Repositioning complete");
            /*stageStrings[STAGE_CALIBRATING] = i18n("Calibrating");*/
            stageStrings[STAGE_GUIDING] = i18n("Guiding");
            stageStrings[STAGE_GUIDING_COMPLETE] = i18n("Guiding complete");
            stageStrings[STAGE_CAPTURING] = i18n("Capturing");
            stageStringUnknown = i18n("Unknown");
        }
        if (stageCell)
        {
            stageCell->setText(stageStrings.value(stage, stageStringUnknown));
            stageCell->tableWidget()->resizeColumnToContents(stageCell->column());
        }
        if (stageLabel)
        {
            stageLabel->setText(QString("%1: %2").arg(name, stageStrings.value(stage, stageStringUnknown)));
        }
    }

    if (startupCell && startupCell->tableWidget())
    {
        /* Display a startup time if job is running, scheduled to run or about to be re-scheduled */
        if (JOB_SCHEDULED == state || JOB_BUSY == state || JOB_ABORTED == state) switch (fileStartupCondition)
        {
            /* If the original condition is START_AT/START_CULMINATION, startup time is fixed */
            case START_AT:
            case START_CULMINATION:
                startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
                startupCell->setIcon(QIcon::fromTheme("chronometer"));
                break;

            /* If the original condition is START_ASAP, startup time is informational */
            case START_ASAP:
                startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
                startupCell->setIcon(QIcon());
                break;

            /* Else do not display any startup time */
            default:
                startupCell->setText(QString());
                startupCell->setIcon(QIcon());
                break;
        }
        /* Display a missed startup time if job is invalid */
        else if (JOB_INVALID == state && START_AT == fileStartupCondition)
        {
            startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
            startupCell->setIcon(QIcon::fromTheme("chronometer"));
        }
        /* Else do not display any startup time */
        else
        {
            startupCell->setText(QString());
            startupCell->setIcon(QIcon());
        }

        startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (completionCell && completionCell->tableWidget())
    {
        /* Display a completion time if job is running, scheduled to run or about to be re-scheduled */
        if (JOB_SCHEDULED == state || JOB_BUSY == state || JOB_ABORTED == state) switch (completionCondition)
        {
            case FINISH_LOOP:
                completionCell->setText(QString("-"));
                completionCell->setIcon(QIcon());
                break;

            case FINISH_AT:
                completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
                completionCell->setIcon(QIcon::fromTheme("chronometer"));
                break;

            case FINISH_SEQUENCE:
            case FINISH_REPEAT:
            default:
                completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
                completionCell->setIcon(QIcon());
                break;
        }
        /* Else do not display any completion time */
        else
        {
            completionCell->setText(QString());
            completionCell->setIcon(QIcon());
        }

        completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (estimatedTimeCell && estimatedTimeCell->tableWidget())
    {
        if (0 < estimatedTime)
            /* Seconds to ms - this doesn't follow dateTimeDisplayFormat, which renders YMD too */
            estimatedTimeCell->setText(QTime::fromMSecsSinceStartOfDay(estimatedTime*1000).toString("HH:mm:ss"));
        else if(0 == estimatedTime)
            /* FIXME: this special case could be merged with the previous, kept for future to indicate actual duration */
            estimatedTimeCell->setText("00:00:00");
        else
            /* Invalid marker */
            estimatedTimeCell->setText("-");

        estimatedTimeCell->tableWidget()->resizeColumnToContents(estimatedTimeCell->column());
    }

    if (captureCountCell && captureCountCell->tableWidget())
    {
        captureCountCell->setText(QString("%1/%2").arg(completedCount).arg(sequenceCount));
        captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    if (scoreCell && scoreCell->tableWidget())
    {
        if (0 <= score)
            scoreCell->setText(QString("%1").arg(score));
        else
            /* FIXME: negative scores are just weird for the end-user */
            scoreCell->setText(QString("<0"));

        scoreCell->tableWidget()->resizeColumnToContents(scoreCell->column());
    }
}

void SchedulerJob::reset()
{
    state = JOB_IDLE;
    stage = STAGE_IDLE;
    estimatedTime = -1;
    startupCondition = fileStartupCondition;
    startupTime = fileStartupCondition == START_AT ? fileStartupTime : QDateTime();
    /* No change to culmination offset */
    repeatsRemaining = repeatsRequired;
}

bool SchedulerJob::estimateJobTime(Scheduler *scheduler)
{
    /* updateCompletedJobsCount(); */

    QList<SequenceJob *> seqJobs;
    bool hasAutoFocus = false;

    if (loadSequenceQueue(scheduler, getSequenceFile().toLocalFile(), seqJobs, hasAutoFocus) == false)
        return false;

    setInSequenceFocus(hasAutoFocus);

    bool lightFramesRequired = false;

    int totalSequenceCount = 0, totalCompletedCount = 0;
    double totalImagingTime  = 0;
    bool rememberJobProgress = Options::rememberJobProgress();
    foreach (SequenceJob *seqJob, seqJobs)
    {
        /* FIXME: find a way to actually display the filter name */
        QString seqName = i18n("Job '%1' %2x%3\" %4", getName(), seqJob->getCount(), seqJob->getExposure(),
                               seqJob->getFilterName());

        if (seqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 duration cannot be estimated time since the sequence saves the files remotely.").arg(seqName);
            setEstimatedTime(-2);

            // Iterate over all sequence jobs, if just one requires FRAME_LIGHT then we set it as is and return
            foreach (SequenceJob *oneJob, seqJobs)
            {
                if (oneJob->getFrameType() == FRAME_LIGHT)
                {
                    lightFramesRequired = true;
                    break;
                }
            }

            setLightFramesRequired(lightFramesRequired);
            qDeleteAll(seqJobs);
            return true;
        }

        int const captures_required = seqJob->getCount()*getRepeatsRequired();

        int captures_completed = 0;
        if (rememberJobProgress)
        {
            /* Enumerate sequence jobs associated to this scheduler job, and assign them a completed count.
             *
             * The objective of this block is to fill the storage map of the scheduler job with completed counts for each capture storage.
             *
             * Sequence jobs capture to a storage folder, and are given a count of captures to store at that location.
             * The tricky part is to make sure the repeat count of the scheduler job is properly transferred to each sequence job.
             *
             * For instance, a scheduler job repeated three times must execute the full list of sequence jobs three times, thus
             * has to tell each sequence job it misses all captures, three times. It cannot tell the sequence job three captures are
             * missing, first because that's not how the sequence job is designed (completed count, not required count), and second
             * because this would make the single sequence job repeat three times, instead of repeating the full list of sequence
             * jobs three times.
             *
             * The consolidated storage map will be assigned to each sequence job based on their signature when the scheduler job executes them.
             *
             * For instance, consider a RGBL sequence of single captures. The map will store completed captures for R, G, B and L storages.
             * If R and G have 1 file each, and B and L have no files, map[storage(R)] = map[storage(G)] = 1 and map[storage(B)] = map[storage(L)] = 0.
             * When that scheduler job executes, only B and L captures will be processed.
             *
             * In the case of a RGBLRGB sequence of single captures, the second R, G and B map items will count one less capture than what is really in storage.
             * If R and G have 1 file each, and B and L have no files, map[storage(R1)] = map[storage(B1)] = 1, and all others will be 0.
             * When that scheduler job executes, B1, L, R2, G2 and B2 will be processed.
             *
             * This doesn't handle the case of duplicated scheduler jobs, that is, scheduler jobs with the same storage for capture sets.
             * Those scheduler jobs will all change state to completion at the same moment as they all target the same storage.
             * This is why it is important to manage the repeat count of the scheduler job, as stated earlier.
             */

            // Retrieve cached count of captures_completed captures for the output folder of this seqJob
            QString const signature = seqJob->getLocalDir() + seqJob->getDirectoryPostfix();
            captures_completed = capturedFramesCount[signature];

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 sees %2 captures in output folder '%3'.").arg(seqName).arg(captures_completed).arg(signature);

            // Enumerate sequence jobs to check how many captures are completed overall in the same storage as the current one
            foreach (SequenceJob *prevSeqJob, seqJobs)
            {
                // Enumerate seqJobs up to the current one
                if (seqJob == prevSeqJob)
                    break;

                // If the previous sequence signature matches the current, reduce completion count to take duplicates into account
                if (!signature.compare(prevSeqJob->getLocalDir() + prevSeqJob->getDirectoryPostfix()))
                {
                    int const previous_captures_required = prevSeqJob->getCount()*getRepeatsRequired();
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 has a previous duplicate sequence job requiring %2 captures.").arg(seqName).arg(previous_captures_required);
                    captures_completed -= previous_captures_required;
                }

                // Now completed count can be needlessly negative for this job, so clamp to zero
                if (captures_completed < 0)
                    captures_completed = 0;

                // And break if no captures remain, this job has to execute
                if (captures_completed == 0)
                    break;
            }

            // Finally we're only interested in the number of captures required for this sequence item
            if (captures_required < captures_completed)
                captures_completed = captures_required;

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 has completed %2/%3 of its required captures in output folder '%4'.").arg(seqName).arg(captures_completed).arg(captures_required).arg(signature);

            // Update the completion count for this signature if we still have captures to take
            // FIXME: setting the whole capture map each time is not very optimal
            QMap<QString, uint16_t> fMap = getCapturedFramesMap();
            if (fMap[signature] != captures_completed)
            {
                fMap[signature] = captures_completed;
                setCapturedFramesMap(fMap);
            }

            // From now on, 'captures_completed' is the number of frames completed for the *current* sequence job
        }


        // Check if we still need any light frames. Because light frames changes the flow of the observatory startup
        // Without light frames, there is no need to do focusing, alignment, guiding...etc
        // We check if the frame type is LIGHT and if either the number of captures_completed frames is less than required
        // OR if the completion condition is set to LOOP so it is never complete due to looping.
        // FIXME: As it is implemented now, FINISH_LOOP may loop over a capture-complete, therefore inoperant, scheduler job.
        bool const areJobCapturesComplete = !(captures_completed < captures_required || getCompletionCondition() == SchedulerJob::FINISH_LOOP);
        if (seqJob->getFrameType() == FRAME_LIGHT)
        {
            if(areJobCapturesComplete)
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 completed its sequence of %2 light frames.").arg(seqName).arg(captures_required);
            }
            else
            {
                lightFramesRequired = true;

                // In some cases we do not need to calculate time we just need to know
                // if light frames are required or not. So we break out
                /*
                if (getCompletionCondition() == SchedulerJob::FINISH_LOOP ||
                    (getStartupCondition() == SchedulerJob::START_AT &&
                     getCompletionCondition() == SchedulerJob::FINISH_AT))
                    break;
                */
            }
        }
        else
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 captures calibration frames.").arg(seqName);
        }

        totalSequenceCount += captures_required;
        totalCompletedCount += rememberJobProgress ? captures_completed : 0;

        /* If captures are not complete, we have imaging time left */
        if (!areJobCapturesComplete)
        {
            /* if looping, consider we always have one capture left - currently this is discarded afterwards as -2 */
            if (getCompletionCondition() == SchedulerJob::FINISH_LOOP)
                totalImagingTime += fabs((seqJob->getExposure() + seqJob->getDelay()) * 1);
            else
                totalImagingTime += fabs((seqJob->getExposure() + seqJob->getDelay()) * (captures_required - captures_completed));

            /* If we have light frames to process, add focus/dithering delay */
            if (seqJob->getFrameType() == FRAME_LIGHT)
            {
                // If inSequenceFocus is true
                if (hasAutoFocus)
                {
                    // Wild guess that each in sequence auto focus takes an average of 30 seconds. It can take any where from 2 seconds to 2+ minutes.
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a focus procedure.").arg(seqName);
                    totalImagingTime += (captures_required - captures_completed) * 30;
                }
                // If we're dithering after each exposure, that's another 10-20 seconds
                if (getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                {
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a dither procedure.").arg(seqName);
                    totalImagingTime += ((captures_required - captures_completed) * 15) / Options::ditherFrames();
                }
            }
        }
    }

    setLightFramesRequired(lightFramesRequired);
    setSequenceCount(totalSequenceCount);
    setCompletedCount(totalCompletedCount);

    qDeleteAll(seqJobs);

    // We can't estimate times that do not finish when sequence is done
    if (getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        // We can't know estimated time if it is looping indefinitely
        scheduler->appendLogText(i18n("Warning! Job '%1' will be looping until Scheduler is stopped manually.", getName()));
        setEstimatedTime(-2);
    }
    // If we know startup and finish times, we can estimate time right away
    else if (getStartupCondition() == SchedulerJob::START_AT &&
        getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        qint64 const diff = getStartupTime().secsTo(getCompletionTime());
        scheduler->appendLogText(i18n("Job '%1' will run for %2.", getName(), dms(diff / 3600.0f).toHMSString()));
        setEstimatedTime(diff);
    }
    // Rely on the estimated imaging time to determine whether this job is complete or not - this makes the estimated time null
    else if (totalImagingTime <= 0)
    {
        scheduler->appendLogText(i18n("Job '%1' will not run, complete with %2/%3 captures.", getName(), totalCompletedCount, totalSequenceCount));
        setEstimatedTime(0);
    }
    else
    {
        if (lightFramesRequired)
        {
            /* FIXME: estimation doesn't need to consider repeats, those will be optimized away by findNextJob (this is a regression) */
            /* FIXME: estimation should base on actual measure of each step, eventually with preliminary data as what it used now */
            // Are we doing tracking? It takes about 30 seconds
            if (getStepPipeline() & SchedulerJob::USE_TRACK)
                totalImagingTime += 30*getRepeatsRequired();
            // Are we doing initial focusing? That can take about 2 minutes
            if (getStepPipeline() & SchedulerJob::USE_FOCUS)
                totalImagingTime += 120*getRepeatsRequired();
            // Are we doing astrometry? That can take about 30 seconds
            if (getStepPipeline() & SchedulerJob::USE_ALIGN)
                totalImagingTime += 30*getRepeatsRequired();
            // Are we doing guiding? Calibration process can take about 2 mins
            if (getStepPipeline() & SchedulerJob::USE_GUIDE)
                totalImagingTime += 120*getRepeatsRequired();
        }

        dms estimatedTime;
        estimatedTime.setH(totalImagingTime / 3600.0);
        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' estimated to take %2 to complete.").arg(getName(), estimatedTime.toHMSString());

        setEstimatedTime(totalImagingTime);
    }

    return true;
}

bool SchedulerJob::updateCompletedJobsCount(Scheduler *scheduler)
{
    /* Use a temporary map in order to limit the number of file searches */
    QMap<QString, uint16_t> newFramesCount;

    QList<SequenceJob*> seqjobs;
    bool hasAutoFocus = false;

    /* do nothing for idle jobs */
    if (getState() == SchedulerJob::JOB_IDLE || getState() == SchedulerJob::JOB_EVALUATION) return true;

    /* Look into the sequence requirements, bypass if invalid */
    if (loadSequenceQueue(scheduler, getSequenceFile().toLocalFile(), seqjobs, hasAutoFocus) == false)
    {
        scheduler->appendLogText(i18n("Warning! Job '%1' has inaccessible sequence '%2', marking invalid.",
                            getName(), getSequenceFile().toLocalFile()));
        return false;
    }

    /* Enumerate the SchedulerJob's SequenceJobs to count captures stored for each */
    /* FIXME: returns wrong values if another SchedulerJob puts the files into the same directory */
    foreach (SequenceJob *oneSeqJob, seqjobs)
    {
        /* Only consider captures stored on client (Ekos) side */
        /* FIXME: ask the remote for the file count */
        if (oneSeqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
            continue;

        /* FIXME: refactor signature determination in a separate function in order to support multiple backends */
        /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
        QString const signature = oneSeqJob->getLocalDir() + oneSeqJob->getDirectoryPostfix();

        /* We recount other jobs if somehow we don't have any count for their signature, else we reuse the previous count */
        QMap<QString, uint16_t>::iterator const sigCount = capturedFramesCount.find(signature);
        if (capturedFramesCount.end() != sigCount)
                {
                    newFramesCount[signature] = sigCount.value();
                    continue;
                }

            /* Count captures already stored */
            newFramesCount[signature] = getCompletedFiles(signature, oneSeqJob->getFullPrefix());
    }

    capturedFramesCount = newFramesCount;
    return true;
}

int SchedulerJob::getCompletedFiles(const QString &path, const QString &seqPrefix)
{
    int seqFileCount = 0;

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Searching in '%1' for prefix '%2'...").arg(path, seqPrefix);
    QDirIterator it(path, QDir::Files);

    /* FIXME: this counts all files with prefix in the storage location, not just captures. DSS analysis files are counted in, for instance. */
    while (it.hasNext())
    {
        QString const fileName = QFileInfo(it.next()).baseName();

        if (fileName.startsWith(seqPrefix))
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("> Found '%1'").arg(fileName);
            seqFileCount++;
        }
    }

    return seqFileCount;
}

bool SchedulerJob::loadSequenceQueue(Scheduler *scheduler, const QString &fileURL,
                                     QList<SequenceJob *> &jobs, bool &hasAutoFocus)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open sequence queue file '%1'", fileURL);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "Autofocus"))
                    hasAutoFocus = (!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                else if (!strcmp(tagXMLEle(ep), "Job"))
                    jobs.append(processJobInfo(ep));
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            scheduler->appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            qDeleteAll(jobs);
            return false;
        }
    }

    return true;
}

SequenceJob *SchedulerJob::processJobInfo(XMLEle *root)
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    const QMap<QString, CCDFrameType> frameTypes = {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    SequenceJob *job = new SequenceJob();
    QString rawPrefix, frameType, filterType;
    double exposure    = 0;
    bool filterEnabled = false, expEnabled = false, tsEnabled = false;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
        {
            exposure = atof(pcdataXMLEle(ep));
            job->setExposure(exposure);
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            filterType = QString(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            frameType = QString(pcdataXMLEle(ep));
            job->setFrameType(frameTypes[frameType]);
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                rawPrefix = QString(pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterEnabled = !strcmp("1", pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                tsEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

            job->setPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            job->setCount(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            job->setDelay(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            job->setLocalDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            job->setRemoteDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            job->setUploadMode(static_cast<ISD::CCD::UploadMode>(atoi(pcdataXMLEle(ep))));
        }
    }

    // Make full prefix
    QString imagePrefix = rawPrefix;

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameType;

    if (filterEnabled && filterType.isEmpty() == false &&
        (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT))
    {
        imagePrefix += '_';

        imagePrefix += filterType;
    }

    if (expEnabled)
    {
        imagePrefix += '_';

        imagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
    }

    job->setFullPrefix(imagePrefix);


    QString targetName = getName().remove(' ');

    // Directory postfix
    QString directoryPostfix;

    directoryPostfix = QLatin1Literal("/") + targetName + QLatin1Literal("/") + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT) && filterType.isEmpty() == false)
        directoryPostfix += QLatin1Literal("/") + filterType;

    job->setDirectoryPostfix(directoryPostfix);

    return job;
}


bool SchedulerJob::decreasingScoreOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getScore() > job2->getScore();
}

bool SchedulerJob::increasingPriorityOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getPriority() < job2->getPriority();
}

bool SchedulerJob::decreasingAltitudeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return  Ekos::Scheduler::findAltitude(job1->getTargetCoords(), job1->getStartupTime()) >
            Ekos::Scheduler::findAltitude(job2->getTargetCoords(), job2->getStartupTime());
}

bool SchedulerJob::increasingStartupTimeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getStartupTime() < job2->getStartupTime();
}
}
