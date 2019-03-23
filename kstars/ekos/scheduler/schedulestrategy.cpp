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
#define DEFAULT_MIN_ALTITUDE        15

namespace Ekos
{

ScheduleStrategy::ScheduleStrategy() {

    moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName("Moon"));

    calculateDawnDusk();
}

double ScheduleStrategy::findAltitude(const SkyPoint &target, const QDateTime &when, bool * is_setting, bool debug)
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    GeoLocation * const geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geo->UTtoLT(KStarsDateTime(when)) : when :
                          KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Calculate alt/az coordinates using KStars instance's geolocation
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(ltWhen).gst());
    o.EquatorialToHorizontal(&LST, geo->lat());

    // Hours are reduced to [0,24[, meridian being at 0
    double offset = LST.Hours() - o.ra().Hours();
    if (24.0 <= offset)
        offset -= 24.0;
    else if (offset < 0.0)
        offset += 24.0;
    bool const passed_meridian = 0.0 <= offset && offset < 12.0;

    if (debug)
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("When:%9 LST:%8 RA:%1 RA0:%2 DEC:%3 DEC0:%4 alt:%5 setting:%6 HA:%7")
                                       .arg(o.ra().toHMSString())
                                       .arg(o.ra0().toHMSString())
                                       .arg(o.dec().toHMSString())
                                       .arg(o.dec0().toHMSString())
                                       .arg(o.alt().Degrees())
                                       .arg(passed_meridian ? "yes" : "no")
                                       .arg(o.ra().Hours())
                                       .arg(LST.toHMSString())
                                       .arg(ltWhen.toString("HH:mm:ss"));

    if (is_setting)
        *is_setting = passed_meridian;

    return o.alt().Degrees();
}

QDateTime ScheduleStrategy::calculateAltitudeTime(SchedulerJob const *job, double minAltitude, double minMoonAngle, QDateTime const &when) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places
    GeoLocation *geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geo->UTtoLT(KStarsDateTime(when)) : when :
                          KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = job->getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Calculate the UT at the argument time
    KStarsDateTime const ut = geo->LTtoUT(ltWhen);

    double const SETTING_ALTITUDE_CUTOFF = Options::settingAltitudeCutoff();

    // Within the next 24 hours, search when the job target matches the altitude and moon constraints
    for (unsigned int minute = 0; minute < 24 * 60; minute++)
    {
        KStarsDateTime const ltOffset(ltWhen.addSecs(minute * 60));

        // Update RA/DEC of the target for the current fraction of the day
        KSNumbers numbers(ltOffset.djd());
        o.updateCoordsNow(&numbers);

        // Compute local sidereal time for the current fraction of the day, calculate altitude
        CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(ltOffset).gst());
        o.EquatorialToHorizontal(&LST, geo->lat());
        double const altitude = o.alt().Degrees();

        if (minAltitude <= altitude)
        {
            // Don't test proximity to dawn in this situation, we only cater for altitude here

            // Continue searching if Moon separation is not good enough
            if (0 < minMoonAngle && getMoonSeparationScore(job, ltOffset) < 0)
                continue;

            // Continue searching if target is setting and under the cutoff
            double offset = LST.Hours() - o.ra().Hours();
            if (24.0 <= offset)
                offset -= 24.0;
            else if (offset < 0.0)
                offset += 24.0;
            if (0.0 <= offset && offset < 12.0)
                if (altitude - SETTING_ALTITUDE_CUTOFF < minAltitude)
                    continue;

            return ltOffset;
        }
    }

    return QDateTime();
}

