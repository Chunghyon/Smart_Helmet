/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       qualcomm_connection_manager.h
\brief      Header file for Qualcomm Connection Manager
*/

#ifndef __QCOM_CON_MANAGER_H
#define __QCOM_CON_MANAGER_H


#include <message.h>
#include <bdaddr.h>
#include <hci.h>
#include "domain_message.h"

/*! \brief vendor information */

typedef struct{

     /*! Company/vendor specific id. */
    uint16 comp_id;
    /*! Minimum lmp version for sc/qhs support. */
    uint8 min_lmp_version;
    /*! Minimum lmp sub version for sc/qhs support. */
    uint16 min_lmp_sub_version;

} QCOM_CON_MANAGER_SC_OVERRIDE_VENDOR_INFO_T;

/*! \brief Vendor information array buffer to be provided by application.
     This array will be terminated by setting company id(comp_id) to 0.
     Maximum number of company ids supported are VSDM_MAX_NO_OF_COMPIDS.
*/
extern const QCOM_CON_MANAGER_SC_OVERRIDE_VENDOR_INFO_T vendor_info[];

/*! \brief Events sent by qualcomm connection manager module to other modules. */
typedef enum
{
    /*! Module initialisation complete */
    QCOM_CON_MANAGER_INIT_CFM = QCOM_CON_MANAGER_MESSAGE_BASE,

    /*! QHS Established Indication to mirroring profile */
    QCOM_CON_MANAGER_QHS_CONNECTED,

    /*! Connection Barge-In Indication */
    QCOM_CON_MANAGER_BARGE_IN_IND,

    /*! Truncated scan enable confirmation */
    QCOM_CON_MANAGER_TRUNCATED_SCAN_ENABLE_CFM,

    /*! Remote supports QHS on ISO indication */
    QCOM_CON_MANAGER_REMOTE_ISO_QHS_CAPABLE_IND,

    /*! This must be the final message */
    QCOM_CON_MANAGER_MESSAGE_END
}qcm_msgs_t;

typedef struct
{
    hci_return_t status;

} QCOM_CON_MANAGER_TRUNCATED_SCAN_ENABLE_CFM_T;

typedef struct
{
    /*! BT address of the device to barge-in. */
    bdaddr bd_addr;

} QCOM_CON_MANAGER_BARGE_IN_IND_T;

typedef struct
{
    /*! BT address of qhs connected device. */
    bdaddr bd_addr;

} QCOM_CON_MANAGER_QHS_CONNECTED_T;

typedef struct
{
    /*! BT address of the remote device. */
    tp_bdaddr tp_addr;

} QCOM_CON_MANAGER_REMOTE_ISO_QHS_CAPABLE_IND_T;


#ifdef INCLUDE_QCOM_CON_MANAGER

/*! \brief Initialise the qualcomm connection manager module.
 */
bool QcomConManagerInit(Task init_task);

/*! \brief Register a client task to receive notifications of qualcomm connection manager.

    \param[in] client_task Task which will receive notifications from qualcomm conenction manager.
 */
void QcomConManagerRegisterClient(Task client_task);

/*! \brief Unregister a client task to stop receiving notifications from qualcomm conenction manager.

    \param[in] client_task Task to unregister.
 */
void QcomConManagerUnRegisterClient(Task client_task);

/*! \brief Query if fast exit sniff subrate is supported by the local and remote
           addressed device.
    \param[in] addr Address of the remote device.
    \return TRUE if both local and remote devices support the feature.
*/
bool QcomConManagerIsFastExitSniffSubrateSupported(const bdaddr *addr);

/*! \brief To enable/disable the support of the Weak Bitmask(WBM) propagation 
           feature on an ISO handle 
    \param[in] conn_handle CIS/BIS connection handle
    \param[in] enable TRUE to Enable WBM FALSE to Disable WBM
*/
void QcomConManagerSetWBMFeature(hci_connection_handle_t conn_handle,bool enable);

/*! \brief To enable/disable truncated page scan

    \param[in] client_task Task to send request to enable truncatd page scan.
    \param[in] enable TRUE to Enable FALSE to Disable truncated page scan
*/
void QcomConManagerEnableTruncatedScan(Task client_task,bool enable);

void QcomConManagerRegisterBargeInClient(Task task);

/*! \brief To set the TWM streaming mode

    \param[in] tp_addrt address of connected device
    \param[in] streaming mode. 0 = disabled, 1 = aptX adaptive Low Latency & High Bandwidth mode, 2 = Gaming Mode
 */
void QcomConManagerSetStreamingMode(tp_bdaddr *tp_addr, uint8_t streaming_mode);

#else
#define QcomConManagerInit(task) UNUSED(task)

#define QcomConManagerRegisterClient(task) UNUSED(task)

#define QcomConManagerUnRegisterClient(task) UNUSED(task)

#define QcomConManagerIsFastExitSniffSubrateSupported(addr) (UNUSED(addr), FALSE)

#define QcomConManagerSetWBMFeature(conn_handle, enable) ((void) conn_handle, (void) enable)

#define QcomConManagerEnableTruncatedScan(task, enable) (UNUSED(task), UNUSED(enable))

#define QcomConManagerRegisterBargeInClient(task) UNUSED(task)

#endif /* INCLUDE_QCOM_CON_MANAGER */

#endif /*__QCOM_CON_MANAGER_H*/
