/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       spatial_data_attitude_filter.c
\brief      Provides Spatial data interface to Attitude Filter library
*/

#ifdef INCLUDE_ATTITUDE_FILTER

#define DEBUG_LOG_MODULE_NAME spatial_data

#include "earbud_config.h"
#include "spatial_data_private.h"

#include "attitude_filter.h"

DEBUG_LOG_DEFINE_LEVEL_VAR /* Enable logging for this module. */

static att_quat_q15_t att_quat_q15_local_in; /* Local quaternion (unfiltered input to update local). */
static att_quat_q15_t att_quat_q15_local_out; /* Filtered local quat to pass to the other bud. */
static att_quat_q15_t att_quat_q15_sibling; /* Filtered sibling quat from the other bud. */
static att_quat_q15_t att_quat_q15_head; /* Head quat. */

/* Head rotation vector scaled to +- pi. */
static att_head_orientation_rotation_vector_t att_head_rot_vector_converted;

/*!
 * \brief Calibration data for centring data from sensors that are not calibrated.
 */
typedef struct
{
    /*! Total of x values during calibration. */
    int32 tot_x;

    /*! Total of y values during calibration. */
    int32 tot_y;

    /*! Total of z values during calibration. */
    int32 tot_z;

    /*! The set of values from the last reading. */
    motion_data_t drift;

    /*! Limit to assume still. */
    int16 stable_limit;

    /*! Number of calibration cycles. */
    uint8 count;

    /*! Max count for setting drift values. */
    uint8 count_max;         
} motion_calibration_t;

static void spatialData_AttitudeFilterMotionOrientSensor(motion_data_t *m, bool is_left_ear);
static void spatialData_AttitudeFilterMotionUpdatePosition(
    motion_data_t *accel_data,
    motion_data_t *gyro_data,
    uint32 ts_us,
    att_quat_q15_t *att_quat_q15_local_in, /* [out] unfiltered local quat. */
    att_quat_q15_t *att_quat_q15_local_out, /* [out] filtered local quat. */
    att_quat_q15_t *att_quat_q15_head /* [out] current head position. */
);
static void spatialData_AttitudeFilterMotionGyroCalibrate(motion_calibration_t *cal, motion_data_t *data);

bool SpatialData_AttitudeFilterInit(void)
{
    DEBUG_LOG_DEBUG("%s", __func__);
    Attitude_Init();

    /* Set the configured accel/gyro scale values. */
    if (spatial_data.motion_sensor_config_ptr)
    {
        Attitude_SetParam(ATT_ACC_DATA_RANGE,  2 * spatial_data.motion_sensor_config_ptr->accel_scale);
        Attitude_SetParam(ATT_GYRO_DATA_RANGE, 2 * spatial_data.motion_sensor_config_ptr->gyro_scale);
        return TRUE;
    }
    else
    {
        DEBUG_LOG_WARN("%s:Invalid motion sensor config ptr", __func__);
        return FALSE;
    }
}

bool SpatialData_AttitudeFilterReset(void)
{
    DEBUG_LOG_INFO("%s", __func__);
    Attitude_Reset();
    return TRUE;
}

