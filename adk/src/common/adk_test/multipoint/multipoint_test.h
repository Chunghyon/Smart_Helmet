/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for common multipoint specifc testing functions.
*/

/*! @{ */

#ifndef MULTIPOINT_TEST_H
#define MULTIPOINT_TEST_H

/*! \brief Enable Multipoint so two BRERDR connections are allowed at a time. */
void MultipointTest_EnableMultipoint(void);

/*! \brief Disable Multipoint so only one BR/EDR connection is allowed at a time. */
void MultipointTest_DisableMultipoint(void);

/*! \brief To check if Multipoint is enabled. */
bool MultipointTest_IsMultipointEnabled(void);

/*! \brief Find the number of connected handsets.

    \return The current number of connected handsets
*/
uint8 MultipointTest_NumberOfHandsetsConnected(void);

/*! \brief Enable or disable A2DP barge in behaviour 

    \param enable Enable or disable barge in
*/
void MultipointTest_EnableA2dpBargeIn(bool enable);

/*! \brief Check if A2DP Barge In behaviour is enabled

    \return TRUE if barge in behaviour is enabled
            FALSE if barge in behaviour is disabled
*/
bool MultipointTest_IsA2dpBargeInEnabled(void);

#endif /* MULTIPOINT_TEST_H */

/*! @} */

