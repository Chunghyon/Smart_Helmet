/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       spatial_data_peer.c
\brief      Provides control and data transfer with peer earbud using sensor profile during spatial data
*/

#ifdef INCLUDE_SPATIAL_DATA
#ifdef INCLUDE_SENSOR_PROFILE

#include "spatial_data.h"
#include "spatial_data_private.h"

#include "sensor_profile.h"

static void spatialData_SensorProfileTaskHandler(Task task, MessageId id, Message message);
static TaskData spatialDataSensorProfileTask = {spatialData_SensorProfileTaskHandler};

static bool spatialData_SendEnabledStatusToPrimaryEarbud(void);
static void spatialData_SendDataToOtherEarbud(void);
static void spatialData_ScheduleSendingDataToOtherEarbud(void);
static void spatialData_StopSchedulingDataToOtherEarbud(void);
static void spatialData_SendSynchronisedDataToLocalClient(rtime_t timestamp, uint8 *data_ptr, uint16 data_len);

/*! Internal messages handled along with sensor_profile messages in the same task. */
typedef enum
{
    /* Send orientation data to peer earbud over sensor profile */
    SPATIAL_DATA_SEND_DATA_TO_OTHER_EARBUD,

    /* Schedule reading next sniff instance. */
    SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE
} spatial_data_peer_internal_msg_t;

/*! Data types sent/received while exchanging data between earbuds in the sensor profile L2CAP channel
 * NOTE:The data types are bitmap values as multiple data types can be exchanged between 
 * the earbuds at the same time.
 */
typedef enum spatial_data_peer_sync_data_t
{
    /* No data to be sent or received between earbuds. */
    SPATIAL_EMPTY_DATA = 0,

    /* Exchange orientation data between earbuds. */
    SPATIAL_ORIENTATION_DATA = 1,

    /* Exchange sync data between earbuds. */
    SPATIAL_SYNC_DATA = 2
} spatial_data_peer_sync_data_t;

bool SpatialData_RegisterSensorProfile(void)
{
    DEBUG_LOG_DEBUG("%s",__func__);

    return SensorProfile_Register(&spatialDataSensorProfileTask);
}

bool SpatialData_EnableSecondary(void)
{
    DEBUG_LOG_INFO("%s", __func__);

    if (!spatial_data.sensor_profile_connected)
    {
        DEBUG_LOG_WARN("%s:Sensor profile not connected", __func__);
        return FALSE;
    }

    if (spatial_data.enabled_status == spatial_data_disabled)
    {
        /* Inform the secondary that the spatial data is disabled. */
        if (!SensorProfile_DisablePeer())
        {
            DEBUG_LOG_WARN("%s: SensorProfile_DisablePeer failed",__func__);
            return FALSE;
        }
    }
    else
    {
        /* Inform the peer that spatial data is enabled */
        sensor_profile_processing_source_t enabled_source = SENSOR_PROFILE_PROCESSING_LOCAL;

        if (spatial_data.enabled_status == spatial_data_enabled_remote)
        {
            enabled_source = SENSOR_PROFILE_PROCESSING_REMOTE;
        }

        if (!SensorProfile_EnablePeer(spatial_data.sensor_sample_rate_hz,
                                      enabled_source,
                                     (uint16)spatial_data.report_id))
        {
            DEBUG_LOG_WARN("%s: SensorProfile_EnablePeer failed",__func__);
            return FALSE;
        }
    }

    /* Wait for confirmation from secondary that it has acted on the enable/disable request. */
    spatial_data.pending_cfm_from_secondary = TRUE;

    return TRUE;

}

bool SpatialData_SendSynchronisedData(uint16 data_len, const uint8 *data_ptr)
{
    DEBUG_LOG_DEBUG("%s", __func__);

    if (!data_ptr || !data_len)
    {
        DEBUG_LOG_WARN("%s:Unexpected input data_ptr:%d data_len:%d", __func__, data_ptr, data_len);
        return FALSE;
    }

    /* Check if there's already a data queued for sending. */
    if (spatial_data.sync_data_exchange_set)
    {
        DEBUG_LOG_WARN("%s:Pending data queued for sending", __func__);
        return FALSE;
    }

    /* Check if the input exceeds the max length to fit into one BT slot. */
    if (data_len > SPATIAL_DATA_MAX_ALLOWED_SYNC_DATA_LENGTH_BYTES)
    {
        DEBUG_LOG_WARN("%s:Data length:%d exceeds the limit:%d", __func__, data_len, SPATIAL_DATA_MAX_ALLOWED_SYNC_DATA_LENGTH_BYTES);
        return FALSE;
    }

    if (spatial_data.enabled_status == spatial_data_disabled)
    {
        DEBUG_LOG_WARN("%s:Spatial audio not enabled", __func__);
        return FALSE;
    }

    if (!spatial_data.sensor_profile_connected)
    {
        DEBUG_LOG_WARN("%s:Sensor profile not connected", __func__);
        return FALSE;
    }

    spatial_data.sync_data_length = data_len;

    /* Copy the input sync_data request. */
    spatial_data.sync_data_ptr = (uint8 *)(PanicUnlessMalloc(data_len));
    memcpy(spatial_data.sync_data_ptr, data_ptr, spatial_data.sync_data_length);

    /* The data will be freed once it's send along with the latest orientation data in the next BT sniff instance. */
    spatial_data.sync_data_exchange_set = TRUE;

    return TRUE;
}

