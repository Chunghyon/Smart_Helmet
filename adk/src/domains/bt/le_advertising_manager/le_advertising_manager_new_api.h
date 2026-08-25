/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   le_advertising_manager_new_api LE Advertising
\ingroup    bt_domain
\brief      Header file for Bluetooth Low Energy advertising functionality

Provides control for Bluetooth Low Energy (BLE) advertisements.
*/

#ifndef LE_ADVERTISING_MANAGER_NEW_API_H_
#define LE_ADVERTISING_MANAGER_NEW_API_H_

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "domain_message.h"

#include <connection.h>
#include <connection_no_ble.h>


/*! Size of a data element header in advertising data

      * Octet[0]=length,
      * Octet[1]=Tag
      * Octets[2]..[n]=Data
*/
#define AD_DATA_LENGTH_OFFSET       (0)
#define AD_DATA_TYPE_OFFSET         (1)
#define AD_DATA_HEADER_SIZE         (2)

/*! \brief Data type to specify the supported advertising config sets */
typedef enum
{
    le_adv_advertising_config_set_1,
    le_adv_advertising_config_set_2,
    le_adv_advertising_config_set_3,
    le_adv_advertising_config_set_max = le_adv_advertising_config_set_3,
    le_adv_advertising_config_set_total
} le_adv_advertising_config_set_t;

/*! \brief Data type to specify the preset advertising interval for advertising events */
typedef enum
{
    le_adv_preset_advertising_interval_slow,
    le_adv_preset_advertising_interval_fast,
    le_adv_preset_advertising_interval_max = le_adv_preset_advertising_interval_fast,
    le_adv_preset_advertising_interval_total,
    le_adv_preset_advertising_interval_invalid = 0xFF
} le_adv_preset_advertising_interval_t;

/*! \brief Parameter config entry */
typedef struct
{
    le_adv_preset_advertising_interval_t set_default;
    uint16 timeout_fallback_in_seconds;
} le_adv_parameters_config_entry_t;

/*! \brief Parameter config table with entries, one for each tuple [index_set_default, timeout_fallback]) */
typedef struct
{
    le_adv_parameters_config_entry_t row[le_adv_advertising_config_set_total];
} le_adv_parameters_config_table_t;

/*! \brief Data type for API function return values to indicate success/error status */
typedef enum{
    le_adv_mgr_status_success,
    le_adv_mgr_status_error_unknown
} le_adv_mgr_status_t;

/*! \brief Common LE advertising parameters */
typedef struct
{
    uint16 le_adv_interval_min;
    uint16 le_adv_interval_max;
} le_adv_parameters_interval_t;

/*! \brief Parameter set (one for each type (slow/fast)) */
typedef struct
{
    le_adv_parameters_interval_t set_type[le_adv_preset_advertising_interval_total];
} le_adv_parameters_set_t;

/*! \brief LE advertising parameter sets (slow and fast parameters only) */
typedef struct
{
    const le_adv_parameters_set_t *sets;
    const le_adv_parameters_config_table_t *table;
} le_adv_parameters_t;

/*! \brief Opaque type for LE Advertising Manager advertising item object. */
struct _le_adv_item;

/*! \brief Handle to LE Advertising Manager advertising item object. */
typedef struct _le_adv_item * le_adv_item_handle;

/*! \brief Data structure for each item's advertising data */
typedef struct
{
    unsigned size;
    const uint8 * data;
} le_adv_item_data_t;

#define ADV_EVENT_BITS_CONNECTABLE      0x01
#define ADV_EVENT_BITS_SCANNABLE        0x02
#define ADV_EVENT_BITS_DIRECTED         0x04
#define ADV_EVENT_BITS_HIGH_DUTY_CYCLE  0x08
#define ADV_EVENT_BITS_USE_LEGACY_PDU   0x10
#define ADV_EVENT_BITS_OMIT_ADDRESS     0x20

/*! \brief Data type used to specify a data items advertising type */
typedef enum
{
    le_adv_type_legacy_connectable_scannable = (ADV_EVENT_BITS_CONNECTABLE | ADV_EVENT_BITS_SCANNABLE | ADV_EVENT_BITS_USE_LEGACY_PDU),
    le_adv_type_legacy_non_connectable_scannable = (ADV_EVENT_BITS_SCANNABLE | ADV_EVENT_BITS_USE_LEGACY_PDU),
    le_adv_type_legacy_non_connectable_non_scannable = ADV_EVENT_BITS_USE_LEGACY_PDU,
    le_adv_type_legacy_directed = (ADV_EVENT_BITS_DIRECTED | ADV_EVENT_BITS_USE_LEGACY_PDU),
    le_adv_type_legacy_anonymous = (ADV_EVENT_BITS_OMIT_ADDRESS | ADV_EVENT_BITS_USE_LEGACY_PDU),
    le_adv_type_extended_connectable = ADV_EVENT_BITS_CONNECTABLE,
    le_adv_type_extended_scannable = ADV_EVENT_BITS_SCANNABLE,
    le_adv_type_extended_anonymous = ADV_EVENT_BITS_OMIT_ADDRESS,
} le_adv_data_type_t;

