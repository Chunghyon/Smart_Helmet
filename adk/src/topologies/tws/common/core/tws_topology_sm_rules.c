/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Rules to control the TWS topology target state.
*/

#include "tws_topology_sm.h"
#include "tws_topology_private.h"
#include "handset_service.h"

#include <hdma.h>
#include <phy_state.h>
#include <mirror_profile.h>
#include <cc_with_case.h>
#include <bandwidth_manager.h>
#include <state_proxy.h>

#include <logging.h>

#define IGNORE FALSE
#define RUN TRUE

/*! \brief The SM rule that decides when the earbud should pair with its peer.
    \return TRUE if rule decides SM should pair with peer.
*/
static bool twsTopology_RulePairWithPeer(void)
{
    bdaddr bd_addr_secondary;

    /* Secondary BD_ADDR will be set iff paired with peer */
    if (appDeviceGetSecondaryBdAddr(&bd_addr_secondary))
    {
        DEBUG_LOG("twsTopology_RulePairWithPeer, ignore as already paired with peer");
        return IGNORE;
    }
    else
    {
        return RUN;
    }
}

/*! \brief The SM rule that decides when the earbud should find its role.
    \return TRUE if rule decides SM should find its role.
*/
static bool twsTopology_RuleFindRole(void)
{
    if (TwsTopology_IsRemainActiveForPeerEnabled())
    {
        DEBUG_LOG("twsTopology_RuleFindRole, run as remaining active with peer");
        return RUN;
    }
    else if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        if (CcWithCase_EventsEnabled())
        {
            if (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN)
            {
                DEBUG_LOG("twsTopology_RuleFindRole, ignore as in case and lid is not open");
                return IGNORE;
            }
            else
            {
                DEBUG_LOG("twsTopology_RuleFindRole, run as in case and lid is open");
                return RUN;
            }
        }
        else
        {
            DEBUG_LOG("twsTopology_RuleFindRole, ignore as in case and lid events not enabled");
            return IGNORE;
        }
    }
    else
    {
        DEBUG_LOG("twsTopology_RuleFindRole, run as out of case");
        return RUN;
    }
}

/*! \brief The SM rule that decides when the earbud should become idle.
    \return TRUE if rule decides SM should become idle.
*/
static bool twsTopology_RuleBecomeIdle(void)
{
    tws_topology_sm_t *sm = TwsTopology_GetSm();

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        DEBUG_LOG("twsTopology_RuleBecomeIdle, ignore as out of case");
        return IGNORE;
    }
    else if (CcWithCase_EventsEnabled() && CcWithCase_GetLidState() == CASE_LID_STATE_OPEN)
    {
        /* this doesn't trigger for the common use-case of entering the case, as the case lid
           state will be CASE_LID_STATE_UNKNOWN having been out of the case and without charger
           comms access. */
        DEBUG_LOG("twsTopology_RuleBecomeIdle, ignore as we're in the case but the lid is open");
        return IGNORE;
    }
    else if (sm->state == TWS_TOPOLOGY_STATE_PRIMARY_HDMA && !sm->handover_failed)
    {
        DEBUG_LOG("twsTopology_RuleBecomeIdle, ignore as will attempt handover first");
        return IGNORE;
    }
    else if (TwsTopology_IsRemainActiveForPeerEnabled())
    {
        DEBUG_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as remaining acting with peer");
        return IGNORE;
    }

    DEBUG_LOG("twsTopology_RuleBecomeIdle, run");
    return RUN;
}

/*! \brief The SM rule that decides when the primary earbud should enable handover.
    The actual decision on when to handover is decided by the handover decision
    making algorithm (HDMA).
    \return TRUE if rule decides primary role SM should enable handover.
*/
static bool twsTopology_RuleHandoverEnable(void)
{
    if (!TwsTopologyConfig_DynamicHandoverSupported())
    {
        DEBUG_LOG("twsTopology_RuleHandoverEnable ignore - handover not supported");
        return IGNORE;
    }
    else if (TwsTopology_IsHandoverProhibited())
    {
        DEBUG_LOG("twsTopology_RuleHandoverEnable ignore - app prohibited handover");
        return IGNORE;
    }
    else
    {
        bool handset_connected = appDeviceIsBredrHandsetConnected();
        bool handset_reconnecting = HandsetService_IsHandsetInContextPresent(handset_context_reconnecting);
        bool peer_connected = appDeviceIsPeerConnected();
        bool state_proxy_rx = StateProxy_InitialStateReceived();

        if ((handset_connected || handset_reconnecting) && peer_connected && state_proxy_rx)
        {
            DEBUG_LOG("twsTopology_RuleHandoverEnable run");
            return RUN;
        }
        else
        {
            DEBUG_LOG("twsTopology_RuleHandoverEnable ignore - (handset_connected %u | handset reconnecting %u) peer %u stateproxy %u",
                      handset_connected, handset_reconnecting, peer_connected, state_proxy_rx);
            return IGNORE;
        }
    }
}

