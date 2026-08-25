/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       spatial_data_remote.c
\brief      Provides spatial data transport with remote device over HID using 3DAMP Specification
*/

#ifdef INCLUDE_SPATIAL_DATA

#ifdef INCLUDE_HIDD_PROFILE

#include "attitude_filter.h"
#include "spatial_data.h"
#include "spatial_data_private.h"
#include "hidd_profile.h"
#include "csr_bt_profiles.h"

/*! HIDD Feature Reports ID - from 3DAMP Specification */
typedef enum 
{
#ifdef INCLUDE_3DAMP
    spatial_data_3damp_supported    = 0x1F,
    spatial_data_sensor_type        = 0x2F,
    spatial_data_sensor_reporting   = 0x3F,
#endif
    head_tracking_sensor_reporting  = 0x01,
    head_tracking_sensor_info       = 0x02,

} spatial_data_hidd_feature_report_id_t;


/*! Source location send to remote device using 3DAMP Spec */
typedef enum
{
    /*! HIDD data for left earbud */
    spatial_hidd_source_left   = 1,

    /*! HIDD data for right earbud */
    spatial_hidd_source_right  = 2,

    /*! HIDD data for head */
    spatial_hidd_source_head   = 3
} spatial_data_hidd_source_t;


#ifdef INCLUDE_3DAMP
/* HIDD report lengths - from 3DAMP Specification */
#define SPATIAL_DATA_SENSOR_TYPE_LEN                   32
#define SPATIAL_DATA_SENSOR_STATUS_REPORTING_LEN       3
#define SPATIAL_DATA_UUID_SUPPORTED_LEN                16
#define SPATIAL_DATA_HIDD_REPORT_1AT_LEN               21
#define SPATIAL_DATA_HIDD_REPORT_1BT_LEN               21
#define SPATIAL_DATA_HIDD_REPORT_DEBUG_LEN             41
#endif

/* HIDD report lengths - Head Tracking Report */
#define SPATIAL_DATA_HIDD_REPORT_1_LEN   13

/* HID report lenghts for Head Tracking Protocol */
#define HEAD_TRACKING_SENSOR_INFO_LEN         (39)
#define HEAD_TRACKING_SENSOR_REPORTING_LEN     (1)
#define HEAD_TRACKING_SENSOR_INPUT_REPORT_LEN (13)

#define HEAD_TRACKING_VERSION_LEN (23)
#define HEAD_TRACKING_VERSION "#AndroidHeadTracker#1.0"


/* Gyro DPS are in units of 125 in the 3DAMP Specification. */
#define SPATIAL_DATA_HIDD_REPORT_GYRO_DPS_SCALE        125

static void spatialData_HiddTaskHandler(Task task, MessageId id, Message message);
static TaskData spatialDataHiddTask = {spatialData_HiddTaskHandler};
static spatial_data_hidd_source_t spatialData_HiddDataSource(spatial_data_source_t data_source);

bool SpatialData_SetHiddSdp(uint16 data_len, const uint8 *data_ptr)
{
    DEBUG_LOG_DEBUG("%s",__func__);

    if (!data_ptr)
    {
        DEBUG_LOG_WARN("%s:Input data_ptr is NULL",__func__);
        return FALSE;
    }

    /* Register HIDD profile with the input descriptors */
    HiddProfile_RegisterDevice(&spatialDataHiddTask, data_len, data_ptr);

    return TRUE;
}

