/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "stereo_topology_rules.h"
#include "stereo_topology_goals.h"

#include <bt_device.h>
#include <device_properties.h>
#include <device_list.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <handset_service_sm.h>
#include <av.h>

#include <logging.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed. */

#define STEREOTOP_RULE_LOG         DEBUG_LOG
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t stereoTopologyRules_CopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
 
    Copies the parameters/data into the rules instance where the rules engine 
    can use it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   stereoTopologyRules_CopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define StereoTopologyRulesGetTaskData()  (&stereo_topology_rules_task_data)

stereo_topology_rules_task_data_t stereo_topology_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */

DEFINE_RULE(ruleStereoTopEnableConnectableHandset);
DEFINE_RULE(ruleStereoTopHandsetLinkLossReconnect);
DEFINE_RULE(ruleStereoTopEnableConnectableHandset);
DEFINE_RULE(ruleStereoTopAllowHandsetConnect);
DEFINE_RULE(ruleStereoTopAutoConnectHandset);
DEFINE_RULE(ruleStereoTopAllowLEConnection);
DEFINE_RULE(ruleStereoTopDisconnectHandset);
DEFINE_RULE(ruleStereoTopStop);
DEFINE_RULE(ruleStereoTopUserRequestConnectHandset);
DEFINE_RULE(ruleStereoTopDisconnectLruHandset);

/*! \} */

/*! \brief STEREO Topology rules deciding behaviour.
*/
const rule_entry_t stereotop_rules_set[] =
{
    /*! When we are shutting down, disconnect everything. */
    RULE(STEREOTOP_RULE_EVENT_STOP,                       ruleStereoTopStop,                     STEREOTOP_GOAL_SYSTEM_STOP),
    /* Upon link-loss of BREDR, Connect the stereo back to the previously connected handset */
    RULE(STEREOTOP_RULE_EVENT_HANDSET_LINKLOSS,           ruleStereoTopHandsetLinkLossReconnect, STEREOTOP_GOAL_CONNECT_HANDSET),
    /* Upon start of day of topology, Allow LE connection, make the handset connectable and connect handset if PDL is not empty */
    RULE(STEREOTOP_RULE_EVENT_START,                      ruleStereoTopAllowLEConnection,        STEREOTOP_GOAL_ALLOW_LE_CONNECTION),
    RULE(STEREOTOP_RULE_EVENT_START,                      ruleStereoTopEnableConnectableHandset, STEREOTOP_GOAL_CONNECTABLE_HANDSET),
    RULE(STEREOTOP_RULE_EVENT_START,                      ruleStereoTopAllowHandsetConnect,      STEREOTOP_GOAL_ALLOW_HANDSET_CONNECT),
    RULE(STEREOTOP_RULE_EVENT_START,                      ruleStereoTopAutoConnectHandset,       STEREOTOP_GOAL_CONNECT_HANDSET),
    /* Prohibit connection upon request from app */
    RULE(STEREOTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET,ruleStereoTopDisconnectHandset,        STEREOTOP_GOAL_DISCONNECT_HANDSET),
    /* Connect handset requested by topology user */
    RULE(STEREOTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET, ruleStereoTopUserRequestConnectHandset,         STEREOTOP_GOAL_CONNECT_HANDSET),
    RULE(STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS, ruleStereoTopDisconnectHandset, STEREOTOP_GOAL_DISCONNECT_HANDSET),
    RULE(STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET, ruleStereoTopDisconnectLruHandset, STEREOTOP_GOAL_DISCONNECT_LRU_HANDSET),
};

/*! Types of event that can initiate a connection rule decision. */
typedef enum
{
    /*! Auto connect to MRU device on stereo power on. */
    rule_auto_connect = 1 << 0,
    /*! Link loss with handset. */
    rule_connect_linkloss = 1 << 1,
    /*! Topology user requests for connection. */
    rule_connect_user = 1 << 2,
} rule_connect_reason_t;


