/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#ifdef INCLUDE_QCOM_CON_MANAGER

#include <message.h>
#include <task_list.h>
#include <panic.h>
#include <logging.h>
#include <vsdm_prim.h>
#include "connection_manager.h"
#include "system_state.h"
#include "bt_device_class.h"

#include "qualcomm_connection_manager.h"
#include "qualcomm_connection_manager_private.h"


#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(QCOM_CON_MANAGER, QCOM_CON_MANAGER_MESSAGE_END)

#endif

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(qcm_msgs_t)
LOGGING_PRESERVE_MESSAGE_TYPE(qualcomm_connection_manager_internal_message_t)

static Task client_truncated_scan = NULL;

static bool qcomConManagerIsQhsSupported(const uint8 *features)
{
    return ((features[QHS_MODE_2345_OCTET] & QHS_MODE_2345_MASK) ||
            (features[QHS_MODE_6_OCTET]    & QHS_MODE_6_MASK));
}

static bool qcomConManagerIsQhsSupportedForIsochronous(const uint8 *features)
{
    return (features[QHS_MODE_ISO_OCTET] & QHS_MODE_ISO_MASK);
}

static bool qcomConManagerIsFastExitSubrateSupported(const uint8 *features)
{
    return ((features[FAST_EXIT_SNIFF_SUBRATE_OCTET] & FAST_EXIT_SNIFF_SUBRATE_MASK) != 0);
}

static void qcomConManagerSendQhsConnectedIndicationToClients(const bdaddr *bd_addr)
{
    qcomConManagerTaskData *sp = QcomConManagerGet();
    MESSAGE_MAKE(qhs_connected_ind, QCOM_CON_MANAGER_QHS_CONNECTED_T);
    qhs_connected_ind->bd_addr = *bd_addr;
    TaskList_MessageSend(&sp->client_tasks, QCOM_CON_MANAGER_QHS_CONNECTED, qhs_connected_ind);
}

static void qcomConManagerSendRemoteQhsCapableIndicationToClients(const tp_bdaddr *tp_addr)
{
    qcomConManagerTaskData *sp = QcomConManagerGet();
    MESSAGE_MAKE(qhs_capable_ind, QCOM_CON_MANAGER_REMOTE_ISO_QHS_CAPABLE_IND_T);
    qhs_capable_ind->tp_addr = *tp_addr;
    TaskList_MessageSend(&sp->client_tasks, QCOM_CON_MANAGER_REMOTE_ISO_QHS_CAPABLE_IND, qhs_capable_ind);
}

static void qcomConManagerSendReadRemoteQlmSuppFeaturesReq(const BD_ADDR_T *bd_addr)
{
    MAKE_VSDM_PRIM_T(VSDM_READ_REMOTE_QLM_SUPP_FEATURES_REQ);
    prim->phandle = 0;
    prim->bd_addr = *bd_addr;
    VmSendVsdmPrim(prim);
}

static void qcomConManagerSendReadRemoteQllSuppFeaturesReq(const TP_BD_ADDR_T *tp_bd_addr)
{
    MAKE_VSDM_PRIM_T(VSDM_READ_REMOTE_QLL_SUPP_FEATURES_REQ);
    prim->phandle = 0;
    prim->tp_addrt = *tp_bd_addr;
    VmSendVsdmPrim(prim);
}

static void qcomConManagerTriggerRemoteFeaturesRead(const BD_ADDR_T *bd_addr, uint32 delay_ms)
{
    MESSAGE_MAKE(msg, QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES_T);
    msg->bd_addr = *bd_addr;
    MessageCancelFirst(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES);
    MessageSendLater(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES, msg, delay_ms);
}

static void qcomConManagerQllTriggerRemoteFeaturesRead(const TP_BD_ADDR_T *tp_addr, uint32 delay_ms)
{
    MESSAGE_MAKE(msg, QCOM_CON_MANAGER_INTERNAL_READ_QLL_REMOTE_FEATURES_T);
    msg->tp_addr = *tp_addr;
    MessageCancelFirst(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLL_REMOTE_FEATURES);
    MessageSendLater(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLL_REMOTE_FEATURES, msg, delay_ms);
}

