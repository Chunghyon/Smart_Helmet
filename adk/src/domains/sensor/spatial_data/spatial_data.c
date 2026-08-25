/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       spatial_data.c
\brief      Provides Spatial data support
*/

#ifdef INCLUDE_SPATIAL_DATA
#include <power_manager.h>
#include "spatial_data.h"
#include "spatial_data_private.h"

static void spatialData_ProcessAndSendSensorData(motion_sensor_data_t *sensor_data);
static void spatialData_MotionSensorTaskHandler(Task task, MessageId id, Message message);
static TaskData spatialDataMotionSensorTask = {spatialData_MotionSensorTaskHandler};
static void spatialData_SendSensorDataToLocalClient(motion_sensor_data_t *sensor_data);

spatial_data_t spatial_data =
{
    .enabled_status = spatial_data_disabled,
    .report_id      = spatial_data_report_disabled,
    .data_source    = spatial_data_head,

#ifdef INCLUDE_ATTITUDE_FILTER
    .attitude_filter_enabled = FALSE,
#endif

#ifdef INCLUDE_SENSOR_PROFILE
    .sensor_profile_connected = FALSE,
    .sync_data_exchange_set = FALSE,
    .pending_cfm_from_secondary = FALSE,
#endif
    .client_task    = NULL,
};

void SpatialData_InitData(Task task)
{
    UNUSED(task);
    DEBUG_LOG_DEBUG("%s", __func__);

    memset(&spatial_data, 0, sizeof(spatial_data_t));

    spatial_data.data_source = spatial_data_head;
}

bool SpatialData_InitMotionSensor(Bus bus_id, Sensor sensor_id, const SPATIAL_DATA_MOTION_SENSOR_CHIP_CONFIG_T *config_ptr)
{
    DEBUG_LOG_DEBUG("%s", __func__);

    /* Check the input Bus and Sensor ID. */
    PanicFalse(bus_id != 0);
    PanicFalse(sensor_id != 0);

    if (!config_ptr)
    {
        DEBUG_LOG_WARN("%s:Invalid input config_ptr", __func__);
        return FALSE;
    }

    /* Store the input Bus and Sensor ID. */
    spatial_data.bus_id = bus_id;
    spatial_data.sensor_id = sensor_id;

    /* Store the input motion sensor config pointer. */
    spatial_data.motion_sensor_config_ptr = config_ptr;

    /* Initialise motion sensor */
    if (!SensorConfigure(spatial_data.sensor_id, SENSOR_CONFIG_INTERRUPT_PIO, RDP_PIO_INT_IMU1))
    {
        DEBUG_LOG_WARN("%s:SENSOR_CONFIG_INTERRUPT_PIO configure failed", __func__);
        return FALSE;
    }

	/* Default to 52Hz data rate */
    if (!SensorConfigure(spatial_data.sensor_id, SENSOR_CONFIG_DATA_RATE, 52))
    {
        DEBUG_LOG_WARN("%s:SENSOR_CONFIG_DATA_RATE configure failed", __func__);
        return FALSE;
    }

    /* Process one message at a time.  Need a message for every one (default is SENSOR_MESSAGES_SOME). */
    if (!SensorConfigure(spatial_data.sensor_id, SENSOR_CONFIG_MESSAGES, SENSOR_MESSAGES_ALL))
    {
        DEBUG_LOG_WARN("%s:SENSOR_CONFIG_MESSAGES configure failed", __func__);
        return FALSE;
    }

    MessageSensorTask(spatial_data.sensor_id, &spatialDataMotionSensorTask);

#ifdef INCLUDE_ATTITUDE_FILTER
    /* Initialise attitude filter. */
    if (SpatialData_AttitudeFilterInit())
    {
        spatial_data.attitude_filter_enabled = TRUE;
    }
    else
    {
        DEBUG_LOG_WARN("%s:Attitude Filter init failed", __func__);
    }
#endif

    return TRUE;

}

bool SpatialData_Register(Task client_task, spatial_data_source_t data_source)
{
    DEBUG_LOG_DEBUG("%s",__func__);

    PanicNull(client_task);

    if (spatial_data.client_task)
    {
        DEBUG_LOG_WARN("%s:One client already registered!",__func__);
        return FALSE;
    }

    spatial_data.client_task = client_task;

    /* Store the input data source. */
    spatial_data.data_source = data_source;

#ifdef INCLUDE_SENSOR_PROFILE
    if (!SpatialData_RegisterSensorProfile())
    {
        DEBUG_LOG_WARN("%s:RegisterSensorProfile failed",__func__);
        return FALSE;
    }
#endif /* INCLUDE_SENSOR_PROFILE */

    return TRUE;
}

