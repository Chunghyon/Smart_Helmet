/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_upgrade_config.h
\brief      Definition of the upgrade config APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_UPGRADE_CONFIG_H
#define DFU_PROTOCOL_UPGRADE_CONFIG_H

/*! \brief Set if Upgrade library supports silent commit
    \param is_supported TRUE if Upgrade library confirms silent commit support, otherwise FALSE */
void DfuProtocol_SetUpgradeSupportsSilentCommit(bool is_supported);

/*! \brief Check if Upgrade library supports silent commit
    \return TRUE if Upgrade supports silent commit, otherwise FALSE */
bool DfuProtocol_DoesUpgradeSupportSilentCommit(void);

#endif // DFU_PROTOCOL_UPGRADE_CONFIG_H