static void qcomConManagerHandleVsdmQlmPhyChangeInd(const VSDM_QCM_PHY_CHANGE_IND_T * ind)
{
    DEBUG_LOG("qcomConManagerHandleVsdmQlmPhyChangeInd  qhs status=%d bdaddr lap=%06lX",
              ind->status,ind->bd_addr.lap);

    if(ind->status == HCI_SUCCESS)
    {
        bdaddr bd_addr;
        BdaddrConvertBluestackToVm(&bd_addr, &ind->bd_addr);
        qcomConManagerTriggerRemoteFeaturesRead(&ind->bd_addr, D_IMMEDIATE);
        ConManagerSetQhsConnectStatus(&bd_addr,TRUE);
        appDeviceSetQhsConnected(&bd_addr, TRUE);
        qcomConManagerSendQhsConnectedIndicationToClients(&bd_addr);
    }
}

static void qcomConManagerHandleVsdmSetWbmFeatureCfm(const VSDM_SET_WBM_FEATURES_CFM_T *cfm)
{
    DEBUG_LOG("qcomConManagerHandleVsdmSetWbmFeatureCfm status=%d", cfm->status);
}

static void qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm(const VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM_T *cfm)
{
    bool qhs_supported = FALSE;
    bool fast_exit_subrate_supported = FALSE;
    bdaddr bd_addr;
    
    BdaddrConvertBluestackToVm(&bd_addr, &cfm->bd_addr);

    /* Check if qhs is supported or not */
    if (cfm->status == HCI_SUCCESS)
    {
        qhs_supported = qcomConManagerIsQhsSupported(cfm->qlmp_supp_features);
        fast_exit_subrate_supported = qcomConManagerIsFastExitSubrateSupported(cfm->qlmp_supp_features);

        if (!qhs_supported)
        {
            /* Device doesn't support QHS, so clear the flag */
            appDeviceSetQhsConnected(&bd_addr, FALSE);
        }

    }

    DEBUG_LOG("qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm status=%d lap:%x qhs:%d fes:%d",
               cfm->status, bd_addr.lap, qhs_supported, fast_exit_subrate_supported);

    ConManagerSetQhsSupportStatus(&bd_addr, qhs_supported);
    ConManagerSetFastExitSniffSubrateSupportStatus(&bd_addr, fast_exit_subrate_supported);
}

static void qcomConManagerHandleVsdmRemoteQllSuppFeaturesCfm(const VSDM_READ_REMOTE_QLL_SUPP_FEATURES_CFM_T *cfm)
{
    tp_bdaddr tp_addr;

    BdaddrConvertTpBluestackToVm(&tp_addr, &cfm->tp_addrt);
    DEBUG_LOG("qcomConManagerHandleVsdmRemoteQllSuppFeaturesCfm status=%d lap:%x",
               cfm->status, tp_addr.taddr.addr.lap);

    /* Check if QHS is supported or not */
    if (cfm->status == HCI_SUCCESS)
    {
        if (qcomConManagerIsQhsSupportedForIsochronous(cfm->qll_supp_features))
        {
            qcomConManagerSendRemoteQhsCapableIndicationToClients(&tp_addr);
        }
    }
}

static void qcomConManagerHandleVsdmQlmConnectionCompleteInd(const VSDM_QLM_CONNECTION_COMPLETE_IND_T *ind)
{
    bool qlmp_connected = FALSE;
    bdaddr bd_addr;

    DEBUG_LOG("qcomConManagerHandleVsdmQlmConnectionCompleteInd status=0x%x lap=%06lX",
               ind->status,ind->bd_addr.lap);

    if(ind->status == HCI_SUCCESS)
    {
        qcomConManagerTriggerRemoteFeaturesRead(&ind->bd_addr, REQUEST_REMOVE_FEATURES_DELAY_MS);
        qlmp_connected = TRUE;
    }

    BdaddrConvertBluestackToVm(&bd_addr, &ind->bd_addr);
    ConManagerSetQlmpConnectStatus(&bd_addr, qlmp_connected);
}

static void qcomConManagerHandleVsdmQllConnectionCompleteInd(const VSDM_QLL_CONNECTION_COMPLETE_IND_T *ind)
{
    DEBUG_LOG("qcomConManagerHandleVsdmQllConnectionCompleteInd status=0x%x lap=%06lX",
               ind->status, ind->tp_addrt.addrt.addr.lap);

    /* QLL got connected. Trigger a remote feature read request */
    if (ind->status == HCI_SUCCESS)
    {
        qcomConManagerQllTriggerRemoteFeaturesRead(&ind->tp_addrt, D_IMMEDIATE);
    }
}

