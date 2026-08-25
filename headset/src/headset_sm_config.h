/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       headset_sm_config.h
\brief      Configuration related definitions for the state machine.
*/

#ifndef HEADSET_SM_CONFIG_H_
#define HEADSET_SM_CONFIG_H_

/*! This timer is active in HEADSET_STATE_IDLE if there is no active bluetooth connection.
    On timeout, the SM will try to poweroff. */
#define headsetConfigIdleTimeoutMs()   D_SEC(600)

/*! This timer is active when headset enters HEADSET_STATE_LIMBO or on recieving charger disconnect indication*/
//#define headsetConfigLimboTimeoutMs()   D_SEC(600)
#define headsetConfigLimboTimeoutMs()   D_SEC(30)

/*! Time to wait for successful disconnection of link with handset. */
#define headsetConfigDisconnectTimeoutMs()  D_SEC(5)

/*! Auto power on after panic. */
#define appConfigEnableAutoPowerOnAfterPanic() (FALSE)

/*! For the post reboot DFU commit phase, limbo to power-on is delayed by this
    period to account for the previous handset link to timeout as DFU reboot
    for commit doesn't precede a link disconnection. This ensures that the
    subsequent re-connection in the post reboot DFU commit phase succeeds and
    DFU proceeds to successful completion. Especially required in iAP DFU use-
    case, where the recommended connection parameters for MFI accessories has
    connection/link supervision timeout (x) within 2sec <= x <= 6sec range. */
#define headsetConfigDfuCommitDelayForLimboToPowerOn() D_SEC(6)
#endif /* HEADSET_SM_CONFIG_H_ */


