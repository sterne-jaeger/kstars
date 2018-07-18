/*  Ekos Scheduling Strategy
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "schedulestrategy.h"

#include "ksalmanac.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "Options.h"

#include <ekos_scheduler_debug.h>

#define BAD_SCORE                   -1000
#define SETTING_ALTITUDE_CUTOFF     3
#define DEFAULT_MIN_ALTITUDE        15

namespace Ekos
{


ScheduleStrategy::ScheduleStrategy() {

    moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName("Moon"));

    calculateDawnDusk();
}

void ScheduleStrategy::updateCompletedJobsCount()
{
    /* Enumerate SchedulerJobs to count captures that are already stored */
    for (SchedulerJob *oneJob : allJobs)
    {
        oneJob->updateCompletedJobsCount();
    }
}

void ScheduleStrategy::evaluateJobs() {
    /* Start by refreshing the number of captures already present */
    updateCompletedJobsCount();

    /* Update dawn and dusk astronomical times - unconditionally in case date changed */
    calculateDawnDusk();

    /* refresh sortings */
    refresh();
}

void ScheduleStrategy::refresh() {
    /* FIXME: it is possible to evaluate jobs while KStars has a time offset, so warn the user about this */
    QDateTime const now = KStarsData::Instance()->lt();

    /* First, filter out non-schedulable jobs */
    /* FIXME: jobs in state JOB_ERROR should not be in the list, reorder states */
    sortedJobs = allJobs;
    sortedJobs.erase(std::remove_if(sortedJobs.begin(), sortedJobs.end(),[](SchedulerJob* job)
    { return SchedulerJob::JOB_ABORTED < job->getState(); }), sortedJobs.end());

    /* Then reorder jobs by priority */
    /* FIXME: refactor so all sorts are using the same predicates */
    /* FIXME: use std::stable_sort as qStableSort is deprecated */
    if (Options::sortSchedulerJobs())
        qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingPriorityOrder);

    /* Then enumerate SchedulerJobs, scheduling only what is required */
    foreach (SchedulerJob *job, sortedJobs)
    {
        refreshJob(job, now);
    }

}

