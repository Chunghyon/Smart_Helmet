/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service config
*/

#include "handset_service_config.h"
#include "handset_service_feature_multipoint.h"
#include "handset_service_pddu.h"
#include "handset_service_protected.h"
#include "handset_service.h"

#include <connection_manager.h>
#include <device_properties.h>
#include <device_db_serialiser.h>

#if defined(INCLUDE_GAIA)
#include "handset_service_gaia_plugin.h"
#endif
#include "context_framework.h"

#include <device_list.h>

#include <panic.h>

#if defined(ENABLE_CONNECTION_BARGE_IN) && defined(MULTIPOINT_BARGE_IN_ENABLED)
#error "MULTIPOINT_BARGE_IN_ENABLED incompatible with ENABLE_CONNECTION_BARGE_IN"
#endif

const handset_service_config_t handset_service_multipoint_config =
{
    /* Two connections supported */
    .max_bredr_connections = 2,
#ifdef ENABLE_LE_MULTIPOINT
    /* Two LE connections supported */
    .max_le_connections = 2,
#else
    /* Only one LE connection supported */
    .max_le_connections = 1,
#endif
    /* Two ACL reconnection attempts per supported connection */
    .acl_connect_attempt_limit = 2,
    /* Page interval of 500ms */
    .initial_page_interval_ms = handsetService_BredrAclConnectRetryDelayMs(),
    .enable_unlimited_acl_reconnection = FALSE,
    .unlimited_reconnection_page_interval_ms = 30 * MS_PER_SEC,
    /* Page timeout 5 seconds*/
    .page_timeout_slots = MS_TO_BT_SLOTS(5 * MS_PER_SEC),
#ifdef ENABLE_CONNECTION_BARGE_IN
    .enable_connection_barge_in = TRUE
#else
    .enable_connection_barge_in = FALSE
#endif
};

const handset_service_config_t handset_service_singlepoint_config =
{
    /* One connection supported */
    .max_bredr_connections = 1,
    /* Only one LE connection supported */
    .max_le_connections = 1,
    /* Three ACL reconnection attempts per supported connection */
    .acl_connect_attempt_limit = 3,
    /* Page interval of 500ms */
    .initial_page_interval_ms = handsetService_BredrAclConnectRetryDelayMs(),
    .enable_unlimited_acl_reconnection = FALSE,
    .unlimited_reconnection_page_interval_ms = 30 * MS_PER_SEC,
    /* Page timeout 10 seconds*/
    .page_timeout_slots = MS_TO_BT_SLOTS(10 * MS_PER_SEC),
#ifdef ENABLE_CONNECTION_BARGE_IN
    .enable_connection_barge_in = TRUE
#else
    .enable_connection_barge_in = FALSE
#endif
};

static handset_service_config_t *handsetService_GetConfig(void)
{
    device_t device = BtDevice_GetSelfDevice();
    if(device)
    {
        handset_service_config_t *config;
        size_t size;
        if(Device_GetProperty(device, device_property_handset_service_config, (void **)&config, &size))
        {
            PanicFalse(size == sizeof(handset_service_config_t));
            return config;
        }
    }

    return NULL;
}

uint8 handsetService_LeAclMaxConnections(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->max_le_connections;
    }
    return 1;
}

bool handsetService_IsUnlimitedAclReconnectionEnabled(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->enable_unlimited_acl_reconnection;
    }
    return FALSE;
}

bool handsetService_SetUnlimitedAclReconnectionEnabled(bool enabled)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        if(config->enable_unlimited_acl_reconnection == enabled)
        {
            return TRUE;
        }

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.enable_unlimited_acl_reconnection = enabled;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

uint32 HandsetService_CalculatePageIntervalMsValue(uint8 page_interval_config)
{
    return (1 << page_interval_config) * 500;
}

uint32 handsetService_GetUnlimitedReconnectionPageInterval(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->unlimited_reconnection_page_interval_ms;
    }
    return handsetService_BredrAclConnectRetryDelayMs();
}


bool handsetService_SetUnlimitedReconnectionPageInterval(uint32 page_interval_ms)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        if(config->unlimited_reconnection_page_interval_ms == page_interval_ms)
        {
            return TRUE;
        }

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.unlimited_reconnection_page_interval_ms = page_interval_ms;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

uint32 handsetService_GetPageInterval(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->initial_page_interval_ms;
    }
    return handsetService_BredrAclConnectRetryDelayMs();
}


bool handsetService_SetPageInterval(uint32 page_interval_ms)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        if(config->initial_page_interval_ms == page_interval_ms)
        {
            return TRUE;
        }

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.initial_page_interval_ms = page_interval_ms;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

uint16 handsetService_GetPageTimeout(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->page_timeout_slots;
    }
    return MS_TO_BT_SLOTS(5 * MS_PER_SEC);
}

bool handsetService_SetPageTimeout(uint16 page_timeout_ms)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        // Convert to slots and limit to max number of 16 bit slots
        uint32 page_timeout_bt_slots = MS_TO_BT_SLOTS(page_timeout_ms);
        page_timeout_bt_slots = MIN(USHRT_MAX, page_timeout_bt_slots);

        if(config->page_timeout_slots == page_timeout_bt_slots)
        {
            return TRUE;
        }

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.page_timeout_slots = page_timeout_bt_slots;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

uint8 handsetService_BredrAclConnectAttemptLimit(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->acl_connect_attempt_limit;
    }

    return 1;
}