bool SpatialData_Enable(spatial_data_enable_status_t enable_input, uint16 sensor_sample_rate_hz, spatial_data_report_id_t report_id)
{
    DEBUG_LOG_INFO("%s:Enable:%d interval:%d report_id:%d", __func__, enable_input, sensor_sample_rate_hz, report_id);

#ifdef INCLUDE_SENSOR_PROFILE
    if (!BtDevice_IsMyAddressPrimary())
    {
        DEBUG_LOG_WARN("%s:Enabling/disabling spatial data must be invoked from primary", __func__);
        return FALSE;
    }
#endif /* INCLUDE_SENSOR_PROFILE */

    return SpatialData_HandleEnable(enable_input, sensor_sample_rate_hz, report_id);
}

bool SpatialData_HandleEnable(spatial_data_enable_status_t enable_input, uint16 sensor_sample_rate_hz, spatial_data_report_id_t report_id)
{
    DEBUG_LOG_INFO("%s: enable:%d interval:%d report_id:%d", __func__, enable_input, sensor_sample_rate_hz, report_id);

    bool enable = FALSE;

    /* Validate the input request against the current state. *
     * The function returns FALSE if the input request is received in an unexpected state.*
     */
    switch (enable_input)
    {
        /* Disable the current spatial audio session. */
        case spatial_data_disabled:
        {
            switch (spatial_data.enabled_status)
            {
                case spatial_data_enabled_local:
                case spatial_data_enabled_remote:
                {
                    enable = FALSE;
                }
                break;

                default:
                {
                    /* Spatial audio already disabled - return without doing anything */
                    DEBUG_LOG_WARN("%s:already disabled, input:%d current_state:%d", 
                                   __func__, enable_input, spatial_data.enabled_status);
                    return FALSE;
                }
            }
        }
        break;

        /* Enable spatial audio . */
        case spatial_data_enabled_local:
        {
            switch (spatial_data.enabled_status)
            {
                /* Enable spatial audio if it's disabled. */
                case spatial_data_disabled:
                {
                    enable = TRUE;
                }
                break;

                /* TBD: If there's a use case we can allow dynamically changing the sensor sample rate by local source. */
                default:
                {
                    /* Spatial audio already enabled - it needs to be disabled first.*/
                    DEBUG_LOG_WARN("%s:already enabled, input:%d current_state:%d", 
                                   __func__, enable_input, spatial_data.enabled_status);
                    return FALSE;
                }
            }
        }
        break;

        /* Enable spatial audio by remote source. */
        case spatial_data_enabled_remote:
        {
            switch (spatial_data.enabled_status)
            {
                /* Enable spatial audio if it's disabled. */
                case spatial_data_disabled:
                case spatial_data_enabled_local:
                /* Already enabled by local source - overwrite it with the new sample rate by the remote device as *
                 * enabling by remote source takes priority. *
                 */
                {
                    enable = TRUE;
                }
                break;

                default:
                {
                    /* Spatial audio already enabled - it needs to be disabled first.*/
                    DEBUG_LOG_WARN("%s:already enabled, input:%d current_state:%d", 
                                   __func__, enable_input, spatial_data.enabled_status);
                    return FALSE;
                }
            }
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("%s: Unhandled input:%d", __func__, enable_input);
            return FALSE;
        }
    }

    /* Validate the input report_id against the currently supported values. */
    switch (report_id)
    {
        case spatial_data_report_disabled:
        case spatial_data_report_1:
#ifdef INCLUDE_3DAMP
        case spatial_data_report_1at:
#ifdef INCLUDE_ATTITUDE_FILTER
        case spatial_data_report_1bt:
        case spatial_data_report_debug:
#endif /* INCLUDE_ATTITUDE_FILTER */
#endif /* INCLUDE_3DAMP */
        {
            DEBUG_LOG_DEBUG("%s:Valid report_id:%d", __func__, report_id);
        }
        break;
        default:
        {
            DEBUG_LOG_WARN("%s:Unexpected report_id:%d", __func__, report_id);
            return FALSE;
        }
    }

    /* Store the input data report. */
    spatial_data.report_id = report_id;

    /* Now act on the current input based on the current value of the "enable" flag */
    if (enable)
    {
        uint32 sensor_config_value = 0;

#ifdef INCLUDE_ATTITUDE_FILTER
        vm_runtime_profile profile;

        appPowerPerformanceProfileRequest();
        profile = VmGetRunTimeProfile();
        DEBUG_LOG_DEBUG(
            "%s: vm_runtime_profile:%d",
            __func__, profile);
#endif

        /* Set the default rate if the input is zero. */
        if (!sensor_sample_rate_hz)
        {
            sensor_sample_rate_hz = SPATIAL_DATA_DEFAULT_SAMPLE_RATE_HZ;
        }

        /* Configure and enable motion sensor sampling. */
        if (!SensorConfigure(spatial_data.sensor_id, SENSOR_CONFIG_DATA_RATE, sensor_sample_rate_hz))
        {
            DEBUG_LOG_WARN("%s:SensorConfigure failed key:%d value:%d", __func__, SENSOR_CONFIG_DATA_RATE, spatial_data.sensor_sample_rate_hz);
            return FALSE;
        }

        /* Power ON the sensor only if it's currently disabled *
         * as we can reconfigure the sample rate without power cylcing the sensor chip. *
         */
        if (spatial_data.enabled_status == spatial_data_disabled)
        {
            if (!BusPowerOn(spatial_data.bus_id))
            {
                DEBUG_LOG_WARN("%s:BusPowerON failed:%d", __func__, spatial_data.bus_id);
                return FALSE;
            }
        }

        if (SensorGetConfiguration(spatial_data.sensor_id, SENSOR_CONFIG_DATA_RATE, &sensor_config_value))
        {
            spatial_data.sensor_sample_rate_hz = (uint16)sensor_config_value;
        }
        else
        {
            DEBUG_LOG_WARN("%s:SensorGetConfiguration failed key:%d value:%d", __func__, SENSOR_CONFIG_DATA_RATE, sensor_config_value);
            spatial_data.sensor_sample_rate_hz = sensor_sample_rate_hz;
        } 

    }
    else
    {
#ifdef INCLUDE_ATTITUDE_FILTER
        vm_runtime_profile profile;

        appPowerPerformanceProfileRelinquish();
        profile = VmGetRunTimeProfile();
        DEBUG_LOG_DEBUG(
            "%s: vm_runtime_profile:%d",
            __func__, profile);
#endif

        if (!BusPowerOff(spatial_data.bus_id))
        {
            DEBUG_LOG_WARN("%s:BusPowerOFF failed bus_id:%d", __func__, spatial_data.bus_id);
            return FALSE;
        }
    }

    /* Reached here after validating the input and the current state, so save the input enable status. */
    spatial_data.enabled_status = enable_input;

    /* Inform the registered client about the current enable status. */
    if (spatial_data.client_task)
    {
        SPATIAL_DATA_ENABLE_STATUS_IND_T *status_message = (SPATIAL_DATA_ENABLE_STATUS_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ENABLE_STATUS_IND_T)));

        status_message->enable_status = spatial_data.enabled_status;
        status_message->sensor_sample_rate_hz = spatial_data.sensor_sample_rate_hz;
        MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ENABLED_STATUS_IND, status_message);
        /* The message memory shall be freed once the message is processed. */
    }

