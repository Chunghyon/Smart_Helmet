/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       sensor_profile_peer_data_sync_l2cap.c
\brief      Sensor Profile L2cap Channel creation for sensor data synchronisation.
*/
#ifdef INCLUDE_SENSOR_PROFILE

#include "sensor_profile_peer_data_sync_l2cap.h"
#include "sensor_profile.h"
#include "sensor_profile_private.h"
#include "l2cap_manager.h"
#include <panic.h>
#include <logging.h>
#include <bt_device.h>
#include <message.h>
#include <sdp.h>
#include <sink.h>
#include <source.h>
#include <stream.h>


/*! Sensor Profile L2cap MTU Size */
#define SENSOR_PROFILE_L2CAP_MTU_SIZE_IN        (672) // Perhaps unnecessarily too big...
#define SENSOR_PROFILE_L2CAP_MTU_SIZE_OUT       (672)

#define SENSOR_PROFILE_SEC_REQ    ((SECL4_IN_SSP | SECL_IN_AUTHENTICATION | SECL_IN_ENCRYPTION) | (SECL4_OUT_SSP | SECL_OUT_AUTHENTICATION | SECL_OUT_ENCRYPTION))

/*! L2cap flush timeout(in us) for Sensor Sync Channel */
#define SENSOR_PROFILE_DATA_SYNC_L2CAP_FLUSH_TIMEOUT (100000u)


/*! \brief The task data for the Sensor Profile L2CAP Peer Link sub-module. */
typedef struct
{
    /*! Task registered to receive Sensor Profile messages. */
    Task                                            reg_task;

    /*! Task that requested connection of the Sensor Profile. */
    Task                                            connect_task;

    /*! Task that requested disconnection of the Sensor Profile. */
    Task                                            disconnect_task;

    /*! Unique l2cap manager identifier */
    l2cap_manager_instance_id                       psm_instance_id;

    /*! The sink of the L2CAP link */
    Sink                                            sink;

    /*! The source of the L2CAP link */
    Source                                          source;

    /*! The bluetooth address of the peer */
    bdaddr                                          peer_addr;
} sensor_profile_peer_data_sync_data_t;


/*! \brief The task data for Sensor Profile L2CAP Peer Link. */
static sensor_profile_peer_data_sync_data_t *sensor_profile_peer_link_data = NULL;

/*! \brief The macro that returns the pointer to the task data. */
#define sensorProfile_GetL2capPeerLinkData() (sensor_profile_peer_link_data)

static CsrBtUuid128 service_uuid_sensor_profile_l2cap_peer_link = {UUID_SENSOR_PROFILE_SERVICE};

/*! \brief Send confirmation of registration message.

    \param[in] status boolean to indicate success or failure of the registration.
*/
static void sensorProfile_SendRegistered(bool status)
{
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    if(task_data->reg_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_CLIENT_REGISTERED_T);
        message->status = status;
        MessageSend(task_data->reg_task, SENSOR_PROFILE_CLIENT_REGISTERED, message);
    }
}

/*! \brief Send indication message of whether or not the profile was successfully connected.

    \param[in] status boolean to indicate success or failure.
    \param[in] sink Sink for the stream associated to this connection. Null on failure.
*/
static void sensorProfile_SendConnected(sensor_profile_status_t status, Sink sink)
{
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    /* Send SENSOR_PROFILE_CONNECTED to registered client. */
    if(task_data->reg_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_CONNECTED_T);
        message->status = status;
        message->sink = sink;
        MessageSend(task_data->reg_task, SENSOR_PROFILE_CONNECTED, message);
    }

    /* Send SENSOR_PROFILE_CONNECTED to client which made the connect request. */
    /* This should only be sent in the Primary earbud, where connect_task is not NULL */
    if(task_data->connect_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_CONNECTED_T);
        message->status = status;
        message->sink = sink;
        MessageSend(task_data->connect_task, SENSOR_PROFILE_CONNECTED, message);
        task_data->connect_task = NULL;
    }
}