void SpatialData_ControlDataExchangeBetweenEarbuds(void)
{
    DEBUG_LOG_DEBUG("%s", __func__);

    if (spatial_data.sensor_profile_connected && spatial_data.enabled_status != spatial_data_disabled)
    {
        /* Schedule for sending orientation data between earbuds. */
        spatialData_ScheduleSendingDataToOtherEarbud();
    }
    else
    {
        /* Stop sending orientation data between earbuds. */
        spatialData_StopSchedulingDataToOtherEarbud();
    }
}

static bool spatialData_SendEnabledStatusToPrimaryEarbud(void)
{
    DEBUG_LOG_INFO("%s", __func__);

    if (!spatial_data.sensor_profile_connected)
    {
        DEBUG_LOG_WARN("%s:Sensor profile not connected", __func__);
        return FALSE;
    }

    if (spatial_data.enabled_status == spatial_data_disabled)
    {
        /* Inform the other earbud that the spatial audio is disabled. */
        if (!SensorProfile_PeerDisabled())
        {
            DEBUG_LOG_WARN("%s: SensorProfile_PeerDisabled failed",__func__);
            return FALSE;
        }
    }
    else
    {
        /* Inform the other earbud that the spatial audio is enabled. */
        sensor_profile_processing_source_t enabled_source = SENSOR_PROFILE_PROCESSING_LOCAL;
        if (spatial_data.enabled_status == spatial_data_enabled_remote)
        {
            enabled_source = SENSOR_PROFILE_PROCESSING_REMOTE;
        }

        if (!SensorProfile_PeerEnabled(spatial_data.sensor_sample_rate_hz,
                                     enabled_source))
        {
            DEBUG_LOG_WARN("%s: SensorProfile_PeerEnabled failed",__func__);
            return FALSE;
        }
    }
    return TRUE;
}


