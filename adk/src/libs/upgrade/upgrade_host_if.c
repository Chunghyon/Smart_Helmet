/****************************************************************************
Copyright (c) 2014 - 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_host_if.c

DESCRIPTION

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <string.h>
#include <logging.h>
#include <panic.h>
#include <print.h>
#include <gaia.h>
#include <upgrade.h>
#include <byte_utils.h>

#include "upgrade_ctx.h"
#include "upgrade_host_if.h"
#include "upgrade_host_if_data.h"
#include "upgrade_msg_internal.h"

/****************************************************************************
NAME
    UpgradeHostIFClientConnect

DESCRIPTION
    Set the clientTask in upgradeCtx.

*/
void UpgradeHostIFClientConnect(Task clientTask)
{
    UpgradeCtxGet()->clientTask = clientTask;
}

/****************************************************************************
NAME
    UpgradeHostIFClientSendData

DESCRIPTION
    Send a data packet to a connected upgrade client.

*/
void UpgradeHostIFClientSendData(uint8 *data, uint16 dataSize)
{
    UpgradeCtx *ctx = UpgradeCtxGet();
    if (ctx->transportTask)
    {
        /* Create DATA_IND to hold packet */
        UPGRADE_TRANSPORT_DATA_IND_T *dataInd = (UPGRADE_TRANSPORT_DATA_IND_T *)PanicUnlessMalloc(sizeof(*dataInd) + dataSize - 1);
        ByteUtilsMemCpyToStream(dataInd->data, data, dataSize);

        dataInd->size_data = dataSize;
        dataInd->is_data_state = UpgradeIsPartitionDataState();

        DEBUG_LOG_VERBOSE("UpgradeHostIFClientSendData, size %u", dataSize);
        DEBUG_LOG_DATA_V_VERBOSE(data, dataSize);

        MessageSend(ctx->transportTask, UPGRADE_TRANSPORT_DATA_IND, dataInd);
    }
    else
    {
        DEBUG_LOG_ERROR("UpgradeHostIFClientSendData, NULL transport");
        DEBUG_LOG_DATA_ERROR(data, dataSize);
    }

    free(data);
}

/****************************************************************************
NAME
    UpgradeHostIFTransportConnect

DESCRIPTION
    Process an upgrade connect request from a client.
    Send a response to the transport in a UPGRADE_TRANSPORT_CONNECT_CFM.

    If Upgrade library has not been initialised return 
    upgrade_status_unexpected_error.

    If an upgrade client is already connected return
    upgrade_status_already_connected_warning.
    
    Otherwise return upgrade_status_success.

*/
void UpgradeHostIFTransportConnect(Task transportTask, upgrade_data_cfm_type_t cfm_type, uint32 max_request_size)
{
    MESSAGE_MAKE(connectCfm, UPGRADE_TRANSPORT_CONNECT_CFM_T);

    if (!UpgradeIsInitialised())
    {
        /*! @todo Should really add a new error type here? */
        connectCfm->status = upgrade_status_unexpected_error;
    }
    else if(UpgradeCtxGet()->transportTask)
    {
        connectCfm->status = upgrade_status_already_connected_warning;
    }
    else
    {
        connectCfm->status = upgrade_status_success;
        UpgradeCtxGet()->transportTask = transportTask;
        UpgradeCtxGet()->data_cfm_type = cfm_type;
        UpgradeCtxGet()->max_request_size = max_request_size;

#ifndef HOSTED_TEST_ENVIRONMENT
        /* Notify app of the transport connect */
        UpgradeSendUpgradeTransportStatus(UpgradeGetAppTask(), upgrade_notify_transport_connect);
#endif
    }

    MessageSend(transportTask, UPGRADE_TRANSPORT_CONNECT_CFM, connectCfm);
}