#define leAdvertisingManager_DataTypeIsConnectable(type) (((type) & ADV_EVENT_BITS_CONNECTABLE) == ADV_EVENT_BITS_CONNECTABLE)

/*! \brief Data type to define the packet types where the data item is aimed at
    le_adv_item_data_placement_advert : Place the item into advert
    le_adv_item_data_placement_scan_response : Place the item into scan response */
typedef enum
{
    le_adv_item_data_placement_advert,
    le_adv_item_data_placement_scan_response
} le_adv_item_data_placement_t;

/*! \brief Data structure to provide optional parameter information for the advertising data item
    \param  override_connectable_state : Client to set this value to TRUE if it needs LE advertising item to ignore its internal logic
            when deciding to advertise the registered advertising item, FALSE otherwise.
    \param  needs_own_set : Client to set this value to TRUE if advertising item needs to be placed into its own advertising set
            separate from other items, FALSE otherwise.
    \param  type : the type of advert specified by the client
    \param  placement : Client can specify whether to place the data into an advertising or scan response packet providing the value for the input parameter of type le_adv_item_data_attributes_t
    \param  size : the size of advertising data being provided by the client */
typedef struct
{
    bool override_connectable_state:1;
    bool needs_own_set:1;
    le_adv_data_type_t type:6;
    le_adv_item_data_placement_t placement:2;
    bool include_if_connectable:1;
    unsigned data_size:16;
} le_adv_item_info_t;


/*! \brief Data structure for each advertising item's advertising parameters.

    \param  primary_adv_interval_min    Minimum advertising interval.
                                        N = 0x20 to 0xFFFFFF  (Time = N * 0.625 ms)
    \param  primary_adv_interval_max    Maximum advertising interval.
                                        N = 0x20 to 0xFFFFFF  (Time = N * 0.625 ms)
    \param  primary_adv_channel_map     Bit mask, if set will be used. (bit 0 = Channel 37, bit 1 = Channel 38 and bit 2 = Channel 39)
    \param  own_addr_type
            0 - Public (Next 2 primitive fields not used)
            1 - Random (Next 2 primitive fields not used)
            2 - Resolvable Private Address/Public
            3 - Resolvable Private Address/Random
    \param  peer_tpaddr   Used to find the local IRK in the resolving list.
                        0xYYXXXXXXXXXXXX
                        YY = 0 - Public Device Address, 1 - Random Device Address
                        XX = 6 octet address
    \param  adv_filter_policy   How to process scan and connect requests.
                                0 - Process scan and connect requests from all devices.
                                1 - Process connect requests from all devices and scan requests from devices on the white list.
                                2 - Process scan requests from all devices and connect requests from devices on the white list.
                                3 - Process scan and connect requests from devices on the white list.
    \param  primary_adv_phy     The phy to transmit the advertising packets on the primary advertising channels.
                                1 - LE 1M
                                3 - LE Coded
    \param  secondary_adv_max_skip  0x0 to 0xFF - Maximum advertising events on the primary advertising channel that can be skipped before sending an AUX_ADV_IND.
    \param  secondary_adv_phy   The phy to transmit the advertising packets on the secondary advertising channel (e.g. AUX PDUs).
                                1 - LE 1M
                                2 - LE 2M
                                3 - LE Coded
    \param  adv_sid     A value in the range 0 to 15 that can be used by a scanner to identify desired extended advertising from a device (On air will be in the ADI field).
*/
typedef struct
{
    uint32 primary_adv_interval_min;
    uint32 primary_adv_interval_max;
    uint8 primary_adv_channel_map;
    uint8 own_addr_type;
    typed_bdaddr peer_tpaddr;
    uint8 adv_filter_policy;
    uint16 primary_adv_phy;
    uint8 secondary_adv_max_skip;
    uint16 secondary_adv_phy;
    uint16 adv_sid;
} le_adv_item_params_t;

/*! \brief Data type for the message identifiers */
typedef enum
{
    LE_ADV_MGR_ENABLE_CONNECTABLE_CFM = ADV_MANAGER_MESSAGE_BASE,
    LE_ADV_MGR_ALLOW_ADVERTISING_CFM,

    /*! This must be the final message */
    ADV_MANAGER_MESSAGE_END
} le_adv_mgr_message_id_t;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_ENABLE_CONNECTABLE_CFM */
typedef struct
{
    le_adv_mgr_status_t status;
    bool enable;
} LE_ADV_MGR_ENABLE_CONNECTABLE_CFM_T;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_ALLOW_ADVERTISING_CFM */
typedef struct
{
    le_adv_mgr_status_t status;
    bool allow;
} LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T;

