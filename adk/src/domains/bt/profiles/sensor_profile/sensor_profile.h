/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   sensor_profile Sensor profile
\ingroup    profiles
\brief      Public interface for Sensor profile.

            This module manages the Sensor profile used in TWM devices for controlling and
            synchronization of spatial audio sensor data between earbuds.
*/

#ifdef INCLUDE_SENSOR_PROFILE

/*! \addtogroup sensor_profile
 @{
*/

#ifndef SENSOR_PROFILE_H_
#define SENSOR_PROFILE_H_

#include "domain_message.h"
#include <message_.h>
#include <bdaddr_.h>
#include <vm.h>


/*! \brief Events sent by Sensor profile to client. */
typedef enum
{
    /*! Module initialisation complete */
    SENSOR_PROFILE_INIT_CFM = SENSOR_PROFILE_MESSAGE_BASE,

    /*! Confirmation of client registration with Sensor profile.
    Sent to both Primary and Secondary earbuds.
    This message is delivered to the
    client_task registered with Sensor profile via the #SensorProfile_Register(Task) call. */
    SENSOR_PROFILE_CLIENT_REGISTERED,

    /*! Confirmation of Sensor profile connection.
    Sent to both Primary and Secondary earbuds.
    This message is delivered to both
    client_task registered with Sensor profile via the #SensorProfile_Register(Task) call and
    connect_task registered with Sensor profile via the #SensorProfile_Connect(Task, const bdaddr*) call. */
    SENSOR_PROFILE_CONNECTED,

    /*! Confirmation of Sensor profile disconnection.
    Sent to both Primary and Secondary earbuds.
    This message is delivered to the both
    client_task registered with Sensor profile via the #SensorProfile_Register(Task) call
    and disconnect_task registered with Sensor profile via the #SensorProfile_Disconnect(Task) call. */
    SENSOR_PROFILE_DISCONNECTED,

    /*! Indication that Sensor data has been received.
    It can be received in both Primary and Secondary earbuds.
    The associated message will contain the Source from which the client must read
    the sensor data sent by its peer's client. Sensor profile itself does not extract
    any data from the Source. */
    SENSOR_PROFILE_DATA_RECEIVED,

    /*! Indication that more sensor data can be sent via the the Sensor profile link.
    It can be received in both Primary and Secondary earbuds.
    This message is delivered to the client_task registered with Sensor profile via
    the SensorProfile_Register(Task) call.
    The Sensor profile client will received this message when the peer earbud is ready
    to receive data. This allows the client to manage the link's throughput without filling
    up the sink and gives the client fine control of the flow of the time sensitive sensor data.
    The client shall wait for this notification before scheduling the next read of sensor data
    to be sent via the the Sensor profile link. Note that to avoid exiting subrating mode, the
    client must obtain the next_subrate_clock instant by calling SensorProfile_NextSniffClock(),
    and schedule the next read to be sent via the link a few ms prior to that instant (~5ms). */
    SENSOR_PROFILE_SEND_DATA,

    /*! Enable request to peer including configuration and control data.
    Sent from Primary to Secondary, it will be received only in Secondary earbud.
    This message is delivered to the client_task registered with Sensor profile via
    the SensorProfile_Register(Task) call.
    The Secondary peer earbud should enable the necessary procedures to acquire the sensor
    data that will be shared with the Primary earbud via the Sensor profile connection. */
    SENSOR_PROFILE_ENABLE_PEER_REQ,

    /*! Disable request sent to peer.
    Sent from Primary to Secondary, it will be received only in Secondary earbud.
    The Secondary peer earbud should stop reading and sharing sensor data with the
    Primary earbud. */
    SENSOR_PROFILE_DISABLE_PEER_REQ,

    /*! Enabled Status confirmation sent from Secondary peer earbud to Primary earbud.
    This message is delivered to the client_task registered with Sensor profile via
    the SensorProfile_Register(Task) call. */
    SENSOR_PROFILE_PEER_ENABLED,

    /*! Disabled Status confirmation sent from Secondary peer earbud to Primary earbud.
    This message is delivered to the client_task registered with Sensor profile via
    the SensorProfile_Register(Task) call. */
    SENSOR_PROFILE_PEER_DISABLED,

    /*! This must be the final message */
    SENSOR_PROFILE_MESSAGE_END
} sensor_profile_msg_t;

/*! \brief Confirmation message with the result of the registration with Sensor profile request
           initiated when SensorProfile_Register(Task) is invoked in each earbud.*/
typedef struct
{
    /*! Status of the registration request. */
    bool status;
} SENSOR_PROFILE_CLIENT_REGISTERED_T;

