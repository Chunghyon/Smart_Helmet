/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       spatial_private_data.h
\brief      Private header file for Spatial data support.
*/

#ifndef SPATIAL_DATA_PRIVATE_H
#define SPATIAL_DATA_PRIVATE_H

#ifdef INCLUDE_SPATIAL_DATA

#include "attitude_filter.h"
#include "spatial_data.h"

#include <logging.h>
#include <message.h>
#include <panic.h>
#include <system_clock.h>
#include <stream.h>
#include <sink.h>
#include <source.h>
#include <stdlib.h>
#include <sensor.h>
#include <bt_device.h>

/* The interval to read the next sniff instance again in case the current read failed or
 * when the returned sniff_subrate_clock value is very close to the current time or is in the past.
 * The typical sniff interval in the buddy link is around 200ms, so read the next instance with a value
 * less than 100ms, but with a value which is more than the typical sniff window (few tens of millisec).
 */
#define SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE_INTERVAL_MS 59

/* Schedule to send the data to over earbud few milliseconds before the next sniff instance. */
/* TODO: verify (by capturing the air traffic) if the data is actually getting sent in the BT sniff window. */
#define SPATIAL_DATA_SEND_DATA_AND_NEXT_SNIFF_INSTANCE_OFFSET_MS 10

/* Read the next sniff instance again in case the current returned next sniff subrate clock is far in the future. */
#define SPATIAL_DATA_MAX_EXPECTED_NEXT_SNIFF_INSTANCE_OFFSET_MS 1000

/* Spatial data sync data exchange offset from the current system time for synchronised execution
 * in both the earbuds, i.e. enough time to send the packet in the buddy link and then to forward it to the client
 * application.
 */
#define SPATIAL_DATA_SYNC_DATA_EXCHANGE_OFFSET_US ((SPATIAL_DATA_SEND_DATA_AND_NEXT_SNIFF_INSTANCE_OFFSET_MS + 60) * US_PER_MS)

/*! Default sample rate in Hz */
#define SPATIAL_DATA_DEFAULT_SAMPLE_RATE_HZ 52

/*! UUID for Qualcomm Spatial Data Profile Service */
/* TODO: this needs to be moved to a common BT file? */
/* CD7964B6-C89D-4B46-8BFE-248E8878A08E */
#define UUID_SPATIAL_DATA_PROFILE_SERVICE 0xcd,0x79,0x64,0xb6,0xc8,0x9d,0x4b,0x46,0x8b,0xfe,0x24,0x8e,0x88,0x78,0xa0,0x8e

/*! Structure holding data for the spatial_data module */
typedef struct
{
    /*! Spatial data task */
    Task Task;

    /*! Spatial data enable status containing source info */
    spatial_data_enable_status_t enabled_status;

    /*! Client task registered for spatial audio data */
    Task client_task;

    /*! Motion sensor BUS */
    Bus bus_id;

    /*! Motion sensor ID */
    Sensor sensor_id;

    /*! Spatial data sensor sampling interval */
    uint16 sensor_sample_rate_hz;

    /*! Spatial data report */
    spatial_data_report_id_t report_id;

    /*! Spatial data source */
    spatial_data_source_t data_source;

    /*! Pointer to the motion sensor chip configuration in application. */
    const SPATIAL_DATA_MOTION_SENSOR_CHIP_CONFIG_T *motion_sensor_config_ptr;

#ifdef INCLUDE_ATTITUDE_FILTER
    /*! Attitude filter enabled - initialised successfully. */
    bool attitude_filter_enabled:1;
    
#ifdef INCLUDE_SENSOR_PROFILE
    /* Orientation rxed from other earbud. */
    bool orientation_rxed:1;
#endif /* INCLUDE_SENSOR_PROFILE */
    
#endif /* INCLUDE_ATTITUDE_FILTER */

#ifdef INCLUDE_HIDD_PROFILE
    /*! HID device wallclock state */
    wallclock_state_t hidd_wallclock_state;

    /*! HID device wallclock conversion enabled. */
    bool hidd_wallclock_enabled:1;

#endif /* INCLUDE_HIDD_PROFILE */

#ifdef INCLUDE_SENSOR_PROFILE
    /*! Sensor profile connected status */
    bool sensor_profile_connected:1;

    /*! Waiting for signalling peer cfm message */
    bool pending_cfm_from_secondary:1;

    /*! Sensor profile wallclock conversion enabled. */
    bool sensor_profile_wallclock_enabled:1;

    /*! Sync data ready to be send. */
    bool sync_data_exchange_set:1;

    /*! Sensor profile L2CAP channel sink */
    Sink sensor_profile_l2cap_sink;

    /*! Sensor profile wallclock state */
    wallclock_state_t sensor_profile_wallclock_state;

    /*! Sync data command. */
    uint8 sync_data_command;

    /*! Sync data ponter. */
    uint8 *sync_data_ptr;

    /*! Sync data length. */
    uint16 sync_data_length;
#endif /* INCLUDE_SENSOR_PROFILE */

} spatial_data_t;