/*! \brief Send indication message of profile disconnection.

    \param[in] reason   Reason for disconnection.
*/
static void sensorProfile_SendDisconnected(sp_disconnect_status_with_reason_t reason)
{
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    /* Send SENSOR_PROFILE_DISCONNECTED to registered client. */
    if(task_data->reg_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_DISCONNECTED_T);
        message->status = sensor_profile_status_peer_disconnected;
        message->reason = reason;
        MessageSend(task_data->reg_task, SENSOR_PROFILE_DISCONNECTED, message);
    }

    /* Send SENSOR_PROFILE_DISCONNECTED to client which made a disconnect request. */
    if(task_data->disconnect_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_DISCONNECTED_T);
        message->status = sensor_profile_status_peer_disconnected;
        message->reason = reason;
        MessageSend(task_data->disconnect_task, SENSOR_PROFILE_DISCONNECTED, message);
        task_data->disconnect_task = NULL;
    }
}

/*! \brief Send message to indicate data has been received from peer.

    \param[in] source   The source of the link, where the data will be read from.
*/
static void sensorProfile_SendDataReceived(Source source)
{
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    if(task_data->reg_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_DATA_RECEIVED_T);
        message->source = source;
        MessageSend(task_data->reg_task, SENSOR_PROFILE_DATA_RECEIVED, message);
    }
}

/*! \brief Send message to indicate peer is ready to receive data.

    \param[in] sink   The sink of the link, where the data will be written to.
*/
static void sensorProfile_SendSendData(Sink sink)
{
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    if(task_data->reg_task != NULL)
    {
        MESSAGE_MAKE(message, SENSOR_PROFILE_SEND_DATA_T);
        message->sink = sink;
        MessageSend(task_data->reg_task, SENSOR_PROFILE_SEND_DATA, message);
    }
}

/*! \brief Registration indication callback handler.

    \param[in] status Indicates success or failure of the operation.
*/
static void sensorProfile_RegisteredInd(l2cap_manager_status_t status)
{
    if (status != l2cap_manager_status_success)
    {
        DEBUG_LOG_WARN("sensorProfile HandleRegisteredInd: ERROR! Failed to register L2CAP PSM!");
        sensorProfile_SendRegistered(FALSE);
    }
    else
    {
        DEBUG_LOG_DEBUG("sensorProfile HandleRegisteredInd: OK");
        sensorProfile_SendRegistered(TRUE);
    }
}

/*! \brief SDP record callback getter.

    \param[in] local_psm The local PSM to be inserted to the SDP record.
    \param[out] sdp_record  SDP record, its size, and the position of the
                            PSM to be inserted within the record.
    \return Returns #l2cap_manager_status_success.
*/
static l2cap_manager_status_t sensorProfile_GetSdpRecord(uint16 local_psm, l2cap_manager_sdp_record_t *sdp_record)
{
    UNUSED(local_psm);
    DEBUG_LOG_DEBUG("sensorProfile_GetSdpRecord");
    sdp_record->service_record      = appSdpGetSensorProfileServiceRecord();
    sdp_record->service_record_size = appSdpGetSensorProfileServiceRecordSize();
    sdp_record->offset_to_psm       = appSdpGetSensorProfileServiceRecordPsmOffset();

    return l2cap_manager_status_success;
}

/*! \brief SDP search pattern callback getter.

    \param[in] tpaddr Transport Bluetooth Address of the remote device
                      that the L2CAP Manager will carry out the SDP search.
    \param[out] sdp_search_pattern  SDP search pattern used for obtaining
                                    the remote PSM from the peer.
    \return Returns #l2cap_manager_status_success.
*/
static l2cap_manager_status_t sensorProfile_GetSdpSearchPattern(const tp_bdaddr *tpaddr, l2cap_manager_sdp_search_pattern_t *sdp_search_pattern)
{
    UNUSED(tpaddr);
    DEBUG_LOG_DEBUG("sensorProfile_GetSdpSearchPattern");
    uint16 length;
    sdp_search_pattern->max_num_of_retries = 3;
    sdp_search_pattern->service_uuid_type = SDP_DATA_ELEMENT_SIZE_128_BITS;
    memcpy(sdp_search_pattern->service_uuid128, service_uuid_sensor_profile_l2cap_peer_link, sizeof(CsrBtUuid128));
    sdp_search_pattern->max_attributes      = 0x32;
    sdp_search_pattern->attribute_list      = appSdpGetSensorProfileLinkAttributeSearchRequest(&length);
    sdp_search_pattern->attribute_list_size = length;

    return l2cap_manager_status_success;
}