#ifdef INCLUDE_ATTITUDE_FILTER
    /* Reset the attitude filter if it's enabled - if spatial audio is enabled/disabled  */
    if (spatial_data.attitude_filter_enabled)
    {
        SpatialData_AttitudeFilterReset();
    }

#ifdef INCLUDE_SENSOR_PROFILE
    /*Reset exchanging orientation flags when spatial audio is enabled/disabled. */
    spatial_data.orientation_rxed = FALSE;
#endif /* INCLUDE_SENSOR_PROFILE */

#endif /* INCLUDE_ATTITUDE_FILTER */

#ifdef INCLUDE_SENSOR_PROFILE
    if (BtDevice_IsMyAddressPrimary())
    {
        /* Enable/Disable spatial audio in the secondary by sending messages over the sensor profile */
        if (!SpatialData_EnableSecondary())
        {
            DEBUG_LOG_WARN("%s:Secondary enable/disable attempt failed",__func__);

            /* Inform the registered client about the failure. */
            if (spatial_data.client_task)
            {
                SPATIAL_DATA_ERROR_IND_T *error_data = (SPATIAL_DATA_ERROR_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ERROR_IND_T)));
                error_data->spatial_error = spatial_data_secondary_enable_attempt_failed;
            
                /* If the current state of primary is disabled, then it will be the disable attempt that has failed */
                if (spatial_data.enabled_status == spatial_data_disabled)
                {
                    error_data->spatial_error = spatial_data_secondary_disable_attempt_failed;
                }
                MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ERROR_MESSAGE_IND, (void *)error_data);
                /* The message memory shall be freed once the message is processed. */
            }
        }
    }

    /* Set exchanging data between earbuds in primary even if the attempt to enable/disable spatial data in the secondary has failed. */
    SpatialData_ControlDataExchangeBetweenEarbuds();
