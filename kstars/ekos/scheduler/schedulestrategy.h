/*  Ekos Scheduling Strategy
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QList>
#include "schedulerjob.h"
#include "indiapi.h"

class GeoLocation;
class KSMoon;

namespace Ekos
{
class SchedulerJob;

class ScheduleStrategy
{

public:
    /**
     * @brief Initialize the scheduling strategy.
     */
    ScheduleStrategy();

    void updateCompletedJobsCount();

    /**
    * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
    * Given that score, the scheduler will decide which is the best job that needs to be executed.
    */
    void evaluateJobs();

    /**
     * @brief delay jobs that overlap with firstJob
     * @param reference job
     */
    void delayOverlappingJobs(SchedulerJob *firstJob);

    /**
     * @brief Obtain the list of jobs sorted due to the scheduling strategy.
     * @return list of jobs
     */
    QList<SchedulerJob *> *getSortedJobs() {return &sortedJobs; }

    void setWeatherStatus(IPState status) { weatherStatus = status; }
    IPState getWeatherStatus() { return weatherStatus; }

    /**
      * @brief updatePreDawn Update predawn time depending on current time and user offset
      */
    void updatePreDawn();

    /// Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
    QDateTime preDawnDateTime;
    void setPreDawnDateTime(QDateTime time) { preDawnDateTime = time; }
    QDateTime getPredawnDateTime() {return preDawnDateTime; }

    /**
         * @brief calculateJobScore Calculate job dark sky score, altitude score, and moon separation scores and returns the sum.
         * @param job job to evaluate
         * @param when time to evaluate constraints
         * @return Total score
         */
    int16_t calculateJobScore(SchedulerJob *job, QDateTime when);

    /**
     * @brief returns the job with the best rating based on the last run of #evaluateJobs()
     * @return job that has been rated best
     */
    SchedulerJob *getBestRatedJob();

    /* FIXME: this should be private. Resolve back loop from SchedulerJob */
    int16_t getDarkSkyScore(const QDateTime &observationDateTime);

    /**
         * @brief getCurrentMoonSeparation Get current moon separation in degrees at current time for the given job
         * @param job scheduler job
         * @return Separation in degrees
         */
    double getCurrentMoonSeparation(SchedulerJob *job);

    /**
         * @brief findAltitude Find altitude given a specific time
         * @param target Target
         * @param when date time to find altitude
         * @return Altitude of the target at the specific date and time given.
         */
    static double findAltitude(const SkyPoint &target, const QDateTime &when);

    /**
     * @brief Determine the current dawn time
     * @return dawn time in minutes
     */
    double getDawn() {return Dawn;}

    /**
     * @brief Determine the current dusk time
     * @return dusk time in minutes
     */
    double getDusk() {return Dusk;}

    QList<SchedulerJob *> *getAllJobs() {return &allJobs; }

    int invalidJobs = 0, completedJobs = 0, abortedJobs = 0, upcomingJobs = 0;
    int getInvalidJobsCount() {return invalidJobs; }
    int getCompletedJobsCount() {return completedJobs; }
    int getAbortedJobsCount() {return abortedJobs; }
    int getUpcomingJobsCount() {return upcomingJobs; }


private:
    /** refresh the sorted values **/
    void refresh();

    /**
     * @brief Refresh job status, score and startup time
     * @param job job to be refreshed
     * @param now current date and time
     */
    void refreshJob(SchedulerJob *job, const QDateTime now);

    /**
     * @brief update dawn and dusk. This should have been executed before a first call to
     * #getDawn() or #getDusk.
     */
    void calculateDawnDusk();

    /**
         * @brief getAltitudeScore Get the altitude score of an object. The higher the better
         * @param job Active job
         * @param when At what time to check the target altitude
         * @return Altitude score. Altitude below minimum default of 15 degrees but above horizon get -20 score. Bad altitude below minimum required altitude or below horizon get -1000 score.
         */
    int16_t getAltitudeScore(SchedulerJob *job, QDateTime when);

    /**
         * @brief getMoonSeparationScore Get moon separation score. The further apart, the better, up a maximum score of 20.
         * @param job Target job
         * @param when What time to check the moon separation?
         * @return Moon separation score
         */
    int16_t getMoonSeparationScore(SchedulerJob *job, QDateTime when);


    /// Keep watch of weather status
    IPState weatherStatus { IPS_IDLE };

    /**
         * @brief getWeatherScore Get weather condition score.
         * @param enforceWeather if true, the score enforces good weather
         * @return If weather condition OK, return 0, if warning return -500, if alert return -1000
         */
    int16_t getWeatherScore(bool enforceWeather);

    /**
     * @brief update job status counters
     */
    void updateJobsCounts();


    bool isWeatherOK(SchedulerJob *job);


    /// all jobs that have to be scheduled
    QList<SchedulerJob *> allJobs;

    // list holding the jobs sorted by the strategy
    QList<SchedulerJob *> sortedJobs;

    /// Pointer to Moon object
    KSMoon *moon { nullptr };

    /// Store day fraction of dawn to calculate dark skies range
    double Dawn { -1 };
    /// Store day fraction of dusk to calculate dark skies range
    double Dusk { -1 };
    /// Dusk date time
    QDateTime duskDateTime;

};

}