static void spatialData_SensorProfileTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case SENSOR_PROFILE_CLIENT_REGISTERED:
        {
            DEBUG_LOG_INFO("%s:SensorProfile registered", __func__);
            SENSOR_PROFILE_CLIENT_REGISTERED_T *sensor_message = (SENSOR_PROFILE_CLIENT_REGISTERED_T *)message;
            PanicNull(sensor_message);

            if (!sensor_message->status)
            {
                DEBUG_LOG_WARN("%s:Sensor profile client not registered", __func__);
            }
        }
        break;

        case SENSOR_PROFILE_CONNECTED:
        {
            SENSOR_PROFILE_CONNECTED_T *sensor_message = (SENSOR_PROFILE_CONNECTED_T *)message;
            PanicNull(sensor_message);

            DEBUG_LOG_INFO("%s:SensorProfile connected status:%d", __func__, sensor_message->status);

            if (sensor_message->status == sensor_profile_status_peer_connected)
            {
                spatial_data.sensor_profile_connected = TRUE;
                spatial_data.sensor_profile_l2cap_sink = sensor_message->sink;

                /* Create a wall-clock time conversion handle for the L2CAP Sink for the sensor profile L2CAP channel. */
                spatial_data.sensor_profile_wallclock_enabled = 
                  RtimeWallClockEnable(&spatial_data.sensor_profile_wallclock_state, spatial_data.sensor_profile_l2cap_sink);

                /* Inform the primary about the current spatial data enable status. */
                if (!BtDevice_IsMyAddressPrimary())
                {
                    /* Inform (over the sensor profile) the primary that spatial audio is enabled/disabled in the secondary.
                     *
                     * NOTE: spatial_data uses SENSOR_PROFILE_PEER_ENABLED/SENSOR_PROFILE_PEER_DISABLED messages asynchronously 
                     * here to inform the primary about its status when the sensor profile gets connected!
                     *
                     * This is used as a handshake mechanism so that secondary will be in state to handle 
                     * SENSOR_PROFILE_PEER_ENABLE/SENSOR_PROFILE_PEER_DISABLE message from primar once the 
                     * sensor profile gets connected in both the devices.
                     *
                     */
                    if (!spatialData_SendEnabledStatusToPrimaryEarbud())
                    {
                        DEBUG_LOG_WARN("%s:Send enable status failed from secondary",__func__);
                    }
                }

                /* Start sending orientation between the earbuds if spatial audio is enabled. */
                SpatialData_ControlDataExchangeBetweenEarbuds();
            }
            else 
            {
                spatial_data.sensor_profile_connected = FALSE;
                spatial_data.sensor_profile_wallclock_enabled = FALSE;
            }
        }
        break;

        case SENSOR_PROFILE_DISCONNECTED:
        {
            DEBUG_LOG_INFO("%s:SensorProfile disconnected", __func__);
            spatial_data.sensor_profile_connected = FALSE;
            spatial_data.sensor_profile_wallclock_enabled = FALSE;

#ifdef INCLUDE_ATTITUDE_FILTER
            /* We cannot be getting the orientation values from peer when the sensor profile is disconnected. */
            spatial_data.orientation_rxed = FALSE;
#endif /* INCLUDE_ATTITUDE_FILTER */

            /* Stop exchanging data between earbuds when the sensor profile is disconnected. */
            SpatialData_ControlDataExchangeBetweenEarbuds();
        }
        break;

        case SENSOR_PROFILE_DATA_RECEIVED:
        {
            DEBUG_LOG_DEBUG("t = %10u %s:SensorProfile data rxed", SystemClockGetTimerTime(), __func__);
            SENSOR_PROFILE_DATA_RECEIVED_T *data_rxed = (SENSOR_PROFILE_DATA_RECEIVED_T *)message;
            PanicNull(data_rxed);

            Source sensor_profile_source = data_rxed->source;
            const uint8 *source_map = SourceMap(sensor_profile_source);
            uint16 len = SourceBoundary(sensor_profile_source);

            spatial_data_peer_sync_data_t message_id = SPATIAL_EMPTY_DATA;
            rtime_t local_timestamp;

            /* Check if the received length is more than the expected maximum length */
            if ((len > (sizeof(message_id) + 
#ifdef INCLUDE_ATTITUDE_FILTER
              sizeof(quaternion_info_exchange_t) + 
#endif /* INCLUDE_ATTITUDE_FILTER */
              (sizeof(rtime_t) + sizeof(spatial_data.sync_data_length) + SPATIAL_DATA_MAX_ALLOWED_SYNC_DATA_LENGTH_BYTES)))
               ||
               (len < sizeof(message_id)))
            {
                DEBUG_LOG_WARN("%s:Unexpected message length in SENSOR_PROFILE_DATA_RECEIVED len:%d!", __func__, len);
            }

            /* The first info is the message_id. */
            memmove(&message_id, source_map, sizeof(message_id));
            source_map += sizeof(message_id);

#ifdef INCLUDE_ATTITUDE_FILTER
            if (message_id & SPATIAL_ORIENTATION_DATA)
            {
                quaternion_info_exchange_t quat_info;
                memmove(&quat_info, source_map, sizeof(quat_info));
                source_map += sizeof(quat_info);

                if (spatial_data.sensor_profile_wallclock_enabled &&
                    RtimeWallClockToLocal(&spatial_data.sensor_profile_wallclock_state, quat_info.timestamp, &local_timestamp))
                {
                    quat_info.timestamp = local_timestamp;
                }
                else
                {
                    /* BT clock conversion failed, so send the time as it is. */
                    DEBUG_LOG_WARN("%s:SENSOR_PROFILE_DATA_RECEIVED quat timestamp wallclock conversion failed to local time ", __func__);
                }
                /* Inform the attitude filter of the received quaternion values from the other earbud. */
                if (SpatialData_AttitudeFilterUpdateOrientationFromOtherEarbud(&quat_info))
                {
                    spatial_data.orientation_rxed = TRUE;
                }
                else
                {
                    spatial_data.orientation_rxed = FALSE;
                }

            }
#endif /* INCLUDE_ATTITUDE_FILTER */

            if (message_id & SPATIAL_SYNC_DATA)
            {
                rtime_t sync_timestamp;
                uint16 data_length;
                uint8 *data_ptr;

                DEBUG_LOG_DEBUG("t = %10u %s:SPATIAL_SYNC_DATA rxed", SystemClockGetTimerTime(), __func__);

                /* First data is the timestamp. */
                memmove(&sync_timestamp, source_map, sizeof(sync_timestamp));
                source_map += sizeof(sync_timestamp);

                /* Convert the BT timestamp (wall clock) to local time. */
                if (!(spatial_data.sensor_profile_wallclock_enabled &&
                    RtimeWallClockToLocal(&spatial_data.sensor_profile_wallclock_state, sync_timestamp, &local_timestamp)))
                {
                    /* BT clock conversion failed, so send the time as it is. */
                    DEBUG_LOG_WARN("%s:SENSOR_PROFILE_DATA_RECEIVED audio timestamp wallclock conversion failed to local time ", __func__);
                    local_timestamp = sync_timestamp;
                }

                /* Second data is the data length. */
                memmove(&data_length, source_map, sizeof(data_length));
                source_map += sizeof(data_length);

                /* Third data is the sync_data. */
                data_ptr = (uint8 *)PanicUnlessMalloc(data_length);
                memmove(data_ptr, source_map, data_length);

                /* Inform the registered client about the sync data from the other earbud. */
                spatialData_SendSynchronisedDataToLocalClient(local_timestamp, data_ptr, data_length);

                /* Free the memory as the data should have been copied into another buffer. */
                free(data_ptr);
            }

            SourceDrop(sensor_profile_source, len);
        }
        break;

        case SENSOR_PROFILE_SEND_DATA:
        {
            DEBUG_LOG_VERBOSE("%s:SensorProfile send data", __func__);
        }
        break;

        case SPATIAL_DATA_SEND_DATA_TO_OTHER_EARBUD:
        {
            DEBUG_LOG_VERBOSE("%s:Send orientation data timeout",__func__);
            /* Handle timeout message for sending orientation data.*/
            if (spatial_data.enabled_status != spatial_data_disabled && 
                spatial_data.sensor_profile_connected)
            {
                /* Send data to other earbud over sensor profile. */
                spatialData_SendDataToOtherEarbud();

                /* Schedule reading the next instance after few tens of milliseconds *
                 * so that the returned value is not very close to the current time. *
                 */
                MessageSendLater(&spatialDataSensorProfileTask,
                                 SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE,
                                 NULL,
                                 SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE_INTERVAL_MS);
            }
        }
        break;

        case SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE:
        {
            DEBUG_LOG_VERBOSE("%s:Read next sniff instance timeout",__func__);
            /* Handle timeout message for sending data to other earbud.*/
            if (spatial_data.enabled_status != spatial_data_disabled && 
                spatial_data.sensor_profile_connected)
            {
                /* Schedule reading the next instance and sending data. */
                spatialData_ScheduleSendingDataToOtherEarbud();
            }
        }
        break;

        case SENSOR_PROFILE_ENABLE_PEER_REQ:
        {
            SENSOR_PROFILE_ENABLE_PEER_REQ_T *sensor_enable = (SENSOR_PROFILE_ENABLE_PEER_REQ_T *)message;

            /* Enable spatial audio in secondary when the command is received over the sensor profile from the primary. */
            if (!BtDevice_IsMyAddressPrimary())
            {
                spatial_data_enable_status_t enable_source = spatial_data_enabled_local;
                PanicNull(sensor_enable);
                DEBUG_LOG_INFO("%s:SensorProfile enable peer source:%d sample_interval:%d remote_tx_value:%d",__func__,
                               sensor_enable->processing_source,
                               sensor_enable->sensor_interval,
                               sensor_enable->report_id);
        
                switch (sensor_enable->processing_source)
                {
                    case SENSOR_PROFILE_PROCESSING_LOCAL:
                    {
                        enable_source = spatial_data_enabled_local;
                    }
                    break;
                
                    case SENSOR_PROFILE_PROCESSING_REMOTE:
                    {
                        enable_source = spatial_data_enabled_remote;
                    }
                    break;
                
                    default:
                    {
                        DEBUG_LOG_WARN("%s:Invalid sensor_profile_enable_peer source:%d", __func__, sensor_enable->processing_source);
                    }
                    break;
                }

                if (!SpatialData_HandleEnable(enable_source,
                                        sensor_enable->sensor_interval, 
                                        (spatial_data_report_id_t)sensor_enable->report_id))
                {
                    DEBUG_LOG_WARN("%s:Spatial data enable failed in Secondary:enable_source:%d, sample_rate:%d, report_id:%d",
                                   __func__, enable_source, sensor_enable->sensor_interval, sensor_enable->report_id);
                }

                /* Inform the primary about the current spatial data status in the secondary. */
                if (!spatialData_SendEnabledStatusToPrimaryEarbud())
                {
                    DEBUG_LOG_WARN("%s:Sending of the current spatial data status failed from secondary",__func__);
                }

            }
            else
            {
                DEBUG_LOG_ERROR("%s:SENSOR_PROFILE_ENABLE_PEER_REQ received in primary!", __func__);
            }
        }
        break;

        case SENSOR_PROFILE_DISABLE_PEER_REQ:
        {
            DEBUG_LOG_INFO("%s:SensorProfile disable peer",__func__);

            /* Disable spatial data in secondary when the command is received over the sensor profile from the primary. */
            if (!BtDevice_IsMyAddressPrimary())
            {
                if (!SpatialData_HandleEnable(spatial_data_disabled, 0, 0))
                {
                    DEBUG_LOG_WARN("%s:Spatial data disable failed in Secondary", __func__);
                }

                /* Inform the primary about the current spatial data status in the secondary. */
                if (!spatialData_SendEnabledStatusToPrimaryEarbud())
                {
                    DEBUG_LOG_WARN("%s:Sending of the current spatial data status failed from secondary",__func__);
                }

            }
            else
            {
                DEBUG_LOG_ERROR("%s:SENSOR_PROFILE_DISABLE_PEER_REQ received in primary!", __func__);
            }
        }
        break;

        case SENSOR_PROFILE_PEER_ENABLED:
        {
            DEBUG_LOG_INFO("%s:SensorProfile peer enabled",__func__);

            /* Reset the flag waiting for a cfm from secondary. */
            spatial_data.pending_cfm_from_secondary = FALSE;

            if (BtDevice_IsMyAddressPrimary())
            {
                /* Check if the spatial audio enable status is in-sync in both the earbuds. */
                if (spatial_data.enabled_status == spatial_data_disabled)
                {
                    DEBUG_LOG_WARN("%s:Spatial audio enabled in secondary when not in primary!!", __func__);
                
                    if (spatial_data.client_task)
                    {
                        SPATIAL_DATA_ERROR_IND_T *error_data = (SPATIAL_DATA_ERROR_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ERROR_IND_T)));
                        memset(error_data, 0, sizeof(SPATIAL_DATA_ERROR_IND_T));
                        error_data->spatial_error = spatial_data_secondary_enabled;
                        MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ERROR_MESSAGE_IND, (void *)error_data);
                        /* The message memory shall be freed once the message is processed. */
                    }

                    /* Send spatial data control message to secondary to keep it in sync with the primary.*/
                    if (!SpatialData_EnableSecondary())
                    {
                        DEBUG_LOG_WARN("%s:Secondary enable/disable attempt failed",__func__);

                        /* Inform the registered client about the failure. */
                        if (spatial_data.client_task)
                        {
                            SPATIAL_DATA_ERROR_IND_T *error_data = (SPATIAL_DATA_ERROR_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ERROR_IND_T)));
                            error_data->spatial_error = spatial_data_secondary_disable_attempt_failed;
                            MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ERROR_MESSAGE_IND, (void *)error_data);
                            /* The message memory shall be freed once the message is processed. */
                        }
                    }
                }
                else
                {
                    DEBUG_LOG_INFO("%s:Both primary and secondary are in-sync spatial data ENABLED", __func__);
                }

            }
            else
            {
                DEBUG_LOG_ERROR("%s:SENSOR_PROFILE_PEER_ENABLED received in secondary!", __func__);
            }
        }
        break;

        case SENSOR_PROFILE_PEER_DISABLED:
        {
            DEBUG_LOG_INFO("%s:SensorProfile peer disabled",__func__);

            /* Reset the flag waiting for a cfm from secondary. */
            spatial_data.pending_cfm_from_secondary = FALSE;

            if (BtDevice_IsMyAddressPrimary())
            {
                /* Check if the spatial audio enable status is in-sync in both the earbuds. */
                if (spatial_data.enabled_status != spatial_data_disabled)
                {
                    DEBUG_LOG_WARN("%s:Spatial audio disabled in secondary when not in primary!!",__func__);
                
                    if (spatial_data.client_task)
                    {
                        SPATIAL_DATA_ERROR_IND_T *error_data = (SPATIAL_DATA_ERROR_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ERROR_IND_T)));
                        memset(error_data, 0, sizeof(SPATIAL_DATA_ERROR_IND_T));
                        error_data->spatial_error = spatial_data_secondary_disabled;
                        MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ERROR_MESSAGE_IND, (void *)error_data);
                        /* The message memory shall be freed once the message is processed. */
                    }

                    /* Send spatial data control message to secondary to keep it in sync with the primary.*/
                    if (!SpatialData_EnableSecondary())
                    {
                        DEBUG_LOG_WARN("%s:Secondary enable/disable attempt failed",__func__);
                        /* Inform the registered client about the failure. */
                        if (spatial_data.client_task)
                        {
                            SPATIAL_DATA_ERROR_IND_T *error_data = (SPATIAL_DATA_ERROR_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_ERROR_IND_T)));
                            error_data->spatial_error = spatial_data_secondary_enable_attempt_failed;
                            MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_ERROR_MESSAGE_IND, (void *)error_data);
                            /* The message memory shall be freed once the message is processed. */
                        }
                    }
                }
                else
                {
                    DEBUG_LOG_INFO("%s:Both primary and secondary are in-sync spatial data DISABLED", __func__);
                }
            }
            else
            {
                DEBUG_LOG_ERROR("%s:SENSOR_PROFILE_PEER_DISABLED received in secondary!", __func__);
            }
        }
        break;

        default:
        {
            DEBUG_LOG_DEBUG("%s:Unhandled message:%d", __func__, id);
        }
        break;
    }
}