QDateTime ScheduleStrategy::calculateCulmination(SchedulerJob const *job, int offset_minutes, QDateTime const &when) const
{
    // FIXME: culmination calculation is a min altitude requirement, should be an interval altitude requirement
    // FIXME: block calculating target coordinates at a particular time is duplicated in calculateCulmination
    GeoLocation *geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geo->UTtoLT(KStarsDateTime(when)) : when :
                          KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = job->getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC for the argument date/time
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Calculate transit date/time at the argument date - transitTime requires UT and returns LocalTime
    KStarsDateTime transitDateTime(ltWhen.date(), o.transitTime(geo->LTtoUT(ltWhen), geo), Qt::LocalTime);

    // Shift transit date/time by the argument offset
    KStarsDateTime observationDateTime = transitDateTime.addSecs(offset_minutes * 60);

    // Relax observation time, culmination calculation is stable at minute only
    KStarsDateTime relaxedDateTime = observationDateTime.addSecs(Options::leadTime() * 60);

    // Verify resulting observation time is under lead time vs. argument time
    // If sooner, delay by 8 hours to get to the next transit - perhaps in a third call
    if (relaxedDateTime < ltWhen)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' startup %2 is posterior to transit %3, shifting by 8 hours.")
                                       .arg(job->getName())
                                       .arg(ltWhen.toString(job->getDateTimeDisplayFormat()))
                                       .arg(relaxedDateTime.toString(job->getDateTimeDisplayFormat()));

        return calculateCulmination(job, offset_minutes, when.addSecs(8 * 60 * 60));
    }

    // Guarantees - culmination calculation is stable at minute level, so relax by lead time
    Q_ASSERT_X(observationDateTime.isValid(), __FUNCTION__, "Observation time for target culmination is valid.");
    Q_ASSERT_X(ltWhen <= relaxedDateTime, __FUNCTION__, "Observation time for target culmination is at or after than argument time");

    // Return consolidated culmination time
    return Qt::UTC == observationDateTime.timeSpec() ? geo->UTtoLT(observationDateTime) : observationDateTime;
}

#if 0
int16_t ScheduleStrategy::getWeatherScore()
{
    if (weatherCheck->isEnabled() == false || weatherCheck->isChecked() == false)
        return 0;

    if (getWeatherStatus() == IPS_BUSY)
        return BAD_SCORE / 2;
    else if (getWeatherStatus() == IPS_ALERT)
        return BAD_SCORE;

    return 0;
}
#endif

int16_t ScheduleStrategy::getDarkSkyScore(QDateTime const &when) const
{
    double const secsPerDay = 24.0 * 3600.0;
    double const minsPerDay = 24.0 * 60.0;

    // Dark sky score is calculated based on distance to today's dawn and next dusk.
    // Option "Pre-dawn Time" avoids executing a job when dawn is approaching, and is a value in minutes.
    // - If observation is between option "Pre-dawn Time" and dawn, score is BAD_SCORE/50.
    // - If observation is before dawn today, score is fraction of the day from beginning of observation to dawn time, as percentage.
    // - If observation is after dusk, score is fraction of the day from dusk to beginning of observation, as percentage.
    // - If observation is between dawn and dusk, score is BAD_SCORE.
    //
    // If observation time is invalid, the score is calculated for the current day time.
    // Note exact dusk time is considered valid in terms of night time, and will return a positive, albeit null, score.

    // FIXME: Dark sky score should consider the middle of the local night as best value.
    // FIXME: Current algorithm uses the dawn and dusk of today, instead of the day of the observation.

    int const earlyDawnSecs = static_cast <int> ((Dawn - static_cast <double> (Options::preDawnTime()) / minsPerDay) * secsPerDay);
    int const dawnSecs = static_cast <int> (Dawn * secsPerDay);
    int const duskSecs = static_cast <int> (Dusk * secsPerDay);
    int const obsSecs = (when.isValid() ? when : KStarsData::Instance()->lt()).time().msecsSinceStartOfDay() / 1000;

    int16_t score = 0;

    if (earlyDawnSecs <= obsSecs && obsSecs < dawnSecs)
    {
        score = BAD_SCORE / 50;

        //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Dark sky score at %1 is %2 (between pre-dawn and dawn).")
        //    .arg(observationDateTime.toString())
        //    .arg(QString::asprintf("%+d", score));
    }
    else if (obsSecs < dawnSecs)
    {
        score = static_cast <int16_t> ((dawnSecs - obsSecs) / secsPerDay) * 100;

        //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Dark sky score at %1 is %2 (before dawn).")
        //    .arg(observationDateTime.toString())
        //    .arg(QString::asprintf("%+d", score));
    }
    else if (duskSecs <= obsSecs)
    {
        score = static_cast <int16_t> ((obsSecs - duskSecs) / secsPerDay) * 100;

        //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Dark sky score at %1 is %2 (after dusk).")
        //    .arg(observationDateTime.toString())
        //    .arg(QString::asprintf("%+d", score));
    }
    else
    {
        score = BAD_SCORE;

        //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Dark sky score at %1 is %2 (during daylight).")
        //    .arg(observationDateTime.toString())
        //    .arg(QString::asprintf("%+d", score));
    }

    return score;
}

