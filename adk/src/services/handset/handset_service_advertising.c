/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "handset_service_advertising.h"

#include "handset_service_protected.h"
#include "handset_service_config.h"
#include "handset_service_sm.h"
#include "le_advertising_manager.h"

static inline bool handsetService_IsConnectableAdvertisingAllowed(void)
{
    return (HandsetService_IsBleConnectable()
            && (HandsetServiceSm_GetLeAclConnectionCount() < handsetService_LeAclMaxConnections()));
}

bool HandsetService_UpdateAdvertising(void)
{
    return LeAdvertisingManager_EnableConnectableAdvertising(NULL, handsetService_IsConnectableAdvertisingAllowed());
}