/*! \brief L2CAP link configuration callback getter.

    \param[in] tpaddr Transport Bluetooth Address of the remote device
                      that the L2CAP Manager will try to connect.
    \param[out] config  Pointer to the configuration parameters for the
                        L2CAP link to be established.
    \return Returns #l2cap_manager_status_success.
*/
static l2cap_manager_status_t sensorProfile_GetL2capLinkConfig(const tp_bdaddr *tpaddr, l2cap_manager_l2cap_link_config_t *config)
{
    UNUSED(tpaddr);
    DEBUG_LOG_DEBUG("sensorProfile_GetL2capLinkConfig");
    static const uint16 conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Flow & Error Control Mode. */
        L2CAP_AUTOPT_FLOW_MODE,
        /* Set to Basic mode with no fallback mode. */
            BKV_16_FLOW_MODE(FLOW_MODE_BASIC, 0),
        /* Local MTU exact value (incoming). */
        L2CAP_AUTOPT_MTU_IN,
        /*  Exact MTU for this L2CAP connection. */
            SENSOR_PROFILE_L2CAP_MTU_SIZE_IN,
        /* Remote MTU Minumum value (outgoing). */
        L2CAP_AUTOPT_MTU_OUT,
        /*  Minimum MTU accepted from the Remote device. */
            SENSOR_PROFILE_L2CAP_MTU_SIZE_OUT,
        /* Local Flush Timeout - Accept Non-default Timeout. */
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(SENSOR_PROFILE_DATA_SYNC_L2CAP_FLUSH_TIMEOUT, 0),
        /* Configuration Table must end with a terminator. */
        L2CAP_AUTOPT_TERMINATOR
    };
    config->security_level = SENSOR_PROFILE_SEC_REQ;
    config->conftab_length = sizeof(conftab);
    config->conftab        = conftab;

    return l2cap_manager_status_success;
}

/*! \brief Peer address callback getter.

    \param[out] tpaddr Typed Bluetooth address of peer.

    \return Returns FALSE if peer bt address is not available, TRUE otherwise.
*/
static bool sensorProfile_GetPeerBdAddr(tp_bdaddr *tpaddr)
{
    DEBUG_LOG_DEBUG("sensorProfile_GetPeerBdAddr");
    bdaddr self;
    bdaddr primary;
    bdaddr secondary;

    if(appDeviceGetPrimaryBdAddr(&primary) && appDeviceGetMyBdAddr(&self))
    {
        if (BdaddrIsSame(&primary, &self))
        {
            /* The peer device is the Secondary. */
            if (appDeviceGetSecondaryBdAddr(&secondary))
            {
                tpaddr->transport  = TRANSPORT_BREDR_ACL;
                tpaddr->taddr.type = TYPED_BDADDR_PUBLIC;
                tpaddr->taddr.addr = secondary;
                DEBUG_LOG_DEBUG("sensorProfile_GetPeerBdAddr: %04X-%02X-%06X (Secondary)",
                               secondary.nap, secondary.uap, secondary.lap);
                return TRUE;
            }
        }
        else
        {
            /* The peer device is the Primary. */
            tpaddr->transport  = TRANSPORT_BREDR_ACL;
            tpaddr->taddr.type = TYPED_BDADDR_PUBLIC;
            tpaddr->taddr.addr = primary;
            DEBUG_LOG_DEBUG("sensorProfile_GetPeerBdAddr: %04X-%02X-%06X (Primary)",
                            primary.nap, primary.uap, primary.lap);
            return TRUE;
        }
    }

    DEBUG_LOG_WARN("sensorProfile_GetPeerBdAddr: WARNING! Failed to get the peer BD-ADDR!");
    return FALSE;
}

