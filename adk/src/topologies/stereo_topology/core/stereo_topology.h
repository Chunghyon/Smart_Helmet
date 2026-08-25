/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   stereo Topology
\ingroup    topologies       
\brief      Stereo topology public interface.
*/

#ifndef STEREO_TOPOLOGY_H_
#define STEREO_TOPOLOGY_H_

#include "domain_message.h"
#include "handset_service.h"

/*! \brief Confirmation of completed handset disconnect request

    This message is sent once handset_topology service has completed the disconnect,
    or it has been cancelled.
*/
typedef struct
{
    /*! Status of the request */
    handset_service_status_t status;
} STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND_T;

/*! Definition of messages that Stereo Topology can send to clients. */
typedef enum
{
    /*! Indication to clients that handset have been disconnected. */
    STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND = STEREO_TOPOLOGY_MESSAGE_BASE,
    STEREO_TOPOLOGY_STOP_CFM,

    /*! This must be the final message */
    STEREO_TOPOLOGY_MESSAGE_END
} stereo_topology_message_t;
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(STEREO_TOPOLOGY, STEREO_TOPOLOGY_MESSAGE_END)

/*! Definition of status code returned by Stereo Topology. */
typedef enum
{
    /*! The operation has been successful */
    stereo_topology_status_success,

    /*! The requested operation has failed. */
    stereo_topology_status_fail,
} stereo_topology_status_t;


/*! Definition of the #STEREO_TOPOLOGY_STOP_CFM message. */
typedef struct 
{
    /*! Result of the #StereoTopology_Stop() operation. 
        If this is not stereo_topology_status_success then the topology was not
        stopped cleanly within the time requested */
    stereo_topology_status_t       status;
} STEREO_TOPOLOGY_STOP_CFM_T;


/*! \brief Initialise the  Stereo topology component

    \param init_task    Task to send init completion message (if any) to

    \returns TRUE
*/
bool StereoTopology_Init(Task init_task);


/*! \brief Start the  Stereo topology

    The topology will run semi-autonomously from this point.

    \param requesting_task Task to send messages to

    \returns TRUE
*/
bool StereoTopology_Start(Task requesting_task);


/*! \brief Register client task to receive  Stereo topology messages.
 
    \param[in] client_task Task to receive messages.
*/
void StereoTopology_RegisterMessageClient(Task client_task);


/*! \brief Unregister client task from Stereo topology.
 
    \param[in] client_task Task to unregister.
*/
void StereoTopology_UnRegisterMessageClient(Task client_task);


/*! \brief function to prohibit or allow connection to handset in  Stereo topology.

    Prohibits or allows topology to connect handset. When prohibited any connection attempt in progress will
    be cancelled and any connected handset will be disconnected.

    Note: By default handset connection is allowed.

    \param prohibit TRUE to prohibit handset connection, FALSE to allow.
*/
void StereoTopology_ProhibitHandsetConnection(bool prohibit);


/*! \brief Stop the Stereo topology

    The topology will enter a known clean state then send a message to 
    confirm.

    The device should be restarted after the STEREO_TOPOLOGY_STOP_CFM message
    is sent.

    \param requesting_task Task to send the response to

    \return TRUE
*/
bool StereoTopology_Stop(Task requesting_task);


/*! \brief Request topology to connect the most recently used handset.
*/
void StereoTopology_ConnectMruHandset(void);

/*! \brief Request topology to disconnect the least recently used handset.
*/
void StereoTopology_DisconnectLruHandset(void);


/*! \brief Request topology disconnect all handsets.
    \note This currently only disconnects one connected handset.
    \todo This functionality will need to be updated when multiple
          handsets connections are supported (multipoint)
*/
void StereoTopology_DisconnectAllHandsets(void);

#endif /* STEREO_TOPOLOGY_H_ */
