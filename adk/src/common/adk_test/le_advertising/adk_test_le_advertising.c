/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of LE advertising related testing functions.
*/

#include "adk_test_le_advertising.h"
#include "le_advertising_manager.h"
#include "le_advertising_manager_test_multiple_sets.h"
#include <logging.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

#ifdef LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS

typedef enum{
    APP_TEST_LE_ADV_MSG_REGISTER_SET,
    APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS,
    APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS,
    APP_TEST_LE_ADV_MSG_CONFIGURE_DATA,
    APP_TEST_LE_ADV_MSG_ENABLE_SET,
    APP_TEST_LE_ADV_MSG_DISABLE_SET,
    APP_TEST_LE_ADV_MSG_MAX = APP_TEST_LE_ADV_MSG_DISABLE_SET
}app_test_le_adv_messages;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_REGISTER_SET_T;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS_T;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS_T;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_CONFIGURE_DATA_T;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_ENABLE_SET_T;

typedef struct{
    uint8 set_id;
}APP_TEST_LE_ADV_MSG_DISABLE_SET_T;

static void appTestLeAdvertisingMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case APP_TEST_LE_ADV_MSG_REGISTER_SET:
        {
            APP_TEST_LE_ADV_MSG_REGISTER_SET_T * m = (APP_TEST_LE_ADV_MSG_REGISTER_SET_T*)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsRegisterSet(m->set_id));
            break;
        }

        case APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS:
        {
            APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS_T * m = (APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS_T*)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsConfigureParamsForSet(m->set_id, DEFAULT_ADV_EVENT_PROPERTIES, DEFAULT_ADV_INTERVAL_MIN, DEFAULT_ADV_INTERVAL_MAX, DEFAULT_SID));
            break;
        }

        case APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS:
        {
            APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS_T * m = (APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS_T*)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsConfigureAddressForSet(m->set_id));
            break;
        }

        case APP_TEST_LE_ADV_MSG_CONFIGURE_DATA:
        {
            APP_TEST_LE_ADV_MSG_CONFIGURE_DATA_T * m = (APP_TEST_LE_ADV_MSG_CONFIGURE_DATA_T *)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsConfigureDataForSet(m->set_id));
            break;
        }

        case APP_TEST_LE_ADV_MSG_ENABLE_SET:
        {
            APP_TEST_LE_ADV_MSG_ENABLE_SET_T * m = (APP_TEST_LE_ADV_MSG_ENABLE_SET_T *)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsEnableSet(m->set_id,TRUE));
            break;
        }

        case APP_TEST_LE_ADV_MSG_DISABLE_SET:
        {
            APP_TEST_LE_ADV_MSG_DISABLE_SET_T * m = (APP_TEST_LE_ADV_MSG_DISABLE_SET_T *)message;
            DEBUG_LOG_VERBOSE("App Test Handle Le Advertising Message with Id %x for Set %x", id, m->set_id);
            PanicFalse(appTestLeAdvertising_MultipleSetsEnableSet(m->set_id,FALSE));
            break;
        }
    }
}

static TaskData app_test_le_advertising_task = {appTestLeAdvertisingMessageHandler};

#define MESSAGE_DELAY_PER_SET 200

#define REGISTER_MESSAGE_DELAY 0
#define CONFIGURE_PARAMS_MESSAGE_DELAY 1000
#define CONFIGURE_ADDRESS_MESSAGE_DELAY 2000
#define CONFIGURE_DATA_MESSAGE_DELAY 3000
#define ENABLE_SET_MESSAGE_DELAY 4000

static void appTestLeAdvertising_RegisterSets(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_REGISTER_SET_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_REGISTER_SET, msg, (set_id*MESSAGE_DELAY_PER_SET) + REGISTER_MESSAGE_DELAY );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_REGISTER_SET, set_id);
    }
}