/*! \brief Connect indication callback handler.

    \param[in] ind Struct to inform a client about an incoming L2CAP connection.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or #l2cap_manager_status_failure.
*/
static l2cap_manager_status_t sensorProfile_RespondConnectInd(const l2cap_manager_connect_ind_t *ind, l2cap_manager_connect_rsp_t *rsp, void **context)
{
    UNUSED(context);
    DEBUG_LOG_DEBUG("sensorProfile_RespondConnectInd");
    /* Check if the connection request is originated from the peer device or not. */
    tp_bdaddr remote_tpaddr;
    if (!sensorProfile_GetPeerBdAddr(&remote_tpaddr))
    {
        DEBUG_LOG_ERROR("sensorProfile RespondConnectInd: ERROR! Failed to get the peer BD-ADDR!");
        Panic();
    }

    if (!BdaddrIsSame(&ind->tpaddr.taddr.addr, &remote_tpaddr.taddr.addr))
    {
        rsp->response       = FALSE;    /* Reject the request. */
        rsp->conftab_length = 0;
        rsp->conftab        = NULL;
        return l2cap_manager_status_failure;
    }

    static const uint16 sensor_profile_peer_data_sync_conftab_response[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Local Flush Timeout - Accept Non-default Timeout. */
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(SENSOR_PROFILE_DATA_SYNC_L2CAP_FLUSH_TIMEOUT, 0),
        L2CAP_AUTOPT_TERMINATOR
    };

    rsp->response       = TRUE;     /* Accept the connection request. */
    rsp->conftab_length = sizeof(sensor_profile_peer_data_sync_conftab_response);
    rsp->conftab        = sensor_profile_peer_data_sync_conftab_response;

    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    // Update connected peer bd address
    task_data->peer_addr = ind->tpaddr.taddr.addr;

    // If execution got here the processing of the callback was successful
    return l2cap_manager_status_success;
}

/*! \brief Connect confirmation callback handler.

    \param[in] cfm  Struct containing the result of the L2CAP connection attempt.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_HandleConnectCfm(const l2cap_manager_connect_cfm_t *cfm, void *context)
{
    UNUSED(context);
    DEBUG_LOG_DEBUG("sensorProfile HandleConnectCfm - Status:0x%04X, Connection ID: 0x%04X",
                    cfm->status, cfm->connection_id);
    DEBUG_LOG_V_VERBOSE("sensorProfile HandleConnectCfm: Remote BD-ADDR:         %04X-%02X-%06X",
                      cfm->tpaddr.taddr.addr.nap, cfm->tpaddr.taddr.addr.uap, cfm->tpaddr.taddr.addr.lap);

    if (l2cap_connect_success != cfm->status)
    {
        if (l2cap_manager_connect_status_failed_sdp_search == cfm->status)
            DEBUG_LOG_WARN("sensorProfile HandleConnectCfm: WARNING! Failed: SDP Search.");
        else
            DEBUG_LOG_WARN("sensorProfile HandleConnectCfm: WARNING! Failed to connect! (Status: 0x%X)", cfm->status);
        sensorProfile_SendConnected(sensor_profile_status_peer_connect_failed, 0);
    }
    else
    {
        PanicNull(cfm->sink);
        sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
        task_data->sink = cfm->sink;
        task_data->source = StreamSourceFromSink(cfm->sink);

        DEBUG_LOG_DEBUG("sensorProfile HandleConnectCfm: Connected!");
        sensorProfile_SendConnected(sensor_profile_status_peer_connected, task_data->sink);
    }

    // If execution got here the processing of the callback was successful
    return l2cap_manager_status_success;
}

/*! \brief Disconnect confirmation callback handler.

    \param[in] status  status of the disconnect request or unsolicited disconnect reason.
    \param[in] Sink sink.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_Disconnect(const l2cap_manager_disconnect_status_t status,
                                                       Sink sink)
{
    DEBUG_LOG_DEBUG("sensorProfile_HandleDisconnect(MESSAGE:l2cap_manager_disconnect_status_t:%d)", status);
    if ((status == l2cap_manager_disconnect_successful)||
        (status == l2cap_manager_disconnect_transferred))
    {
        sensorProfile_SendDisconnected(sp_disconnect_successful);
    }
    else if (status == l2cap_manager_disconnect_link_loss)
    {
        sensorProfile_SendDisconnected(sp_disconnect_l2cap_link_loss);
    }
    else
    {
        sensorProfile_SendDisconnected(sp_disconnect_error);
    }

    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    PanicFalse(task_data->sink == sink);
    task_data->sink              = 0;
    task_data->source            = 0;
    task_data->disconnect_task   = 0;

    // If execution got here the processing of the callback was successful
    return l2cap_manager_status_success;
}

/*! \brief Disconnect indication callback handler.

    \param[in] ind  Struct to inform that an L2CAP connection has been disconnected.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_RespondDisconnectInd(const l2cap_manager_disconnect_ind_t *ind, void *context)
{
    UNUSED(context);              
    return sensorProfile_Disconnect(ind->status, ind->sink);
}

/*! \brief Disconnect confirmation callback handler.

    \param[in] cfm  Struct containing the result of the L2CAP disconnection attempt.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_HandleDisconnectCfm(const l2cap_manager_disconnect_cfm_t *cfm, void *context)
{
    UNUSED(context);
    return sensorProfile_Disconnect(cfm->status, cfm->sink);
}

/*! \brief Process that a source associated with an L2CAP connection has received data.

    \param[in] more_data  Data struct that contains the source, in which new data is received.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_ProcessMoreData(const l2cap_manager_message_more_data_t *more_data, void *context)
{
    UNUSED(context);
    DEBUG_LOG_VERBOSE("sensorProfile_ProcessMoreData: (connection_id: 0x%X)", more_data->connection_id);
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    Source source = task_data->source;
    if (source != more_data->source)
    {
        DEBUG_LOG_ERROR("sensorProfile HandleMoreData: unmatched source ERROR!!!,"
                        " expected:0x%04X actual:0x%04X", source, more_data->source);
        Panic();
    }   
    sensorProfile_SendDataReceived(source);
    return l2cap_manager_status_success;
}

/*! \brief Process that a source associated with an L2CAP connection has received data.

    \param[in/out] more_space Data struct that contains the sink that gets some space to send data.
    \param[in] context - Not used.

    \return Returns #l2cap_manager_status_success or panic.
*/
static l2cap_manager_status_t sensorProfile_ProcessMoreSpace(l2cap_manager_message_more_space_t *more_space, void *context)
{
    UNUSED(context);
    DEBUG_LOG_VERBOSE("sensorProfile_ProcessMoreSpace: (connection_id: 0x%X)", more_space->connection_id);
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    Sink sink = task_data->sink;
    if (sink != more_space->sink)
    {
        DEBUG_LOG_ERROR("sensorProfile_ProcessMoreSpace: unmatched sink ERROR!, "
                        "expected:0x%04X actual:0x%04X", sink, more_space->sink);
        Panic();
    }
    // We need to notify the client when the peer earbud is ready to receive data. This allows the client
    // to manage the link's throughput without filling up the sink and gives the client fine control
    // of the flow of the time sensitive sensor data.
    // The client shall wait for this notification before scheduling the next read of sensor data to be sent
    // via the the sensor profile link.
    sensorProfile_SendSendData(sink);
    return l2cap_manager_status_success;
}