bool SpatialData_ConvertSensorDataToQuaternion(motion_sensor_data_t *sensor_data)
{
    static motion_calibration_t gyro_calibration;
    rtime_t prev_time = 0;
    rtime_t current_time = 0;

    PanicNull(sensor_data);

    spatialData_AttitudeFilterMotionGyroCalibrate(&gyro_calibration, &sensor_data->gyro);

    /* Gyro drift fix. */
    sensor_data->gyro.x -= gyro_calibration.drift.x;
    sensor_data->gyro.y -= gyro_calibration.drift.y;
    sensor_data->gyro.z -= gyro_calibration.drift.z;

    /* Orient sensor within earbud. */
    spatialData_AttitudeFilterMotionOrientSensor(&sensor_data->gyro, appConfigIsLeft());
    spatialData_AttitudeFilterMotionOrientSensor(&sensor_data->accel, appConfigIsLeft());

    /* Attitude Filter. */
    prev_time = SystemClockGetTimerTime();
    spatialData_AttitudeFilterMotionUpdatePosition(
        &sensor_data->accel,
        &sensor_data->gyro,
        sensor_data->timestamp,
        &att_quat_q15_local_in, /* [out] unfiltered local quat. */
        &att_quat_q15_local_out, /* [out] filtered local quat. */
        &att_quat_q15_head /* [out] current head position. */
    );
    current_time = SystemClockGetTimerTime();

    DEBUG_LOG_VERBOSE(
        "t = %10u, %s:Attitude Filter update_time: %5d us "
        "local in: v:%d ts:%10u W:%6d X:%6d Y:%6d Z:%6d "
        "local out: v:%d ts:%10u W:%6d X:%6d Y:%6d Z:%6d ",
        current_time,
        __func__,
        current_time - prev_time,
        att_quat_q15_local_in.valid,
        att_quat_q15_local_in.timestamp_ms,
        att_quat_q15_local_in.q15.w,
        att_quat_q15_local_in.q15.x,
        att_quat_q15_local_in.q15.y,
        att_quat_q15_local_in.q15.z,
        att_quat_q15_local_out.valid,
        att_quat_q15_local_out.timestamp_ms,
        att_quat_q15_local_out.q15.w,
        att_quat_q15_local_out.q15.x,
        att_quat_q15_local_out.q15.y,
        att_quat_q15_local_out.q15.z
    );

    DEBUG_LOG_VERBOSE(
        "head: v:%d ts:%10u W:%6d X:%6d Y:%6d Z:%6d",
        att_quat_q15_head.valid,
        att_quat_q15_head.timestamp_ms,
        att_quat_q15_head.q15.w,
        att_quat_q15_head.q15.x,
        att_quat_q15_head.q15.y,
        att_quat_q15_head.q15.z
    );

    /* Replace the quaternion data with either the local filtered or head values. */
    if (SpatialData_GetDataSource() == spatial_data_head)
    {
        if (att_quat_q15_head.valid)
        {
            /* Head orientation. */
            sensor_data->timestamp = att_quat_q15_head.timestamp_ms * US_PER_MS;
            sensor_data->quaternion.w = att_quat_q15_head.q15.w;
            sensor_data->quaternion.x = att_quat_q15_head.q15.x;
            sensor_data->quaternion.y = att_quat_q15_head.q15.y;
            sensor_data->quaternion.z = att_quat_q15_head.q15.z;
        }
    }
    else
    {
        if (att_quat_q15_local_out.valid)
        {
            /* Local filtered orientation. */
            sensor_data->timestamp = att_quat_q15_local_out.timestamp_ms * US_PER_MS;
            sensor_data->quaternion.w = att_quat_q15_local_out.q15.w;
            sensor_data->quaternion.x = att_quat_q15_local_out.q15.x;
            sensor_data->quaternion.y = att_quat_q15_local_out.q15.y;
            sensor_data->quaternion.z = att_quat_q15_local_out.q15.z;
        }
    }

    SpatialData_AttitudeFilterHeadRotationVectorGet(&att_head_rot_vector_converted);

    return TRUE;
}

bool SpatialData_AttitudeFilterGetCurrentOrientation(quaternion_info_exchange_t *quaternion_info)
{
    DEBUG_LOG_DEBUG("t = %10u, %s", SystemClockGetTimerTime(), __func__);

    PanicNull(quaternion_info);

    /* Get the local orientation to pass to the other bud. */
    /* quat is q15 (norm is 2**15). */
    quaternion_info->timestamp = att_quat_q15_local_out.timestamp_ms * US_PER_MS;
    quaternion_info->quaternion_data.w = att_quat_q15_local_out.q15.w;
    quaternion_info->quaternion_data.x = att_quat_q15_local_out.q15.x;
    quaternion_info->quaternion_data.y = att_quat_q15_local_out.q15.y;
    quaternion_info->quaternion_data.z = att_quat_q15_local_out.q15.z;

    return TRUE;
}

bool SpatialData_AttitudeFilterUpdateOrientationFromOtherEarbud(const quaternion_info_exchange_t *quaternion_info)
{
    if (!quaternion_info)
    {
        DEBUG_LOG_WARN("%s:Empty quaternion_info",__func__);
        return FALSE;
    }

#ifdef INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG
    static rtime_t prev_time = 0;
    rtime_t delta_time = 0;
    if (rtime_gt(quaternion_info->timestamp, prev_time))
    {
        delta_time = rtime_sub(quaternion_info->timestamp, prev_time);
    }
    else
    {
        delta_time = rtime_sub(prev_time, quaternion_info->timestamp);
    }
    DEBUG_LOG_INFO("%s:Delta:%10u ms W:%d X:%d Y:%d Z:%d",
                    __func__,
                    delta_time/US_PER_MS,
                    quaternion_info->quaternion_data.w,
                    quaternion_info->quaternion_data.x,
                    quaternion_info->quaternion_data.y,
                    quaternion_info->quaternion_data.z);
    prev_time = quaternion_info->timestamp;

#else
    DEBUG_LOG_DEBUG("t = %10u, %s:Timestamp:%10u us W:%6d X:%6d Y:%6d Z:%6d",
                     SystemClockGetTimerTime(),
                     __func__,
                     quaternion_info->timestamp,
                     quaternion_info->quaternion_data.w,
                     quaternion_info->quaternion_data.x,
                     quaternion_info->quaternion_data.y,
                     quaternion_info->quaternion_data.z);
#endif /* INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG */

    att_quat_q15_sibling.valid = TRUE; /* todo: change quaternion_info_exchange_t to include .valid */
    att_quat_q15_sibling.timestamp_ms = (quaternion_info->timestamp + 500) / 1000; /* Round us to ms. */
    att_quat_q15_sibling.q15.w = quaternion_info->quaternion_data.w;
    att_quat_q15_sibling.q15.x = quaternion_info->quaternion_data.x;
    att_quat_q15_sibling.q15.y = quaternion_info->quaternion_data.y;
    att_quat_q15_sibling.q15.z = quaternion_info->quaternion_data.z;

    Attitude_HeadSiblingQuatUpdate(&att_quat_q15_sibling);
    return TRUE;
}