static void spatialData_ScheduleSendingDataToOtherEarbud(void)
{
    next_sniff_clock_t *next_sniff_clock = PanicUnlessMalloc(sizeof(next_sniff_clock_t));
    rtime_t next_send_instance = 0;
    bool  schedule_sending_data = FALSE;

#ifdef INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG
    static rtime_t sniff_read_prev_time = 0;
    rtime_t sniff_read_current_time = SystemClockGetTimerTime();
    rtime_t delta_time = 0;

    if (rtime_gt(sniff_read_current_time, sniff_read_prev_time))
    {
        delta_time = rtime_sub(sniff_read_current_time, sniff_read_prev_time);
    }
    else
    {
        delta_time = rtime_sub(sniff_read_prev_time, sniff_read_current_time);
    }

    DEBUG_LOG_INFO("%s:Read sniff instance interval:%dms", __func__, (delta_time/US_PER_MS));
    sniff_read_prev_time = sniff_read_current_time;
#endif /* INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG */

    /* Read the next BT sniff instance in the buddy link to schedule packet to the other earbud *
     * so that the packet would go in the sniff window.
     */
    rtime_t current_time = SystemClockGetTimerTime();
    if (SensorProfile_NextSniffClock(next_sniff_clock))
    {
        /* The next sniff instance time is supposed to be in the future (more than the current time), *
         * but double check the returned next_subrate_clock value.
         */
        if (rtime_gt(next_sniff_clock->next_subrate_clock, current_time))
        {
            next_send_instance = rtime_sub(next_sniff_clock->next_subrate_clock, current_time)/US_PER_MS;

            if (next_send_instance > SPATIAL_DATA_MAX_EXPECTED_NEXT_SNIFF_INSTANCE_OFFSET_MS)
            {
                DEBUG_LOG_WARN("%s:UNEXPECTED large next_subrate_clock:%dus > current_time:%dus next_send_instance:%dmsec", 
                                                            __func__, 
                                                            next_sniff_clock->next_subrate_clock,
                                                            current_time,
                                                            next_send_instance);
            }
            else if (next_send_instance > SPATIAL_DATA_SEND_DATA_AND_NEXT_SNIFF_INSTANCE_OFFSET_MS)
            {
                /* Schedule to send the orientation value if the next sniff instance is not very close to the current time. *
                 * to give enough time for the application to schedule a packet before the next sniff instance. *
                 */
                /* NOTE: this needs to be fine tuned by looking at the air frames. */
                schedule_sending_data = TRUE;
#ifdef INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG
                DEBUG_LOG_INFO("%s:Schedule sending data between earbuds after:%dms", __func__, next_send_instance);
#endif
            }
#ifdef INCLUDE_SPATIAL_DATA_TIMESTAMP_DEBUG
            else
            {
                DEBUG_LOG_INFO("%s:Next sniff instance:%dmsec is very close to the current time", __func__, next_send_instance);
            }
#endif
        }
        else
        {
            /* The current_time was set before invoking the trap to get the next sniff instance *
             * so the returned future next_subrate_clock should not be less than the current_time! *
             */
            DEBUG_LOG_WARN("%s:UNEXPECTED next_subrate_clock:%dus < current local time:%dus",
                                                                __func__,
                                                                next_sniff_clock->next_subrate_clock,
                                                                current_time);
        }
    }
    else
    {
        DEBUG_LOG_WARN("%s:SensorProfile_NextSniffClock() FAILED", __func__);
    }

    /* Free the allocated memory. */
    free(next_sniff_clock);

    if (schedule_sending_data)
    {
        /* Schedule to send data to other earbud values few milliseconds before the the next BT sniff instance *
         *  - to get enough time to prepare and send the packet to the BT controller so that it goes out in the *
         * next BT sniff window.*
         */
        MessageSendLater(&spatialDataSensorProfileTask,
                         SPATIAL_DATA_SEND_DATA_TO_OTHER_EARBUD,
                         NULL,
                         (next_send_instance - SPATIAL_DATA_SEND_DATA_AND_NEXT_SNIFF_INSTANCE_OFFSET_MS));
    }
    else
    {
        /* Schedule to read next BT sniff instance if the current read failed or *
         * if the sniff instance is very close to the current time - i.e. too near/far to *
         * read and the send data to the other earbud.*
         */
        MessageSendLater(&spatialDataSensorProfileTask,
                         SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE,
                         NULL,
                         SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE_INTERVAL_MS);
    }

}

