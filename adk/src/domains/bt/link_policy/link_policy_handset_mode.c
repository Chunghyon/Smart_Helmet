/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy manager control of link mode with the handset.
*/

#include <adk_log.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <hfp_profile_instance.h>
#include <av.h>
#include <focus_audio_source.h>
#include <focus_device.h>
#include <panic.h>
#include <connection_abstraction.h>
#include <stream.h>
#include <va_profile.h>
#include <upgrade_gaia_plugin.h>
#include "link_policy_private.h"

/*! Lower power table when idle or unfocused (i.e. no streaming or SCO) */
static const lp_power_table powertable_singlepoint_idle[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       2},  /* Passive mode for 2 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

static const lp_power_table powertable_multipoint_idle[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      310,          310,          4,       4,       0}   /* Enter sniff mode*/
};

/*! When broadcast is active, more sniff attempts are required to allow
    receiving the broadcast to be prioritised whilst still maintaining the ACL
    link in sniff mode */
static const lp_power_table powertable_idle_with_broadcast_active[] =
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1}, /* Passive mode */
    {lp_sniff,      48,           400,          2,       4,       0}  /* Enter sniff mode */
};


/*! Lower power table when VA is active */
static const lp_power_table powertable_va_active[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       5},  /* Active mode for 5 sec */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when only DFU is active */
static const lp_power_table powertable_dfu[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       10}, /* Active mode for 10 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when A2DP streaming */
static const lp_power_table powertable_a2dp_streaming[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       5},  /* Active mode for 5 sec */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      48,           48,           2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when SCO active */
static const lp_power_table powertable_sco_active[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode */
    {lp_sniff,      48,           144,          2,       8,       0}   /* Enter sniff mode (30-90ms)*/
};

/*! \cond helper */
#define ARRAY_AND_DIM(ARRAY) (ARRAY), ARRAY_DIM(ARRAY)
/*! \endcond helper */

/*! Structure for storing power tables */
struct powertable_data
{
    const lp_power_table *table;
    uint16 rows;
};

/*! Array of structs used to store the power tables for standard phones */
static const struct powertable_data powertables_standard[] = {
    [POWERTABLE_IDLE] = {ARRAY_AND_DIM(powertable_singlepoint_idle)},
    [POWERTABLE_MULTIPOINT_IDLE] = {ARRAY_AND_DIM(powertable_multipoint_idle)},
    [POWERTABLE_IDLE_WITH_BROADCAST] = {ARRAY_AND_DIM(powertable_idle_with_broadcast_active)},
    [POWERTABLE_VA_ACTIVE] = {ARRAY_AND_DIM(powertable_va_active)},
    [POWERTABLE_DFU] = {ARRAY_AND_DIM(powertable_dfu)},
    [POWERTABLE_A2DP_STREAMING] = {ARRAY_AND_DIM(powertable_a2dp_streaming)},
    [POWERTABLE_SCO_ACTIVE] = {ARRAY_AND_DIM(powertable_sco_active)},
};

static void appLinkPolicyUpdateAllHandsetsAndSinks(bool force);

#if defined(INCLUDE_LEA_LINK_POLICY)
typedef enum
{
    A2DP_TERMINATED_DELAY_INACTIVE,
    A2DP_TERMINATED_DELAY_RUNNING,
    A2DP_TERMINATED_DELAY_EXPIRED,
} appLinkPolicyA2dpTermDelay;

/*! \brief State of timer started when A2DP ends (and an LE link to the same device was active) */
static appLinkPolicyA2dpTermDelay appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_INACTIVE;

/*! \brief LE Address of device with delay running */
static tp_bdaddr appLinkPolicyA2dpTerminatedDelayAddr = {0};
#endif /* LEA Link Policy */

static bool appLinkPolicyIsDeviceInFocus(const bdaddr *bd_addr)
{
    bool focused = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    if (device && Focus_GetFocusForDevice(device) == focus_foreground)
    {
        focused = TRUE;
    }
    DEBUG_LOG("appLinkPolicyIsDeviceInFocus 0x%06x device:%x Focus:%d",
              bd_addr->lap, device, focused);

    return focused;
}

static Sink appLinkPolicyGetSink(const bdaddr *bdaddr)
{
    tp_bdaddr tbdaddr;
    uint16 max = 1;
    Sink sink = 0;
    BdaddrTpFromBredrBdaddr(&tbdaddr, bdaddr);
    StreamSinksFromBdAddr(&max, &sink, &tbdaddr);
    return max ? sink : 0;
}