void SpatialData_AttitudeFilterHeadRotationVectorGet(att_head_orientation_rotation_vector_t *rot_vector)
{
    /* Get the latest Head Rotation Vector */
    Attitude_HeadRotationVectorGet(rot_vector);

    /* Convert to +/- pi by dividing by (2 x pi) */
    rot_vector->rads_q16[0] = rot_vector->rads_q16[0] * 163 / 1024;
    rot_vector->rads_q16[1] = rot_vector->rads_q16[1] * 163 / 1024;
    rot_vector->rads_q16[2] = rot_vector->rads_q16[2] * 163 / 1024;

    /* Convert to 32rad/s by dividing by 64 */
    rot_vector->rads_q16_per_s[0] /= 64;
    rot_vector->rads_q16_per_s[1] /= 64;
    rot_vector->rads_q16_per_s[2] /= 64;

    DEBUG_LOG_V_VERBOSE(
        "t = %10u, ROTATION_VECTOR: valid:%d timestamp:%10u rads:%6d %6d %6d rads_per_s:%6d %6d %6d",
        SystemClockGetTimerTime(),
        rot_vector->valid, rot_vector->timestamp_ms * US_PER_MS,
        rot_vector->rads_q16[0], rot_vector->rads_q16[1], rot_vector->rads_q16[2],
        rot_vector->rads_q16_per_s[0], rot_vector->rads_q16_per_s[1], rot_vector->rads_q16_per_s[2]
    );
}

/* Store calibration data when sensor fairly still. */
static void spatialData_AttitudeFilterMotionGyroCalibrate(motion_calibration_t *cal, motion_data_t *data)
{
    if(cal->stable_limit == 0)
    {
        cal->stable_limit = 100; /* Choose a sensible value. */
    }
    if(cal->count_max == 0)
    {
        cal->count_max = 100; /* Choose a sensible value. */
    }
    if(cal->count >= cal->count_max)
    {
        return; /* Already calibrated. */
    }
    if(
        (ABS(data->x) < cal->stable_limit)
        && (ABS(data->y) < cal->stable_limit)
        && (ABS(data->z) < cal->stable_limit)
    )
    {
        /* Fairly stable.  Accumulate drift. */
        cal->tot_x += data->x;
        cal->tot_y += data->y;
        cal->tot_z += data->z;
        cal->count++;
        if(cal->count >= cal->count_max)
        {
            /* Calibration done.  Store drift. */
            cal->drift.x = (int16)(cal->tot_x / cal->count);
            cal->drift.y = (int16)(cal->tot_y / cal->count);
            cal->drift.z = (int16)(cal->tot_z / cal->count);
            DEBUG_LOG_INFO("%s:Gyro calibration %4d %4d %4d",
                __func__,cal->drift.x, cal->drift.y, cal->drift.z);
        }
    }
    else
    {
        /* Could reset the values and restart the calibration but not necessary. */
    }
}

static void spatialData_AttitudeFilterMotionOrientSensor(motion_data_t *m, bool is_left_ear)
{
    /* Orient data to match earbud. */
    motion_data_t t;
    t.x = m->x;
    t.y = m->y;
    t.z = m->z;
    if (is_left_ear)
    {
        m->x = t.z;
        m->y = -t.x;
        m->z = -t.y;
    }
    else
    {
        m->x = -t.z;
        m->y = t.y;
        m->z = t.x;
    }
}


/* Use the attitude filter library. */
static void spatialData_AttitudeFilterMotionUpdatePosition(
    motion_data_t *accel_data,
    motion_data_t *gyro_data,
    uint32 ts_us,
    att_quat_q15_t *att_quat_q15_local_in, /* [out] unfiltered local quat. */
    att_quat_q15_t *att_quat_q15_local_out, /* [out] filtered local quat. */
    att_quat_q15_t *att_quat_q15_head /* [out] current head position. */
)
{
    /* Convert MEMS axes (x = right, y = forwards)
     * to standard orientation (x = forwards, y = left).
     */
    att_motion_data_t a;
    att_motion_data_t g;
    a.x = accel_data->y;  /* roll */
    a.y = (int16)-accel_data->x; /* pitch */
    a.z = accel_data->z;  /* yaw */
    g.x = gyro_data->y;   /* roll */
    g.y = (int16)-gyro_data->x;  /* pitch */
    g.z = gyro_data->z;   /* yaw */
    Attitude_Update(&a, &g, ts_us, att_quat_q15_local_in, att_quat_q15_local_out, att_quat_q15_head);
}

#endif /* INCLUDE_ATTITUDE_FILTER */
