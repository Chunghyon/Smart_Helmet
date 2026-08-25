/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_anc.c
\brief      Implementation of the ANC handling for Amazon AVS
*/

#ifdef INCLUDE_AMA
#ifdef ENABLE_ANC
#include <logging.h>
#include "ama_anc.h"
#include "ama_state.h"
#include "ama_send_command.h"

void AmaAnc_AncEnableUpdate(bool enabled)
{
    DEBUG_LOG("AmaAnc_AncEnableUpdate(%d)", enabled);
    AmaSendCommand_SyncState(AMA_FEATURE_ANC_ENABLE, STATE__VALUE_BOOLEAN, (uint16)enabled);
}

void AmaAnc_PassthroughEnableUpdate(bool enabled)
{
    DEBUG_LOG("AmaAnc_PassthroughEnableUpdate(%d)", enabled);
    AmaSendCommand_SyncState(AMA_FEATURE_PASSTHROUGH_ENABLE, STATE__VALUE_BOOLEAN, (uint16)enabled);
}

void AmaAnc_PassthroughLevelUpdate(uint8 level_as_percentage)
{
    DEBUG_LOG("AmaAnc_PassthroughLevelUpdate(%d)", level_as_percentage);
    AmaSendCommand_SyncState(AMA_FEATURE_PASSTHROUGH_LEVEL, STATE__VALUE_INTEGER, (uint16)level_as_percentage);
}

/***************************************************************************/

bool AmaAnc_Init(void)
{
    DEBUG_LOG("AmaAnc_Init");
    return TRUE;
}
#endif /* ENABLE_ANC */
#endif /* INCLUDE_AMA */