static void qcomConManagerHandleVsdmScHostSuppOverrideCfm(const VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM_T *cfm)
{
    PanicFalse(cfm->status == HCI_SUCCESS);
}

static void qcomConManagerSendScHostSuppOverrideReq(void)
{
    uint8 count =0;

    MAKE_VSDM_PRIM_T(VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_REQ);
    prim->phandle = 0;
    prim->num_compIDs = count;

    while(vendor_info[count].comp_id != 0)
    {
        prim->compID[count] = vendor_info[count].comp_id;
        prim->min_lmpVersion[count] = vendor_info[count].min_lmp_version;
        prim->min_lmpSubVersion[count] = vendor_info[count].min_lmp_sub_version;
        prim->num_compIDs = ++count;

        /* Check if input number of comp ids are not exceeding
           maximum number of comp ids supported by vs prims */
        if(prim->num_compIDs == VSDM_MAX_NO_OF_COMPIDS)
        {
            Panic();
        }
    }

    if(prim->num_compIDs != 0)
    {
        VmSendVsdmPrim(prim);
    }

    /* Send init confirmation */
    MessageSend(SystemState_GetTransitionTask(), QCOM_CON_MANAGER_INIT_CFM, NULL);
}

static void qcomConManagerSetLocalSupportedFeaturesFlags(const uint8 *features)
{
    /* Check if qhs is supported or not */
    if (qcomConManagerIsQhsSupported(features))
    {
        QcomConManagerGet()->qhs = TRUE;
    }
    if (qcomConManagerIsFastExitSubrateSupported(features))
    {
        QcomConManagerGet()->fast_exit_sniff_subrate = TRUE;
    }
}

static void qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm(const VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM_T *cfm)
{
    PanicFalse(cfm->status == HCI_SUCCESS);

    qcomConManagerSetLocalSupportedFeaturesFlags(cfm->qlmp_supp_features);

    if (QcomConManagerGet()->qhs)
    {
        /* Send SC host override req as local device supports qhs*/
        qcomConManagerSendScHostSuppOverrideReq();
    }
    else
    {
        DEBUG_LOG("qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm: QHS not supported");
        /* Inform application that qcom con manager init completed */
        MessageSend(SystemState_GetTransitionTask(), QCOM_CON_MANAGER_INIT_CFM, NULL);
    }
}


static void qcomConManagerSendReadLocalQlmSuppFeaturesReq(void)
{
    /*send VSDM_READ_LOCAL_QLM_SUPP_FEATURES_REQ  to find if qhs is supported or not*/
    MAKE_VSDM_PRIM_T(VSDM_READ_LOCAL_QLM_SUPP_FEATURES_REQ);
    prim->phandle = 0;
    VmSendVsdmPrim(prim);
}


static void qcomConManagerHandleVsdmRegisterCfm(const VSDM_REGISTER_CFM_T *cfm)
{
    PanicFalse(cfm->result == VSDM_RESULT_SUCCESS);
    /*Check if local device supports qhs or not and proceed to create qhs with remote
      handset if local supports qhs */
    qcomConManagerSendReadLocalQlmSuppFeaturesReq();

}

static void qcomConManagerRegisterReq(void)
{
    MAKE_VSDM_PRIM_T(VSDM_REGISTER_REQ);
    prim->phandle = 0;
    VmSendVsdmPrim(prim);
}

static Task barge_in_client;

static Task GetBargeInClientTask(void)
{
    return barge_in_client;
}

static void qcomConManagerSendTruncatedPageScanInd(bdaddr * addr)
{
    DEBUG_LOG("qcomConManagerSendTruncatedPageScanInd");
    MESSAGE_MAKE(ind, QCOM_CON_MANAGER_BARGE_IN_IND_T);
    ind->bd_addr.uap = addr->uap;
    ind->bd_addr.lap = addr->lap;
    ind->bd_addr.nap = addr->nap;

    MessageSend(GetBargeInClientTask(), QCOM_CON_MANAGER_BARGE_IN_IND, ind);
}

