/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sensor profile test module.
*/

#ifdef INCLUDE_SENSOR_PROFILE

#include "sensor_profile_test.h"
#include "sensor_profile.h"
#include <logging.h>
#include <sink.h>
#include <source.h>
#include <stream.h>
#include <panic.h>
#include <stdlib.h>
#include <rtime.h>
#include <vm.h>

LOGGING_PRESERVE_MESSAGE_TYPE(sensor_profile_msg_t)

#define SENSOR_PROFILE_PEER_DATA_SYNC_L2CAP_MESSAGE_SIZE   12

#define SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG   195 //

// The sniff interval is 13*1.25 = 16.25 ms.
// Use a small offset of 5 ms prior to start the subrate_sniff instant.
#define SENSOR_PROFILE_SNIFF_INSTANT_OFFSET   5

// The sniff clock reading interval has been estimated as the subrating anchor interval for the Primary,
// (i.e. next_sniff_clock->sniff_interval * next_sniff_clock->tx_subrate * 0.625)
// minus 1 master to slave slot less than the sniff interval (ie. 12*1.25=15ms)
// That is 26 * 12 * 0.625 - 15
#define SENSOR_PROFILE_CLOCK_READING_INTERVAL 180

/*! Dummy definitions of HID reports supported in spatial audio for testing */
typedef enum
{
earbud_spatial_audio_hid_report_disable = 0x00,
earbud_spatial_audio_hid_report_1at = 0x01,
earbud_spatial_audio_hid_report_1amt = 0x02,
earbud_spatial_audio_hid_report_1bt = 0x03,
earbud_spatial_audio_hid_report_1bmt = 0x04,
earbud_spatial_audio_hid_report_2at = 0x05,
earbud_spatial_audio_hid_report_2amt = 0x06,
earbud_spatial_audio_hid_report_debug = 0x07
} dummy_earbud_spatial_hid_report_t;

typedef enum
{
    /* Provisional to simulate sensor traffic */
    SENSOR_PROFILE_TEST_SCHEDULE_NEXT_DATA_PACKET = INTERNAL_MESSAGE_BASE,
    SENSOR_PROFILE_TEST_INJECT_SUBRATED_TRAFFIC,
} sensor_service_internal_msg_t;

typedef struct
{
    uint16  size_data;
    uint8   data[1];
} sensor_pdu_t;

/*! \brief The task data for this Sensor Service */
typedef struct
{
    TaskData                                        task;
    Sink                                            sink;
    Source                                          source;
    sensor_pdu_t                                    *pdu;
    uint32_t                                        packet_counter;   // Added to aid with testing
    bool                                            connected:1;
    bool                                            service_streaming:1;
    bool                                            sink_has_space:1; // SENSOR_PROFILE_SEND_DATA has been
                                                                      // received, space is available in sink.
} sensor_service_task_data_t;

static sensor_service_task_data_t *sensor_service_data = NULL;
#define sensorProfileTest_GetTaskData()      (sensor_service_data)
#define sensorProfileTest_GetTask()      (sensor_service_data->task)

static void sensorProfileTest_HandleRegistered(const SENSOR_PROFILE_CLIENT_REGISTERED_T * message)
{
    if (message->status)
    {
        DEBUG_LOG_VERBOSE(" I am now registered with SensorProfile!");
    }
    else
    {
        DEBUG_LOG_VERBOSE(" Registration with SensorProfile failed!");
    }

}

static void sensorProfileTest_HandleConnected(const SENSOR_PROFILE_CONNECTED_T * message)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_HandleConnected, Sink 0x%x", message->sink);
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (message->status == sensor_profile_status_peer_connected)
    {
        uint16 payload_size = SENSOR_PROFILE_PEER_DATA_SYNC_L2CAP_MESSAGE_SIZE;
        sensor_pdu_t *pdu = NULL;
        pdu = malloc(sizeof(sensor_pdu_t) + sizeof(uint8) * payload_size);
        memset(pdu, 0, sizeof(sensor_pdu_t) + sizeof(uint8) * payload_size);
        task_data->pdu = pdu;
        task_data->pdu->size_data = payload_size;
        PanicNull(message->sink);
        task_data->sink = message->sink;
        task_data->source = StreamSourceFromSink(message->sink);
        task_data->connected = TRUE;
        task_data->sink_has_space = TRUE;
        task_data->packet_counter = 0;
    }
    else
    {
        task_data->connected = FALSE;
    }
}

