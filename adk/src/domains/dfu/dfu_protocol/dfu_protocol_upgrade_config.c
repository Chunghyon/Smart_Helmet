/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_upgrade_config.c
\brief      Implemetation of the upgrade config APIs for the dfu_protocol module
*/

#include "dfu_protocol_upgrade_config.h"

static bool upgrade_supports_silent_commit = FALSE;

void DfuProtocol_SetUpgradeSupportsSilentCommit(bool is_supported)
{
    upgrade_supports_silent_commit = is_supported;
}

bool DfuProtocol_DoesUpgradeSupportSilentCommit(void)
{
    return upgrade_supports_silent_commit;
}