bool SensorProfile_L2capManagerRegister(Task client_task)
{
    DEBUG_LOG_DEBUG("SensorProfile_L2capManager_Register");
    // Currently SensorProfile only allows a single client registration
    if (sensorProfile_GetL2capPeerLinkData())
    {
        DEBUG_LOG_DEBUG("SensorProfile_L2capManagerRegister - A client is already registered)");
        return FALSE;
    }

    static const l2cap_manager_functions_t functions =
    {
        .registered_ind         = sensorProfile_RegisteredInd,
        .get_sdp_record         = sensorProfile_GetSdpRecord,
        .get_sdp_search_pattern = sensorProfile_GetSdpSearchPattern,
        .get_l2cap_link_config  = sensorProfile_GetL2capLinkConfig,
        .respond_connect_ind    = sensorProfile_RespondConnectInd,
        .handle_connect_cfm     = sensorProfile_HandleConnectCfm,
        .respond_disconnect_ind = sensorProfile_RespondDisconnectInd,
        .handle_disconnect_cfm  = sensorProfile_HandleDisconnectCfm,
        .process_more_data      = sensorProfile_ProcessMoreData,
        .process_more_space     = sensorProfile_ProcessMoreSpace,
    };

    sensor_profile_peer_link_data = (sensor_profile_peer_data_sync_data_t*) PanicUnlessMalloc(sizeof(sensor_profile_peer_data_sync_data_t));
    sensor_profile_peer_link_data->reg_task          = client_task;
    sensor_profile_peer_link_data->connect_task      = 0;
    sensor_profile_peer_link_data->disconnect_task   = 0;
    sensor_profile_peer_link_data->psm_instance_id   = L2CAP_MANAGER_PSM_INSTANCE_ID_INVALID;
    sensor_profile_peer_link_data->sink              = 0;
    sensor_profile_peer_link_data->source            = 0;

    l2cap_manager_status_t result = L2capManager_Register(L2CAP_MANAGER_PSM_DYNAMIC_ALLOCATION, &functions, &sensor_profile_peer_link_data->psm_instance_id);
    if (result != l2cap_manager_status_success)
    {
        DEBUG_LOG_ERROR("SensorProfile_L2capManagerRegister - Some error has occurred!(Result: 0x%X)", result);
        Panic();
    }
    return TRUE;
}