int16_t ScheduleStrategy::calculateJobScore(SchedulerJob const *job, QDateTime const &when) const
{
    if (nullptr == job)
        return BAD_SCORE;

    /* Only consolidate the score if light frames are required, calibration frames can run whenever needed */
    if (!job->getLightFramesRequired())
        return 1000;

    int16_t total = 0;

    /* As soon as one score is negative, it's a no-go and other scores are unneeded */

    if (job->getEnforceTwilight())
    {
        int16_t const darkSkyScore = getDarkSkyScore(when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' dark sky score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", darkSkyScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += darkSkyScore;
    }

    /* We still enforce altitude if the job is neither required to track nor guide, because this is too confusing for the end-user.
     * If we bypass calculation here, it must also be bypassed when checking job constraints in checkJobStage.
     */
    if (0 <= total /*&& ((job->getStepPipeline() & SchedulerJob::USE_TRACK) || (job->getStepPipeline() & SchedulerJob::USE_GUIDE))*/)
    {
        int16_t const altitudeScore = getAltitudeScore(job, when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' altitude score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", altitudeScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += altitudeScore;
    }

    if (0 <= total)
    {
        int16_t const moonSeparationScore = getMoonSeparationScore(job, when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' Moon separation score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", moonSeparationScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += moonSeparationScore;
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a total score of %2 at %3.")
                                  .arg(job->getName())
                                  .arg(QString::asprintf("%+d", total))
                                  .arg(when.toString(job->getDateTimeDisplayFormat()));

    return total;
}

int16_t ScheduleStrategy::getAltitudeScore(SchedulerJob const *job, QDateTime const &when) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places
    GeoLocation *geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geo->UTtoLT(KStarsDateTime(when)) : when :
                          KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = job->getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Compute local sidereal time for the current fraction of the day, calculate altitude
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(ltWhen).gst());
    o.EquatorialToHorizontal(&LST, geo->lat());
    double const altitude = o.alt().Degrees();

    double const SETTING_ALTITUDE_CUTOFF = Options::settingAltitudeCutoff();
    int16_t score = BAD_SCORE - 1;

    // If altitude is negative, bad score
    // FIXME: some locations may allow negative altitudes
    if (altitude < 0)
    {
        score = BAD_SCORE;
    }
    else if (-90 < job->getMinAltitude())
    {
        // If under altitude constraint, bad score
        if (altitude < job->getMinAltitude())
            score = BAD_SCORE;
        // Else if setting and under altitude cutoff, job would end soon after starting, bad score
        // FIXME: half bad score when under altitude cutoff risk getting positive again
        else
        {
            double offset = LST.Hours() - o.ra().Hours();
            if (24.0 <= offset)
                offset -= 24.0;
            else if (offset < 0.0)
                offset += 24.0;
            if (0.0 <= offset && offset < 12.0)
                if (altitude - SETTING_ALTITUDE_CUTOFF < job->getMinAltitude())
                    score = BAD_SCORE / 2;
        }
    }
    // If not constrained but below minimum hard altitude, set score to 10% of altitude value
    else if (altitude < DEFAULT_MIN_ALTITUDE)
    {
        score = static_cast <int16_t> (altitude / 10.0);
    }

    // Else default score calculation without altitude constraint
    if (score < BAD_SCORE)
        score = static_cast <int16_t> ((1.5 * pow(1.06, altitude)) - (DEFAULT_MIN_ALTITUDE / 10.0));

    //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' target altitude is %3 degrees at %2 (score %4).")
    //    .arg(job->getName())
    //    .arg(when.toString(job->getDateTimeDisplayFormat()))
    //    .arg(currentAlt, 0, 'f', minAltitude->decimals())
    //    .arg(QString::asprintf("%+d", score));

    return score;
}

double ScheduleStrategy::getCurrentMoonSeparation(SchedulerJob const *job) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places
    GeoLocation *geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = job->getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Update moon
    //ut = geo->LTtoUT(ltWhen);
    //KSNumbers ksnum(ut.djd()); // BUG: possibly LT.djd() != UT.djd() because of translation
    //LST = geo->GSTtoLST(ut.gst());
    CachingDms LST = geo->GSTtoLST(geo->LTtoUT(ltWhen).gst());
    moon->updateCoords(&numbers, true, geo->lat(), &LST, true);

    // Moon/Sky separation p
    return moon->angularDistanceTo(&o).Degrees();
}

int16_t ScheduleStrategy::getMoonSeparationScore(SchedulerJob const *job, QDateTime const &when) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places
    GeoLocation *geo = KStarsData::Instance()->geo();

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geo->UTtoLT(KStarsDateTime(when)) : when :
                          KStarsData::Instance()->lt());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = job->getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Update moon
    //ut = geo->LTtoUT(ltWhen);
    //KSNumbers ksnum(ut.djd()); // BUG: possibly LT.djd() != UT.djd() because of translation
    //LST = geo->GSTtoLST(ut.gst());
    CachingDms LST = geo->GSTtoLST(geo->LTtoUT(ltWhen).gst());
    moon->updateCoords(&numbers, true, geo->lat(), &LST, true);

    double const moonAltitude = moon->alt().Degrees();

    // Lunar illumination %
    double const illum = moon->illum() * 100.0;

    // Moon/Sky separation p
    double const separation = moon->angularDistanceTo(&o).Degrees();

    // Zenith distance of the moon
    double const zMoon = (90 - moonAltitude);
    // Zenith distance of target
    double const zTarget = (90 - o.alt().Degrees());

    int16_t score = 0;

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

    //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' target is %L3 degrees from Moon (score %2).")
    //    .arg(job->getName())
    //    .arg(separation, 0, 'f', 3)
    //    .arg(QString::asprintf("%+d", score));

    return score;
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

    // FIXME: reduce spam by moving twilight time to a text label
    //appendLogText(i18n("Astronomical twilight: dusk at %1, dawn at %2, and current time is %3",
    //                   dusk.toString(), dawn.toString(), now.toString()));
}
bool ScheduleStrategy::isWeatherOK(SchedulerJob *job)
{
    if (weatherStatus == IPS_OK)
        return true;
    else if (weatherStatus == IPS_IDLE)
    {
//        if (indiState == INDI_READY)
//            qCInfo(KSTARS_EKOS_SCHEDULER) << i18n("Weather information is pending...");
        return true;
    }

    // Temporary BUSY is ALSO accepted for now
    // TODO Figure out how to exactly handle this
    if (weatherStatus == IPS_BUSY)
        return true;

    if (weatherStatus == IPS_ALERT)
    {
        job->setState(SchedulerJob::JOB_ABORTED);
        qCInfo(KSTARS_EKOS_SCHEDULER) << i18n("Job '%1' suffers from bad weather, marking aborted.", job->getName());
    }
    /*else if (weatherStatus == IPS_BUSY)
    {
        appendLogText(i18n("%1 observation job delayed due to bad weather.", job->getName()));
        schedulerTimer.stop();
        connect(this, &Scheduler::weatherChanged, this, &Scheduler::resumeCheckStatus);
    }*/

    return false;
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

} // namespace Ekos
