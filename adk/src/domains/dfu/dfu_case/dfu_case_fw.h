/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_fw.h
\brief      Header file for the fw state machine of the dfu_case.

            This component can handle following messages,
            DFU_CASE_INTERNAL_INITIATE_CASE_DFU,
            DFU_CASE_INTERNAL_DFU_COMPATIBLE,
            DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE,
            DFU_CASE_INTERNAL_REBOOT_CASE,
            DFU_CASE_INTERNAL_COMMIT_UPGRADE
*/

#ifndef DFU_CASE_FW_H_
#define DFU_CASE_FW_H_

#ifdef INCLUDE_DFU_CASE

#include <message.h>
#include <upgrade_protocol.h>

typedef enum
{
    DFU_CASE_FW_BANK_A = 'A',

    DFU_CASE_FW_BANK_B,
} dfu_case_fw_bank_t;

extern TaskData dfu_case_fw;

/*! Get pointer to dfu_case task. */
#define dfuCase_FWGetTask()  (&dfu_case_fw)

/*! \brief Initialize the dfu_case_fw FSM
*/
bool DfuCase_FWInit(void);

/*! \brief Initialze the dfu_case_fw for an upcoming DFU
*/
void DfuCase_FWInitiateCaseDfu(void);

/*! \brief Abort the DFU with case if needed and cleanup.
*/
void dfuCase_FWAbort(UpgradeHostErrorCode error_code, bool inform_case);

/*! \brief Has case DFU already started. 
*/
bool DfuCase_FWIsDfuStarted(void);

/*! \brief Has case DFU data transfer started. 
*/
bool DfuCase_FWIsDfuInDataTransfer(void);

/*! \brief Has CHECK message from case been received. 
*/
bool DfuCase_FWIsDfuCheckReceived(void);

/*! \brief Has VERIFY message from case been received after reboot. 
*/
bool DfuCase_FWIsCaseRebooted(void);

/*! \brief Get the image bank of the case which needs to be updated.

    \return image bank which needs to be updated
*/
dfu_case_fw_bank_t DfuCase_FWGetBankToUpgrade(void);

/*! \brief Clean up after DFU comlpletes or aborts
*/
void DfuCase_FWCleanUp(void);

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_FW_H_ */