static void qcomConManagerHandleVsdmIncomingPageInd(const VSDM_INCOMING_PAGE_IND_T *ind)
{
    DEBUG_LOG("qcomConManagerHandleVsdmIncomingPageInd");
    bdaddr bd_addr;
    BdaddrConvertBluestackToVm(&bd_addr, &ind->bd_addr);
    qcomConManagerSendTruncatedPageScanInd(&bd_addr);
}

static void qcomConManagerHandleVsdmTruncatedPageScanEnableCfm(const VSDM_WRITE_TRUNCATED_PAGE_SCAN_ENABLE_CFM_T *cfm)
{
    DEBUG_LOG("qcomConManagerHandleVsdmTruncatedPageScanEnableCfm status:0x%x", cfm->status);
    MESSAGE_MAKE(msg, QCOM_CON_MANAGER_TRUNCATED_SCAN_ENABLE_CFM_T);
    msg->status = cfm->status;

    MessageSend(client_truncated_scan, QCOM_CON_MANAGER_TRUNCATED_SCAN_ENABLE_CFM, msg);
}

static void qcomConManagerHandleVsdmSetStreamingModeCfm(const VSDM_SET_STREAMING_MODE_CFM_T *cfm)
{
    DEBUG_LOG("qcomConManagerHandleVsdmSetStreamingModeCfm status:0x%x", cfm->status);
}

static void qcomConManagerHandleBluestackVsdmPrim(const VSDM_UPRIM_T *vsprim)
{
    switch (vsprim->type)
    {
    case VSDM_REGISTER_CFM:
        qcomConManagerHandleVsdmRegisterCfm((const VSDM_REGISTER_CFM_T *)vsprim);
        break;
    case VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM:
        qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm((VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM_T *)vsprim);
        break;
    case VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM:
        qcomConManagerHandleVsdmScHostSuppOverrideCfm((const VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM_T *)vsprim);
        break;
     case VSDM_QLM_CONNECTION_COMPLETE_IND:
        qcomConManagerHandleVsdmQlmConnectionCompleteInd((const VSDM_QLM_CONNECTION_COMPLETE_IND_T *)vsprim);
        break;
    case VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM:
       qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm((const VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM_T *)vsprim);
       break;
    case VSDM_QCM_PHY_CHANGE_IND:
        qcomConManagerHandleVsdmQlmPhyChangeInd((const VSDM_QCM_PHY_CHANGE_IND_T *)vsprim);
        break;
    case VSDM_SET_WBM_FEATURES_CFM:
        qcomConManagerHandleVsdmSetWbmFeatureCfm((const VSDM_SET_WBM_FEATURES_CFM_T *)vsprim);
        break;

    case VSDM_INCOMING_PAGE_IND:
        qcomConManagerHandleVsdmIncomingPageInd((const VSDM_INCOMING_PAGE_IND_T *)vsprim);
        break;

    case VSDM_WRITE_TRUNCATED_PAGE_SCAN_ENABLE_CFM:
        qcomConManagerHandleVsdmTruncatedPageScanEnableCfm((const VSDM_WRITE_TRUNCATED_PAGE_SCAN_ENABLE_CFM_T *)vsprim);
        break;
    case VSDM_SET_STREAMING_MODE_CFM:
        qcomConManagerHandleVsdmSetStreamingModeCfm((const VSDM_SET_STREAMING_MODE_CFM_T*)vsprim);
        break;
    case VSDM_QLL_CONNECTION_COMPLETE_IND:
        qcomConManagerHandleVsdmQllConnectionCompleteInd((const VSDM_QLL_CONNECTION_COMPLETE_IND_T *)vsprim);
        break;
    case VSDM_READ_REMOTE_QLL_SUPP_FEATURES_CFM:
        qcomConManagerHandleVsdmRemoteQllSuppFeaturesCfm((const VSDM_READ_REMOTE_QLL_SUPP_FEATURES_CFM_T *)vsprim);
        break;
    default:
        DEBUG_LOG("qcomConManagerHandleBluestackVsdmPrim : unhandled vsprim type %x", vsprim->type);
        break;
    }
}