static void appTestLeAdvertising_ConfigureSetsParams(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS, msg, (set_id*MESSAGE_DELAY_PER_SET) + CONFIGURE_PARAMS_MESSAGE_DELAY );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_CONFIGURE_PARAMS, set_id);
    }
}

static void appTestLeAdvertising_ConfigureSetsAddress(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS, msg, (set_id*MESSAGE_DELAY_PER_SET) + CONFIGURE_ADDRESS_MESSAGE_DELAY );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_CONFIGURE_ADDRESS, set_id);
    }
}

static void appTestLeAdvertising_ConfigureSetsData(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_CONFIGURE_DATA_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_CONFIGURE_DATA, msg, (set_id*MESSAGE_DELAY_PER_SET) + CONFIGURE_DATA_MESSAGE_DELAY );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_CONFIGURE_DATA, set_id);
    }
}

static void appTestLeAdvertising_EnableSets(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_ENABLE_SET_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_ENABLE_SET, msg, (set_id*MESSAGE_DELAY_PER_SET) + ENABLE_SET_MESSAGE_DELAY );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_ENABLE_SET, set_id);
    }
}

static void appTestLeAdvertising_DisableSets(void)
{
    uint8 min = 0;
    uint8 max = 0;
    LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(&min,&max);

    for(int set_id=min; set_id<=max; set_id++)
    {
        MESSAGE_MAKE(msg, APP_TEST_LE_ADV_MSG_DISABLE_SET_T);
        msg->set_id = set_id;
        MessageSendLater(&app_test_le_advertising_task, APP_TEST_LE_ADV_MSG_DISABLE_SET, msg, (set_id*MESSAGE_DELAY_PER_SET) );
        DEBUG_LOG_VERBOSE("App Test Send Le Advertising Message with Id %x for Set %x", APP_TEST_LE_ADV_MSG_DISABLE_SET, set_id);
    }
}

void appTestLeAdvertising_MultipleSetsStartDemo(void)
{
    appTestLeAdvertising_MultipleSetsEnableAdvertisingMultipleSets(TRUE);
    appTestLeAdvertising_RegisterSets();
    appTestLeAdvertising_ConfigureSetsParams();
    appTestLeAdvertising_ConfigureSetsAddress();
    appTestLeAdvertising_ConfigureSetsData();
    appTestLeAdvertising_EnableSets();
}

void appTestLeAdvertising_MultipleSetsStopDemo(void)
{
    appTestLeAdvertising_DisableSets();
}

void appTestLeAdvertising_MultipleSetsEnableAdvertisingMultipleSets(bool enable)
{
    LeAdvertisingManager_MultipleSetsEnableAdvertisingMultipleSets(enable);
}

bool appTestLeAdvertising_MultipleSetsRegisterSet(uint8 set_id)
{
    return LeAdvertisingManager_MultipleSetsRegisterSet(set_id);
}

bool appTestLeAdvertising_MultipleSetsConfigureParamsForSet(uint8 set_id, uint16 events, uint32 interval_min, uint32 interval_max, uint16 sid)
{
    return LeAdvertisingManager_MultipleSetsConfigureParamsForSet(set_id, events, interval_min, interval_max, sid);
}

bool appTestLeAdvertising_MultipleSetsConfigureAddressForSet(uint8 set_id)
{
    return LeAdvertisingManager_MultipleSetsConfigureAddressForSet(set_id);
}

bool appTestLeAdvertising_MultipleSetsConfigureDataForSet(uint8 set_id)
{
    return LeAdvertisingManager_MultipleSetsConfigureDataForSet(set_id);
}

bool appTestLeAdvertising_MultipleSetsEnableSet(uint8 set_id, bool enable)
{
    return LeAdvertisingManager_MultipleSetsEnableSet(set_id, enable);
}

void appTestLeAdvertising_MultipleSetsInfo(uint32 task)
{
    ConnectionDmBleExtAdvSetsInfoReq((Task)task);
}

#endif /* LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS */





