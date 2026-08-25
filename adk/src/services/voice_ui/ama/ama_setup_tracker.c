/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_setup_tracker.c
\brief      Implementation of the setup tracker APIs for Amazon AVS
*/

#include "ama_setup_tracker.h"
#include "ama_ble.h"
#include "bt_device.h"
#include "voice_ui_va_client_if.h"

void Ama_CompleteSetup(void)
{
    VoiceUi_SetDeviceFlag(device_va_flag_ama_setup_done, TRUE);
    AmaBle_UpdateLeAdvertisingData();
}

bool Ama_IsSetupComplete(void)
{
    return VoiceUi_GetDeviceFlag(device_va_flag_ama_setup_done);
}