static void qcomConManagerHandleMessage(Task task, MessageId id, Message message)
{
   UNUSED(task);

   switch (id)
   {
     /* VSDM prims from firmware */
    case MESSAGE_BLUESTACK_VSDM_PRIM:
        qcomConManagerHandleBluestackVsdmPrim((const VSDM_UPRIM_T *)message);
        break;

    case QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES:
    {
        const QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES_T *msg = message;
        qcomConManagerSendReadRemoteQlmSuppFeaturesReq(&msg->bd_addr);
        break;
    }

    case QCOM_CON_MANAGER_INTERNAL_READ_QLL_REMOTE_FEATURES:
    {
        const QCOM_CON_MANAGER_INTERNAL_READ_QLL_REMOTE_FEATURES_T *msg = message;
        qcomConManagerSendReadRemoteQllSuppFeaturesReq(&msg->tp_addr);
        break;
    }

    default:
        break;
    }
}
/* Global functions */

bool QcomConManagerInit(Task task)
{
    UNUSED(task);

    DEBUG_LOG("QcomConManagerInit");
    memset(&qcom_con_manager, 0, sizeof(qcom_con_manager));
    qcom_con_manager.task.handler = qcomConManagerHandleMessage;
    TaskList_Initialise(&qcom_con_manager.client_tasks);

    /* Register with the firmware to receive MESSAGE_BLUESTACK_VSDM_PRIM messages */
    MessageVsdmTask(QcomConManagerGetTask());

    /* Register with the VSDM service */
    qcomConManagerRegisterReq();

    return TRUE;
}

void QcomConManagerRegisterClient(Task client_task)
{
    DEBUG_LOG("QcomConManagerRegisterClient");
    TaskList_AddTask(&qcom_con_manager.client_tasks, client_task);
}

void QcomConManagerUnRegisterClient(Task client_task)
{
    TaskList_RemoveTask(&qcom_con_manager.client_tasks, client_task);
}

bool QcomConManagerIsFastExitSniffSubrateSupported(const bdaddr *addr)
{
    return QcomConManagerGet()->fast_exit_sniff_subrate &&
           ConManagerGetFastExitSniffSubrateSupportStatus(addr);
}

void QcomConManagerSetWBMFeature(hci_connection_handle_t conn_handle, bool enable)
{
    DEBUG_LOG("QcomConManagerSetWBMFeature, handle 0x%x, Enable %d", conn_handle, enable);

    /* send VSDM_SET_WBM_FEATURES_REQ to SET WBM ENABLE_BIT MASK */
    MAKE_VSDM_PRIM_T(VSDM_SET_WBM_FEATURES_REQ);
    prim->phandle = 0;
    prim->enable_mask = enable ? WBM_FEATURES_BIT_ENABLE : WBM_FEATURES_BIT_DISABLE;
    prim->conn_handle = conn_handle;
    VmSendVsdmPrim(prim);
}

void QcomConManagerSetStreamingMode(tp_bdaddr *tp_addr, uint8_t streaming_mode)
{
    DEBUG_LOG("QcomConManagerSetStreamingMode, mode %d", streaming_mode);

    MAKE_VSDM_PRIM_T(VSDM_SET_STREAMING_MODE_REQ);
    prim->phandle = 0x0;  /* Destination phandle */
    /* Address of the remote device */
    prim->tp_addrt.tp_type = tp_addr->transport;
    prim->tp_addrt.addrt.type = tp_addr->taddr.type;
    prim->tp_addrt.addrt.addr.lap = tp_addr->taddr.addr.lap;
    prim->tp_addrt.addrt.addr.nap = tp_addr->taddr.addr.nap;
    prim->tp_addrt.addrt.addr.uap = tp_addr->taddr.addr.uap;

    prim->streaming_mode = streaming_mode; /* Streaming mode to be set */
    VmSendVsdmPrim(prim);
}

void QcomConManagerEnableTruncatedScan(Task client_task, bool enable)
{
    MAKE_VSDM_PRIM_T(VSDM_WRITE_TRUNCATED_PAGE_SCAN_ENABLE_REQ);
    prim->phandle = 0x0;
    prim->enable = enable ? WRITE_TRUNCATED_PAGE_SCAN_ENABLE : WRITE_TRUNCATED_PAGE_SCAN_DISABLE;
    VmSendVsdmPrim(prim);
    client_truncated_scan = client_task;
}

void QcomConManagerRegisterBargeInClient(Task task)
{
    barge_in_client = task;
}
#endif /* INCLUDE_QCOM_CON_MANAGER */