void SpatialData_SendSensorDataToRemoteDevice(const motion_sensor_data_t *sensor_data)
{
    DEBUG_LOG_VERBOSE("%s", __func__);
    spatial_data_hidd_source_t data_source = spatialData_HiddDataSource(SpatialData_GetDataSource());

#ifndef INCLUDE_3DAMP
    UNUSED(data_source);
#endif

    if(!sensor_data)
    {
        DEBUG_LOG_ERROR("%s:Input sensor_data is NULL", __func__);
        Panic();
    }

    /* Check if HIDD is connected. */
    if (!HiddProfile_IsConnected())
    {
        DEBUG_LOG_WARN("%s:HIDD not connected for sending reports", __func__);
        return;
    }

    switch (spatial_data.report_id)
    {
        case spatial_data_report_1:
        {
            uint8 *data = malloc(SPATIAL_DATA_HIDD_REPORT_1_LEN);
            att_head_orientation_rotation_vector_t rot_vector;
            int16 value;

            if (!data)
            {
                DEBUG_LOG_WARN("%s:Memory creation failed for HIDD report 1", __func__);
                return;
            }

            /* Get the latest Head Rotation Vector */
            Attitude_HeadRotationVectorGet(&rot_vector);

            /* Copy Orientation data converting to +/- pi by dividing by (2 x pi) */
            value = (int16)((rot_vector.rads_q16[0] * 163) / 1024);
            memcpy(&data[0], &value, sizeof(value));

            value = (int16)((rot_vector.rads_q16[1] * 163) / 1024);
            memcpy(&data[2], &value, sizeof(value));

            value = (int16)((rot_vector.rads_q16[2] * 163) / 1024);
            memcpy(&data[4], &value, sizeof(value));

            /* Copy Angular Velocity data conveting to 32rad/s by dividing by 64 */
            value = (int16)(rot_vector.rads_q16_per_s[0] / 64);
            memcpy(&data[6], &value, sizeof(value));

            value = (int16)(rot_vector.rads_q16_per_s[1] / 64);
            memcpy(&data[8], &value, sizeof(value));

            value = (int16)(rot_vector.rads_q16_per_s[2] / 64);
            memcpy(&data[10], &value, sizeof(value));

            /* Copy Reference Frame Count data */
            memcpy(&data[12], 0, sizeof(uint8));

            DEBUG_LOG("Sent spatial_data_report_1:");
            DEBUG_LOG_DATA(data, SPATIAL_DATA_HIDD_REPORT_1_LEN);

            /* Send the HID report */
            HiddProfile_DataReq((uint8)spatial_data.report_id, SPATIAL_DATA_HIDD_REPORT_1_LEN, data);
            free(data);
        }
        break;

#ifdef INCLUDE_3DAMP
        case spatial_data_report_1at:
        {
            /* Raw sensor data */
            uint8 i = 0;
            rtime_t wallclock_time;

            uint8 *data = malloc(SPATIAL_DATA_HIDD_REPORT_1AT_LEN);
            if (!data)
            {
                DEBUG_LOG_WARN("%s:Memory creation failed for 3DAMP HIDD report 1AT", __func__);
                return;
            }

            /* Set the current data source. */
            data[i++] = (uint8)data_source;

            /* Convert the sensor timestamp to BT clock (wallclock) */
            if (spatial_data.hidd_wallclock_enabled && 
               RtimeLocalToWallClock(&spatial_data.hidd_wallclock_state, (rtime_t)sensor_data->timestamp, &wallclock_time))
            {
                memcpy(&data[i], &wallclock_time, sizeof(wallclock_time));
            }
            else
            {
                /* Send the sensor timestamp as it is if wallclock conversion failed. */
                DEBUG_LOG_WARN("%s:Sensor Timestamp to wallclock conversion failed", __func__);
                memcpy(&data[i], &sensor_data->timestamp, sizeof(wallclock_time));
            }
            i += sizeof(wallclock_time);

            /* Temperature */
            memcpy(&data[i], &sensor_data->temp, sizeof(sensor_data->temp));
            i += sizeof(sensor_data->temp);

            /* X-axis linear accel */
            memcpy(&data[i], &sensor_data->accel.x, sizeof(sensor_data->accel.x));
            i += sizeof(sensor_data->accel.x);

            /* Y-axis linear accel */
            memcpy(&data[i], &sensor_data->accel.y, sizeof(sensor_data->accel.y));
            i += sizeof(sensor_data->accel.y);

            /* Z-axis linear accel */
            memcpy(&data[i], &sensor_data->accel.z, sizeof(sensor_data->accel.z));
            i += sizeof(sensor_data->accel.z);

            /* Send the Accel scale configuration - read the value from the pointer passed by the application. */
            if (spatial_data.motion_sensor_config_ptr)
            {
                data[i++] = (uint8)spatial_data.motion_sensor_config_ptr->accel_scale;
            }
            else
            {
                /* Default value in case not read from the application. */
                data[i++] = 16;
            }
                
            /* X-axis velocity  */
            memcpy(&data[i], &sensor_data->gyro.x, sizeof(sensor_data->gyro.x));
            i += sizeof(sensor_data->gyro.x);

            /* Y-axis velocity */
            memcpy(&data[i], &sensor_data->gyro.y, sizeof(sensor_data->gyro.y));
            i += sizeof(sensor_data->gyro.y);

            /* Z-axis velocity */
            memcpy(&data[i], &sensor_data->gyro.z, sizeof(sensor_data->gyro.z));
            i += sizeof(sensor_data->gyro.z);

            /* Send the Gyro scale configuration - read the value from the pointer passed by the application. */
            if (spatial_data.motion_sensor_config_ptr)
            {
                data[i++] = (uint8)(spatial_data.motion_sensor_config_ptr->gyro_scale/SPATIAL_DATA_HIDD_REPORT_GYRO_DPS_SCALE);
            }
            else
            {
                /* Default value in case not read from the application. */
                data[i++] = 8;
            }

            HiddProfile_DataReq((uint8)spatial_data.report_id, SPATIAL_DATA_HIDD_REPORT_1AT_LEN, data);
            free(data);
        }
        break;

#ifdef INCLUDE_ATTITUDE_FILTER
        case spatial_data_report_1bt:
        {
            uint8 i = 0;
            rtime_t wallclock_time;

            /* Raw sensor data */
            uint8 *data = malloc(SPATIAL_DATA_HIDD_REPORT_1BT_LEN);
            if (!data)
            {
                DEBUG_LOG_WARN("%s:Memory creation failed for 3DAMP HIDD report 1BT", __func__);
                return;
            }

            /* Set the current data source. */
            data[i++] = (uint8)data_source;

            /* Convert the sensor timestamp to BT clock (wallclock) */
            if (spatial_data.hidd_wallclock_enabled && 
              RtimeLocalToWallClock(&spatial_data.hidd_wallclock_state, (rtime_t)sensor_data->timestamp, &wallclock_time))
            {
               memcpy(&data[i], &wallclock_time, sizeof(wallclock_time));
            }
            else
            {
               /* Send the sensor timestamp as it is if wallclock conversion failed. */
               DEBUG_LOG_WARN("%s:Sensor Timestamp to wallclock conversion failed",__func__);
               memcpy(&data[i], &sensor_data->timestamp, sizeof(wallclock_time));
            }
            i += sizeof(wallclock_time);

            /* Temperature */
            memcpy(&data[i], &sensor_data->temp, sizeof(sensor_data->temp));
            i += sizeof(sensor_data->temp);

            /* Angular position (X, Y, Z each of int16) - all zeros for now. */
            i += sizeof(int16) * 3;

            /* Quaternion X-axis */
            memcpy(&data[i], &sensor_data->quaternion.x, sizeof(sensor_data->quaternion.x));
            i += sizeof(sensor_data->quaternion.x);

            /* Quaternion Y-axis */
            memcpy(&data[i], &sensor_data->quaternion.y, sizeof(sensor_data->quaternion.y));
            i += sizeof(sensor_data->quaternion.y);

            /* Quaternion Z-axis */
            memcpy(&data[i], &sensor_data->quaternion.z, sizeof(sensor_data->quaternion.z));
            i += sizeof(sensor_data->quaternion.z);

            /* Quaternion W */
            memcpy(&data[i], &sensor_data->quaternion.w, sizeof(sensor_data->quaternion.w));
            i += sizeof(sensor_data->quaternion.w);

            HiddProfile_DataReq((uint8)spatial_data.report_id, SPATIAL_DATA_HIDD_REPORT_1BT_LEN, data);
            free(data);
        }
        break;

        case spatial_data_report_debug:
        {
           /* Raw sensor data */

           uint8 i = 0;
           rtime_t wallclock_time;

           uint8 *data = malloc(SPATIAL_DATA_HIDD_REPORT_DEBUG_LEN);
           if (!data)
           {
               DEBUG_LOG_WARN("%s:Memory creation failed for 3DAMP HIDD report DEBUG", __func__);
               return;
           }

           /* Set the current data source. */
           data[i++] = (uint8)data_source;

           /* Convert the sensor timestamp to BT clock (wallclock) */
           if (spatial_data.hidd_wallclock_enabled && 
              RtimeLocalToWallClock(&spatial_data.hidd_wallclock_state, (rtime_t)sensor_data->timestamp, &wallclock_time))
           {
               memcpy(&data[i], &wallclock_time, sizeof(wallclock_time));
           }
           else
           {
               /* Send the sensor timestamp as it is if wallclock conversion failed. */
               DEBUG_LOG_WARN("%s:Sensor Timestamp to wallclock conversion failed",__func__);
               memcpy(&data[i], &sensor_data->timestamp, sizeof(wallclock_time));
           }
           i += sizeof(wallclock_time);

           /* Temperature */
           memcpy(&data[i], &sensor_data->temp, sizeof(sensor_data->temp));
           i += sizeof(sensor_data->temp);

           /* X-axis linear accel */
           memcpy(&data[i], &sensor_data->accel.x, sizeof(sensor_data->accel.x));
           i += sizeof(sensor_data->accel.x);

           /* Y-axis linear accel */
           memcpy(&data[i], &sensor_data->accel.y, sizeof(sensor_data->accel.y));
           i += sizeof(sensor_data->accel.y);

           /* Z-axis linear accel */
           memcpy(&data[i], &sensor_data->accel.z, sizeof(sensor_data->accel.z));
           i += sizeof(sensor_data->accel.z);

           /* Send the Accel scale configuration - read the value from the pointer passed by the application. */
           if (spatial_data.motion_sensor_config_ptr)
           {
               data[i++] = (uint8)spatial_data.motion_sensor_config_ptr->accel_scale;
           }
           else
           {
               /* Default value in case not read from the application. */
               data[i++] = 16;
           }

           /* X-axis velocity  */
           memcpy(&data[i], &sensor_data->gyro.x, sizeof(sensor_data->gyro.x));
           i += sizeof(sensor_data->gyro.x);

           /* Y-axis velocity */
           memcpy(&data[i], &sensor_data->gyro.y, sizeof(sensor_data->gyro.y));
           i += sizeof(sensor_data->gyro.y);

           /* Z-axis velocity */
           memcpy(&data[i], &sensor_data->gyro.z, sizeof(sensor_data->gyro.z));
           i += sizeof(sensor_data->gyro.z);

           /* Send the Gyro scale configuration - read the value from the pointer passed by the application. */
           if (spatial_data.motion_sensor_config_ptr)
           {
               data[i++] = (uint8)(spatial_data.motion_sensor_config_ptr->gyro_scale/SPATIAL_DATA_HIDD_REPORT_GYRO_DPS_SCALE);
           }
           else
           {
               /* Default value in case not read from the application. */
               data[i++] = 8;
           }

           /* Angular position (X, Y, Z each of int16) - all zeros for now. */
           i += sizeof(int16) * 3;
           
           /* Quaternion X-axis */
           memcpy(&data[i], &sensor_data->quaternion.x, sizeof(sensor_data->quaternion.x));
           i += sizeof(sensor_data->quaternion.x);
            
           /* Quaternion Y-axis */
           memcpy(&data[i], &sensor_data->quaternion.y, sizeof(sensor_data->quaternion.y));
           i += sizeof(sensor_data->quaternion.y);
            
           /* Quaternion Z-axis */
           memcpy(&data[i], &sensor_data->quaternion.z, sizeof(sensor_data->quaternion.z));
           i += sizeof(sensor_data->quaternion.z);
            
           /* Quaternion W */
           memcpy(&data[i], &sensor_data->quaternion.w, sizeof(sensor_data->quaternion.w));
           i += sizeof(sensor_data->quaternion.w);

           /* Magnetic flux - all zeros for now. */

           HiddProfile_DataReq((uint8)spatial_data.report_id, SPATIAL_DATA_HIDD_REPORT_DEBUG_LEN, data);
           free(data);
        }
        break;
#endif /* INCLUDE_ATTITUDE_FILTER */
#endif /* INCLUDE_3DAMP */

        default:
        {
           /* Unsupported HIDD report type */
           DEBUG_LOG_WARN("%s:Unsupported HIDD report type:%d for forwarding data",__func__, spatial_data.report_id);
        }
        break;
    }
}


