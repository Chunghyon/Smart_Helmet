/*!
\copyright  Copyright (c) 2018 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       attitude_filter.h
\brief      Public API for attitude filter library.
*/

#ifndef _ATTITUDE_FILTER_H
#define _ATTITUDE_FILTER_H

#include <types.h>

/*! Euler in degrees. */
typedef struct att_euler_deg_t {
    int16 yaw; /*! +-180 +ve left */
    int16 pitch; /*! +-90 +ve down */
    int16 roll; /*! +-180 +ve right */
} att_euler_deg_t;

typedef struct {
    bool valid;
    uint32 timestamp_ms;
    int32 rads_q16[3];
    int32 rads_q16_per_s[3];
} att_head_orientation_rotation_vector_t;

typedef struct att_quat_16_t
{
    int16 w;
    int16 x;
    int16 y;
    int16 z;
} att_quat_16_t;

typedef struct att_motion_data_t
{
    int16 x;
    int16 y;
    int16 z;
} att_motion_data_t;

/*! Quaternion for local, sibling and head attitude. */
typedef struct {
    bool valid;
    uint32 timestamp_ms; /*!< timestamp of result */
    att_quat_16_t q15;   /*!< 4xint16 wxyz (norm is 2**15) */
} att_quat_q15_t;


/* set the parameters of the head attitude auto cal filtering.
 * This (optionally) returns the head orientation to pointing forward.
 *
 * A small correction is applied to the quaternion on every sensor update
 * the size of the correction is:
 *
 * (quaternion to return to zero pitch roll yaw)*auto_cal_num/2**auto_cal_bitshift
 *
 * The default values for these paramters are:
 *
 * auto_cal_slow_filter_on 1
 * auto_cal_num 1
 * auto_cal_bitshift 13
 *
 * Therefore we move 0.012% of the way back to pointing forward on every
 * sample (so at 100Hz we will move around 50% of the way back to forward in about 40s).
 * Setting auto_cal_bitshift to 4 will correct 1/16th of the error every sample.
 * Very fast recentring should only be used briefly when use case demands it.
 */
void Attitude_HeadAutoCalFilterSet(
    uint32 auto_cal_slow_filter_on,
    uint32 auto_cal_num,
    uint32 auto_cal_bitshift
);

void Attitude_HeadInit(void);

void Attitude_HeadQuatGet(att_quat_q15_t *att_quat_q15);

void Attitude_HeadRotationVectorGet(att_head_orientation_rotation_vector_t *rotation_vector);

void Attitude_HeadSiblingQuatUpdate(att_quat_q15_t *att_quat_q15);

/* Initialise data structures. */
void Attitude_Init(void);

/* Convert quaternion in 2**15 scale to Euler in degrees. */
void Attitude_QuatToEuler(att_quat_16_t *q, att_euler_deg_t *e);

/* Reset the attitude to: horizontal, pointing forward. */
void Attitude_Reset(void);

/* Update the attitude from sensor data. */
/* Get back local unfiltered, local filtered and head postitions. */
void Attitude_Update(
    const att_motion_data_t *acc,
    const att_motion_data_t *gyro,
    uint32 ts_us,
    att_quat_q15_t *att_quat_q15_local_in, /* [out] unfiltered local quat. */
    att_quat_q15_t *att_quat_q15_local_out, /* [out] filtered local quat. */
    att_quat_q15_t *att_quat_q15_head /* [out] current head position. */
);

typedef enum {
    ATT_PAR_UPDATE_GYRO_COV_TIME_STEP,    /*!< Time in ms after which the covariance update takes place.
                                               The larger value, the faster and less accurate the algorithm.
                                               Default: 30 */
    ATT_PAR_MAX_GYRO_BIAS,                /*!< Gyro bias in deg/s */
    ATT_PAR_DUR_STABLE_ACC_FOR_UPDATE,    /*!< Time in ms after which the accelerometer update takes place.
                                               Default: 500 */
    ATT_ACC_DATA_RANGE,                   /*!< Accelerometer data range */
    ATT_GYRO_DATA_RANGE,                  /*!< Gyro data range */
    ATT_COMPUTE_CONFIDENCE                /*!< Is the confidence needed; true/false? Default: false. */
} AttParamKey;

/* Update parameters using the key in AttParamsKey */
void Attitude_SetParam(const AttParamKey key, const uint16 value);

#endif /* _ATTITUDE_FILTER_H */
