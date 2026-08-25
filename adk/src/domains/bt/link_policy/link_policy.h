/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       link_policy.h
\brief      Header file for the Link policy manager.

            This module controls the settings for classic Bluetooth links, selecting
            the link parameters based on the current activity.
*/

#ifndef LINK_POLICY_H_
#define LINK_POLICY_H_

#include "connection_abstraction.h"
#include <connection.h>


/*! Power table indexes */
typedef enum lp_power_table_index
{
    /*! Power table used when idle and one handset connected */
    POWERTABLE_IDLE,
    /*! Power table used when idle and two handsets are connected */
    POWERTABLE_MULTIPOINT_IDLE,
    /*! Power table used when the BR/EDR ACL is idle and the controller is also receiving a broadcast */
    POWERTABLE_IDLE_WITH_BROADCAST,
    /*! Power table used when VA is active */
    POWERTABLE_VA_ACTIVE,
    /*! Power table used when only DFU is active */
    POWERTABLE_DFU,
    /*! Power table used when A2DP streaming */
    POWERTABLE_A2DP_STREAMING,
    /*! Power table used when SCO active */
    POWERTABLE_SCO_ACTIVE,
    /*! Must be the final value */
    POWERTABLE_UNASSIGNED,
} lpPowerTableIndex;

/*! Link policy state per ACL connection, stored for us by the connection manager. */
typedef struct
{
    lpPowerTableIndex pt_index;     /*!< Current powertable in use */
} lpPerConnectionState;


/*! \brief Callback functions that can be used to adjust parameters requested
           for connections.

    Connection parameters are selected by the link policy component. The callbacks
    give an opportunity for the valid values to be varied... but only within the 
    range of values already selected by link policy.
 */
typedef struct
{
    /*! Function (callback) that can vary the Bluetooth Low Energy (LE) connection 
        interval to be requested.

        The callback is used when selecting a new set of parameters. The
        parameter update may be triggered from the link policy module if 
        all links are checked.

        \paramp[in] tpaddr The address of the connection being updated
        \param[in,out] min_interval the minimum connection interval requested
        \param[in,out] max_interval the maximum connection interval requested

        \returns Return TRUE if parameters changed, FALSE otherwise */
    bool (*LeParams)(const tp_bdaddr *tpaddr, uint16 *min_interval, uint16 *max_interval);

    /*! Function (callback) that can vary the classic Bluetooth parameters.

        The callback is used when selecting a new set of parameters. The
        parameter update may be triggered from the link policy module if 
        all links are checked.

        The callback function supplied is passed the range of the sniff 
        interval in slots (0.625ms) and should adjust within that range.

        \paramp[in] tpaddr The address of the connection being updated
        \param[in,out] min_interval the minimum sniff interval requested
        \param[in,out] max_interval the maximum sniff interval requested

        \returns Return TRUE if parameters changed, FALSE otherwise */
    bool (*BredrParams)(const tp_bdaddr *tpaddr, uint16 *min_interval, uint16 *max_interval);
} link_policy_parameter_callbacks_t;


bool appLinkPolicyInit(Task init_task);

/*! @brief Update the link policy of connected handsets based on the system state.
    @param bd_addr The Bluetooth address of the handset whose state has changed.
    This may be set NULL if the state change is not related to any single handset.
    This address is not currently used, but may be used in the future.
*/
void appLinkPolicyUpdatePowerTable(const bdaddr *bd_addr);

/*! @brief Force update the link policy of connected handsets based on the system state.
    @param bd_addr The Bluetooth address of the handset whose state has changed.
    This may be set NULL if the state change is not related to any single handset.
    This address is not currently used, but may be used in the future.
*/
void appLinkPolicyForceUpdatePowerTable(const bdaddr *bd_addr);

/*! Handler for connection library messages not sent directly

    This function is called to handle any connection library messages sent to
    the application that the link policy module is interested in. If a message
    is processed then the function returns TRUE.

    \note Some connection library messages can be sent directly as the
        request is able to specify a destination for the response.

    \param  id              Identifier of the connection library message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool appLinkPolicyHandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled);

/*! \brief Link policy handles the opening of an ACL by discovering the link
           role. If the local device is central, a role switch will be
           requested to become peripheral.
    \param ind The indication.
*/
void appLinkPolicyHandleClDmAclOpenedIndication(const bdaddr *bd_addr, bool is_ble, bool is_local);

/*! \brief Make changes to link policy following an address swap.
*/
void appLinkPolicyHandleAddressSwap(void);

/*! @brief Force update the handset link policy on the new primary after handover.
    @param bd_addr The Bluetooth address of the handset that has just been
                   handed-over.
*/
void appLinkPolicyHandoverForceUpdateHandsetLinkPolicy(const bdaddr *bd_addr);


/*! \brief Register callbacks to allow LE and BREDR parameters to be adjusted.

    \param[in] callback        Function to accept or reject incoming connections
*/  
void LinkPolicy_SetParameterCallbacks(link_policy_parameter_callbacks_t *callback);


#endif /* LINK_POLICY_H_ */
