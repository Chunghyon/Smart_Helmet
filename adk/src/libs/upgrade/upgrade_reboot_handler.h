/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_reboot_handler.h

DESCRIPTION
    Header file for upgrade_reboot_handler.
*/

#ifndef UPGRADE_REBOOT_HANDLER_H_
#define UPGRADE_REBOOT_HANDLER_H_

#include <upgrade.h>

void UpgradeRebootHandlerCalculateDfuOffset(void);

bool UpgradeRebootHandlerGetPartitionStateFromDfuFileOffset(uint32 req_offset, upgrade_partition_state_t* expected_state);

#endif /* UPGRADE_REBOOT_HANDLER_H_ */
