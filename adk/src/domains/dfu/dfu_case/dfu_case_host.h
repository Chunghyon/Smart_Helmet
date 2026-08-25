/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_host.h
\brief      Header file for the host state machine of the dfu_case.

            This component can handle following messages,
            DFU_CASE_INTERNAL_CHECK_RECEIVED,
            DFU_CASE_INTERNAL_REQUEST_MORE_DATA,
            DFU_CASE_INTERNAL_CHECKSUM_VERIFIED,
            DFU_CASE_INTERNAL_CASE_READY_FOR_DATA,
            DFU_CASE_INTERNAL_CASE_REBOOTED,
            DFU_CASE_INTERNAL_UPGRADE_COMPLETE
*/

#ifndef DFU_CASE_HOST_H_
#define DFU_CASE_HOST_H_

#ifdef INCLUDE_DFU_CASE

#include <message.h>
#include <upgrade_protocol.h>

extern TaskData dfu_case_host;

/*! Get pointer to dfu_case task. */
#define dfuCase_HostGetTask()  (&dfu_case_host)

/*! \brief Initialize the dfu_case_host FSM
*/
bool DfuCase_HostInit(void);

/*! \brief Start the Case DFU on Host side
*/
void DfuCase_HostStartCaseDfu(void);

/*! \brief User has enabled DFU mode from UI which needs to be handled.
*/
void DfuCase_HostHandleDfuMode(void);

/*! \brief User selected earbud file during in case DFU.
*/
void DfuCase_HostHandleEarbudDfu(void);

/*! \brief Pause the data transfer with the host due to transport linkloss.
*/
void DfuCase_HostPauseCaseDfu(void);

/*! \brief Resume the data transfer with the host after transport reconnection.
*/
void DfuCase_HostResumeCaseDfu(void);

/*! \brief Confirm that DFU file indeed contains a case image.
*/
bool DfuCase_HostIsCaseDfuConfirmed(void);

/*! \brief TRUE if we have requested the host to put earbuds in case and not confirmed .
*/
bool DfuCase_HostIsEarbudsInCaseRequested(void);

/*! \brief Cleanup the dfu_case_host FSM after a DFU finishes.
*/
void DfuCase_HostCleanUp(bool isError);

/*! \brief Send error indication to host if present otherwise cleanup on host side.
*/
void DfuCase_HostRequestAbort(UpgradeHostErrorCode error_code);

/*! \brief Clean up the case DFU and abort DFU process in charger case side
*/
void DfuCase_HostAbortWithCase(void);


#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_HOST_H_ */