static void spatialData_HiddTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case HIDD_PROFILE_DATA_CFM:
        {
            DEBUG_LOG_VERBOSE("%s:HIDD_PROFILE_DATA_CFM",__func__);
        }
        break;

        case HIDD_PROFILE_CONNECT_IND:
        {
            DEBUG_LOG_INFO("%s:HIDD_PROFILE_CONNECT_IND",__func__);

            HIDD_PROFILE_CONNECT_IND_T *conn_ind = (HIDD_PROFILE_CONNECT_IND_T*)message;
            PanicNull(conn_ind);

            /* Create a wall-clock time conversion handle for the L2CAP Sink for the HIDD connection. */
            if (conn_ind->status == hidd_profile_status_connected)
            {
                spatial_data.hidd_wallclock_enabled = 
                    RtimeWallClockEnable(&spatial_data.hidd_wallclock_state, StreamL2capSink(conn_ind->connid));
            }
        }
        break;

        case HIDD_PROFILE_DISCONNECT_IND:
        {
            DEBUG_LOG_INFO("%s:HIDD_PROFILE_DISCONNECT_IND",__func__);
        }
        break;

        case HIDD_PROFILE_GET_REPORT_IND:
        {
            DEBUG_LOG_INFO("%s:HIDD_PROFILE_GET_REPORT_IND",__func__);
            HIDD_PROFILE_GET_REPORT_IND_T *ind = (HIDD_PROFILE_GET_REPORT_IND_T*)message;

            PanicNull(ind);
            DEBUG_LOG_INFO("%s:Report Type:0x%x, Report ID:0x%x, Report Size:0x%x",__func__, ind->report_type, ind->reportid, ind->size);

            switch(ind->report_type)
            {
                case hidd_profile_report_type_feature:
                {
                    switch(ind->reportid)
                    {
#ifdef INCLUDE_3DAMP
                        case spatial_data_3damp_supported:
                        {
                            /* Send a control response data - 16-octet UUID */
                            CsrBtUuid128 spatial_data_uuid = {UUID_SPATIAL_DATA_PROFILE_SERVICE};
                            uint16 uuid_len = sizeof(spatial_data_uuid);
                            uint8 *data = PanicUnlessMalloc(sizeof(spatial_data_uuid));
                            memset(data, 0, uuid_len);

                            memcpy(data, spatial_data_uuid, uuid_len);

                            HiddProfile_DataRes(hidd_profile_report_type_feature, ind->reportid, uuid_len, data);
                            free(data);
                        }
                        break;

                        case spatial_data_sensor_type:
                        {
                            /* Sensor type string - 8 bytes */
                            /* Sensor serial number - 8 bytes */
                            /* Sensor calibration data - 16 bytes*/
                            /* Send a control response data: */
                            uint8 *data = PanicUnlessMalloc(SPATIAL_DATA_SENSOR_TYPE_LEN);
                            memset(data, 0, SPATIAL_DATA_SENSOR_TYPE_LEN);

                            /* Copy the motion sensor chip name - from the pointer in the application.*/
                            if (spatial_data.motion_sensor_config_ptr)
                            {
                                memcpy(data, spatial_data.motion_sensor_config_ptr->chip_name, SPATIAL_DATA_MOTION_SENSOR_CHIP_NAME_3DAMP_LEN);
                            }

                            /* Sensor serial number - all zeros for now! */

                            /* Sensor calibration data - all zeros for now! */

                            HiddProfile_DataRes(hidd_profile_report_type_feature, ind->reportid, SPATIAL_DATA_SENSOR_TYPE_LEN, data);
                            free(data);
                        }
                        break;

                        case spatial_data_sensor_reporting:
                        {
                            /* Send a control response data */
                            uint8 *data = PanicUnlessMalloc(SPATIAL_DATA_SENSOR_STATUS_REPORTING_LEN);
                            memset(data, 0, SPATIAL_DATA_SENSOR_STATUS_REPORTING_LEN);

                            /* Current value of data report type */
                            data[0] = (uint8)spatial_data.report_id;

                           /* Current value of sensor sample interval */
                            memcpy(&data[1], &spatial_data.sensor_sample_rate_hz, 2);

                            HiddProfile_DataRes(hidd_profile_report_type_feature, ind->reportid, SPATIAL_DATA_SENSOR_STATUS_REPORTING_LEN, data);
                            free(data);
                        }
                        break;
#endif /* INCLUDE_3DAMP */

                        case head_tracking_sensor_info:
                        {
                            static const uint8 uuid_const[] = {
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x54
                                 }; /* 0,0,0,0,0,0,0,0,'B','T' */
                            bdaddr primary = {0};

                            /* Send a control response data */
                            uint8 *data = PanicUnlessMalloc(HEAD_TRACKING_SENSOR_INFO_LEN);

                            memset(data, 0, HEAD_TRACKING_SENSOR_INFO_LEN);

                            /* Copy the head tracking version */
                            memcpy(&data[0], HEAD_TRACKING_VERSION, HEAD_TRACKING_VERSION_LEN);

                            /* Copy the UUID const section */
                            memcpy(&data[HEAD_TRACKING_VERSION_LEN], uuid_const, sizeof(uuid_const));

                            /* Copy the 6 byte BT MAC in Big Endian format [nap | uap | lap] */
                            PanicFalse(appDeviceGetPrimaryBdAddr(&primary));
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+0] = (uint8)((primary.nap >> 8) & 0xff);
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+1] = (uint8)(primary.nap & 0xff);
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+2] = (uint8)(primary.uap & 0xff);
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+3] = (uint8)((primary.lap >> 16) & 0xff);
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+4] = (uint8)((primary.lap >>  8) & 0xff);
                            data[HEAD_TRACKING_VERSION_LEN + sizeof(uuid_const)+5] = (uint8)((primary.lap >>  0) & 0xff);

                            DEBUG_LOG_INFO("head_tracking_sensor_info:");
                            DEBUG_LOG_DATA_INFO(data, HEAD_TRACKING_SENSOR_INFO_LEN);

                            /* Send the report */
                            HiddProfile_DataRes(hidd_profile_report_type_feature, ind->reportid, HEAD_TRACKING_SENSOR_INFO_LEN, data);
                            free(data);
                        }
                        break;

                        case head_tracking_sensor_reporting:
                        {
                            #define HEAD_TRACKING_REPORTING_ALL   (1<<0)
                            #define HEAD_TRACKING_REPORTING_NONE  (0<<0)
                            #define HEAD_TRACKING_POWER_FULL      (1<<1)
                            #define HEAD_TRACKING_POWER_OFF       (0<<1)

                            /* Send a control response data */
                            uint8 *data = PanicUnlessMalloc(HEAD_TRACKING_SENSOR_REPORTING_LEN);
                            uint16 interval;

                            memset(data, 0, HEAD_TRACKING_SENSOR_REPORTING_LEN);

                            /* bit0 Reporting State: 0=None, 1=All */
                            /* bit1 Power State    : 0=Off,  1=Full */
                            /* bit2-7 Interval     : 0=10ms - 0x63=100ms */
                            if (spatial_data.enabled_status == spatial_data_enabled_remote)
                            {
                                /* Currently tie power status to reporting status */
                                data[0] |= HEAD_TRACKING_REPORTING_ALL | HEAD_TRACKING_POWER_FULL;
                            }


                            /* If the rate hasn't been set... set the default */
                            if (!spatial_data.sensor_sample_rate_hz)
                                spatial_data.sensor_sample_rate_hz = SPATIAL_DATA_DEFAULT_SAMPLE_RATE_HZ;

                            /* Convert to 6-bit ms interval with range 10ms - 100ms */
                            #define INTERVAL_MIN   (10) /* ms */
                            #define INTERVAL_MAX  (100) /* ms */
                            #define INTERVAL_RANGE (63) /* 6-bits*/

                            interval = (((((1000 * INTERVAL_RANGE)/ spatial_data.sensor_sample_rate_hz) /* multiply by physical range and convert to ms */
                                        - INTERVAL_MIN)   /* scale down to min interval */                                        
                                        + (INTERVAL_MAX - INTERVAL_MIN - 1)) /* round up */
                                        / (INTERVAL_MAX - INTERVAL_MIN));    /* scale to interval range */

                            data[0] |= (interval & 0x3f) << 2;

                            DEBUG_LOG_INFO("head_tracking_sensor_reporting: 0x%02x", data[0]);

                            /* Send the report */
                            HiddProfile_DataRes(hidd_profile_report_type_feature, ind->reportid, HEAD_TRACKING_SENSOR_REPORTING_LEN, data);
                            free(data);

                        }
                        break;

                        default:
                        {
                            DEBUG_LOG_WARN("%s:Unsupported GET_REPORT Feature report ID:0x%x", __func__,ind->reportid);
                            HiddProfile_Handshake(hidd_profile_handshake_invalid_report_id);
                        }
                        break;
                    }
                    break;
                }
                default:
                {
                    DEBUG_LOG_WARN("%s:Unhandled HIDD_PROFILE_GET_REPORT_IND type:0x%x",__func__, ind->report_type);
                    HiddProfile_Handshake(hidd_profile_handshake_invalid_parameter);
                }
                break;

            }
            break;
        }
        case HIDD_PROFILE_ACTIVATE_CFM:
        {
            DEBUG_LOG_INFO("%s:HIDD_PROFILE_ACTIVATE_CFM",__func__);
        }
        break;

        case HIDD_PROFILE_DEACTIVATE_CFM:
        {
            DEBUG_LOG_INFO("%s:HIDD_PROFILE_DEACTIVATE_CFM",__func__);
        }
        break;

        case HIDD_PROFILE_SET_REPORT_IND:
        {
            DEBUG_LOG_INFO("%s;HIDD_PROFILE_SET_REPORT_IND",__func__);
            HIDD_PROFILE_SET_REPORT_IND_T *ind = (HIDD_PROFILE_SET_REPORT_IND_T*)message;

            PanicNull(ind);
            DEBUG_LOG_INFO("%s:Report Type:0x%x, Report ID:0x%x, Report Len:0x%x",__func__, ind->type, ind->reportid, ind->reportLen);
            DEBUG_LOG_DATA(ind->data, ind->reportLen);

            switch(ind->type)
            {
                case hidd_profile_report_type_feature:
                {
                    switch(ind->reportid)
                    {
#ifdef INCLUDE_3DAMP
                        case spatial_data_sensor_reporting:
                        {
                            spatial_data_report_id_t data_report_id;
                            spatial_data_enable_status_t enable = spatial_data_disabled;

                            /* Default sample rate. */
                            uint16 sensor_sample_rate_hz = SPATIAL_DATA_DEFAULT_SAMPLE_RATE_HZ;

                            if(ind->reportLen >= 1)
                            {
                                data_report_id = (spatial_data_report_id_t)ind->data[0];
                            }
                            else
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_invalid_parameter);
                                break;
                            }

                            /* If the input contains sensor sample interval as well */
                            if(ind->reportLen == SPATIAL_DATA_SENSOR_STATUS_REPORTING_LEN)
                            {
                                memcpy(&sensor_sample_rate_hz, &ind->data[1], 2);
                            }

                            /* Check if the input HIDD report is supported or not */
                            /* Convert the input report_id to "enable" status. */
                            switch(data_report_id)
                            {
                                case spatial_data_report_disabled:
                                {
                                    enable = spatial_data_disabled;
                                }
                                break;

                                case spatial_data_report_1at:
#ifdef INCLUDE_ATTITUDE_FILTER
                                /* The below HIDD reports are only supported if Attitude Filter is included. */
                                case spatial_data_report_1bt:
                                case spatial_data_report_debug:
#endif
                                {
                                    /* Spatial audio is enabled by the remote device. */
                                    enable = spatial_data_enabled_remote;
                                }
                                break;

                                default:
                                {
                                    DEBUG_LOG_WARN("%s:Invalid HIDD REPORT ID:%d", __func__, data_report_id);
                                    HiddProfile_Handshake(hidd_profile_handshake_unknown);
                                }
                                break;
                            }

                            /* Enable/disable spatial audio. */
                            if (SpatialData_Enable(enable, sensor_sample_rate_hz, data_report_id))
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_success);
                            }
                            else
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_unknown);
                            }
                        }
                        break;