/*! \brief Status codes used by Sensor profile. */
typedef enum
{
    /*! Sensor profile connected to peer. */
    sensor_profile_status_peer_connected = 0,

    /*! Unable to connect Sensor profile with peer. */
    sensor_profile_status_peer_connect_failed,

    /*! Sensor profile disconnected. */
    sensor_profile_status_peer_disconnected
} sensor_profile_status_t;

/*! \brief Confirmation message with the result of a Sensor profile connection request initiated
           when #SensorProfile_Connect(Task, const bdaddr*) is invoked in the Primary earbud.*/
typedef struct
{
    /*! Status of the connection request. */
    sensor_profile_status_t status;

    /*! Sink for the stream associated to this connection. */
    Sink sink;
} SENSOR_PROFILE_CONNECTED_T;

/*! \brief Disconnect reasons. */
typedef enum
{
    /*! The disconnection request was successful.*/
    sp_disconnect_successful,

    /*! Unsolicited disconnection due to L2CAP layer link loss. */
    sp_disconnect_l2cap_link_loss,

    /*! The disconnection request failed.*/
    sp_disconnect_error
} sp_disconnect_status_with_reason_t;

/*! \brief Message used to convey the result of a disconnection request
           initiated when SensorProfile_Disconnect(Task) in invoked,
           or as a result of an unsolicited disconnection indication. */
typedef struct
{
    /*! Reason of the disconnection. */
    sp_disconnect_status_with_reason_t reason;

    /*! Status of the disconnection. */
    sensor_profile_status_t status;
} SENSOR_PROFILE_DISCONNECTED_T;

/*! \brief Message indicating that sensor data has been received from the peer earbud. */
typedef struct
{
    /*! Source for the stream associated to this profile's connection.
        Sensor profile does not remove any data from this source.
        The client must read the sensor data from this source when
        receiving this message. */
    Source source;
} SENSOR_PROFILE_DATA_RECEIVED_T;

/*! \brief Message indicating that Sensor data can be sent to the peer earbud. */
typedef struct
{
    /*! Sink for the stream associated to this connection. */
    Sink sink;
} SENSOR_PROFILE_SEND_DATA_T;

/*! \brief Sensor data processing source. */
typedef enum
{
    /*! Processing of data has not been set. */
    SENSOR_PROFILE_PROCESSING_INVALID,

    /*! Processing of data will take place locally. */
    SENSOR_PROFILE_PROCESSING_LOCAL,

    /*! Processing of data will take place in remote device. */
    SENSOR_PROFILE_PROCESSING_REMOTE,
} sensor_profile_processing_source_t;

/*! \brief Configuration data received in Secondary earbud when Primary has invoked
           #SensorProfile_EnablePeer(uint16, sensor_profile_processing_source_t, uint16)
           to control and configure sensor behaviour in Secondary earbud.
           When receiving this message the application in the Secondary earbud will enable
           spatial audio and will start reading motion sensor data in the Secondary earbud.
*/
typedef struct
{
    /*! Sensor interval to configure application. */
    uint16 sensor_interval;

    /*! Sensor data processing source.*/
    sensor_profile_processing_source_t processing_source;

    /*! Report value requested by the application or remote device. */
    uint16 report_id;
} SENSOR_PROFILE_ENABLE_PEER_REQ_T;

/*! \brief Confirmation of configuration received in Primary earbud after Secondary earbud
           has invoked #SensorProfile_PeerEnabled(uint16, sensor_profile_processing_source_t)
           to confirm completion of sensor behaviour configuration.
*/
typedef struct
{
    /*! Sensor interval being used to configure the application. */
    uint16 sensor_interval;

    /*! Sensor data processing source being used to configure the application.*/
    sensor_profile_processing_source_t processing_source;
} SENSOR_PROFILE_PEER_ENABLED_T;

/*! \brief Initialise the sensor_profile module.
    A Sensor profile channel will be registered with the Peer Signalling protocol
    which will be used by the Sensor profile to send control messages between
    the peered earbuds.

    \param task The init task to send SENSOR_PROFILE_INIT_CFM to.

    \return TRUE if initialisation is in progress; FALSE if it failed.
*/
bool SensorProfile_Init(Task task);

/*! \brief Called by the application in both Primary and Secondary earbuds to
    register its Task with the Sensor profile.
    A SensorProfile L2CAP channel will be established and the SensorProfile SDP
    record will be entered in the SDP database.

    A #SENSOR_PROFILE_CLIENT_REGISTERED message will be sent to the client's
    client_task being registered with the Sensor profile.

    \param client_task The task of the client application/module using this profile.

    \return TRUE if registration succeeded; FALSE if it failed.
*/
bool SensorProfile_Register(Task client_task);