#endif /* INCLUDE_SENSOR_PROFILE */

    /* Return the enable/disable attempt status for the local device. */
    return TRUE;
}

spatial_data_source_t SpatialData_GetDataSource(void)
{

#if defined(INCLUDE_ATTITUDE_FILTER) && defined(INCLUDE_SENSOR_PROFILE)
    /* It's the head orientation when the orientation values are combined by the *
     * new attitude filter *
     */
    if (spatial_data.orientation_rxed)
    {
        return spatial_data_head;
    }
#endif /* INCLUDE_ATTITUDE_FILTER && INCLUDE_SENSOR_PROFILE */

    return spatial_data.data_source;
}

static void spatialData_MotionSensorTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Sensor data from Sensor Hub . */
        case MESSAGE_SENSOR_DATA:
        {
            Sensor sid = ((MessageSensorData *)message)->sensor;
            uint16 data_size = SensorDataSize(sid);
            motion_sensor_data_t sensor_data;

            if (data_size > 1)
            {
                DEBUG_LOG_DEBUG(
                    "t = %10u, %s: case MESSAGE_SENSOR_DATA, data_size:%d",
                    SystemClockGetTimerTime(), __func__, data_size);
                /* Remove all the stale data. */
                while (SensorDataSize(sid) > 1)
                {
                    SensorDataFlush(sid);
                }
            }
            /* Process latest data. */
            if (SensorDataSize(sid))
            {
                const void *sensor_data_map = SensorDataMap(sid);
                const fifo_item_t *fi_ptr = (fifo_item_t *)sensor_data_map;
                bool valid_data = FALSE;

                if (fi_ptr)
                {
                    /* Save the latest sensor readings */
                    sensor_data.gyro.x  = fi_ptr->ts_xyz2_int16_temp.x[0];
                    sensor_data.gyro.y  = fi_ptr->ts_xyz2_int16_temp.y[0];
                    sensor_data.gyro.z  = fi_ptr->ts_xyz2_int16_temp.z[0];

                    sensor_data.accel.x = fi_ptr->ts_xyz2_int16_temp.x[1];
                    sensor_data.accel.y = fi_ptr->ts_xyz2_int16_temp.y[1];
                    sensor_data.accel.z = fi_ptr->ts_xyz2_int16_temp.z[1];

                    sensor_data.timestamp = fi_ptr->ts_xyz2_int16_temp.ts;

                    sensor_data.temp = fi_ptr->ts_xyz2_int16_temp.temp >> 8;

                    valid_data = TRUE;

                    DEBUG_LOG_V_VERBOSE("%s:Timestamp:%10u::Gyro %d,%d,%d, Accel %d,%d,%d, Temp %d.%u",
                                       __func__,
                                       sensor_data.timestamp,
                                       sensor_data.gyro.x,
                                       sensor_data.gyro.y,
                                       sensor_data.gyro.z,
                                       sensor_data.accel.x,
                                       sensor_data.accel.y,
                                       sensor_data.accel.z,
                                       sensor_data.temp,
                                       (sensor_data.temp & 0xFF) * 100 / 256);
                }
                SensorDataFlush(sid);

                /* Process and send the sensor data either to the remote device or local client for the 3D audio processing. */
                if (valid_data)
                {
                    spatialData_ProcessAndSendSensorData(&sensor_data);
                }
            }
        }
        break;

        /* We're not handling other sensor message indications.*/
        default:
        break;
    }
}

static void spatialData_ProcessAndSendSensorData(motion_sensor_data_t *sensor_data)
{
    DEBUG_LOG_V_VERBOSE("t = %10u, %s", SystemClockGetTimerTime(), __func__);

    PanicNull(sensor_data);

    if(spatial_data.enabled_status == spatial_data_disabled)
    {
        DEBUG_LOG_WARN("%s invoked when spatial audio disabled",__func__);
        return;
    }

#ifdef INCLUDE_ATTITUDE_FILTER

    /* Convert to Quaternion if it's requested by the remote device/client application - by checking the requested data report type */
    switch (spatial_data.report_id)
    {
        case spatial_data_report_1:
#ifdef INCLUDE_3DAMP
        case spatial_data_report_1bt:
        case spatial_data_report_debug:
#endif /* INCLUDE_3DAMP */
        {
            /* Return without trying to convert to quaternion if attitude filter is not enabled. */
            if (!spatial_data.attitude_filter_enabled)
            {
                DEBUG_LOG_WARN("%s:Attitude Filter not enabled",__func__);
                return;
            }

            if (!SpatialData_ConvertSensorDataToQuaternion(sensor_data))
            {
                DEBUG_LOG_WARN("%s:Conversion to Quaternion FAILED",__func__);
                return;
            }
        }
        break;

        default:
        break;
    }
#endif /* INCLUDE_ATTITUDE_FILTER */

    /* Send the processed sensor data to remote device (Phone/PC) or to a local registered client. */
    switch (spatial_data.enabled_status)
    {
#ifdef INCLUDE_HIDD_PROFILE

        case spatial_data_enabled_remote:
        {

#ifdef INCLUDE_SENSOR_PROFILE
            /* Only the primary would be sending the data to the remote device (Phone/PC) in earbud use case. */
            if (!BtDevice_IsMyAddressPrimary())
            {
                break;
            }
#endif /* INCLUDE_SENSOR_PROFILE */
            SpatialData_SendSensorDataToRemoteDevice(sensor_data);
        }
        break;
#endif /* INCLUDE_HIDD_PROFILE */

        case spatial_data_enabled_local:
        {
            spatialData_SendSensorDataToLocalClient(sensor_data);
        }
        break;

        /* Should not have reached here! */
        default:
        break;
    }
}