#endif /* INCLUDE_3DAMP */
                        case head_tracking_sensor_reporting:
                        {
                            spatial_data_enable_status_t enable = spatial_data_disabled;
                            uint8 data;

                            if(ind->reportLen >= 1)
                            {
                                data = ind->data[0];
                            }
                            else
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_invalid_parameter);
                                break;
                            }

                            /* bit0 Reporting State: 0=None, 1=All */
                            /* bit1 Power State    : 0=Off,  1=Full */
                            if ((data & HEAD_TRACKING_REPORTING_ALL) &&
                                (data & HEAD_TRACKING_POWER_FULL))
                            {
                                enable = spatial_data_enabled_remote;
                            }

                            /* bit2-7 Interval     : 0=10ms - 0x63=100ms */
                            /* Currenlty the interval value is not used. */

                            /* Enable/disable spatial audio. */
                            if (SpatialData_Enable(enable, SPATIAL_DATA_DEFAULT_SAMPLE_RATE_HZ, spatial_data_report_1))
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_success);
                            }
                            else
                            {
                                HiddProfile_Handshake(hidd_profile_handshake_unknown);
                            }
                        }
                        break;

                        default:
                        {
                            DEBUG_LOG_WARN("%s: Unsupported SET_REPORT Feature report ID:0x%x",__func__, ind->reportid);
                            HiddProfile_Handshake(hidd_profile_handshake_invalid_report_id);
                        }
                        break;
                    }
                }
                break;

                default: 
                {
                    DEBUG_LOG_WARN("%s:Unsupported SET_REPORT type:0x%x",__func__, ind->type);
                    HiddProfile_Handshake(hidd_profile_handshake_invalid_parameter);
                }
                break;
            }
            free(ind->data);
        }
        break;

        case HIDD_PROFILE_DATA_IND:
        {
            DEBUG_LOG_VERBOSE("%s:HIDD_PROFILE_DATA_IND",__func__);
            HIDD_PROFILE_DATA_IND_T *ind = (HIDD_PROFILE_DATA_IND_T*)message;

            PanicNull(ind);
            DEBUG_LOG_VERBOSE("%s:Report Type:0x%x, Report ID:0x%x, Report Len:0x%x",__func__, ind->type, ind->reportid,  ind->reportLen);
            DEBUG_LOG_DATA(ind->data, ind->reportLen);
            free(ind->data);
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("%s:HIDD_PROFILE Unhandled message id:0x%x", __func__, id);
        }
        break;
    }
}

/* Convert the input spatial data source to the value to be send over HIDD using 3DAMP Spec. */
static spatial_data_hidd_source_t spatialData_HiddDataSource(spatial_data_source_t data_source)
{
    DEBUG_LOG_VERBOSE("%s:Input source:%d",__func__, data_source);

    spatial_data_hidd_source_t ret_source = spatial_hidd_source_left;

    switch (data_source)
    {
        case spatial_data_left:
        {
            ret_source = spatial_hidd_source_left;
        }
        break;

        case spatial_data_right:
        {
            ret_source = spatial_hidd_source_right;
        }
        break;

        case spatial_data_head:
        {
            ret_source = spatial_hidd_source_head;
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("%s:Invalid source:%d",__func__, data_source);
        }
        break;
    }

    return ret_source;
}

#endif /* INCLUDE_HIDD_PROFILE */

#endif /* INCLUDE_SPATIAL_DATA */