void ScheduleStrategy::refreshJob(SchedulerJob *job, QDateTime const now)
{
    /* Let aborted jobs be rescheduled later instead of forgetting them */
    /* FIXME: minimum altitude and altitude cutoff may cause loops here */
    switch (job->getState())
    {
        /* If job is idle, set it for evaluation */
        case SchedulerJob::JOB_IDLE:
            job->setState(SchedulerJob::JOB_EVALUATION);
            job->setEstimatedTime(-1);
            break;

        /* If job is aborted, reset it for evaluation */
        case SchedulerJob::JOB_ABORTED:
            // aborted jobs will be reset only if all others failed
            // job->setState(SchedulerJob::JOB_EVALUATION);
            return;

        /* If job is scheduled, quick-check startup and bypass evaluation if in future */
        case SchedulerJob::JOB_SCHEDULED:
            if (job->getStartupTime() < now)
                break;
            return;

        /* If job is in error, invalid or complete, bypass evaluation */
        case SchedulerJob::JOB_ERROR:
        case SchedulerJob::JOB_INVALID:
        case SchedulerJob::JOB_COMPLETE:
            return;

        /* If job is busy, edge case, bypass evaluation */
        case SchedulerJob::JOB_BUSY:
            return;

        /* Else evaluate */
        case SchedulerJob::JOB_EVALUATION:
        default:
            break;
    }

    // In case of a repeating jobs, let's make sure we have more runs left to go
    if (job->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
    {
        if (job->getRepeatsRemaining() == 0)
        {
            // appendLogText(i18n("Job '%1' has no more batches remaining.", job->getName()));
            job->setState(SchedulerJob::JOB_EVALUATION);
        }
    }

    // -1 = Job is not estimated yet
    // -2 = Job is estimated but time is unknown
    // > 0  Job is estimated and time is known
    if (job->getEstimatedTime() == -1)
    {
        if (job->estimateJobTime() == false)
        {
            job->setState(SchedulerJob::JOB_INVALID);
            return;
        }
    }

    if (job->getEstimatedTime() == 0)
    {
        job->setRepeatsRemaining(0);
        job->setState(SchedulerJob::JOB_COMPLETE);
        return;
    }

    // #1 Check startup conditions
    switch (job->getStartupCondition())
    {
        // #1.1 ASAP?
        case SchedulerJob::START_ASAP:
        {
            /* Job is to be started as soon as possible, so check its current score */
            int16_t const score = calculateJobScore(job, now);

            /* If it's not possible to run the job now, find proper altitude time */
            if (score < 0)
            {
                // If Altitude or Dark score are negative, we try to schedule a better time for altitude and dark sky period.
                if (job->calculateAltitudeTime(getMoonSeparationScore(job, job->getStartupTime())))
                {
                    //appendLogText(i18n("%1 observation job is scheduled at %2", job->getName(), job->getStartupTime().toString()));
                    job->setState(SchedulerJob::JOB_SCHEDULED);
                    // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                    /*job->setScore(BAD_SCORE);
                    continue;*/
                }
                else
                {
                    job->setState(SchedulerJob::JOB_INVALID);
                    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("Ekos failed to schedule %1.").arg(job->getName());
                }

                /* Keep the job score for current time, score will refresh as scheduler progresses */
                /* score = calculateJobScore(job, job->getStartupTime()); */
                job->setScore(score);
            }
            /* If it's possible to run the job now, check weather */
            else if (isWeatherOK(job) == false)
            {
                // appendLogText(i18n("Job '%1' cannot run now because of bad weather.", job->getName()));
                job->setState(SchedulerJob::JOB_ABORTED);
                job->setScore(BAD_SCORE);
            }
            /* If weather is ok, schedule the job to run now */
            else
            {
                // appendLogText(i18n("Job '%1' is due to run as soon as possible.", job->getName()));
                /* Give a proper start time, so that job can be rescheduled if others also start asap */
                job->setStartupTime(now);
                job->setState(SchedulerJob::JOB_SCHEDULED);
                job->setScore(score);
            }
        }
        break;

            // #1.2 Culmination?
        case SchedulerJob::START_CULMINATION:
        {
            if (job->calculateCulmination(this))
            {
                // appendLogText(i18n("Job '%1' is scheduled at %2 for culmination.", job->getName(),
                //                   job->getStartupTime().toString()));
                job->setState(SchedulerJob::JOB_SCHEDULED);
                // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                /*job->setScore(BAD_SCORE);
                continue;*/
            }
            else
            {
                // appendLogText(i18n("Job '%1' culmination cannot be scheduled, marking invalid.", job->getName()));
                job->setState(SchedulerJob::JOB_INVALID);
            }
        }
        break;

            // #1.3 Start at?
        case SchedulerJob::START_AT:
        {
            if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
            {
                if (job->getCompletionTime() <= job->getStartupTime())
                {
                    // appendLogText(i18n("Job '%1' completion time (%2) could not be achieved before start up time (%3), marking invalid", job->getName(),
                    //                   job->getCompletionTime().toString(), job->getStartupTime().toString()));
                    job->setState(SchedulerJob::JOB_INVALID);
                    return;
                }
            }

            int const timeUntil = now.secsTo(job->getStartupTime());

            // If starting time already passed by 5 minutes (default), we mark the job as invalid or aborted
            if (timeUntil < (-1 * Options::leadTime() * 60))
            {
                dms const passedUp(-timeUntil / 3600.0);

                /* Mark the job invalid only if its startup time was a user request, else just abort it for later reschedule */
                if (job->getFileStartupCondition() == SchedulerJob::START_AT)
                {
                    // appendLogText(i18n("Job '%1' startup time was fixed at %2, and is already passed by %3, marking invalid.",
                    //                   job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat()), passedUp.toHMSString()));
                    job->setState(SchedulerJob::JOB_INVALID);
                }
                else
                {
                    // appendLogText(i18n("Job '%1' startup time was %2, and is already passed by %3, marking aborted.",
                    //                   job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat()), passedUp.toHMSString()));
                    job->setState(SchedulerJob::JOB_ABORTED);
                }
            }
            // Start scoring once we reach startup time
            else if (timeUntil <= 0)
            {
                /* Consolidate altitude, moon separation and sky darkness scores */
                int16_t const score = calculateJobScore(job, now);

                if (score < 0)
                {
                    /* If job score is already negative, silently abort the job to avoid spamming the user */
//                        if (0 < job->getScore())
//                        {
//                            if (job->getState() == SchedulerJob::JOB_EVALUATION)
//                                appendLogText(i18n("Job '%1' evaluation failed with a score of %2, marking aborted.",
//                                                   job->getName(), score));
//                            else if (timeUntil == 0)
//                                appendLogText(i18n("Job '%1' updated score is %2 at startup time, marking aborted.",
//                                                   job->getName(), score));
//                            else
//                                appendLogText(i18n("Job '%1' updated score is %2 %3 seconds after startup time, marking aborted.",
//                                                   job->getName(), score, abs(timeUntil)));
//                        }

                    job->setState(SchedulerJob::JOB_ABORTED);
                    job->setScore(score);
                }
                /* Positive score means job is already scheduled, so we check the weather, and if it is not OK, we set bad score until weather improves. */
                else if (isWeatherOK(job) == false)
                {
                    // appendLogText(i18n("Job '%1' cannot run now because of bad weather.", job->getName()));
                    job->setState(SchedulerJob::JOB_ABORTED);
                    job->setScore(BAD_SCORE);
                }
                /* Else record current score */
                else
                {
                    // appendLogText(i18n("Job '%1' will be run at %2.", job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
                    job->setState(SchedulerJob::JOB_SCHEDULED);
                    job->setScore(score);
                }
            }
#if 0
            // If it is in the future and originally was designated as ASAP job
            // Job must be less than 12 hours away to be considered for re-evaluation
            else if (timeUntil > (Options::leadTime() * 60) && (timeUntil < 12 * 3600) &&
                     job->getFileStartupCondition() == SchedulerJob::START_ASAP)
            {
                QDateTime nextJobTime = now.addSecs(Options::leadTime() * 60);
                if (job->getEnforceTwilight() == false || (now > duskDateTime && now < preDawnDateTime))
                {
                    appendLogText(i18n("Job '%1' can be scheduled under 12 hours, but will be re-evaluated at %2.",
                                       job->getName(), nextJobTime.toString(job->getDateTimeDisplayFormat())));
                    job->setStartupTime(nextJobTime);
                }
                job->setScore(BAD_SCORE);
            }
            // If time is far in the future, we make the score negative
            else
            {
                if (job->getState() == SchedulerJob::JOB_EVALUATION &&
                    calculateJobScore(job, job->getStartupTime()) < 0)
                {
                    appendLogText(i18n("Job '%1' can only be scheduled in more than 12 hours, marking aborted.",
                                       job->getName()));
                    job->setState(SchedulerJob::JOB_ABORTED);
                    continue;
                }

                /*score += BAD_SCORE;*/
            }
#endif
            /* Else simply refresh job score */
            else
            {
                // appendLogText(i18n("Job '%1' unmodified, will be run at %2.", job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
                job->setState(SchedulerJob::JOB_SCHEDULED);
                job->setScore(calculateJobScore(job, now));
            }

        }
        break;
    }


    if (job->getState() == SchedulerJob::JOB_EVALUATION)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << job->getName() << "' was unexpectedly not scheduled by evaluation.";
    }

}

