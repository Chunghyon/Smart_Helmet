/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_client_config.c
\brief      Implemetation of the client config APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol_client_config.h"

#include "dfu_protocol_sm.h"
#include "dfu_protocol_sm_types.h"

#include "dfu.h"
#include "state_machine.h"

#include <panic.h>
#include <upgrade.h>

#ifdef HOSTED_TEST_ENVIRONMENT
/* Keep this value fixed for unit testing */
#define MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS (2)
#else
#define MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS (2)
#endif

static const dfu_protocol_client_config_t * config_list[MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS] = {NULL};

/* Set when a client starts the OTA, and when initialising this module to catch reboots caused by DFU */
static upgrade_context_t active_client_context = UPGRADE_CONTEXT_UNUSED;
static upgrade_context_t context_at_boot = UPGRADE_CONTEXT_UNUSED;

static const dfu_protocol_client_config_t * dfuProtocol_GetActiveClientConfig(void)
{
    const dfu_protocol_client_config_t * active_client_config = NULL;
    unsigned i;
    
    for(i=0; i<MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS; i++)
    {
        if((config_list[i] != NULL) && (config_list[i]->context == active_client_context))
        {
            active_client_config = config_list[i];
            break;
        }
    }
    
    PanicNull((void *)active_client_config);
    
    return active_client_config;
}

void DfuProtocol_InitialiseClientConfig(void)
{
    unsigned i;

    for(i=0; i<MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS; i++)
    {
        config_list[i] = NULL;
    }

    context_at_boot = Upgrade_GetContext();

    DEBUG_LOG("DfuProtocol_InitialiseClientConfig context_at_boot=enum:upgrade_context_t:%d", context_at_boot);
}

void DfuProtocol_RegisterClientConfig(const dfu_protocol_client_config_t * config)
{
    unsigned i;
    
    PanicNull((void *)config->task);
    PanicNull((void *)config->GetBtAddress);
    
    for(i=0; i<MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS; i++)
    {
        if(config_list[i] != NULL)
        {
            /* Trying to register the same Upgrade context twice */
            PanicFalse(config_list[i]->context != config->context);
        }
        else
        {
            config_list[i] = config;

            if(context_at_boot == config->context)
            {
                active_client_context = context_at_boot;
#ifndef INCLUDE_DFU_PEER
                /* Trigger post-reboot sequence for builds not waiting for DFU peer */
                dfu_protocol_start_params_t start_params = { 0, active_client_context };
                StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_start_post_reboot_event, &start_params);
#endif
            }
            break;
        }
    }
    
    PanicFalse(i < MAX_NUMBER_OF_DFU_PROTOCOL_CLIENTS);
}

void DfuProtocol_SetActiveClient(upgrade_context_t context)
{
    active_client_context = context;
    Dfu_SetContext(context);
}

Task DfuProtocol_GetClientTask(void)
{
    return dfuProtocol_GetActiveClientConfig()->task;
}

upgrade_context_t DfuProtocol_GetActiveClientContext(void)
{
    return active_client_context;
}

const bdaddr * DfuProtocol_GetClientHandsetBtAddress(void)
{
    return dfuProtocol_GetActiveClientConfig()->GetBtAddress();
}

uint32 DfuProtocol_GetClientInProgressId(void)
{
    return dfuProtocol_GetActiveClientConfig()->in_progress_id;
}

bool DfuProtocol_GetClientSupportSilentCommit(void)
{
    return dfuProtocol_GetActiveClientConfig()->supports_silent_commit;
}

bool DfuProtocol_DidActiveClientCauseReboot(void)
{
#ifdef INCLUDE_DFU
    return (context_at_boot != UPGRADE_CONTEXT_UNUSED) &&
           (context_at_boot == active_client_context) &&
           (Dfu_GetRebootReason() == REBOOT_REASON_DFU_RESET);
#else
    return (context_at_boot != UPGRADE_CONTEXT_UNUSED) &&
           (context_at_boot == active_client_context);
#endif
}
