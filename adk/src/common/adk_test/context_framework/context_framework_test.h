/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef CONTEXT_FRAMEWORK_TEST_H_
#define CONTEXT_FRAMEWORK_TEST_H_

/*! \brief Retrieve physical state through context framework

    \return TRUE if context item was retrieved, else FALSE;
*/
bool ContextFrameworkTest_GetPhysicalState(void);

/*! \brief Retrieve maximum handsets through context framework

    \return TRUE if context item was retrieved, else FALSE;
*/
bool ContextFrameworkTest_GetMaximumHandsets(void);

/*! \brief Retrieve connected handsets info through context framework

    \return TRUE if context item was retrieved, else FALSE;
*/
bool ContextFrameworkTest_GetConnectedHandsetsInfo(void);

/*! \brief Retrieve active source through context framework

    \return TRUE if context item was retrieved, else FALSE;
*/
bool ContextFrameworkTest_GetActiveSourceInfo(void);

#endif /* CONTEXT_FRAMEWORK_TEST_H_ */