static void sensorProfileTest_HandleDisconnected(const SENSOR_PROFILE_DISCONNECTED_T * message)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_HandleDisconnected - with reason %d:",
                    message->reason);
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    task_data->sink              = 0;
    task_data->source            = 0;
    task_data->service_streaming = FALSE;
    task_data->connected = FALSE;
    task_data->sink_has_space = TRUE;
    task_data->packet_counter = 0;
    if (task_data->pdu)
    {
        free(task_data->pdu);
        task_data->pdu = NULL;
    }
}

static void sensorProfileTest_ScheduleNextSniffClockReading(int32_t delay)
{
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    MessageSendLater((Task)&task_data->task,
                      SENSOR_PROFILE_TEST_SCHEDULE_NEXT_DATA_PACKET,
                      NULL,
                      delay);
}

static bool sensorProfileTest_ScheduleNextPacket(void)
{
    DEBUG_LOG_VERBOSE(" sensorProfileTest_ScheduleNextPacket");
    next_sniff_clock_t *next_sniff_clock = NULL;
    next_sniff_clock = malloc(sizeof(next_sniff_clock_t));
    bool result = SensorProfile_NextSniffClock(next_sniff_clock);
    if (!result)
    {
        DEBUG_LOG_VERBOSE(" SensorProfile_Next_Sniff_Clock Failed !!!!!");
        sensorProfileTest_ScheduleNextSniffClockReading(SENSOR_PROFILE_CLOCK_READING_INTERVAL);
    }
    else
    {
        DEBUG_LOG_VERBOSE(" VmReadNextSniffClock() in us:  next_subrate_clock = %d, next_sniff_clock = %d, "
                          "sniff_interval = %d, rx_subrate = %d, tx_subrate = %d",
                          next_sniff_clock->next_subrate_clock,
                          next_sniff_clock->next_sniff_clock,
                          next_sniff_clock->sniff_interval,
                          next_sniff_clock->rx_subrate,
                          next_sniff_clock->tx_subrate);

        DEBUG_LOG_VERBOSE(" VmGetTimerTime %d us", VmGetTimerTime());
        int32_t us_subrate_delay = rtime_sub(next_sniff_clock->next_subrate_clock, VmGetTimerTime());
        int32_t ms_subrate_delay = us_subrate_delay/1000;
        DEBUG_LOG_VERBOSE(" Delay to send later is %d us %d ms ", us_subrate_delay, ms_subrate_delay);
        sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();

        if (ms_subrate_delay < SENSOR_PROFILE_SNIFF_INSTANT_OFFSET)
        {
//            ms_subrate_delay += SENSOR_PROFILE_CLOCK_READING_INTERVAL;
//            DEBUG_LOG_VERBOSE(" Negative Delay, try to send the packet before next anchor point (delay=%d)!!!!!", ms_subrate_delay);
              DEBUG_LOG_VERBOSE(" Already almost late, let's try to send the packet in next round immediately!");
              sensorProfileTest_ScheduleNextSniffClockReading(SENSOR_PROFILE_SNIFF_INSTANT_OFFSET);
        }
        else
        {
            ms_subrate_delay -=SENSOR_PROFILE_SNIFF_INSTANT_OFFSET;
            MessageSendLater((Task)&task_data->task,
                              SENSOR_PROFILE_TEST_INJECT_SUBRATED_TRAFFIC,
                              NULL,
                              ms_subrate_delay ); // ms
        }
    }
    free(next_sniff_clock);
    return result;
}