/*! \brief The SM rule that decides when the primary earbud should start handover.
    \param reason The current reason provided by hdma.
    \return TRUE if rule decides SM should start handover.
*/
static bool twsTopology_RuleHandoverStart(hdma_handover_reason_t handover_reason)
{
    if (handover_reason == HDMA_HANDOVER_REASON_INVALID)
    {
        return IGNORE;
    }
    else if (!MirrorProfile_IsBredrMirroringConnected())
    {
        DEBUG_LOG("twsTopology_RuleHandoverStart, ignore as mirroring disconnected");
        return IGNORE;
    }
    else if ((handover_reason == HDMA_HANDOVER_REASON_RSSI) && (BandwidthManager_IsFeatureRunning(BANDWIDTH_MGR_FEATURE_A2DP_LL)))
    {
        DEBUG_LOG("twsTopology_RuleHandoverStart, ignore as aptX adaptive is in low latency mode");
        return IGNORE;
    }

    DEBUG_LOG("twsTopology_RuleHandoverStart, run");
    return RUN;
}

void twsTopology_SetTargetState(tws_topology_sm_t *sm)
{
    tws_topology_state_t target;

    if (sm->stop_task)
    {
        target = TWS_TOPOLOGY_STATE_STOPPED;
    }
    else
    {
        switch (sm->elected_role)
        {
            case tws_topology_elected_role_none:
                if (twsTopology_RulePairWithPeer())
                {
                    target = TWS_TOPOLOGY_STATE_PEER_PAIRING;
                }
                else if (twsTopology_RuleFindRole())
                {
                    /* Becoming a candidate results in the elected_role being
                    set. This result in this function being called again causing
                    the target state to be re-determined. */
                    target = TWS_TOPOLOGY_STATE_CANDIDATE;
                }
                else
                {
                    target = TWS_TOPOLOGY_STATE_IDLE;
                }
            break;

            case tws_topology_elected_role_acting_primary:
                target = TWS_TOPOLOGY_STATE_ACTING_PRIMARY;
                if (twsTopology_RuleBecomeIdle())
                {
                    target = TWS_TOPOLOGY_STATE_IDLE;
                }
            break;

            case tws_topology_elected_role_primary:
                target = TWS_TOPOLOGY_STATE_PRIMARY;

                if (twsTopology_RuleHandoverEnable())
                {
                    target = TWS_TOPOLOGY_STATE_PRIMARY_HDMA;
                    if (twsTopology_RuleHandoverStart(sm->handover_reason))
                    {
                        target = TWS_TOPOLOGY_STATE_SECONDARY;
                    }
                    else if (twsTopology_RuleBecomeIdle())
                    {
                        target = TWS_TOPOLOGY_STATE_IDLE;
                    }
                }
                else if (twsTopology_RuleBecomeIdle())
                {
                    target = TWS_TOPOLOGY_STATE_IDLE;
                }
            break;

            case tws_topology_elected_role_secondary:
                target = TWS_TOPOLOGY_STATE_SECONDARY;
                if (twsTopology_RuleBecomeIdle())
                {
                    target = TWS_TOPOLOGY_STATE_IDLE;
                }
            break;

            default:
                Panic();
                target = TWS_TOPOLOGY_STATE_IDLE;
            break;
        }
    }

    if (target != sm->target_state)
    {
        DEBUG_LOG_STATE("twsTopology_SetTargetState "
                        "enum:tws_topology_state_t:%d->"
                        "enum:tws_topology_state_t:%d",
                        sm->target_state, target);

        sm->target_state = target;
    }
}

