/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case.h
\brief      Interface of the dfu_case module.

    dfu_case module is used to do the DFU of stm32 based Charger Case over the
    fastComms protocol.
*/

#ifndef DFU_CASE_H_
#define DFU_CASE_H_

#ifdef INCLUDE_DFU_CASE

#include <message.h>
#include <domain_message.h>
#include <upgrade.h>

#include <rtime.h>
#include <task_list.h>

/*! Messages that are sent by the dfu_case module */
typedef enum {
    /* dfu domain has requested host to put earbuds in case for case DFU. */
    DFU_CASE_EARBUDS_IN_CASE_REQUSETED = DFU_CASE_MESSAGE_BASE,

    /* earbuds are in case and dfu domain has received the check message from case. */
    DFU_CASE_EARBUDS_IN_CASE_CONFIRM,

    /*! This must be the final message */
    DFU_CASE_MESSAGE_END
} dfu_case_messages_t;

/*! \brief Initialize the task_list of dfu_case module

    \param init_task step_task for step_function
    \return TRUE if successful
 */
bool DfuCase_EarlyInit(Task init_task);

/*! \brief Initialize the dfu_case module

    \param init_task step_task for step_function
    \return TRUE if successful
 */
bool DfuCase_Init(Task init_task);

/*! \brief Add a client to the dfu_case module

    Messages will be sent to any task registered through this API

    \param task Task to register as a client
 */
void DfuCase_ClientRegister(Task task);

/*! \brief Handle the last case related operation done by upgrade library.

    \return none
*/
void DfuCase_HandleLibOperation(MessageId id, Message message);

/*! \brief User has enabled DFU mode from UI which needs to be handled.

    \return none
*/
void DfuCase_HandleDfuMode(void);

/*! \brief Handle the transport disconnection, pause the case DFU.
*/
void DfuCase_HandleTransportDisconnect(void);

/*! \brief Abort the case DFU if its on going.

    \return none
*/
void DfuCase_Abort(void);

/*! \brief Is case DFU required or not.

    case_comms domain can use this API to check if the DFU related bit
    in the earbud status needs to be set.

    \return TRUE if already started.
*/
bool DfuCase_IsDfuNeeded(void);

/*! \brief TRUE if we have requested the host to put earbuds in case and not confirmed .
*/
bool DfuCase_IsEarbudsInCaseRequested(void);

#else
#define DfuCase_EarlyInit(init_task)                                                                ((void)0)
#define DfuCase_Init(init_task)                                                                     ((void)0)
#define DfuCase_ClientRegister(tsk)                                                                 ((void)0)
#define DfuCase_HandleLibOperation(id, message)                                                     ((void)0)
#define DfuCase_HandleDfuMode()                                                                     ((void)0)
#define DfuCase_HandleTransportDisconnect()                                                         ((void)0)
#define DfuCase_Abort()                                                                             ((void)0)
#define DfuCase_IsDfuNeeded(void)                                                                   ((bool)0)
#define DfuCase_IsEarbudsInCaseRequested(void)                                                      ((bool)0)

#endif /* INCLUDE_DFU_CASE */

#endif /* DFU_CASE_H_ */