void ScheduleStrategy::delayOverlappingJobs(SchedulerJob *firstJob)
{
    QDateTime firstStartTime    = firstJob->getStartupTime();
    QDateTime lastStartTime     = firstJob->getStartupTime();
    double lastJobEstimatedTime = firstJob->getEstimatedTime();
    int daysCount               = 0;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Option to sort jobs based on priority and altitude is" << Options::sortSchedulerJobs();
    qCDebug(KSTARS_EKOS_SCHEDULER) << "First job after sort is" << firstJob->getName() << "starting at" << firstJob->getStartupTime().toString(firstJob->getDateTimeDisplayFormat());

    // Make sure no two jobs have the same scheduled time or overlap with other jobs
    foreach (SchedulerJob *job, sortedJobs)
    {
        // If this job is not scheduled, continue
        // If this job startup conditon is not to start at a specific time, continue
        if (job == firstJob || job->getState() != SchedulerJob::JOB_SCHEDULED ||
            job->getStartupCondition() != SchedulerJob::START_AT)
            continue;

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Examining job" << job->getName() << "starting at" << job->getStartupTime().toString(job->getDateTimeDisplayFormat());

        double timeBetweenJobs = (double)std::abs(firstStartTime.secsTo(job->getStartupTime()));

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job starts in" << timeBetweenJobs << "seconds (lead time" << Options::leadTime()*60 << ")";

        // If there are within 5 minutes of each other, try to advance scheduling time of the lower altitude one
        if (timeBetweenJobs < (Options::leadTime()) * 60)
        {
            double delayJob = timeBetweenJobs + lastJobEstimatedTime;

            if (delayJob < (Options::leadTime() * 60))
                delayJob = Options::leadTime() * 60;

            QDateTime otherjob_time = lastStartTime.addSecs(delayJob);
            QDateTime nextPreDawnTime = preDawnDateTime.addDays(daysCount);
            // If other jobs starts after pre-dawn limit, then we schedule it to the next day.
            // But we only take this action IF the job we are checking against starts _before_ dawn and our
            // job therefore carry us after down, then there is an actual need to schedule it next day.
            // FIXME: After changing time we are not evaluating job again when we should.
            if (job->getEnforceTwilight() && lastStartTime < nextPreDawnTime && otherjob_time >= nextPreDawnTime)
            {
                QDateTime date;

                daysCount++;

                lastStartTime = job->getStartupTime().addDays(daysCount);
                job->setStartupTime(lastStartTime);
                date = lastStartTime.addSecs(delayJob);
            }
            else
            {
                lastStartTime = lastStartTime.addSecs(delayJob);
                job->setStartupTime(lastStartTime);
            }

            job->setState(SchedulerJob::JOB_SCHEDULED);

            /* Kept the informative log now that aborted jobs are rescheduled */
            // appendLogText(i18n("Jobs '%1' and '%2' have close start up times, job '%2' is rescheduled to %3.",
            //                   firstJob->getName(), job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
        }

        lastJobEstimatedTime = job->getEstimatedTime();
    }

}