static bool appLinkPolicyIsScoActive(const bdaddr *bdaddr)
{
    bool active = FALSE;
#ifdef INCLUDE_HFP
    hfpInstanceTaskData *hfp_inst = HfpProfileInstance_GetInstanceForBdaddr(bdaddr);
    active = (hfp_inst && HfpProfile_IsScoActiveForInstance(hfp_inst));
#endif
    return active;
}

static bool appLinkPolicyIsA2dpStreaming(const bdaddr *bdaddr)
{
    bool active = FALSE;
#ifdef INCLUDE_AV
        avInstanceTaskData *av_inst = appAvInstanceFindFromBdAddr(bdaddr);
        active = (av_inst && appA2dpIsStreaming(av_inst));
#endif
    return active;
}

static bool appLinkPolicyIsVaActive(const bdaddr * bdaddr)
{
    return VaProfile_IsVaActiveAtBdaddr(bdaddr);
}

static bool appLinkPolicyIsDfuActive(const bdaddr *bdaddr)
{
    bool active = FALSE;
#if defined(INCLUDE_GAIA) && defined(INCLUDE_DFU)
    tp_bdaddr tp_bd_addr;
    BdaddrTpFromBredrBdaddr(&tp_bd_addr, bdaddr);
    active = UpgradeGaiaPlugin_IsHandsetTransferActive(&tp_bd_addr);
#else
    UNUSED(bdaddr);
#endif
    return active;
}


static bool appLinkPolicySetPowerTable(const bdaddr *bd_addr, lpPowerTableIndex index)
{
    const struct powertable_data *selected = &powertables_standard[index];

    Sink sink = appLinkPolicyGetSink(bd_addr);
    if (sink)
    {
        ConnectionSetLinkPolicy(sink, selected->rows, selected->table);
    }
    return (sink != 0);
}

/* \brief Select link mode settings to reduce power consumption.

    This function checks what activity the application currently has,
    and decides what the best link settings are for the connection
    to the specified device. This may include full power (#lp_active),
    sniff (#lp_sniff), or passive(#lp_passive) where full power is
    no longer required but the application would prefer not to enter
    sniff mode yet.

    The function also considers multipoint scenarios where a device may be
    the focus device or the out of focus device.

    \param bd_addr  Bluetooth address of the device to update link settings
    \param force The link policy will be updated, even if no change in link
    is detected.
*/
static void appLinkPolicyUpdateBredrPowerTableImpl(const bdaddr *bd_addr, bool force)
{
    lpPerConnectionState lp_state;
    lpPowerTableIndex pt_index = POWERTABLE_IDLE;

    if (BtDevice_GetNumberOfHandsetsConnectedOverBredr() > 1)
    {
        pt_index = POWERTABLE_MULTIPOINT_IDLE;
    }

    if (appLinkPolicyIsDeviceInFocus(bd_addr))
    {
        if (appLinkPolicyIsScoActive(bd_addr))
        {
            pt_index = POWERTABLE_SCO_ACTIVE;
        }
        else if (appLinkPolicyIsA2dpStreaming(bd_addr))
        {
            pt_index = POWERTABLE_A2DP_STREAMING;
        }
        else if (appLinkPolicyIsDfuActive(bd_addr))
        {
            pt_index = POWERTABLE_DFU;
        }
		else if (appLinkPolicyIsVaActive(bd_addr))
        {
            pt_index = POWERTABLE_VA_ACTIVE;
        }
    }

    if (pt_index == POWERTABLE_IDLE)
    {
        if (Focus_GetFocusForAudioSource(audio_source_le_audio_broadcast) == focus_foreground)
        {
            pt_index = POWERTABLE_IDLE_WITH_BROADCAST;
        }
    }

    ConManagerGetLpState(bd_addr, &lp_state);
    lpPowerTableIndex old_index = lp_state.pt_index;

    if ((pt_index != old_index) || force)
    {
        lp_state.pt_index = pt_index;

        if (appLinkPolicySetPowerTable(bd_addr, pt_index))
        {
            DEBUG_LOG("appLinkPolicyUpdateBredrPowerTableImpl lap=%x, from enum:lpPowerTableIndex:%d to enum:lpPowerTableIndex:%d",
                      bd_addr->lap, old_index, pt_index);
            ConManagerSetLpState(bd_addr, lp_state);
        }
    }
}