/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/
static rule_action_t ruleStereoTopEnableConnectableHandset(void)
{
    bdaddr handset_addr;
    const STEREOTOP_GOAL_CONNECTABLE_HANDSET_T enable_connectable = {.enable = TRUE};

    /* Ignore the rule if no devices is PDL */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        STEREOTOP_RULE_LOG("ruleStereoTopEnableConnectableHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    /* Ignore the rule if already connected with handset */
    if (ConManagerIsConnected(&handset_addr))
    {
        STEREOTOP_RULE_LOG("ruleStereoTopEnableConnectableHandset, ignore as connected to handset");
        return rule_action_ignore;
    }

    /* Ignore the rule if we are in shutdown mode */
    if(StereoTopologyGetTaskData()->shutdown_in_progress)
    {
        STEREOTOP_RULE_LOG("ruleStereoTopEnableConnectableHandset, ignore as we are in shutdown mode");
        return rule_action_ignore;
    }

    STEREOTOP_RULE_LOG("ruleStereoTopEnableConnectableHandset, run as stereo not connected to handset");

    return RULE_ACTION_RUN_PARAM(enable_connectable);
}


/*! Decide whether to allow handset BR/EDR connections */
static rule_action_t ruleStereoTopAllowHandsetConnect(void)
{
    const bool allow_connect = TRUE;

    /* Ignore the rule if we are in shutdown mode */
    if(StereoTopologyGetTaskData()->shutdown_in_progress)
    {
        STEREOTOP_RULE_LOG("ruleStereoTopAllowHandsetConnect, ignore as we are in shutdown mode");
        return rule_action_ignore;
    }
    STEREOTOP_RULE_LOG("ruleStereoTopAllowHandsetConnect, run ");

    return RULE_ACTION_RUN_PARAM(allow_connect);
}


static rule_action_t ruleStereoTopConnectHandset(rule_connect_reason_t reason)
{
    bdaddr handset_addr;
    const STEREOTOP_GOAL_CONNECT_HANDSET_T reconnect_link_loss = {.link_loss = TRUE};
    const STEREOTOP_GOAL_CONNECT_HANDSET_T reconnect_normal = {.link_loss = FALSE};

    STEREOTOP_RULE_LOG("ruleStereoTopConnectHandset, reason %u", reason);

    /* Ignore the rule if no devices is PDL */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        STEREOTOP_RULE_LOG("ruleStereoTopConnectHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    /* Ignore the rule as prohibit connect is true */
    if(StereoTopologyGetTaskData()->prohibit_connect_to_handset)
    {
        STEREOTOP_RULE_LOG("ruleStereoTopConnectHandset, ignore as handset connection is disabled");
        return rule_action_ignore;
    }

    if (reason == rule_connect_linkloss)
    {
        STEREOTOP_RULE_LOG("ruleStereoConnectHandset, run as handset had a link loss");
        RULE_ACTION_RUN_PARAM(reconnect_link_loss);
    }
    else
    {
         STEREOTOP_RULE_LOG("ruleStereoConnectHandset, run as handset we were connected to before");
         RULE_ACTION_RUN_PARAM(reconnect_normal);
    }
    return rule_action_run_with_param;

}


static rule_action_t ruleStereoTopAutoConnectHandset(void)
{
    STEREOTOP_RULE_LOG("ruleStereoTopAutoConnectHandset");

    return ruleStereoTopConnectHandset(rule_auto_connect);
}

/*! Decide whether to allow handset LE connections */
static rule_action_t ruleStereoTopAllowLEConnection(void)
{
    const STEREOTOP_GOAL_ALLOW_LE_CONNECTION_T allow_connect = {.allow = TRUE};

    STEREOTOP_RULE_LOG("ruleStereoTopAllowLEConnection, run ");
    return RULE_ACTION_RUN_PARAM(allow_connect);
}


static rule_action_t ruleStereoTopHandsetLinkLossReconnect(void)
{
    STEREOTOP_RULE_LOG("ruleStereoTopHandsetLinkLossReconnect");

    return ruleStereoTopConnectHandset(rule_connect_linkloss);
}


static rule_action_t ruleStereoTopDisconnectHandset(void)
{
    STEREOTOP_RULE_LOG("ruleStereoTopDisconnectHandset");

    return rule_action_run;
}

static rule_action_t ruleStereoTopDisconnectLruHandset(void)
{
    if(StereoTopology_IsGoalActive(stereo_topology_goal_disconnect_handset)
        || StereoTopology_IsGoalActive(stereo_topology_goal_connect_handset)
        || (!HandsetService_IsAnyBredrConnected()))
    {
        STEREOTOP_RULE_LOG("ruleStereoTopDisconnectLruHandset, ignore");
        return rule_action_ignore;
    }

    STEREOTOP_RULE_LOG("ruleStereoTopDisconnectLruHandset, run");
    return rule_action_run;
}

static rule_action_t ruleStereoTopStop(void)
{
    STEREOTOP_RULE_LOG("ruleStereoTopStop");

    return rule_action_run;
}

static rule_action_t ruleStereoTopUserRequestConnectHandset(void)
{
    STEREOTOP_RULE_LOG("ruleStereoTopUserRequestConnectHandset");

    return ruleStereoTopConnectHandset(rule_connect_user);
}

/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the stereo rules module. */
bool StereoTopologyRules_Init(Task result_task)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    rule_set_init_params_t rule_params;

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = stereotop_rules_set;
    rule_params.rules_count = ARRAY_DIM(stereotop_rules_set);
    rule_params.nop_message_id = STEREOTOP_GOAL_NOP;
    rule_params.event_task = result_task;
    stereo_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t StereoTopologyRules_GetRuleSet(void)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    return stereo_rules->rule_set;
}

void StereoTopologyRules_SetEvent(rule_events_t event_mask)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    RulesEngine_SetEvent(stereo_rules->rule_set, event_mask);
}

void StereoTopologyRules_ResetEvent(rule_events_t event)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    RulesEngine_ResetEvent(stereo_rules->rule_set, event);
}

rule_events_t StereoTopologyRules_GetEvents(void)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    return RulesEngine_GetEvents(stereo_rules->rule_set);
}

void StereoTopologyRules_SetRuleComplete(MessageId message)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    RulesEngine_SetRuleComplete(stereo_rules->rule_set, message);
}

void StereoTopologyRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(stereo_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t stereoTopologyRules_CopyRunParams(const void* param, size_t size_param)
{
    stereo_topology_rules_task_data_t *stereo_rules = StereoTopologyRulesGetTaskData();
    RulesEngine_CopyRunParams(stereo_rules->rule_set, param, size_param);

    return rule_action_run_with_param;
}