SchedulerJob *ScheduleStrategy::getBestRatedJob()
{
    updatePreDawn();

    QList<SchedulerJob *> activeJobs = sortedJobs;

    /* Remove complete and invalid jobs that could have appeared during the last evaluation */
    activeJobs.erase(std::remove_if(activeJobs.begin(), activeJobs.end(),[](SchedulerJob* job)
    { return SchedulerJob::JOB_ABORTED <= job->getState(); }), activeJobs.end());

    if (activeJobs.isEmpty())
        return nullptr;

    /* Now that jobs are scheduled, possibly at the same time, reorder by altitude and priority again */
    if (Options::sortSchedulerJobs())
    {
        qStableSort(activeJobs.begin(), activeJobs.end(), SchedulerJob::decreasingAltitudeOrder);
        qStableSort(activeJobs.begin(), activeJobs.end(), SchedulerJob::increasingPriorityOrder);
    }

    /* Reorder jobs by schedule time */
    qStableSort(activeJobs.begin(), activeJobs.end(), SchedulerJob::increasingStartupTimeOrder);

    // Our first job now takes priority over ALL others.
    // So if any other jobs conflicts with ours, we re-schedule that job to another time.
    delayOverlappingJobs(activeJobs.first());

    // Sort again by schedule, sooner first, as some jobs may have shifted during the last step
    qStableSort(activeJobs.begin(), activeJobs.end(), SchedulerJob::increasingStartupTimeOrder);

    return activeJobs.first();
}

void ScheduleStrategy::updatePreDawn()
{
    double earlyDawn = Dawn - Options::preDawnTime() / (60.0 * 24.0);
    int dayOffset    = 0;
    QTime dawn       = QTime(0, 0, 0).addSecs(Dawn * 24 * 3600);
    if (KStarsData::Instance()->lt().time() >= dawn)
        dayOffset = 1;
    preDawnDateTime.setDate(KStarsData::Instance()->lt().date().addDays(dayOffset));
    preDawnDateTime.setTime(QTime::fromMSecsSinceStartOfDay(earlyDawn * 24 * 3600 * 1000));
}


/** ********************** scoring functions ********************** */
int16_t ScheduleStrategy::calculateJobScore(SchedulerJob *job, QDateTime when)
{
    /* Only consolidate the score if light frames are required, calibration frames can run whenever needed */
    if (!job->getLightFramesRequired())
        return 1000;

    int16_t total = 0;

    /* As soon as one score is negative, it's a no-go and other scores are unneeded */

    if (job->getEnforceTwilight())
        total += getDarkSkyScore(when);

    if (0 <= total && job->getStepPipeline() != SchedulerJob::USE_NONE)
        total += getAltitudeScore(job, when);

    if (0 <= total)
        total += getMoonSeparationScore(job, when);

    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a total score of %2").arg(job->getName()).arg(total);
    return total;
}