/*! \brief Initialise LE Advertising Manager
    \param init_task Task to send init completion message to
    \return TRUE to indicate successful initialisation, FALSE otherwise.
*/
bool LeAdvertisingManager_Init(Task init_task);

/*! \brief Public API to allow/disallow LE advertising events
    \param[in] task Task to send confirmation message
    \param[in] allow Boolean value, TRUE for allow operation, FALSE for disallow operation.
    \return TRUE to indicate successful LE advertising allow operation, FALSE otherwise.
    \return Sends LE_ADV_MGR_ALLOW_ADVERTISING_CFM message to the task
*/
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow);

/*! \brief Public API to enable/disable connectable LE advertising events
    \param[in] task Task to send confirmation message
    \param[in] enable Boolean value, TRUE for enable connectable operation, FALSE for disable connectable operation.
    \return TRUE to indicate successful LE advertising enable connectable operation, FALSE otherwise.
    \return Sends LE_ADV_MGR_ENABLE_CONNECTABLE_CFM message to the task
*/
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable);

/*! \brief Register the parameters to be used by LE advertising manager.
    \param[in] params Pointer to LE advertising parameters
    \return TRUE for a successful call to API for parameters register operation, FALSE otherwise.
*/
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params);

/*! \brief Select the use of set in the array of LE advertising parameters.
    param[in] index The index to activate
    \return TRUE for a successful call to API for parameters select operation, FALSE otherwise.
*/
bool LeAdvertisingManager_ParametersSelect(uint8 index);

/*! \brief Data structure to specify the callback functions for the registered item */
typedef struct
{
    /*! \brief Callback function to be implemented by the client to return the registered advertising item's data.
        \param[out] data Size and valid pointer to the buffer for the advertising item's data.
        \return TRUE if the client returns valid data for the item, FALSE otherwise. */
    bool (*GetItemData)(le_adv_item_data_t * data);

    /*! \brief Callback function to be implemented by the client to free the dynamically allocated memory for the
        registered advertising item's data.
        Client can set this to NULL if memory for the advertising item's data is not dynamically allocated. */
    void (*ReleaseItemData)(void);

    /*! \brief Callback function to be implemented by the client to return the registered advertising item's additional info.
        \param[out] info Pointer to the parameters info for additional information on the advertising item
        \return TRUE if the client returns valid info for the item, FALSE otherwise. */
    bool (*GetItemInfo)(le_adv_item_info_t *info);

    /*! \brief Optional callback function to be implemented by the client to return the registered advertising item's parameters
        \param[out] params Pointer to the parameters for LE advertising item
        \return TRUE if the client returns valid parameters and info for the item, FALSE otherwise. */
    bool (*GetItemParameters)(le_adv_item_params_t * params);
} le_adv_item_callback_t;

/*! \brief Public API for the clients to register callback functions for an advertising item
    \param[in] task The task to receive messages from LE advertising manager when advertising states change
    \param[in] callback Pointer to the data structure of type le_adv_data_callback_t to specify function pointers for LE Advertising Manager to use to collect the data items to be advertised
    \return Valid pointer to the handle of type le_adv_item_handle, NULL otherwise.
*/
le_adv_item_handle LeAdvertisingManager_RegisterAdvertisingItemCallback(Task task, const le_adv_item_callback_t * callback);

/*! \brief Public API to update advertising item's data and parameters
    \param[in] handle Handle returned from the call to the API LeAdvertisingManager_RegisterAdvertisingItem().
    \return TRUE for a successful call to API for update of advertising item, FALSE otherwise.
*/
bool LeAdvertisingManager_UpdateAdvertisingItem(const le_adv_item_handle handle);

/*! \brief Make the advertising item with the given handle advertised exclusively, by disabling advertising of all other advertising items.
    Only one  advertising item can be assigned exclusive use at a time. Once an item is assigned exclusivity, subsequent requests to make another item exclusive will be rejected, until the first item calls the API again with a FALSE flag to release exclsuive use.denied
    \param[in] handle The handle for the advertising item
    \param[in] enable TRUE to make exclusive use of the advertising item, FALSE to release exclusive use
    \return TRUE if item can be made exclusive, FALSE otherwise. */
bool LeAdvertisingManager_MakeAdvertisingItemExclusive(const le_adv_item_handle handle, bool enable);

/*! \brief Public API to populate client advertising parameters with default values provided by the advertising manager
    \param[in] params_to_populate pointer to structure to populate.
    \return TRUE if params were populated successfully, else FALSE
*/
bool LeAdvertisingManager_PopulateDefaultAdvertisingParams(le_adv_item_params_t * params_to_populate);


// typedefs to aid compilation while LEAM internals are still to be modified
typedef le_adv_item_handle le_adv_mgr_register_handle;


#endif /* #ifdef LE_ADVERTISING_MANAGER_NEW_API */

#endif /* LE_ADVERTISING_MANAGER_NEW_API_H_ */
