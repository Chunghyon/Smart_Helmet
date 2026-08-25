/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_output_ultra_quiet_dac.c
\brief      Kymera output ultra quiet dac Manager
*/

#include "kymera_output_ultra_quiet_dac.h"

#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE

static void kymera_UltraQuietDac_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type);
static void kymera_UltraQuietDac_OutputDisconnectingIndication(output_users_t disconnected_user, output_connection_t connection_type);
static void kymera_DisableUltraQuietDac(void);
static void kymera_EnableUltraQuietDac(void);
void kymera_UpdateUltraQuietDacEnableStatus(bool status);

/*Registering Callback with Output manager to configure Ultra Quiet Dac*/
static const output_indications_registry_entry_t ultraQuietDac_IndicationCallbacks =
{
    .OutputConnectingIndication = kymera_UltraQuietDac_OutputConnectingIndication,
    .OutputDisconnectedIndication = kymera_UltraQuietDac_OutputDisconnectingIndication,
};

static void kymera_UltraQuietDac_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    DEBUG_LOG_INFO("kymera_UltraQuietDac_OutputConnectingIndication");

    if (((Kymera_OutputGetConnectedUsers() | connecting_user) & ULTRA_QUIET_DAC_VALID_USER) \
                                                              != ULTRA_QUIET_DAC_VALID_USER)
    {
        if(Kymera_IsUltraQuietDacEnabled())
        {
            kymera_DisableUltraQuietDac();
            KymeraGetTaskData()->ultra_quiet_dac_enable_request = TRUE;
        }
    }
}

static void kymera_UltraQuietDac_OutputDisconnectingIndication(output_users_t disconnected_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    UNUSED(disconnected_user);
    DEBUG_LOG_INFO("kymera_UltraQuietDac_OutputDisconnectingIndication");

    if ((Kymera_OutputGetConnectedUsers() & ULTRA_QUIET_DAC_VALID_USER) == ULTRA_QUIET_DAC_VALID_USER)
    {
        if(Kymera_IsUltraQuiteDacRequested())
        {
            Kymera_RequestUltraQuietDac();
        }
    }
}

static void kymera_EnableUltraQuietDac(void)
{
    if(AudioOutputEnableDacUltraQuietMode())
    {
        DEBUG_LOG("Kymera_EnableUltraQuietDacMode, DAC ultra quiet mode enabled");
        kymera_UpdateUltraQuietDacEnableStatus(TRUE);
        KymeraGetTaskData()->ultra_quiet_dac_enable_request = FALSE;
    }
    else
    {
        DEBUG_LOG("Kymera_EnableUltraQuietDacMode, DAC is inactive and enable request ignored");
    }
}

static void kymera_DisableUltraQuietDac(void)
{
    kymera_UpdateUltraQuietDacEnableStatus(FALSE);

    if(AudioOutputDisableDacUltraQuietMode())
    {
        DEBUG_LOG("Kymera_DisableUltraQuietDacMode, DAC ultra quiet mode disabled");
    }
    else
    {
        DEBUG_LOG("Kymera_DisableUltraQuietDacMode, DAC is inactive and disable request ignored");
    }
}

bool Kymera_IsUltraQuietDacEnabled(void)
{
    return (KymeraGetTaskData()->ultra_quiet_dac_enable_status);
}

bool Kymera_IsUltraQuiteDacRequested(void)
{
    return (KymeraGetTaskData()->ultra_quiet_dac_enable_request);
}


void kymera_UpdateUltraQuietDacEnableStatus(bool status)
{
    KymeraGetTaskData()->ultra_quiet_dac_enable_status = status;
}

void Kymera_RequestUltraQuietDac(void)
{
    if(!Kymera_IsUltraQuietDacEnabled())
    {
        /* Check if ultra quiet dac supported user is connected */
        if(Kymera_OutputGetConnectedUsers() == ULTRA_QUIET_DAC_VALID_USER)
        {
            kymera_EnableUltraQuietDac();
        }
        else
        {
            KymeraGetTaskData()->ultra_quiet_dac_enable_request = TRUE;
        }
    }
}

void Kymera_CancelUltraQuietDac(void)
{
    KymeraGetTaskData()->ultra_quiet_dac_enable_request = FALSE;
    if(Kymera_IsUltraQuietDacEnabled())
    {
        kymera_DisableUltraQuietDac();
    }
}

void Kymera_UltraQuietDacInit(void)
{
    Kymera_OutputRegisterForIndications(&ultraQuietDac_IndicationCallbacks);
    KymeraGetTaskData()->ultra_quiet_dac_enable_request = FALSE;
    KymeraGetTaskData()->ultra_quiet_dac_enable_status = FALSE;
}
#endif