void appLinkPolicyHandleA2dpTerminatedDelayExpired(void)
{
#if defined(INCLUDE_LEA_LINK_POLICY)
    DEBUG_LOG("appLinkPolicyHandleA2dpTerminatedDelayExpired. Was enum:appLinkPolicyA2dpTermDelay:%d",
              appLinkPolicyA2dpTerminatedDelayState);

    if (appLinkPolicyA2dpTerminatedDelayState == A2DP_TERMINATED_DELAY_RUNNING)
    {
        appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_EXPIRED;
        appLinkPolicyUpdateAllHandsetsAndSinks(FALSE);
    }
    else
    {
        appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_INACTIVE;
    }
#endif /* LEA Link Policy */
}

/*! \brief Reset the A2DP status after processed all entries

    If there is complexity such as multiple BLE links or multiple A2DP, 
    delays will sequentially */
static void appLinkPolicyA2dpDelayClearIfTerminated(void)
{
#if defined(INCLUDE_LEA_LINK_POLICY)
    if (appLinkPolicyA2dpTerminatedDelayState == A2DP_TERMINATED_DELAY_EXPIRED)
    {
        DEBUG_LOG("appLinkPolicyA2dpDelayClearIfTerminated. Clearing");

        appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_INACTIVE;
        BdaddrTpSetEmpty(&appLinkPolicyA2dpTerminatedDelayAddr);
    }
#endif /* LEA Link Policy */
}

#if defined(INCLUDE_LEA_LINK_POLICY)
#define appLinkPolicyCurrentQosIsA2dp(state) ((cm_qos_t)(state).pt_index == cm_qos_lea_idle)

static void appLinkPolicyA2dpDelayTerminate(const tp_bdaddr *bd_addr)
{
    
    if (BdaddrTpIsSame(bd_addr, &appLinkPolicyA2dpTerminatedDelayAddr))
    {
        DEBUG_LOG("appLinkPolicyA2dpDelayTerminate. Was enum:appLinkPolicyA2dpTermDelay:%d",
                  appLinkPolicyA2dpTerminatedDelayState);

        MessageCancelAll(LinkPolicyGetTask(), LINK_POLICY_A2DP_TERMINATED_DELAY_EXPIRED);
        appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_INACTIVE;
        BdaddrTpSetEmpty(&appLinkPolicyA2dpTerminatedDelayAddr);
    }
    else
    {
        DEBUG_LOG("appLinkPolicyA2dpDelayTerminate. Mismatched bdaddr");
    }
}

static bool appLinkPolicyA2dpDelayStartedOrIsActive(const tp_bdaddr *bd_addr)
{
    switch (appLinkPolicyA2dpTerminatedDelayState)
    {
        case A2DP_TERMINATED_DELAY_INACTIVE:
            MessageSendLater(LinkPolicyGetTask(), 
                             LINK_POLICY_A2DP_TERMINATED_DELAY_EXPIRED, NULL,
                             appConfigLinkPolicyA2dpTerminatedDelayMs());
            appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_RUNNING;
            appLinkPolicyA2dpTerminatedDelayAddr = *bd_addr;
            break;

        case A2DP_TERMINATED_DELAY_RUNNING:
            break;

        case A2DP_TERMINATED_DELAY_EXPIRED:
            if (BdaddrTpIsSame(bd_addr, &appLinkPolicyA2dpTerminatedDelayAddr))
            {
                DEBUG_LOG("appLinkPolicyA2dpDelayStartedOrIsActive. Expired, address match");
                return FALSE;
            }
            break;

        default:
            /* Should never reach here.
               Allow the update (return FALSE) and reset the state */
            appLinkPolicyA2dpTerminatedDelayState = A2DP_TERMINATED_DELAY_EXPIRED;
            return FALSE;
    }
    /* Either a new delay has been started, or a delay was already running,
       not neccesarily for the same device. */
    return TRUE;
}