bool SensorProfile_L2capConnect(Task connect_task, const bdaddr *peer_addr)
{
    DEBUG_LOG_DEBUG("SensorProfile_L2capConnect");
    if (!SensorProfile_IsRolePrimary())
    {
        // It should never be here. But flag this anyway.
        DEBUG_LOG_ERROR("SensorProfile Connect request can only be called on Primary earbud!");
        Panic();
    }
    if(!peer_addr)
    {
        DEBUG_LOG_ERROR("SensorProfile_L2capConnect - Peer address is NULL");
        Panic();
    }
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    PanicNull(task_data);
    if (task_data->disconnect_task != NULL)
    {
        PanicFalse(task_data->connect_task == connect_task);
    }

    task_data->connect_task = connect_task;
    task_data->peer_addr = *peer_addr;

    tp_bdaddr tpaddr;
    tpaddr.transport  = TRANSPORT_BREDR_ACL;
    tpaddr.taddr.type = TYPED_BDADDR_PUBLIC;
    tpaddr.taddr.addr = task_data->peer_addr;
    if (L2capManager_IsConnected(&tpaddr, task_data->psm_instance_id))
    {
        DEBUG_LOG_DEBUG("SensorProfile_L2capConnect - already connecting/ed");
        sensorProfile_SendConnected(sensor_profile_status_peer_connected, task_data->sink);
        return FALSE; // It will return FALSE as there was not an actual connection command execution.
    }

    l2cap_manager_status_t result = L2capManager_Connect(&tpaddr, task_data->psm_instance_id, 0);
    if (result != l2cap_manager_status_success)
    {
        DEBUG_LOG_WARN("SensorProfile_L2cap_Connect - Some error has occurred!(Result: 0x%X)", result);
        sensorProfile_SendConnected(sensor_profile_status_peer_connect_failed, 0);
        return FALSE;
    }
    return TRUE;
}

bool SensorProfile_L2capDisconnect(Task disconnect_task)
{
    DEBUG_LOG_DEBUG("SensorProfile_L2capDisconnect");
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    PanicNull(task_data);
    if (task_data->disconnect_task != NULL)
    {
        PanicFalse(task_data->disconnect_task == disconnect_task);
    }

    tp_bdaddr tpaddr;
    tpaddr.transport  = TRANSPORT_BREDR_ACL;
    tpaddr.taddr.type = TYPED_BDADDR_PUBLIC;
    tpaddr.taddr.addr = task_data->peer_addr;
    if (!L2capManager_IsConnected(&tpaddr, task_data->psm_instance_id))
    {
        DEBUG_LOG_DEBUG("SensorProfile_L2capDisconnect - already disconnected/ing");
        sensorProfile_SendDisconnected(sp_disconnect_successful);
        return FALSE; // It will return FALSE as there was not an actual disconnection command execution.
    }

    task_data->disconnect_task = disconnect_task;
    l2cap_manager_status_t result = L2capManager_Disconnect(task_data->sink, task_data->psm_instance_id);
    if (result != l2cap_manager_status_success)
    {
        DEBUG_LOG_WARN("SensorProfile_L2capDisconnect - Some error has occurred!(Result: 0x%X)", result);
        sensorProfile_SendDisconnected(sp_disconnect_error);
        return FALSE;
    }
    return TRUE;
}

bool SensorProfile_NextSniffClock(next_sniff_clock_t *next_sniff)
{
    DEBUG_LOG_VERBOSE("SensorProfile_NextSniffClock");
    sensor_profile_peer_data_sync_data_t *task_data = sensorProfile_GetL2capPeerLinkData();
    tp_bdaddr tpbdaddr;
    BdaddrTpFromBredrBdaddr(&tpbdaddr, &task_data->peer_addr);

    return VmReadNextSniffClock(&tpbdaddr, next_sniff);
}
#endif /* INCLUDE_SENSOR_PROFILE */