static bool sensorProfileTest_InjectData(void)
{
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    sensor_pdu_t *pdu = task_data->pdu;
    if ( pdu == NULL ) // Should never be Null, but check anyway
    {
        DEBUG_LOG_DEBUG(" PDU in sensorProfileTest_InjectData is NULL!!!!");
        return FALSE;
    }

    Sink sink = task_data->sink;
    /* Check sink is valid */
    if ( SinkIsValid(sink) )
    {
        uint16 sink_slack;
        /* Get the amount of available space left in the sink */
        sink_slack = SinkSlack(sink);

        /* Make sure we have enough space for this pdu */
        uint16 size = pdu->size_data;
        if ( sink_slack >= size )
        {
            for ( int i = 0; i < size; i++ )
            {
                pdu->data[i]='1';
            }
            SinkClaim(sink, size);
            uint8 *base = SinkMap(sink);
            memcpy(base, pdu->data, size);
            SinkFlush(sink, size);
            DEBUG_LOG_VERBOSE(" Sink Flush >>>>>>>>>");
            task_data->packet_counter += 1;
        }
        else
        {
            DEBUG_LOG_DEBUG(" Insufficient space in Sink!");
            task_data->sink_has_space = FALSE;
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_WARN("Sink invalid when trying to stream data!");
        return FALSE;
    }
    return TRUE;
}

static void sensorProfileTest_DataReceived(const SENSOR_PROFILE_DATA_RECEIVED_T * message)
{
    DEBUG_LOG_VERBOSE("sensorProfileTest_DataReceived");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    PanicFalse(message->source == task_data->source);

    const uint8 *s = SourceMap(message->source);
    uint16 len = SourceBoundary(message->source);
    if ( len > task_data->pdu->size_data )
    {
        DEBUG_LOG_ERROR("Unexpected long message length!");
        Panic();
    }
    sensor_pdu_t *pdu = task_data->pdu;
    memmove(pdu->data, s, len);
    SourceDrop(message->source, len);

    // In this test application will just print the data received.
    DEBUG_LOG_VERBOSE("<<< Received, length %d: %c%c%c%c%c%c%c%c%c%c%c%c  <<<", len,
                      pdu->data[0], pdu->data[1], pdu->data[2],
                      pdu->data[3], pdu->data[4], pdu->data[5],
                      pdu->data[6], pdu->data[7], pdu->data[8],
                      pdu->data[9], pdu->data[10], pdu->data[11]);
}

static void sensorProfileTest_SendData(const SENSOR_PROFILE_SEND_DATA_T * message)
{
    DEBUG_LOG_VERBOSE("sensorProfileTest_SendData");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    PanicFalse(message->sink == task_data->sink);

    if (!task_data->service_streaming)
    {
        return;
    }

    // We flag that sink is ready to receive more data.
    task_data->sink_has_space = TRUE;
}

static void sensorProfileTest_HandleSensorTraffic(void)
{
    DEBUG_LOG_VERBOSE("sensorProfileTest_HandleSensorTraffic");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (!(task_data->service_streaming))
    {
        return;
    }
    if ( task_data->sink_has_space )
    {
        if (!sensorProfileTest_ScheduleNextPacket())
        {
            DEBUG_LOG_VERBOSE("Failed to schedule next packet!");
        }
    }
}

static bool sensorProfileTest_StartDataStreaming(void)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_StartDataStreaming");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (!task_data->connected)
    {
        DEBUG_LOG_DEBUG("sensorProfileTest_StartDataStreaming -  Sensor Profile no connected!");
        return FALSE;
    }
    if (task_data->service_streaming )
    {
        DEBUG_LOG_DEBUG("sensorProfileTest_StartDataStreaming already streaming");
        return FALSE;
    }
    task_data->service_streaming = TRUE;

    // Start scheduling data traffic
    sensorProfileTest_ScheduleNextSniffClockReading(SENSOR_PROFILE_CLOCK_READING_INTERVAL);

    return TRUE;
}

static bool sensorProfileTest_StopDataStreaming(void)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_StopDataStreaming");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (!task_data->connected)
    {
        DEBUG_LOG_DEBUG("sensorProfileTest_StopDataStreaming -  Sensor Profile no connected!");
        return FALSE;
    }
    if (task_data->service_streaming == FALSE)
    {
        DEBUG_LOG_DEBUG("sensorProfileTest_StopDataStreaming - No streaming");
        return FALSE;
    }
    task_data->service_streaming = FALSE;
    task_data->packet_counter = 0;
    return TRUE;
}