bool handsetService_SetBredrAclConnectAttemptLimit(uint8 num_attempts)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        if(config->acl_connect_attempt_limit == num_attempts)
        {
            return TRUE;
        }

        /* Range check the requested number of reconnection attempts */
        num_attempts = MIN(7, num_attempts);

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.acl_connect_attempt_limit = num_attempts;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

uint8 handsetService_BredrAclMaxConnections(void)
{
    if (HandsetService_FeatureMultipointAllowed())
    {
        handset_service_config_t *config = handsetService_GetConfig();
        if(config)
        {
            return config->max_bredr_connections;
        }
    }

    return 1;
}

bool handsetService_IsConnectionBargeInEnabled(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->enable_connection_barge_in;
    }

    return FALSE;
}

static bool handsetService_MaxBredrAclContextCallback(unsigned * context_data, uint8 context_data_size)
{
    PanicZero(context_data_size >= sizeof(context_maximum_connected_handsets_t));
    memset(context_data, 0, sizeof(context_maximum_connected_handsets_t));
    *(context_maximum_connected_handsets_t *)context_data = handsetService_BredrAclMaxConnections();
    return TRUE;
}

bool handsetService_SetConnectionBargeInEnable(bool enabled)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        if(config->enable_connection_barge_in == enabled)
        {
            return TRUE;
        }

        handset_service_config_t new_config;
        memcpy(&new_config, config, sizeof(handset_service_config_t));
        new_config.enable_connection_barge_in = enabled;
        return HandsetService_Configure(new_config);
    }
    return FALSE;
}

void HandsetServiceConfig_Init(void)
{
    HandsetServicePddu_CheckToImportBtDeviceHandsetServiceConfig();

    handsetService_HandleConfigUpdate();
    ContextFramework_RegisterContextProvider(context_maximum_connected_handsets, handsetService_MaxBredrAclContextCallback);

    /* Handle situation when the SELF device is already created, but device_property_handset_service_config wasn't yet set.
       That is expected on first boot of non-earbud applications.
    */
    device_t device = BtDevice_GetSelfDevice();
    if(device)
    {
        if(!Device_IsPropertySet(device, device_property_handset_service_config))
        {
            handsetService_HandleSelfCreated();
        }
    }
}

bool HandsetService_Configure(handset_service_config_t config)
{
    if(config.max_bredr_connections > HANDSET_SERVICE_MAX_PERMITTED_BREDR_CONNECTIONS)
    {
        return FALSE;
    }

    if(config.max_bredr_connections < 1)
    {
        return FALSE;
    }
    
#ifdef MULTIPOINT_BARGE_IN_ENABLED
    /* Connection barge-in is not compatible with the legacy version. 
       Attempts to enable connection barge-in when MULTIPOINT_BARGE_IN_ENABLED
       will fail. MULTIPOINT_BARGE_IN_ENABLED should only be used where truncated 
       page scan is not supported. */
    if(config.enable_connection_barge_in)
    {
        return FALSE;
    }
#endif

    ConManager_SetPageTimeout(config.page_timeout_slots);

    device_t device = BtDevice_GetSelfDevice();
    if(device)
    {
        Device_SetProperty(device, device_property_handset_service_config, &config, sizeof(config));
        DeviceDbSerialiser_SerialiseDevice(device);
    }

#if defined(INCLUDE_GAIA)
    if(config.max_bredr_connections > 1)
    {
        HandsetServicegGaiaPlugin_MultipointEnabledChanged(TRUE);
    }
    else
    {
        HandsetServicegGaiaPlugin_MultipointEnabledChanged(FALSE);
    }
#endif /* INCLUDE_GAIA */

    HandsetService_FeatureMultipointUpdateState(&config);

    return TRUE;
}

void HandsetService_SetDefaultConfig(void *value, uint8 size)
{
    PanicFalse(size == sizeof(handset_service_config_t));
#ifdef ENABLE_MULTIPOINT
    memcpy(value, &handset_service_multipoint_config, size);
#else
    memcpy(value, &handset_service_singlepoint_config, size);
#endif
}

void handsetService_HandleSelfCreated(void)
{
#ifdef ENABLE_MULTIPOINT
    HandsetService_Configure(handset_service_multipoint_config);
#else
    HandsetService_Configure(handset_service_singlepoint_config);
#endif

    device_t device = BtDevice_GetSelfDevice();
    if(device)
    {
        DeviceDbSerialiser_SerialiseDevice(device);
    }
}

void handsetService_HandleConfigUpdate(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        ConManager_SetPageTimeout(config->page_timeout_slots);

        HandsetService_FeatureMultipointUpdateState(config);
        ContextFramework_NotifyContextUpdate(context_maximum_connected_handsets);
    }
}

void HandsetService_ConfigureLinkLossReconnectionParameters(
        bool use_unlimited_reconnection_attempts,
        uint8 num_connection_attempts,
        uint32 reconnection_page_interval_ms,
        uint16 reconnection_page_timeout_ms)
{
    handsetService_SetBredrAclConnectAttemptLimit(num_connection_attempts);
    handsetService_SetUnlimitedReconnectionPageInterval(reconnection_page_interval_ms);
    handsetService_SetPageTimeout(reconnection_page_timeout_ms);
    handsetService_SetUnlimitedAclReconnectionEnabled(use_unlimited_reconnection_attempts);
}