static void appLinkPolicyUpdateLePowerTableImpl(const tp_bdaddr *bd_addr, bool force)
{
    lpPerConnectionState le_state;

    tp_bdaddr bredr_addr = {0};
    bool a2dp = FALSE;
    cm_qos_t target_qos = cm_qos_invalid;

    DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl 0x%6lx force:%d",
              bd_addr->taddr.addr.lap, force);

    device_t device = BtDevice_GetDeviceForTpbdaddr(bd_addr);
    if (device && Focus_GetFocusForDevice(device) == focus_foreground)
    {
        if (BtDevice_GetTpBdaddrForDevice(device, &bredr_addr))
        {
            a2dp = appLinkPolicyIsA2dpStreaming(&bredr_addr.taddr.addr);
        }
        DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl BREDR 0x%06x device:%p a2dp:%d",
                  bredr_addr.taddr.addr.lap, device, a2dp);
    }
    else
    {
        DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl device:%p or not focussed",
                  device);
    }

    if (a2dp)
    {
        appLinkPolicyA2dpDelayTerminate(bd_addr);
        target_qos = cm_qos_lea_idle;
    }

    /* This is the default once LE connected (with a link policy) */
    ConManagerRequestDefaultQos(cm_transport_ble, cm_qos_low_latency);

    if (ConManagerGetLpStateTp(bd_addr, &le_state))
    {
        DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl 0x%6lx QoS enum:cm_qos_t:%d => enum:cm_qos_t:%d",
                   bd_addr->taddr.addr.lap, le_state.pt_index, target_qos);
        if (target_qos != (cm_qos_t)le_state.pt_index)
        {
            if (appLinkPolicyCurrentQosIsA2dp(le_state))
            {
                if (appLinkPolicyA2dpDelayStartedOrIsActive(bd_addr))
                {
                    DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl Delaying LE update");
                    /* We are delaying this update */
                    return;
                }
            }

            /* Request the new qos. If it is higher than the currently
               requested QOS then it will be selected now and releasing 
               the old one will make no difference.
               If it is lower then this will have no effect now, but will
               take effect when the old, higher, QOS is released next */
            if (target_qos != cm_qos_invalid)
            {
                ConManagerRequestDeviceQos(bd_addr, target_qos);
            }
            switch (le_state.pt_index)
            {
                case cm_qos_lea_idle:
                    ConManagerReleaseDeviceQos(bd_addr, le_state.pt_index);
                    break;
                default:
                    break;
            }
            le_state.pt_index = (lpPowerTableIndex)target_qos;
            ConManagerSetLpStateTp(bd_addr, le_state);
        }
        else if (force)
        {
            ConManagerUpdateDeviceQos(bd_addr);
        }
    }
    else
    {
        DEBUG_LOG("appLinkPolicyUpdateLePowerTableImpl 0x%6lx NO ConManagerLpState",
                   bd_addr->taddr.addr.lap);
    }
}
#endif


static void appLinkPolicyUpdateAllHandsetsAndSinks(bool force)
{
    tp_bdaddr addr = {0};
    cm_connection_iterator_t iterator;

    DEBUG_LOG("appLinkPolicyUpdateAllHandsetsAndSinks");

    if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
    {
        do
        {
            DEBUG_LOG("Device [%d] 0x%6lx", addr.transport, addr.taddr.addr.lap);
            bdaddr *bredr_addr = &addr.taddr.addr;
            if (addr.transport == TRANSPORT_BREDR_ACL)
            {
                if (   appDeviceTypeIsHandset(bredr_addr) 
                    || appDeviceTypeIsSink(bredr_addr))
                {
                    appLinkPolicyUpdateBredrPowerTableImpl(bredr_addr, force);
                }
                else
                {
                    DEBUG_LOG("appLinkPolicyUpdateAllHandsetsAndSinks BREDR Not handset or sink ?");
                }
            }
#if defined(INCLUDE_LEA_LINK_POLICY)
            else if (addr.transport == TRANSPORT_BLE_ACL)
            {
                appLinkPolicyUpdateLePowerTableImpl(&addr, force);
            }
            else
            {
                DEBUG_LOG("appLinkPolicyUpdateAllHandsetsAndSinks Not BREDR or LE ?");
            }
#endif /* INCLUDE_LEA_LINK_POLICY */
        } while (ConManager_IterateNextActiveConnection(&iterator, &addr));
    }
    appLinkPolicyA2dpDelayClearIfTerminated();
}

void appLinkPolicyUpdatePowerTable(const bdaddr *bd_addr)
{
    /* The bd_addr triggering the update is not currently used, but may be in
       future so is retained in the interface and ignored */
    UNUSED(bd_addr);
    appLinkPolicyUpdateAllHandsetsAndSinks(FALSE);
}

void appLinkPolicyForceUpdatePowerTable(const bdaddr *bd_addr)
{
    /* The bd_addr triggering the update is not currently used, but may be in
       future so is retained in the interface and ignored */
    UNUSED(bd_addr);
    appLinkPolicyUpdateAllHandsetsAndSinks(TRUE);
}
