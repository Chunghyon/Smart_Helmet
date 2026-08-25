/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine instance header.
*/

#ifndef SINK_SERVICE_SM_H_
#define SINK_SERVICE_SM_H_

#include <device.h>
#include <message.h>
#include <bdaddr.h>
#include <connection_manager.h>
#include "sink_service_protected.h"

/*! \brief Clear the data for an SM instance and reset it

    \param sm The SM to reset
*/
void SinkServiceSm_ClearInstance(sink_service_state_machine_t *sm);

/*! \brief Request a SM instance attempts to connect to a device
           If there is no suitable device in the device list then pairing will begin
           If there is an MRU that will be used. If no MRU then the first sink device
           in the device list will be used.

    \param sm The SM to connect
    \return TRUE if the request was successfully made
*/
bool SinkServiceSm_ConnectRequest(sink_service_state_machine_t *sm);

/*! \brief Request a SM instance to pair to a new device
           This will start rssi pairing to search for an suitable candidate

    \param sm The SM to start pairing on
    \return TRUE if the request was successfully made
*/
bool SinkServiceSm_PairRequest(sink_service_state_machine_t *sm);

/*! \brief Request a SM instance attempts to disconnect all profiles from its device.
           This function will disconnect all configured profiles from the SM device

    \param sm The SM to disconnect
    \return TRUE if the request was successfully made
*/
bool SinkServiceSm_DisconnectRequest(sink_service_state_machine_t *sm);

/*! \brief Request a SM instance handle an incomming connection indication

    \param sm The SM to handle the message
    \param ind The indication to handle
*/
void SinkServiceSm_HandleConManagerBredrTpConnectInd(sink_service_state_machine_t *sm,
                                                     const CON_MANAGER_TP_CONNECT_IND_T *ind);

/*! \brief Have max supported BR/EDR connections been established?

    \return TRUE if max BR/EDR connections reached, otherwise FALSE
*/
bool SinkServiceSm_MaxBredrAclConnectionsReached(void);

/*! \brief Get the number of BR/EDR ACL connections
    \return Number of BR/EDR ACL connections.
*/
unsigned SinkServiceSm_GetBredrAclConnectionCount(void);

/*! \brief Set a device for the SM instance

    \param sm The SM set the device for
    \param device The device to set
*/
void SinkServiceSm_SetDevice(sink_service_state_machine_t *sm, device_t device);

/*! \brief Create a new instance of a sink state machine.

    This will return NULL if a new state machine cannot be created,
    for example if the maximum number of sinks already exists.

    \param device Device to create state machine for.

    \return Pointer to new state machine, or NULL if it couldn't be created.
*/
sink_service_state_machine_t *sinkServiceSm_CreateSm(device_t device);

/*! \brief Try to find an active sink state machine for a device_t.

    \param device Device to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
sink_service_state_machine_t *sinkServiceSm_GetSmForDevice(device_t device);

/*! \brief Try to find an active BR/EDR sink state machine for an address.

    \param[in] addr BR/EDR address to search for.

    \return Pointer to the matching state machine, or NULL if no match.
*/
sink_service_state_machine_t *sinkServiceSm_GetSmForBredrAddr(const bdaddr *addr);

/*! \brief Retreive the existing instance or create a new sink service statemachine instance
           for the requested bluetooth transport address

    \note If the transport is not TRANSPORT_BREDR_ACL then an instance cannot be created
          and this function will return NULL.

    \param tp_addr Address to find and instance for or create an instance for.
    \return Pointer to the found or created statemachine instance. This will be
            NULL if an instance was not found and an instance could not be created.
            This might happen if the maximum number of BR/EDR connection has been reached.
*/
sink_service_state_machine_t *sinkServiceSm_FindOrCreateSm(const tp_bdaddr *tp_addr);

/*! \brief Transition the Sink Service State Machine from DISCONNECTED to DISABLED */
void SinkServiceSm_DisableAll(void);

/*! \brief Transition the Sink Service State Machine from DISABLED to DISCONNECTED */
void SinkServiceSm_EnableAll(void);

#endif /* SINK_SERVICE_SM_H_ */
