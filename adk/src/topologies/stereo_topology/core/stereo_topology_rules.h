/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Rules for stereo topology
*/

#ifndef _STEREO_TOPOLOGY_RULES_H_
#define _STEREO_TOPOLOGY_RULES_H_

#include "stereo_topology_rule_events.h"
#include "stereo_topology_private.h"

#include <rules_engine.h>

#include <message.h>


enum stereo_topology_goals
{
    STEREOTOP_GOAL_CONNECTABLE_HANDSET = STEREOTOP_INTERNAL_RULE_MSG_BASE,
    STEREOTOP_GOAL_CONNECT_HANDSET,
    STEREOTOP_GOAL_ALLOW_HANDSET_CONNECT,
    STEREOTOP_GOAL_DISCONNECT_HANDSET,
    STEREOTOP_GOAL_ALLOW_LE_CONNECTION,
    STEREOTOP_GOAL_SYSTEM_STOP,
    STEREOTOP_GOAL_DISCONNECT_LRU_HANDSET,
    STEREOTOP_GOAL_NOP,
};

typedef struct
{
    uint32 profiles;
} STEREOTOP_GOAL_DISCONNECT_HANDSET_PROFILES_T;

typedef struct
{
    bool enable;
} STEREOTOP_GOAL_CONNECTABLE_HANDSET_T;

typedef struct
{
    bool allow;
} STEREOTOP_GOAL_ALLOW_HANDSET_CONNECT_T;

typedef struct
{
    bool allow;
} STEREOTOP_GOAL_ALLOW_LE_CONNECTION_T;

typedef struct
{
    bool link_loss;
} STEREOTOP_GOAL_CONNECT_HANDSET_T;


/*! \brief Stereo Topology rules task data. */
typedef struct
{
    rule_set_t rule_set;
} stereo_topology_rules_task_data_t;

/*! \brief Initialise the connection rules module. */
bool StereoTopologyRules_Init(Task init_task);

/*! \brief Get handle on the rule set, in order to directly set/clear events.
    \return rule_set_t The rule set.
 */
rule_set_t StereoTopologyRules_GetRuleSet(void);

/*! \brief Set an event or events
    \param[in] event Events to set that will trigger rules to run
    This function is called to set an event or events that will cause the relevant
    rules in the rules table to run.  Any actions generated will be sent as message
    to the client_task
*/
void StereoTopologyRules_SetEvent(rule_events_t event_mask);

/*! \brief Reset/clear an event or events
    \param[in] event Events to clear
    This function is called to clear an event or set of events that was previously
    set. Clear event will reset any rule that was run for event.
*/
void StereoTopologyRules_ResetEvent(rule_events_t event);

/*! \brief Get set of active events
    \return The set of active events.
*/
rule_events_t StereoTopologyRules_GetEvents(void);

/*! \brief Mark rules as complete from messaage ID
    \param[in] message Message ID that rule(s) generated
    This function is called to mark rules as completed, the message parameter
    is used to determine which rules can be marked as completed.
*/
void StereoTopologyRules_SetRuleComplete(MessageId message);

/*! \brief Mark rules as complete from message ID and set of events
    \param[in] message Message ID that rule(s) generated
    \param[in] event Event or set of events that trigger the rule(s)
    This function is called to mark rules as completed, the message and event parameter
    is used to determine which rules can be marked as completed.
*/
void StereoTopologyRules_SetRuleWithEventComplete(MessageId message, rule_events_t event);

#endif /* _STEREO_TOPOLOGY_RULES_H_ */