/*! \brief Connect Sensor profile.
    This function should ONLY be called on a Primary earbud, typically by Topology
    procedure on earbud becoming Primary. It will set the role of the
    SensorProfile to "Primary" and it will handle the L2CAP establishment
    procedure to create the L2CAP link that will be used by this profile.

    A #SENSOR_PROFILE_CONNECTED message will be sent to the registered client_task
    and the connect_task tasks in both Primary and Secondary earbuds to inform
    about the outcome of this request. This message will also be sent immediately
    with not further action if the profile is already Connected.

    \param connect_task The task that requested the connect request.
    \param peer_addr The address of the remote earbud device to connect to.
*/
void SensorProfile_Connect(Task connect_task, const bdaddr *peer_addr);

/*! \brief Disconnect Sensor profile.
    This function will handle the L2CAP tearing down procedure for the L2CAP link assigned
    to this profile.

    A #SENSOR_PROFILE_DISCONNECTED message will be sent to the registered and the
    disconnect_task tasks in both Primary and Secondary earbuds to inform about
    the outcome of this request. This message will also be sent immediately
    with not further action if the profile is already Disconnected.

    \param disconnect_task The task that requested the disconnect request.
*/
void SensorProfile_Disconnect(Task disconnect_task);

/*! \brief Enable and configure Sensor profile in Secondary peer requested from Primary.
    This function should ONLY be called in the Primary earbud to configure and control sensor
    behaviour in Secondary earbud.

    A #SENSOR_PROFILE_ENABLE_PEER_REQ message will be received in the Secondary earbud.
    A #SENSOR_PROFILE_PEER_ENABLED message will be sent to the Primary earbud to inform
    about the outcome of this request.

    \param sensor_interval The sensor sampling interval as configured by the driver when spatial audio
                           is enabled (in hz).
    \param processing_source The sensor data processing source, that could be either local or remote.

    \param report_id Actual transmition type value to communicate with remote device.
                           Only relevant when processing_source is "remote".

    \return TRUE if sending the request succeeded; FALSE if it failed.
*/
bool SensorProfile_EnablePeer(uint16 interval,
                              sensor_profile_processing_source_t processing_source,
                              uint16 report_id);

/*! \brief Disable Sensor profile in Secondary peer requested from Primary.
    This function should ONLY be called in a Primary earbud.

    A #SENSOR_PROFILE_DISABLE_PEER_REQ message will be received in the Secondary earbud.
    A #SENSOR_PROFILE_PEER_DISABLED message will be sent to the Primary earbud to inform
    about the outcome of this request.


    \return TRUE if sending the request succeeded; FALSE if it failed.
*/
bool SensorProfile_DisablePeer(void);

/*! \brief Confirmation that spatial audio functionality has been enabled in Secondary earbud's application.
    This function should ONLY be called in a Secondary earbud.

    A #SENSOR_PROFILE_PEER_ENABLED message will be sent to the Primary earbud.

    \param sensor_interval The sensor sampling interval as configured by the driver when spatial audio
                           is enabled (in hz).
    \param processing_source The sensor data processing source, that could be either local or remote.

    \return TRUE if sending the confirmation succeeded; FALSE if it failed.
*/
bool SensorProfile_PeerEnabled(uint16 sensor_interval,
                               sensor_profile_processing_source_t processing_source);

/*! \brief Confirmation that spatial audio functionality has been disabled in the Secondary earbud.
    This function should ONLY be called in a Secondary earbud.

    A #SENSOR_PROFILE_PEER_DISABLED message will be sent to the Primary earbud.

    \return TRUE if sending the confirmation succeeded; FALSE if it failed.
*/
bool SensorProfile_PeerDisabled(void);

/*! \brief Return the system clock value in us for the next Sniff Subrating instant.
 *
    A next_sniff_clock_t structure will be returned with the next Sniff and Sniff Subrating
    instants, the sniff interval, and the tx and rx subrates values.

    This function must be used by the client to avoid exiting subrating mode by obtaining
    the next_subrate_clock instant and scheduling its next read of sensor data to be sent
    via the Sensor profile link a few miliseconds prior to that instant (~5ms).

    \param next_sniff Pointer to the memory to write sniff clock information if successful.
    \return TRUE on success, else FALSE.
*/
bool SensorProfile_NextSniffClock(next_sniff_clock_t *next_sniff);


/*! \brief Inform sensor profile of current device Primary/Secondary role.

    \param primary TRUE to set Primary role, FALSE to set Secondary role.
*/
void SensorProfile_SetRole(bool primary);

/*! \brief Query the Sensor profile's current role.

    \return TRUE if Primary, FALSE if Secondary.
*/
bool SensorProfile_IsRolePrimary(void);

#endif /* SENSOR_PROFILE_H_ */

/*! @} End group documentation in Doxygen */

#endif /* INCLUDE_SENSOR_PROFILE */
