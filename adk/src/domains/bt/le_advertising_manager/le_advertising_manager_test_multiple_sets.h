/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for LE advertising manager test APIs to demonstrate the use of multiple advertising sets
*/

#ifndef LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS_H_
#define LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS_H_

void LeAdvertisingManager_MultipleSetsEnableAdvertisingMultipleSets(bool enable);
bool LeAdvertisingManager_MultipleSetsRegisterSet(uint8 set_id);
bool LeAdvertisingManager_MultipleSetsConfigureParamsForSet(uint8, uint16, uint32, uint32, uint16);
bool LeAdvertisingManager_MultipleSetsConfigureAddressForSet(uint8 set_id);
bool LeAdvertisingManager_MultipleSetsConfigureDataForSet(uint8 set_id);
bool LeAdvertisingManager_MultipleSetsEnableSet(uint8 set_id, bool enable);
bool LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(uint8 *min, uint8 *max);

#endif