int16_t ScheduleStrategy::getAltitudeScore(SchedulerJob *job, QDateTime when)
{
    int16_t score     = 0;
    double currentAlt = findAltitude(job->getTargetCoords(), when);

    if (currentAlt < 0)
    {
        score = BAD_SCORE;
    }
    // If minimum altitude is specified
    else if (job->getMinAltitude() > 0)
    {
        // if current altitude is lower that's not good
        if (currentAlt < job->getMinAltitude())
            score = BAD_SCORE;
        else
        {
            // Get HA of actual object, and not of the mount as was done below
            double HA = KStars::Instance()->data()->lst()->Hours() - job->getTargetCoords().ra().Hours();

#if 0
            if (indiState == INDI_READY)
            {
                QDBusReply<double> haReply = mountInterface->call(QDBus::AutoDetect, "getHourAngle");
                if (haReply.error().type() == QDBusError::NoError)
                    HA = haReply.value();
            }
#endif

            // If already passed the merdian and setting we check if it is within setting alttidue cut off value (3 degrees default)
            // If it is within that value then it is useless to start the job which will end very soon so we better look for a better job.
            /* FIXME: don't use BAD_SCORE/2, a negative result implies the job has to be aborted - we'd be annoyed if that score became positive again */
            /* FIXME: bug here, raising target will get a negative score if under cutoff, issue mitigated by aborted jobs getting rescheduled */
            if (HA > 0 && (currentAlt - SETTING_ALTITUDE_CUTOFF) < job->getMinAltitude())
                score = BAD_SCORE / 2.0;
            else
                // Otherwise, adjust score and add current altitude to score weight
                score = (1.5 * pow(1.06, currentAlt)) - (DEFAULT_MIN_ALTITUDE / 10.0);
        }
    }
    // If it's below minimum hard altitude (15 degrees now), set score to 10% of altitude value
    else if (currentAlt < DEFAULT_MIN_ALTITUDE)
    {
        score = currentAlt / 10.0;
    }
    // If no minimum altitude, then adjust altitude score to account for current target altitude
    else
    {
        score = (1.5 * pow(1.06, currentAlt)) - (DEFAULT_MIN_ALTITUDE / 10.0);
    }

    /* Kept the informative log now that scores are displayed */
    // appendLogText(i18n("Job '%1' target altitude is %3 degrees at %2 (score %4).", job->getName(), when.toString(job->getDateTimeDisplayFormat()),
    //                   QString::number(currentAlt, 'g', 3), QString::asprintf("%+d", score)));

    return score;
}

double ScheduleStrategy::getCurrentMoonSeparation(SchedulerJob *job)
{
    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight(KStarsData::Instance()->lt().date(), QTime());
    GeoLocation *geo    = KStarsData::Instance()->geo();
    KStarsDateTime ut   = geo->LTtoUT(KStarsDateTime(midnight));
    KStarsDateTime myUT = ut.addSecs(KStarsData::Instance()->lt().time().msecsSinceStartOfDay() / 1000);
    CachingDms LST      = geo->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, geo->lat());

    // Update moon
    ut = geo->LTtoUT(KStarsData::Instance()->lt());
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST(ut.gst());
    moon->updateCoords(&ksnum, true, geo->lat(), &LST, true);

    // Moon/Sky separation p
    return moon->angularDistanceTo(&p).Degrees();
}

int16_t ScheduleStrategy::getMoonSeparationScore(SchedulerJob *job, QDateTime when)
{
    int16_t score = 0;

    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight(when.date(), QTime());

    GeoLocation *geo    = KStarsData::Instance()->geo();
    KStarsDateTime ut   = geo->LTtoUT(KStarsDateTime(midnight));
    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay() / 1000);
    CachingDms LST      = geo->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, geo->lat());
    double currentAlt = p.alt().Degrees();

    // Update moon
    ut = geo->LTtoUT(KStarsDateTime(when));
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST(ut.gst());
    moon->updateCoords(&ksnum, true, geo->lat(), &LST, true);

    double moonAltitude = moon->alt().Degrees();

    // Lunar illumination %
    double illum = moon->illum() * 100.0;

    // Moon/Sky separation p
    double separation = moon->angularDistanceTo(&p).Degrees();

    // Zenith distance of the moon
    double zMoon = (90 - moonAltitude);
    // Zenith distance of target
    double zTarget = (90 - currentAlt);

    // If target = Moon, or no illuminiation, or moon below horizon, return static score.
    if (zMoon == zTarget || illum == 0 || zMoon >= 90)
        score = 100;
    else
    {
        // JM: Some magic voodoo formula I came up with!
        double moonEffect = (pow(separation, 1.7) * pow(zMoon, 0.5)) / (pow(zTarget, 1.1) * pow(illum, 0.5));

        // Limit to 0 to 100 range.
        moonEffect = KSUtils::clamp(moonEffect, 0.0, 100.0);

        if (job->getMinMoonSeparation() > 0)
        {
            if (separation < job->getMinMoonSeparation())
                score = BAD_SCORE * 5;
            else
                score = moonEffect;
        }
        else
            score = moonEffect;
    }

    // Limit to 0 to 20
    score /= 5.0;

    /* Kept the informative log now that score is displayed */
    // appendLogText(i18n("Job '%1' target is %3 degrees from Moon (score %2).", job->getName(), QString::asprintf("%+d", score), separation));

    return score;
}

