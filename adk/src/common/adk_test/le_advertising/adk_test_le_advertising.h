/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_le_advertising
\brief      Interface for LE advertising related testing functions.
*/

/*! @{ */

#ifndef ADK_TEST_LE_ADVERTISING_H
#define ADK_TEST_LE_ADVERTISING_H

#ifdef LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS

void appTestLeAdvertising_MultipleSetsEnableAdvertisingMultipleSets(bool enable);

bool appTestLeAdvertising_MultipleSetsRegisterSet(uint8 set_id);

bool appTestLeAdvertising_MultipleSetsConfigureParamsForSet(uint8, uint16, uint32, uint32, uint16);

bool appTestLeAdvertising_MultipleSetsConfigureAddressForSet(uint8 set_id);

bool appTestLeAdvertising_MultipleSetsConfigureDataForSet(uint8 set_id);

bool appTestLeAdvertising_MultipleSetsEnableSet(uint8 set_id, bool enable);

void appTestLeAdvertising_MultipleSetsInfo(uint32 task);

void appTestLeAdvertising_MultipleSetsStartDemo(void);

void appTestLeAdvertising_MultipleSetsStopDemo(void);

#endif /* LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS */

#endif /* ADK_TEST_LE_ADVERTISING_H */

/*! @} */

