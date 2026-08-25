/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private functions and helpers for the Sensor Profile module.
*/
#ifdef INCLUDE_SENSOR_PROFILE

#ifndef SENSOR_PROFILE_PRIVATE_H_
#define SENSOR_PROFILE_PRIVATE_H_

#include <message_.h>


/*! \brief Sensor Profile internal state. */
typedef struct
{
    /*! Sensor Profile task */
    TaskData    task_data;

    /*! Task for the application registered to use the Profile */
    Task        reg_task;

    /*! Sensor Profile role, if set is Primary, otherwise Secondary */
    unsigned    is_primary : 1;
} sensor_profile_task_data_t;


extern sensor_profile_task_data_t sensor_profile;

#define SensorProfile_Get() (&sensor_profile)

#define SensorProfile_GetTask() (&SensorProfile_Get()->task_data)

#endif /* SENSOR_PROFILE_PRIVATE_H_ */

#endif /* INCLUDE_SENSOR_PROFILE */