bool ScheduleStrategy::isWeatherOK(SchedulerJob *job)
{
    if (weatherStatus == IPS_OK || (!job->getEnforceWeather()))
        return true;
    else if (weatherStatus == IPS_IDLE)
    {
//        if (indiState == INDI_READY)
//            appendLogText(i18n("Weather information is pending..."));
        return true;
    }

    // Temporary BUSY is ALSO accepted for now
    // TODO Figure out how to exactly handle this
    if (weatherStatus == IPS_BUSY)
        return true;

    if (weatherStatus == IPS_ALERT)
    {
        job->setState(SchedulerJob::JOB_ABORTED);
        // appendLogText(i18n("Job '%1' suffers from bad weather, marking aborted.", job->getName()));
    }
    /*else if (weatherStatus == IPS_BUSY)
    {
        appendLogText(i18n("%1 observation job delayed due to bad weather.", job->getName()));
        schedulerTimer.stop();
        connect(this, SIGNAL(weatherChanged(IPState)), this, SLOT(resumeCheckStatus()));
    }*/

    return false;
}

int16_t ScheduleStrategy::getWeatherScore(bool enforceWeather)
{
    if (! enforceWeather)
        return 0;

    if (weatherStatus == IPS_BUSY)
        return BAD_SCORE / 2;
    else if (weatherStatus == IPS_ALERT)
        return BAD_SCORE;

    return 0;
}

int16_t ScheduleStrategy::getDarkSkyScore(const QDateTime &observationDateTime)
{
    //  if (job->getStartingCondition() == SchedulerJob::START_CULMINATION)
    //    return -1000;

    int16_t score      = 0;
    double dayFraction = 0;

    // Anything half an hour before dawn shouldn't be a good candidate
    double earlyDawn = getDawn() - Options::preDawnTime() / (60.0 * 24.0);

    dayFraction = observationDateTime.time().msecsSinceStartOfDay() / (24.0 * 60.0 * 60.0 * 1000.0);

    // The farther the target from dawn, the better.
    if (dayFraction > earlyDawn && dayFraction < getDawn())
        score = BAD_SCORE / 50;
    else if (dayFraction < getDawn())
        score = (getDawn() - dayFraction) * 100;
    else if (dayFraction > getDusk())
    {
        score = (dayFraction - getDusk()) * 100;
    }
    else
        score = BAD_SCORE;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dark sky score is" << score << "for time" << observationDateTime.toString();

    return score;
}



/* *********************** helper functions *********************** */

double ScheduleStrategy::findAltitude(const SkyPoint &target, const QDateTime &when)
{
    // Make a copy
    /*SkyPoint p = target;
    QDateTime lt(when.date(), QTime());
    KStarsDateTime ut = KStarsData::Instance()->geo()->LTtoUT(KStarsDateTime(lt));

    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay() / 1000);

    CachingDms LST = KStarsData::Instance()->geo()->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, KStarsData::Instance()->geo()->lat());

    return p.alt().Degrees();*/

    SkyPoint p = target;
    KStarsDateTime lt(when);
    CachingDms LST = KStarsData::Instance()->geo()->GSTtoLST(lt.gst());
    p.EquatorialToHorizontal(&LST, KStarsData::Instance()->geo()->lat());

    return p.alt().Degrees();
}


void ScheduleStrategy::calculateDawnDusk()
{
    KSAlmanac ksal;
    Dawn = ksal.getDawnAstronomicalTwilight();
    Dusk = ksal.getDuskAstronomicalTwilight();

    QTime now  = KStarsData::Instance()->lt().time();
    QTime dawn = QTime(0, 0, 0).addSecs(Dawn * 24 * 3600);
    QTime dusk = QTime(0, 0, 0).addSecs(Dusk * 24 * 3600);

    duskDateTime.setDate(KStars::Instance()->data()->lt().date());
    duskDateTime.setTime(dusk);

}



} // namespace Ekos
