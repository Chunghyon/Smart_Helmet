/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sink service types and data to be used within sink_service only
*/

#ifndef SINK_SERVICE_PRIVATE_H
#define SINK_SERVICE_PRIVATE_H

#include <domain_message.h>
#include <task_list.h>
#include <bdaddr.h>
#include <bt_device.h>


#define SINK_SERVICE_CLIENT_TASKS_LIST_INIT_CAPACITY 1
#define SINK_SERVICE_MAX_SM      4

/*! \brief Internal messages for the sink_service */
typedef enum
{
    /*! Request to connect to a handset */
    SINK_SERVICE_INTERNAL_CONNECT_REQ = INTERNAL_MESSAGE_BASE,

    /*! Request to disconnect a handset */
    SINK_SERVICE_INTERNAL_DISCONNECT_REQ,

    /*! Delivered when an ACL connect request has completed. */
    SINK_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,

    /*! Request to cancel any in-progress connect to SINK. */
    SINK_SERVICE_INTERNAL_CONNECT_STOP_REQ,

    /*! Request to re-try the ACL connection after a failure. */
    SINK_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ,

    /*! Timeout message to clear the possible pairing flag for an SM */
    SINK_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT,

    /*! Request to connect profiles */
    SINK_SERVICE_INTERNAL_CONNECT_PROFILES_REQ,

    /*! This must be the final message */
    SINK_SERVICE_INTERNAL_MESSAGE_END
} sink_service_internal_msg_t;

ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(SINK_SERVICE_INTERNAL_MESSAGE_END)

/*! \brief Sink Service state machine states */
typedef enum
{
    SINK_SERVICE_STATE_DISABLED     = 0,        /*!< disabled, no connections, or pairing allowed */
    SINK_SERVICE_STATE_DISCONNECTED = 1,        /*!< enabled, but not connected to a device */
    SINK_SERVICE_STATE_PAIRING,                 /*!< pairing to a device */
    SINK_SERVICE_STATE_CONNECTING_BREDR_ACL,    /*!< connecting the ACL */
    SINK_SERVICE_STATE_CONNECTING_PROFILES,     /*!< waiting for profiles to connect */
    SINK_SERVICE_STATE_CONNECTED,               /*!< one or more profiles are connected */
} sink_service_state_t;

/*! \brief Context for an instance of a sink service state machine */
typedef struct
{
    /*! Task for this instance. */
    TaskData task_data;

    /*! Current state */
    sink_service_state_t state;

    /*! Device instance this state machine represents */
    device_t sink_device;

    /*! device address for the device that the instance is holding an ACL for */
    bdaddr acl_hold_addr;


    /*! Bitmask of profiles that have been requested 
        \note This is needed to record if profiles connect and disconnect
              during the profile connection timeout */
    uint32 profiles_requested;

} sink_service_state_machine_t;

/*! sink service data */
typedef struct
{
    /*! Init's local task */
    TaskData task;

    /*! List of clients */
    TASK_LIST_WITH_INITIAL_CAPACITY(SINK_SERVICE_CLIENT_TASKS_LIST_INIT_CAPACITY) clients;

    /*! Flag to set if the sink service should pair when there is no device in device list */
    bool pairing_enabled;

    /*! Flag to set if there is a request to Disable the Sink Service. */
    bool disable_request_pending;

    /*! Flag to set if there is a pending pairing request. */
    bool pairing_request_pending;

    /*! Sink Service state machine */
    sink_service_state_machine_t state_machines[SINK_SERVICE_MAX_SM];

} sink_service_data_t;

extern sink_service_data_t sink_service_data;

/*! Get pointer to Sink Service task*/
static inline Task SinkService_GetTask(void)
{
    return &sink_service_data.task;
}

/*! Get pointer to Sink Service data structure */
static inline sink_service_data_t * SinkService_GetTaskData(void)
{
    return &sink_service_data;
}

/*! Get pointer to Sink Service client list */
static inline task_list_flexible_t * SinkService_GetClientList(void)
{
    return (task_list_flexible_t *)&sink_service_data.clients;
}

/* Macro for iterating through all SM instances in the sink service */
#define FOR_EACH_SINK_SM(_sm) for(sink_service_state_machine_t *_sm = sink_service_data.state_machines;\
                                     _sm < &sink_service_data.state_machines[SINK_SERVICE_MAX_SM];\
                                     _sm++)

#endif // SINK_SERVICE_PRIVATE_H
