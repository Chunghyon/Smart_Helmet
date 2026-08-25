/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module for controling the external amp
*/

#include "kymera_external_amp.h"
#include "kymera.h"
#include "kymera_data.h"
#include "kymera_internal_msg_ids.h"
#include "pio_common.h"

static uint8 audio_ss_client_count = 0;

void appKymeraExternalAmpSetup(void)
{
    if (appConfigExternalAmpControlRequired())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
        int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

        /* Reset usage count */
        theKymera->dac_amp_usage = 0;

        /* map in PIO */
        PioSetMapPins32Bank(pio_bank, pio_mask, pio_mask);
        /* set as output */
        PioSetDir32Bank(pio_bank, pio_mask, pio_mask);
        /* start disabled */
        PioSet32Bank(pio_bank, pio_mask,
                     appConfigExternalAmpControlDisableMask());
    }
}

void appKymeraExternalAmpControl(bool enable)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (appConfigExternalAmpControlRequired())
    {
        theKymera->dac_amp_usage += enable ? 1 : - 1;

        /* Drive PIO high if enabling AMP and usage has gone from 0 to 1,
         * Drive PIO low if disabling AMP and usage has gone from 1 to 0 */
        if ((enable && theKymera->dac_amp_usage == 1) ||
            (!enable && theKymera->dac_amp_usage == 0))
        {
            int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
            int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

            PioSet32Bank(pio_bank, pio_mask,
                         enable ? appConfigExternalAmpControlEnableMask() :
                                  appConfigExternalAmpControlDisableMask());
        }
    }

    if(enable)
    {
        /* If we're enabling the amp then also call OperatorFrameworkEnable() so that the audio S/S will
           remain on even if the audio chain is destroyed, this allows us to control the timing of when the audio S/S
           and DACs are powered off to mitigate audio pops and clicks.*/

        /* Cancel any pending audio s/s disable message since we're enabling.  If message was cancelled no need
           to call OperatorFrameworkEnable() as audio S/S is still powered on from previous time */
        if(MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE))
        {
            DEBUG_LOG("appKymeraExternalAmpControl, there is already a client for the audio SS");
        }
        else
        {
            DEBUG_LOG("appKymeraExternalAmpControl, adding a client to the audio SS");
            OperatorsFrameworkEnable();
        }

        audio_ss_client_count++;
    }
    else
    {
        if (audio_ss_client_count > 1)
        {
            OperatorsFrameworkDisable();
            audio_ss_client_count--;
            DEBUG_LOG("appKymeraExternalAmpControl, removed audio source, count is %d", audio_ss_client_count);
        }
        else
        {
            /* If we're disabling the amp then send a timed message that will turn off the audio s/s later rather than
            immediately */
            DEBUG_LOG("appKymeraExternalAmpControl, sending later KYMERA_INTERNAL_AUDIO_SS_DISABLE, count is %d", audio_ss_client_count);
            MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE, NULL, appKymeraDacDisconnectionDelayMs());
            audio_ss_client_count = 0;
        }
    }
}