static void spatialData_SendSensorDataToLocalClient(motion_sensor_data_t *sensor_data)
{
    DEBUG_LOG_V_VERBOSE("%s", __func__);

    PanicNull(sensor_data);

    if (!spatial_data.client_task || spatial_data.enabled_status != spatial_data_enabled_local)
    {
          DEBUG_LOG_WARN("%s:NULL client task(:%d) or spatial audio not enabled(:%d) for local processing",__func__,
                          spatial_data.client_task, spatial_data.enabled_status);
          return;
    }

    if (spatial_data.report_id == spatial_data_report_disabled)
    {
        DEBUG_LOG_DEBUG("%s:No data reporting set when spatial audio is enabled locally",__func__);
        return;
    }

    SPATIAL_DATA_REPORT_DATA_IND_T *report_data = (SPATIAL_DATA_REPORT_DATA_IND_T *)(malloc(sizeof(SPATIAL_DATA_REPORT_DATA_IND_T)));

    if (!report_data)
    {
        DEBUG_LOG_WARN("%s:Not enough memory to send report_data to client!");
        return;
    }

    report_data->report_id = spatial_data.report_id;

    /* Timestamp from motion sensor. */
    report_data->timestamp = sensor_data->timestamp;

    /* Get the data source - left/right/head. */
    report_data->data_source = SpatialData_GetDataSource();

    /* Timestamp and temperature are always send. */
    switch (report_data->report_id)
    {
        case spatial_data_report_1:
        {
            /* Use the quaternion data for this configured report format */
            report_data->report_data.quat_data.temp = sensor_data->temp;
            memcpy(&report_data->report_data.quat_data.quaternion, &sensor_data->quaternion, sizeof(quaternion_data_t));
        }
        break;

#ifdef INCLUDE_3DAMP
        case spatial_data_report_1at:
        {
            report_data->report_data.raw_data.temp = sensor_data->temp;
            memcpy(&report_data->report_data.raw_data.accel, &sensor_data->accel, sizeof(motion_data_t));
            memcpy(&report_data->report_data.raw_data.gyro, &sensor_data->gyro, sizeof(motion_data_t));
        }
        break;

#ifdef INCLUDE_ATTITUDE_FILTER
        /* The following data reports are only supported if attitude filter is included. */
        case spatial_data_report_1bt:
        {
            report_data->report_data.quat_data.temp = sensor_data->temp;
            memcpy(&report_data->report_data.quat_data.quaternion, &sensor_data->quaternion, sizeof(quaternion_data_t));
        }
        break;

        case spatial_data_report_debug:
        {
            report_data->report_data.debug_data.temp = sensor_data->temp;
            memcpy(&report_data->report_data.debug_data.accel, &sensor_data->accel, sizeof(motion_data_t));
            memcpy(&report_data->report_data.debug_data.gyro, &sensor_data->gyro, sizeof(motion_data_t));
            memcpy(&report_data->report_data.debug_data.quaternion, &sensor_data->quaternion, sizeof(quaternion_data_t));
        }
        break;
#endif /* INCLUDE_ATTITUDE_FILTER */
#endif /* INCLUDE_3DAMP */
        default:
        {
            DEBUG_LOG_WARN("%s:Unsupported report_id:%d",__func__,report_data->report_id);
            free(report_data);
            return;
        }
    }

    /* Send the data report to the registered client. */
    MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_REPORT_DATA_IND, (void *)report_data);
    /* The message memory shall be freed once the message is processed. */
}


#endif /* INCLUDE_SPATIAL_DATA */