static void spatialData_SendDataToOtherEarbud(void)
{
    DEBUG_LOG_DEBUG("%s", __func__);

    spatial_data_peer_sync_data_t message_id = SPATIAL_EMPTY_DATA;
    rtime_t send_wallclock_time = 0;
    rtime_t send_local_time = 0;

    /* First calculate the data_size (for SinkFlush) by multiplexing the orientation data *
     * and sync_data in the current BT sniff instance. *
     */
    uint16 data_size = sizeof(message_id);

#ifdef INCLUDE_ATTITUDE_FILTER
    /* Orientation data should be send to the other earbud every BT sniff instance. */
    quaternion_info_exchange_t quat_info;
    if (SpatialData_AttitudeFilterGetCurrentOrientation(&quat_info))
    {
        message_id |= SPATIAL_ORIENTATION_DATA;
        data_size += sizeof(quaternion_info_exchange_t);

        /* Convert the quaternion timestamp to BT clock (wallclock) */
        if (!spatial_data.sensor_profile_wallclock_enabled ||
           !RtimeLocalToWallClock(&spatial_data.sensor_profile_wallclock_state, quat_info.timestamp, &send_wallclock_time))
        {
            /* Send quaternion timestamp as it is if the wallclock conversion failed. */
            DEBUG_LOG_WARN("%s:Quaternion timestamp to wallclock conversion failed",__func__);
            send_wallclock_time = quat_info.timestamp;
        }
        quat_info.timestamp = send_wallclock_time;

        DEBUG_LOG_DEBUG("t = %10u clock_offset = %10u %s:Quat data to other earbud: ts:%10u W:%d X:%d Y:%d Z:%d",
                        SystemClockGetTimerTime(),
                        spatial_data.sensor_profile_wallclock_state.offset,
                        __func__,
                        quat_info.timestamp,
                        quat_info.quaternion_data.w,
                        quat_info.quaternion_data.x,
                        quat_info.quaternion_data.y,
                        quat_info.quaternion_data.z);
    }
#endif /* INCLUDE_ATTITUDE_FILTER */

    /* Sync data is only sent when requested by the upper layer. */
    if (spatial_data.sync_data_exchange_set)
    {
        message_id |= SPATIAL_SYNC_DATA;

        /* Timestamp, sync_data_length and sync_data are sent to the other earbud. */
        data_size += sizeof(rtime_t) + sizeof(spatial_data.sync_data_length)+ spatial_data.sync_data_length;
    }

    /* Return if no data is ready for sending to other earbud. */
    if (message_id == SPATIAL_EMPTY_DATA)
    {
        DEBUG_LOG_DEBUG("%s:No spatial audio data to exchange between earbuds", __func__);
        return;
    }

    /* Send the data to other earbud over the sensor profile L2CAP channel. */
    if (SinkIsValid(spatial_data.sensor_profile_l2cap_sink))
    {
        /* Get the amount of available space left in the sink */
        uint16 sink_slack = SinkSlack(spatial_data.sensor_profile_l2cap_sink);
    
        /* Make sure we have enough space for sending the data */
        if (sink_slack >= data_size)
        {
            SinkClaim(spatial_data.sensor_profile_l2cap_sink, data_size);
            
            uint8 *base = SinkMap(spatial_data.sensor_profile_l2cap_sink);

            /* Copy the message_id into the first byte of the sink memory. */
            memcpy(base, (void *)&message_id, sizeof(message_id));
            base += sizeof(message_id);

#ifdef INCLUDE_ATTITUDE_FILTER
            /* Copy the quaternion data into the sink memory. */
            if (message_id & SPATIAL_ORIENTATION_DATA)
            {
                memcpy(base, (void *)&quat_info, sizeof(quat_info));
                base += sizeof(quat_info);
            }
#endif /* INCLUDE_ATTITUDE_FILTER */
            
            /* Prepare and copy the sync data into the sink memory. */
            if (message_id & SPATIAL_SYNC_DATA)
            {
                /* Add offset to the current time so that both earbuds will be able to act on the value *
                 * at the same time, i.e. enough time to send the packet over air and then to receive the *
                 * the message in the application in the other earbud. *
                 */
                send_local_time = rtime_add(SystemClockGetTimerTime(), SPATIAL_DATA_SYNC_DATA_EXCHANGE_OFFSET_US);
                
                /* Convert the current local time to BT clock (wallclock) */
                if (!(spatial_data.sensor_profile_wallclock_enabled && 
                   RtimeLocalToWallClock(&spatial_data.sensor_profile_wallclock_state, 
                                         send_local_time, 
                                         &send_wallclock_time)))
                {
                    /* Send the local timestamp if the wallclock conversion failed. */
                    DEBUG_LOG_WARN("%s:Sync data timestamp to wallclock conversion failed",__func__);
                    send_wallclock_time = send_local_time;
                }

                /* Copy the timestamp. */
                memcpy(base, (void *)&send_wallclock_time, sizeof(send_wallclock_time));
                base += sizeof(send_wallclock_time);

                /* Copy the data_length. */
                memcpy(base, (void *)&spatial_data.sync_data_length, sizeof(spatial_data.sync_data_length));
                base += sizeof(spatial_data.sync_data_length);

                /* Copy the data. */
                memcpy(base, (void *)spatial_data.sync_data_ptr, spatial_data.sync_data_length);

                DEBUG_LOG_DEBUG("%s:SPATIAL_SYNC_DATA queued data_len:%d", spatial_data.sync_data_length);
            }

            /* Now send the data to other earbud over the sensor profile L2CAP channel. */
            if (!SinkFlush(spatial_data.sensor_profile_l2cap_sink, data_size))
            {
                DEBUG_LOG_WARN("%s:Sensor profile SinkFlush FAILED",__func__);
            }
        }
        else
        {
            DEBUG_LOG_WARN("%s:Not enough sink slack to send data to other earbud",__func__);
        }
    }
    else
    {
        DEBUG_LOG_WARN("%s:Invalid sensor_profile L2CAP sink",__func__);
    }

    /* Inform the local registered client about the sync data time. *
     * The same time (converted to BT wallclock) is passed to the remote earbud so that both the *
     * earbuds shall act on the sync_data at the same time.*
     */
    if (spatial_data.sync_data_exchange_set)
    {
        spatialData_SendSynchronisedDataToLocalClient(send_local_time,
                                                     spatial_data.sync_data_ptr, 
                                                     spatial_data.sync_data_length);

        free(spatial_data.sync_data_ptr);
        spatial_data.sync_data_exchange_set = FALSE;
    }

}

