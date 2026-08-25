/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common testing functions.
*/

#include "common_test.h"

#include <logging.h>
#include <connection_manager.h>
#include <vm.h>
#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_typedef.h"
#include "kymera_output_common_chain.h"
#include "handset_service.h"
#include "handset_service_config.h"
#include <av.h>
#include "le_advertising_manager.h"
#include "ui.h"
#ifdef INCLUDE_FAST_PAIR
#include "sass.h"
#endif

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

bool appTestIsHandsetQhsConnectedAddr(const bdaddr* handset_bd_addr)
{
    bool qhs_connected_status = FALSE;

    if (handset_bd_addr != NULL)
    {
        qhs_connected_status = ConManagerGetQhsConnectStatus(handset_bd_addr);

        DEBUG_LOG_ALWAYS("appTestIsHandsetQhsConnectedAddr addr [%04x,%02x,%06lx] qhs_connected:%d", 
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         qhs_connected_status);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetQhsConnectedAddr BT adrress is NULL");
    }

    return qhs_connected_status;
}

bool appTestIsHandsetAddrConnected(const bdaddr* handset_bd_addr)
{
    bool is_connected = FALSE;

    if (handset_bd_addr != NULL)
    {
        device_t device = BtDevice_GetDeviceForBdAddr(handset_bd_addr);
        if (device != NULL)
        {
            uint32 connected_profiles = BtDevice_GetConnectedProfiles(device);
            if ((connected_profiles & (DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP | DEVICE_PROFILE_AVRCP)) != 0)
            {
                is_connected = TRUE;
            }
        }

        DEBUG_LOG_ALWAYS("appTestIsHandsetAddrConnected addr [%04x,%02x,%06lx] device:%p is_connected:%d",
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         device,
                         is_connected);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetAddrConnected BT address is NULL");
    }

    return is_connected;
}

bool appTestIsHandsetHfpScoActiveAddr(const bdaddr* handset_bd_addr)
{
    bool is_sco_active = FALSE;

    if (handset_bd_addr != NULL)
    {
        hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(handset_bd_addr);

        is_sco_active = HfpProfile_IsScoActiveForInstance(instance);
        DEBUG_LOG_ALWAYS("appTestIsHandsetHfpScoActiveAddr addr [%04x,%02x,%06lx] is_sco_active:%d", 
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         is_sco_active);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetHfpScoActiveAddr BT adrress is NULL");
    }

    return is_sco_active;
}

void appTestEnableCommonChain(void)
{
    DEBUG_LOG_ALWAYS("appTestEnableCommonChain");
    Kymera_OutputCommonChainEnable();
}

void appTestDisableCommonChain(void)
{
    DEBUG_LOG_ALWAYS("appTestDisableCommonChain");
    Kymera_OutputCommonChainDisable();
}

int16 appTestGetRssiOfTpAddr(tp_bdaddr *tpaddr)
{
    int16 rssi = 0;
    if(VmBdAddrGetRssi(tpaddr, &rssi) == FALSE)
    {
        rssi = 0;
    }
    DEBUG_LOG_ALWAYS("appTestGetRssiOfConnectedTpAddr transport=%d tpaddr=%04x,%02x,%06lx RSSI=%d",
                    tpaddr->transport, tpaddr->taddr.addr.lap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.nap, rssi);
    return rssi;
}

int16 appTestGetBredrRssiOfConnectedHandset(void)
{
    tp_bdaddr tp_addr;
    if(HandsetService_GetConnectedBredrHandsetTpAddress(&tp_addr))
    {
        return appTestGetRssiOfTpAddr(&tp_addr);
    }
    return 0;
}

int16 appTestGetLeRssiOfConnectedHandset(void)
{
    tp_bdaddr tp_addr;
    if(HandsetService_GetConnectedLeHandsetTpAddress(&tp_addr))
    {
        return appTestGetRssiOfTpAddr(&tp_addr);
    }
    return 0;
}

bool appTestEnableConnectionBargeIn(void)
{
    /* Connection barge-in is not compatible with the legacy version. 
       Attempts to enable connection barge-in when MULTIPOINT_BARGE_IN_ENABLED
       will fail. MULTIPOINT_BARGE_IN_ENABLED should only be used where truncated 
       page scan is not supported. */
    return handsetService_SetConnectionBargeInEnable(TRUE);
}

bool appTestDisableConnectionBargeIn(void)
{
    return handsetService_SetConnectionBargeInEnable(FALSE);
}

bool appTestIsConnectionBargeInEnabled(void)
{
    return HandsetService_IsConnectionBargeInEnabled();
}

/*! \brief Return if Earbud/Headset is in A2DP streaming mode with any connected handset
*/
bool appTestIsHandsetA2dpStreaming(void)
{
    bool streaming = FALSE;
    bdaddr* bd_addr = NULL;
    unsigned num_addresses = 0;

    if (BtDevice_GetAllHandsetBdAddr(&bd_addr, &num_addresses))
    {
        unsigned index;
        for(index = 0; index < num_addresses; index++)
        {
            if(appTestIsHandsetA2dpStreamingBdaddr(&bd_addr[index]))
            {
                streaming = TRUE;
                break;
            }
        }
        free(bd_addr);
        bd_addr  = NULL;
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpStreaming:%d", streaming);
    return streaming;
}

/*! \brief Return if Earbud/Headset is in A2DP streaming mode with a handset having specified bluetooth address
*/
bool appTestIsHandsetA2dpStreamingBdaddr(bdaddr* bd_addr)
{
    bool streaming = FALSE;

    /* Find handset AV instance */
    avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(bd_addr);
    streaming = theInst && appA2dpIsStreaming(theInst);

    return streaming;
}

void appTestConfigureLinkLossReconnectionParameters(
        bool use_unlimited_reconnection_attempts,
        uint8 num_connection_attempts,
        uint32 reconnection_page_interval_ms,
        uint16 reconnection_page_timeout_ms)
{
    DEBUG_LOG_ALWAYS("appTestConfigureLinkLossReconnectionParameters use_unlimited_reconnection_attempts=%d "
                     "num_connection_attempts=%d reconnection_page_interval_ms=%d reconnection_page_timeout_ms=%d",
                     use_unlimited_reconnection_attempts, num_connection_attempts,
                     reconnection_page_interval_ms, reconnection_page_timeout_ms);

    HandsetService_ConfigureLinkLossReconnectionParameters(
                use_unlimited_reconnection_attempts,
                num_connection_attempts,
                reconnection_page_interval_ms,
                reconnection_page_timeout_ms);
}

#ifdef INCLUDE_FAST_PAIR
void appTest_SassDisableConnectionSwitch(void)
{
    DEBUG_LOG_ALWAYS("appTest_SassDisableConnectionSwitch");
    Ui_InjectUiInput(ui_input_disable_sass_connection_switch);
}

void appTest_SassEnableConnectionSwitch(void)
{
    DEBUG_LOG_ALWAYS("appTest_SassEnableConnectionSwitch");
    Ui_InjectUiInput(ui_input_enable_sass_connection_switch);
}

bool appTest_IsSassSwitchDisabled(void)
{
    bool disabled = Sass_IsConnectionSwitchDisabled();
    DEBUG_LOG_ALWAYS("appTest_IsSassSwitchDisabled:%d",disabled);
    return disabled;
}
#endif /* INCLUDE_FAST_PAIR */