static void sensorProfileTest_HandlePeerEnableReq(const SENSOR_PROFILE_ENABLE_PEER_REQ_T * msg)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_HandlePeerEnableReq: ");
    DEBUG_LOG_DEBUG("sensor_interval = %d", msg->sensor_interval);
    DEBUG_LOG_DEBUG("processing_source = %d", msg->processing_source);
    DEBUG_LOG_DEBUG("report_id = %d", msg->report_id);

    // Configuration data, sensor_interval and Tx type would be handled here.

    dummy_earbud_spatial_hid_report_t report_id = (dummy_earbud_spatial_hid_report_t)msg->report_id;
    if (report_id != earbud_spatial_audio_hid_report_2at)
    {
        DEBUG_LOG_DEBUG("report_id expected = %d, got = %d",
                        msg->report_id, earbud_spatial_audio_hid_report_2at);
    }

    if (msg->sensor_interval != SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG)
    {
        DEBUG_LOG_DEBUG("sensor_interval expected = %d, got = %d",
                        msg->sensor_interval, SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG);
    }

    // In any case this test application will just start streaming.
    sensorProfileTest_StartDataStreaming();

    // Finally trigger Peer Enabled confirmation status message to Primary.
    SensorProfile_PeerEnabled(SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG,
                              SENSOR_PROFILE_PROCESSING_REMOTE);
}

static void sensorProfileTest_HandlePeerDisableReq(void)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_HandlePeerDisableReq");

    // In this test application will just stop streaming.
    sensorProfileTest_StopDataStreaming();

    // Finally trigger Peer Enabled confirmation status message to Primary.
    SensorProfile_PeerDisabled();
}

static void sensorProfileTest_HandlePeerEnabled(const SENSOR_PROFILE_PEER_ENABLED_T * msg)
{
    DEBUG_LOG_DEBUG("sensorProfileTest_HandlePeerEnabled: ");
    DEBUG_LOG_DEBUG("sensor_interval = %d", msg->sensor_interval);
    DEBUG_LOG_DEBUG("processing_source = %d", msg->processing_source);

    // Status data would be handled here.
    if (msg->sensor_interval != SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG)
    {
        DEBUG_LOG_DEBUG("sensor_interval got = %d, expected = %d",
                        msg->sensor_interval, SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG);
    }
    if (msg->processing_source != SENSOR_PROFILE_PROCESSING_REMOTE)
    {
        DEBUG_LOG_DEBUG("processing_source got = %d, expected = %d",
                        msg->processing_source, SENSOR_PROFILE_PROCESSING_REMOTE);
    }
}

static void sensorProfile_Test_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_VERBOSE("sensorProfile_Test_HandleMessage MESSAGE:sensor_profile_msg_t:0x%x", id);
    switch (id)
    {
        case SENSOR_PROFILE_CLIENT_REGISTERED:
            sensorProfileTest_HandleRegistered((const SENSOR_PROFILE_CLIENT_REGISTERED_T *)message);
        break;
        case SENSOR_PROFILE_CONNECTED:
            sensorProfileTest_HandleConnected((const SENSOR_PROFILE_CONNECTED_T *)message);
        break;
        case SENSOR_PROFILE_DISCONNECTED:
            sensorProfileTest_HandleDisconnected((const SENSOR_PROFILE_DISCONNECTED_T *)message);
        break;
        case SENSOR_PROFILE_DATA_RECEIVED:
            sensorProfileTest_DataReceived((const SENSOR_PROFILE_DATA_RECEIVED_T *)message);
        break;
        case SENSOR_PROFILE_SEND_DATA:
            sensorProfileTest_SendData((const SENSOR_PROFILE_SEND_DATA_T *)message);
        break;
        case SENSOR_PROFILE_TEST_SCHEDULE_NEXT_DATA_PACKET:
            sensorProfileTest_HandleSensorTraffic();
        break;
        case SENSOR_PROFILE_TEST_INJECT_SUBRATED_TRAFFIC:
            sensorProfileTest_InjectData();
            sensorProfileTest_ScheduleNextSniffClockReading(SENSOR_PROFILE_CLOCK_READING_INTERVAL);
        break;
        case SENSOR_PROFILE_ENABLE_PEER_REQ:
            sensorProfileTest_HandlePeerEnableReq((const SENSOR_PROFILE_ENABLE_PEER_REQ_T *)message);
        break;
        case SENSOR_PROFILE_DISABLE_PEER_REQ:
            sensorProfileTest_HandlePeerDisableReq();
        break;
        case SENSOR_PROFILE_PEER_ENABLED:
            sensorProfileTest_HandlePeerEnabled((const SENSOR_PROFILE_PEER_ENABLED_T *)message);
        break;
        case SENSOR_PROFILE_PEER_DISABLED:
            DEBUG_LOG_DEBUG("sensorProfile_Test_HandleMessage: SENSOR_PROFILE_PEER_DISABLED received");
        break;
        default:
            DEBUG_LOG_DEBUG("sensorProfile_Test_HandleMessage: Unhandled message: 0x%04X", id);
            break;
    }
}