extern spatial_data_t spatial_data;

/*! Motion sensor data structure */
typedef struct
{
    /*! Current value of motion sensor timestamp */
    uint32 timestamp;

    /*! Current value of temperature */
    int16 temp;

    /*! Current values of Gyroscope */
    motion_data_t gyro;

    /*! Current values of Accelerometer */
    motion_data_t accel;

    /*! Quaternion values */
    quaternion_data_t quaternion;
} motion_sensor_data_t;

/*! \brief Get spatial data source

    This function will return the entity which is generating the spatial data.

    \param None
    \return data source
*/
spatial_data_source_t SpatialData_GetDataSource(void);

/*! \brief Register sensor profile

    This function will register with BT sensor profile for communicating with the other earbud.

    \param None
    \return TRUE if registration is successful, else FALSE
*/
bool SpatialData_RegisterSensorProfile(void);

/*! \brief Handle enable/disable of spatial data

    This function will internally handle enabling/disabling of spatial data when requested by upper application, 
    or by the remote device (Phone/PC), or by the peer earbud.

    "SPATIAL_DATA_ERROR_MESSAGE_IND" message shall be sent to the registered client 
    if the attempt to enable/disable spatial audio in the secondary fails.

    \param enable - Spatial data enable input
    \param sensor_sample_rate_hz - Sensor sample rate in Hz. Spatial data will set the default value (100Hz) if the input is zero.
    \param data_report - Requested data report type.

    \return TRUE if enable/disable is successful for the local device, else returns FALSE.
*/
bool SpatialData_HandleEnable(spatial_data_enable_status_t enable_input, uint16 sensor_sample_rate_hz, spatial_data_report_id_t report_id);

/*! \brief Enable/Disable spatial data in secondary

    The primary earbud will trigger enabling/disabling of spatial data in the secondary using the sensor profile.

    \param None

    \return TRUE if enabling/disabling of spatial data is initiated successfully, else FALSE.
*/
bool SpatialData_EnableSecondary(void);

/*! \brief Control data exchange between earbuds

    This function will start/stop data exchange between earbuds during spatial data.

    \param None
    \return None
*/
void SpatialData_ControlDataExchangeBetweenEarbuds(void);

/*! Quaternion info passed between earbuds */
typedef struct
{
    /*! Timestamp */
    rtime_t    timestamp;

    /*! Quaternion data */
    quaternion_data_t  quaternion_data;
} quaternion_info_exchange_t;

/*! \brief Initialise attitude filter algorithm 

    NOTE: this must be only invoked after initialising the motion sensor chip.

    \return TRUE if feature initialisation was successful, otherwise FALSE.
*/
bool SpatialData_AttitudeFilterInit(void);

/*! \brief Convert raw sensor data to quaternion

    This function will convert the input raw sensor data to quaternion.

    NOTE: The input gyro/accelerometer raw data can be modified by the algorithm for drift corrections.

    \param sensor_data Pointer to sensor data to be sent to remote device.

    \return TRUE if the conversion is successful, else FALSE.
*/
bool SpatialData_ConvertSensorDataToQuaternion(motion_sensor_data_t *sensor_data);

/*! \brief Get the current value of orientation

    \param quaternion_position Pointer to return the quaternion values

    \return TRUE if getting the orientation is successful, otherwise FALSE.
*/
bool SpatialData_AttitudeFilterGetCurrentOrientation(quaternion_info_exchange_t *quaternion_info);

/*! \brief Update orientation from the other earbud

    \param quaternion_position Pointer containing the Quaternion values

    \return TRUE if the data is successfully passed to the attitude filter, else returns FALSE.
*/
bool SpatialData_AttitudeFilterUpdateOrientationFromOtherEarbud(const quaternion_info_exchange_t *quaternion_info);

/*! \brief Get the head orientation vector.  Scaled to +- pi rads.

    \param rot_vector Pointer to head orientation vector structure.

    \return None.
*/
void SpatialData_AttitudeFilterHeadRotationVectorGet(att_head_orientation_rotation_vector_t *rot_vector);

/*! \brief Send sensor data to remote device

    This function will send input sensor data to remote device over HID

    \param sensor_data Pointer to sensor data to be sent to remote device.

    \return TRUE if the data is sent successfully, else FALSE.
*/
void SpatialData_SendSensorDataToRemoteDevice(const motion_sensor_data_t *sensor_data);

#endif /* INCLUDE_SPATIAL_DATA */

#endif /* SPATIAL_DATA_PRIVATE_H */