static void spatialData_StopSchedulingDataToOtherEarbud(void)
{
    DEBUG_LOG_VERBOSE("%s", __func__);

    /* Cancel all pending orientation data exchange messages. */
    MessageCancelAll(&spatialDataSensorProfileTask,
                         SPATIAL_DATA_SEND_DATA_TO_OTHER_EARBUD);

    /* Cancel all pending sniff read messages. */
    MessageCancelAll(&spatialDataSensorProfileTask,
                         SPATIAL_DATA_READ_NEXT_SNIFF_INSTANCE);
}

static void spatialData_SendSynchronisedDataToLocalClient(rtime_t timestamp, uint8 *data_ptr, uint16 data_len)
{
    DEBUG_LOG_DEBUG("%s:timestamp:%dus, data_len:%d", __func__, timestamp, data_len);

    if (data_len > SPATIAL_DATA_MAX_ALLOWED_SYNC_DATA_LENGTH_BYTES)
    {
        DEBUG_LOG_WARN("%s:Invalid data_len:%d", __func__, data_len);
        return;
    }

    if (spatial_data.client_task)
    {
        SPATIAL_DATA_SYNC_DATA_IND_T *sync_data_msg = (SPATIAL_DATA_SYNC_DATA_IND_T *)(PanicUnlessMalloc(sizeof(SPATIAL_DATA_SYNC_DATA_IND_T)));
        memset(sync_data_msg, 0, sizeof(SPATIAL_DATA_SYNC_DATA_IND_T));
        sync_data_msg->timestamp = timestamp;
        sync_data_msg->data_len = data_len;

        memcpy(sync_data_msg->data, data_ptr, data_len);

        MessageSend(spatial_data.client_task, (MessageId)SPATIAL_DATA_SYNC_DATA_IND, (void *)sync_data_msg);
        /* The message memory shall be freed once the message is processed. */
    }
    else
    {
        DEBUG_LOG_WARN("%s:Invalid client task",__func__);
    }
}

#endif /* INCLUDE_SENSOR_PROFILE */

#endif /* INCLUDE_SPATIAL_DATA */
