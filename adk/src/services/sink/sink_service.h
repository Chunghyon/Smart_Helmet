/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      A service that provides procedures for connecting and managing
            connections to sink(s).

            The sink service is intended to be used with applications that act as a source of audio
            such as a USB Dongle or Bluetooth transmitter. Its responsibility is to provide connections
            and connect profiles to sink devices such as headphones, earbuds and speakers.

            If there are no paired devices, the sink service will utilise the RSSI Pairing manager with
            configured settings to seek out candidate sink devices.

            Once a device is paired or an already paired device is retrieved from the device list, the
            sink service will create a BR/EDR ACL connection with the device before using the profile
            manager to connect the configured profiles.

            The Sink Service will retain control of the ACL connection until it receives confirmation
            of successful or failed profile connections, at which point it will release control of the ACL.

            When Disabled, the service will disconnect from all devices, and move to a non-connectable state, 
			and all connect requests will be ignored.
*/

#ifndef SINK_SERVICE_H_
#define SINK_SERVICE_H_

#include <message.h>

#include <bdaddr.h>
#include <bt_device.h>

#include "domain_message.h"
#include "rssi_pairing.h"
#include "profile_manager.h"

/*!< \brief Default config define for the maximum number of simultaneous connections
            the Sink Service can support which can be used in sink_service_config_t
            in the application.
     \note This value might not be used in the application.
           In this case, changing this value will have no impact. */
#define SINK_SERVICE_CONFIG_BREDR_CONNECTIONS_MAX 4




/*! \brief Configuration of sink service. */
typedef struct
{
    /*! A bitmask for the supported profiles for sink devices */
    uint32 supported_profile_mask;

    /*! The list of profiles with the order determining the connect order */
    const profile_t * profile_list;

    /*! The length of the profile list */
    size_t profile_list_length;

    /*! The parameters that should be passed to the rssi pairing manager when
        discovering and pairing new devices */
    const rssi_pairing_parameters_t * rssi_pairing_params;

    /*! The maximum number of active bredr connections that the sink service supports */
    uint8 max_bredr_connections;


    /*! The page timeout (seconds) for connection requests. */
    unsigned page_timeout;

} sink_service_config_t;

/*! This is the config for the sink service. It should be defined in the application */
extern const sink_service_config_t sink_service_config;

/*! \brief Status codes for the sink service. */
typedef enum
{
    sink_service_status_success,
    sink_service_status_failed,
    sink_service_status_cancelled,
    sink_service_status_no_mru,
    sink_service_status_connected,
    sink_service_status_disconnected,
    sink_service_status_link_loss,
} sink_service_status_t;

/*! \brief Events sent by the sink service to other modules. */
typedef enum
{
    /*! Module initialisation complete */
    SINK_INIT_CFM = SINK_SERVICE_MESSAGE_BASE,
    SINK_SERVICE_CONNECTED_CFM,
    SINK_SERVICE_DISCONNECTED_CFM,
    
    /*! The first profile has been connected by the profile manager */
    SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND,

    /*! This must be the final message */
    SINK_SERVICE_MESSAGE_END
} sink_service_msg_t;

/*! \brief Sink Service UI Provider contexts */
typedef enum
{
    context_sink_connected,
    context_sink_connecting,
    context_sink_disconnected,
    context_sink_pairing,
    context_sink_disabled
} sink_service_context_t;

/*! \brief Definition of the #SINK_SERVICE_CONNECTED_CFM_T message content */
typedef struct
{
    /*! Connected Device. */
    device_t              device;

    /*! Status of the request */
    sink_service_status_t status;
} SINK_SERVICE_CONNECTED_CFM_T;

/*! \brief Definition of the #SINK_SERVICE_DISCONNECTED_CFM_T message content */
typedef struct
{
    /*! Disconnected Device. */
    device_t              device;
} SINK_SERVICE_DISCONNECTED_CFM_T;

/*! \brief Definition of the #SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND_T message content */
typedef struct
{
    /*! Device connecting profile. */
    device_t              device;
} SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND_T;

/*! \brief Initialise the sink_service module.

    \param task The init task to send SINK_SERVICE_INIT_CFM to.

    \return TRUE if initialisation is in progress; FALSE if it failed.
*/
bool SinkService_Init(Task task);

/*! \brief Register a Task to receive notifications from sink_service.

    Once registered, #client_task will receive #sink_msg_t messages.

    \param client_task Task to register to receive sink_service notifications.
*/
void SinkService_ClientRegister(Task client_task);

/*! \brief Enable Pairing in the Sink Service.

    If pairing is not enabled and there are no devices. A call to Connect will do nothing

    \param enable True if enable.
*/
void SinkService_EnablePairing(bool enable);

/*! \brief Disable the Sink Service.

    When disabled the sink service will hold it's state and not handle any incomming messages
*/
void SinkService_Disable(void);

/*! \brief Enable the Sink Service.

    When enabled the sink service will react to messages and ui inputs.

    \note The function itself does not perform any actions within in the sink service or
          notify the state machine instances. This simple stops all of the instances from
          reacting to messages and events.
*/
void SinkService_Enable(void);


/*! \brief Request the Sink Service to Connect to a device

    If there are no devices in the device list then this will start pairing (using RSSI pairing)
    If there is a device in the device list it will either use the MRU or the first device in the list.
    After pairing or creating a BREDR connection. The configured profiles will be connected.
    SINK_SERVICE_CONNECTED_CFM should be expected when a connection is successful.

    \note The Sink Service only supports initiating connection to a single device.

    \return True if the request was successfully made.
*/
bool SinkService_Connect(void);

/*! \brief Request the Sink Service to Disconnect from all devices

    This will request all instances to disconnect from their devices.

    \return True if the request was successfully made.
*/
void SinkService_DisconnectAll(void);

/*! \brief Un-register a Task that is receiving notifications from sink_service.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from handet_service notifications.
*/
void SinkService_ClientUnregister(Task client_task);

/*! \brief Check if any of the sink service instances are in the connected state

    \return TRUE if an instance is connected
*/
bool SinkService_IsConnected(void);

/*! \brief Enable BR/EDR connections */
void SinkService_ConnectableEnableBredr(bool enable);
#endif /* SINK_SERVICE_H_ */
