/*  Ekos Scheduling Strategy
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "schedulerjob.h"
#include "indiapi.h"

class GeoLocation;
class KSMoon;

namespace Ekos
{
class ScheduleStrategy
{
public:
    ScheduleStrategy();

    /**
         * @brief getDarkSkyScore Get the dark sky score of a date and time. The further from dawn the better.
         * @param when date and time to check the dark sky score, now if omitted
         * @return Dark sky score. Daylight get bad score, as well as pre-dawn to dawn.
         */
    int16_t getDarkSkyScore(QDateTime const &when = QDateTime()) const;

    /**
         * @brief getAltitudeScore Get the altitude score of an object. The higher the better
         * @param job Target job
         * @param when date and time to check the target altitude, now if omitted.
         * @return Altitude score. Target altitude below minimum altitude required by job or setting target under 3 degrees below minimum altitude get bad score.
         */
    int16_t getAltitudeScore(SchedulerJob const *job, QDateTime const &when = QDateTime()) const;

    /**
         * @brief getMoonSeparationScore Get moon separation score. The further apart, the better, up a maximum score of 20.
         * @param job Target job
         * @param when date and time to check the moon separation, now if omitted.
         * @return Moon separation score
         */
    int16_t getMoonSeparationScore(SchedulerJob const *job, QDateTime const &when = QDateTime()) const;

    /**
         * @brief calculateJobScore Calculate job dark sky score, altitude score, and moon separation scores and returns the sum.
         * @param job Target
         * @param when date and time to evaluate constraints, now if omitted.
         * @return Total score
         */
    int16_t calculateJobScore(SchedulerJob const *job, QDateTime const &when = QDateTime()) const;

#if 0
    /**
         * @brief getWeatherScore Get current weather condition score.
         * @return If weather condition OK, return score 0, else bad score.
         */
    int16_t getWeatherScore();
#endif

    /**
         * @brief calculateAltitudeTime calculate the altitude time given the minimum altitude given.
         * @param job active target
         * @param minAltitude minimum altitude required
         * @param minMoonAngle minimum separation from the moon. -1 to ignore.
         * @param when date and time to start searching from, now if omitted.
         * @return The date and time the target is at or above the argument altitude, valid if found, invalid if not achievable (always under altitude).
         */
    QDateTime calculateAltitudeTime(SchedulerJob const *job, double minAltitude, double minMoonAngle = -1, QDateTime const &when = QDateTime()) const;

    /**
         * @brief calculateCulmination find culmination time adjust for the job offset
         * @param job active target
         * @param offset_minutes offset in minutes before culmination to search for.
         * @param when date and time to start searching from, now if omitted
         * @return The date and time the target is in entering the culmination interval, valid if found, invalid if not achievable (currently always valid).
         */
    QDateTime calculateCulmination(SchedulerJob const *job, int offset_minutes, QDateTime const &when = QDateTime()) const;

    /**
         * @brief calculateDawnDusk Get dawn and dusk times for today
         */
    void calculateDawnDusk();

    double getDawn() const { return Dawn; }
    double getDusk() const { return Dusk; }

    /**
         * @brief getCurrentMoonSeparation Get current moon separation in degrees at current time for the given job
         * @param job scheduler job
         * @return Separation in degrees
         */
    double getCurrentMoonSeparation(SchedulerJob const *job) const;

    /**
         * @brief updatePreDawn Update predawn time depending on current time and user offset
         */
    void updatePreDawn();

    QDateTime getPreDawnDateTime() const { return preDawnDateTime; }

    /**
         * @brief findAltitude Find altitude given a specific time
         * @param target Target
         * @param when date time to find altitude
         * @param is_setting whether target is setting at the argument time (optional).
         * @param debug outputs calculation to log file (optional).
         * @return Altitude of the target at the specific date and time given.
         * @warning This function uses the current KStars geolocation.
         */
    static double findAltitude(const SkyPoint &target, const QDateTime &when, bool *is_setting = nullptr, bool debug = false);

    void setWeatherStatus(IPState status) { weatherStatus = status; }
    IPState getWeatherStatus() { return weatherStatus; }


    bool isWeatherOK(SchedulerJob *job);

private:
    /// Pointer to Moon object
    KSMoon *moon { nullptr };
    /// Store day fraction of dawn to calculate dark skies range
    double Dawn { -1 };
    /// Store day fraction of dusk to calculate dark skies range
    double Dusk { -1 };
    /// Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
    QDateTime preDawnDateTime;
    /// Dusk date time
    QDateTime duskDateTime;
    /// Keep watch of weather status
    IPState weatherStatus { IPS_IDLE };


};


}
