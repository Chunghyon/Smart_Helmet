#include "headset_init_bt.h"

#include "headset_init.h"
#include "bt_device_class.h"
#include "headset_config.h"
#include <connection_no_ble.h>
#include <unexpected_message.h>
#include <system_state.h>
#include <connection_message_dispatcher.h>
#include <app_task.h>
#include <app/bluestack/dm_prim.h>
#include <multidevice.h>

#include <panic.h>
#include <logging.h>

/* Needed only for message dispatcher registration */
#include <le_scan_manager.h>
#include <pairing.h>
#include <connection_manager.h>
#include <link_policy.h>
#include <authentication.h>
#include <le_advertising_manager.h>
#include <local_addr.h>
#include <fast_pair.h>
#include <bt_device.h>
#include <device_db_serialiser.h>


#define SELF_DVICE_KEY_SIZE             (0x08)

#define INVALID_LINK_KEY                (0xFFFF)

/*!< Structure used while initialising */
initData    headset_init;

TaskData *HeadsetInitBt_GetTask(void)
{
   return (&headset_init.task);
}

/*! \brief Forward CL_INIT_CFM message to the init task handler. */
static void headsetInitFwdClInitCfm(const CL_INIT_CFM_T * cfm)
{
    CL_INIT_CFM_T *copy = PanicUnlessNew(CL_INIT_CFM_T);
    *copy = *cfm;
    MessageSend(SystemState_GetTransitionTask(), CL_INIT_CFM, copy);
}

/*! \brief Handle Connection library confirmation message */
static void headsetInitHandleClInitCfm(const CL_INIT_CFM_T *cfm)
{
    if (cfm->status != success)
        Panic();

    /* Set the class of device to indicate this is a headset */
    ConnectionWriteClassOfDevice(AUDIO_MAJOR_SERV_CLASS | RENDER_MAJOR_SERV_CLASS |
                                 AV_MAJOR_DEVICE_CLASS | HEADSET_MINOR_DEVICE_CLASS);

    /* Allow SDP without security, requires authorisation */
    ConnectionSmSetSecurityLevel(0, 1, ssp_secl4_l0, TRUE, TRUE, FALSE);

    /* Reset security mode config - always turn off debug keys on power on */
    ConnectionSmSecModeConfig(appGetAppTask(), cl_sm_wae_acl_owner_none, FALSE, TRUE);

    headsetInitFwdClInitCfm(cfm);
}

/*! \brief Handle Connection library confirmation message to store SELF device in PDL */
bool HeadsetInitBt_InitHandleClDmLocalBdAddrCfm(Message message)
{
    CL_DM_LOCAL_BD_ADDR_CFM_T *cfm = (CL_DM_LOCAL_BD_ADDR_CFM_T *)message;
    uint16 self_device_lk[SELF_DVICE_KEY_SIZE] = {INVALID_LINK_KEY};

    if (cfm->status != success)
        Panic();

    /* Store the SELF device in the PDL */
    ConnectionSmAddAuthDevice(&headset_init.task,
                              &cfm->bd_addr,
                              FALSE,
                              TRUE,
                              cl_sm_link_key_unauthenticated_p192,
                              SELF_DVICE_KEY_SIZE,
                              self_device_lk);

    return TRUE;
}

/*! \brief Handle Connection library confirmation message to add SELF BT device */
static void headsetInitHandleClSmAddAuthDeviceCfm(const CL_SM_ADD_AUTH_DEVICE_CFM_T *cfm)
{
    if (cfm->status != success)
    {
        Panic();
    }

    uint32 supported_profiles = (DEVICE_PROFILE_AVRCP | DEVICE_PROFILE_A2DP | DEVICE_PROFILE_HFP);

    device_t device = BtDevice_GetDeviceCreateIfNew(&cfm->bd_addr, DEVICE_TYPE_SELF);
    PanicNull(device);

    /* Set the protection for SELF device so just in case when trusted device list(appConfigMaxTrustedDevices)
    is exhasuted, connection library will not overwrite entry of this SELF device in the PDL */
    ConnectionAuthSetPriorityDevice(&cfm->bd_addr, TRUE);

    /* Setting profiles supported by SELF. */
    BtDevice_AddSupportedProfilesToDevice(device, supported_profiles);

    /* Save the self BT device in PS store so that we don't lose any data in case of unexpected power cycle */
    DeviceDbSerialiser_Serialise();
    BtDevice_PrintAllDevices();
}

/*! \brief Connection library Message Handler

    This function is the main message handler for the main application task, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void headsetHandleClMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_V_VERBOSE("headsetHandleClMessage called, message id MESSAGE:0x%x", id);
    /* Handle Connection Library messages that are not sent directly to
       the requestor */
    if (CL_MESSAGE_BASE <= id && id < CL_MESSAGE_TOP)
    {
        bool handled = FALSE;

        if (id == CL_INIT_CFM)
        {
            headsetInitHandleClInitCfm((const CL_INIT_CFM_T *)message);
            DEBUG_LOG_DEBUG("headsetHandleClMessage:CL_INIT_CFM Received ");

            return;
        }

        if(id == CL_SM_ADD_AUTH_DEVICE_CFM)
        {
            DEBUG_LOG_VERBOSE("headsetHandleClMessage:CL_SM_ADD_AUTH_DEVICE_CFM Received ");
            headsetInitHandleClSmAddAuthDeviceCfm((const CL_SM_ADD_AUTH_DEVICE_CFM_T *)message);

            return;
        }
        /* Pass connection library messages in turn to the modules that
           are interested in them.
         */
        handled |= Pairing_HandleConnectionLibraryMessages(id, message, handled);
        handled |= ConManagerHandleConnectionLibraryMessages(id, message, handled);
        handled |= appLinkPolicyHandleConnectionLibraryMessages(id, message, handled);
        handled |= appAuthHandleConnectionLibraryMessages(id, message, handled);
        handled |= LeAdvertisingManager_HandleConnectionLibraryMessages(id, message, handled);
        handled |= LocalAddr_HandleConnectionLibraryMessages(id, message, handled);
        handled |= BtDevice_HandleConnectionLibraryMessages(id, message, handled);
#ifdef INCLUDE_FAST_PAIR
        handled |= FastPair_HandleConnectionLibraryMessages(id, message, handled);
#endif
        if (handled)
        {
            return;
        }
    }

    DEBUG_LOG_VERBOSE("headsetHandleClMessage called but unhandled, message id MESSAGE:0x%x", id);
    UnexpectedMessage_HandleMessage(id);
}

/*! \brief Connection library initialisation */
bool HeadsetInitBt_ConnectionInit(Task init_task)
{
    static const msg_filter filter = {msg_group_acl | msg_group_mode_change};

    UNUSED(init_task);

    ConnectionMessageDispatcher_Init();

    /* Initialise the Connection Manager */
#if defined(APP_SECURE_CONNECTIONS)
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigHeadsetMaxDevicesSupported(), CONNLIB_OPTIONS_SC_ENABLE);
#else
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigHeadsetMaxDevicesSupported(), CONNLIB_OPTIONS_NONE);
#endif
    ConnectionMessageDispatcher_RegisterInitClient(&headset_init.task);

    return TRUE;
}

void HeadsetInitBt_StartBtInit(void)
{
    headset_init.task.handler = headsetHandleClMessage;
}