/****************************************************************************
NAME
    UpgradeHostIFProcessDataRequest

DESCRIPTION
    Process a data packet from an Upgrade client.
    Send a response to the transport in a UPGRADE_TRANSPORT_DATA_CFM.

    If Upgrade library has not been initialised or there is no
    transport connected, do nothing.
*/
bool UpgradeHostIFProcessDataRequest(uint8 *data, uint16 dataSize)
{
    DEBUG_LOG_VERBOSE("UpgradeHostIFProcessDataRequest, data %p, data_size %u", data, dataSize);

    if (!UpgradeIsInitialised())
    {
        DEBUG_LOG_ERROR("UpgradeHostIFProcessDataRequest, not initialised");
        return FALSE;
    }

    UpgradeCtx *ctx = UpgradeCtxGet();
    if (!ctx->transportTask)
    {
        DEBUG_LOG_ERROR("UpgradeHostIFProcessDataRequest, not connected");
        return FALSE;
    }

    bool is_data_cfm_needed;
    bool status = UpgradeHostIFDataBuildIncomingMsg(ctx->clientTask, data, dataSize, &is_data_cfm_needed);

    if(!is_data_cfm_needed)
    {
        /* No need to send the data confirmation. Client task will send it on its own. */
        DEBUG_LOG("UpgradeHostIFProcessDataRequest, cfm skipped");
        return TRUE;
    }

    MESSAGE_MAKE(msg, UPGRADE_TRANSPORT_DATA_CFM_T);
    msg->packet_type = data[0];
    msg->status = status ? upgrade_status_success : upgrade_status_unexpected_error;
    msg->data = data;
    msg->size_data = dataSize;

    if(ctx->isTransportCfmDelayed)
    {
        UpgradeCtxGet()->delayDataCfm = TRUE;
#ifndef HOSTED_TEST_ENVIRONMENT
        MessageSendConditionally(ctx->transportTask, UPGRADE_TRANSPORT_DATA_CFM, msg,
                                        (uint16 *)&UpgradeCtxGet()->delayDataCfm);
#else
        MessageSend(ctx->transportTask, UPGRADE_TRANSPORT_DATA_CFM, msg);
#endif
        return status;
    }

    MessageSend(ctx->transportTask, UPGRADE_TRANSPORT_DATA_CFM, msg);

    return TRUE;
}


/****************************************************************************
NAME
    UpgradeHostIFTransportDisconnect

DESCRIPTION
    Process a disconnect request from an Upgrade client.
    Send a response to the transport in a UPGRADE_TRANSPORT_DISCONNECT_CFM.

    If Upgrade library has not been initialised or there is no
*/
void UpgradeHostIFTransportDisconnect(void)
{
    if (UpgradeIsInitialised())
    {
        UpgradeCtx *ctx = UpgradeCtxGet();
        if (ctx->transportTask)
        {
            if(ctx->isTransportCfmDelayed && UpgradeCtxGet()->delayDataCfm)
            {
                /* Data confirmations are delayed and will be sent by the client task so, delay the transport disconnect 
                 * confirmation also till all the data confirmations get sent.
                 * This internal message allows the outstanding UPGRADE_HOST_DATA messages to reach the client and their
                 * data confirmations to be scheduled before we schedule the disconnect confirmation. */
                MESSAGE_MAKE(schedDisconnectCfm, UPGRADE_INTERNAL_SCHEDULE_TRANSPORT_DISCONNECT_CFM_T);
                schedDisconnectCfm->transportTask = ctx->transportTask;
#ifndef HOSTED_TEST_ENVIRONMENT
                MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_SCHEDULE_TRANSPORT_DISCONNECT_CFM, schedDisconnectCfm);
#endif
            }
            else
            {
                MESSAGE_MAKE(disconnectCfm, UPGRADE_TRANSPORT_DISCONNECT_CFM_T);
                disconnectCfm->status = 0;
                MessageSend(UpgradeCtxGet()->transportTask, UPGRADE_TRANSPORT_DISCONNECT_CFM, disconnectCfm);
            }

            ctx->transportTask = 0;

#ifndef HOSTED_TEST_ENVIRONMENT
            /* Notify app of the transport disconnect */
            UpgradeSendUpgradeTransportStatus(UpgradeGetAppTask(), upgrade_notify_transport_disconnect);
#endif
        }
        else
            DEBUG_LOG_ERROR("UpgradeHostIFTransportDisconnect, not connected");

    }
    else
        DEBUG_LOG_ERROR("UpgradeHostIFTransportDisconnect, not initialised");
}


/****************************************************************************
NAME
    UpgradeHostIFTransportInUse

DESCRIPTION
    Return an indication of whether or not the
*/
bool UpgradeHostIFTransportInUse(void)
{
    if (UpgradeIsInitialised() && UpgradeCtxGet()->transportTask)
    {
        return TRUE;
    }

    return FALSE;
}