#ifdef GC_SECTIONS
    /* Move all functions in KEEP_PM section to ensure they are not removed during
     * garbage collection */
    #pragma unitcodesection KEEP_PM
#endif

bool SensorProfileTest_Register(void)
{
    DEBUG_LOG_DEBUG("SensorProfileTest_Register");
    sensor_service_data = (sensor_service_task_data_t*) PanicUnlessMalloc(sizeof(*sensor_service_data));
    sensor_service_data->task.handler = sensorProfile_Test_HandleMessage;
    sensor_service_data->sink                      = 0;
    sensor_service_data->source                    = 0;
    sensor_service_data->pdu                       = NULL;
    sensor_service_data->connected                 = FALSE;
    sensor_service_data->service_streaming         = FALSE;
    sensor_service_data->sink_has_space            = TRUE;
    sensor_service_data->packet_counter            = 0;

    return SensorProfile_Register(&sensorProfileTest_GetTask());
}

bool SensorProfileTest_Enable(void)
{
    DEBUG_LOG_DEBUG("SensorProfileTest_Enable");
    if (!SensorProfile_IsRolePrimary())
    {
        return FALSE;
    }
    return sensorProfileTest_StartDataStreaming();
}

bool SensorProfileTest_Disable(void)
{
    DEBUG_LOG_DEBUG("SensorProfileTest_Disable");
    if (!SensorProfile_IsRolePrimary())
    {
        return FALSE;
    }
    return sensorProfileTest_StopDataStreaming();
}

bool SensorProfileTest_EnablePeer(void)
{
    DEBUG_LOG_DEBUG("SensorProfileTest_EnablePeer");
    if (!SensorProfile_IsRolePrimary())
    {
        return FALSE;
    }
    return SensorProfile_EnablePeer(SENSOR_PROFILE_DUMMY_SENSOR_INTERVAL_CONFIG,
                                    SENSOR_PROFILE_PROCESSING_REMOTE,
                                    earbud_spatial_audio_hid_report_2at);
}

bool SensorProfileTest_DisablePeer(void)
{
    DEBUG_LOG_DEBUG("SensorProfileTest_DisablePeer");
    if (!SensorProfile_IsRolePrimary())
    {
        return FALSE;
    }
    SensorProfile_DisablePeer();
    return TRUE;
}

uint32_t SensorProfileTest_PacketCounter(void)
{
    DEBUG_LOG_DEBUG("SensorProfile_Test_PacketCounter");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (task_data)
    {
        return task_data->packet_counter;
    }
    return 0;
}
bool SensorProfileTest_Connected(void)
{
    DEBUG_LOG_DEBUG("SensorProfile_Test_Connected");
    sensor_service_task_data_t *task_data = sensorProfileTest_GetTaskData();
    if (task_data)
    {
        return task_data->connected;
    }
    return FALSE;
}

#endif // INCLUDE_SENSOR_PROFILE
