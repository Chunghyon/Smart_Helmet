/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Peer find role handover interfaces

*/
#ifdef INCLUDE_MIRRORING
#include "app_handover_if.h"
#include "peer_find_role_private.h"
#include "av.h"
#include "mirror_profile.h"

#include <panic.h>
#include <logging.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static void peerFindRole_Commit(bool is_primary);


/******************************************************************************
 * Global Declarations
 ******************************************************************************/
REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(PEER_FIND_ROLE, NULL, peerFindRole_Commit);
/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*!
    \brief Update the media busy flag for Audio Streaming.

    Add the PEER_FIND_ROLE_AUDIO_STREAMING bitmask to media busy if streaming is active.
    Remove PEER_FIND_ROLE_AUDIO_STREAMING bitmask from media busy otherwise.
*/
static void peerFindRole_UpdateMediaBusyFlagForAudioStreaming(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if(Av_IsA2dpSinkStreaming())
    {
        pfr->media_busy |= PEER_FIND_ROLE_AUDIO_STREAMING;
    }
    else
    {
        /* Remove the PEER_FIND_ROLE_AUDIO_STREAMING bitmask */
        pfr->media_busy &= ~PEER_FIND_ROLE_AUDIO_STREAMING;
    }
    DEBUG_LOG("peerFindRole_UpdateMediaBusyFlagForAudioStreaming. post update media_busy %d",pfr->media_busy);
}

/*!
    \brief Update the media busy flag for Active Call.

    Add the PEER_FIND_ROLE_CALL_ACTIVE bitmask to media busy if call active.
    Remove PEER_FIND_ROLE_CALL_ACTIVE bitmask from media busy otherwise.
*/
static void peerFindRole_UpdateMediaBusyFlagForActiveCall(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if(MirrorProfile_IsEscoActive())
    {
        pfr->media_busy |= PEER_FIND_ROLE_CALL_ACTIVE;
    }
    else
    {
        /* Remove PEER_FIND_ROLE_CALL_ACTIVE bitmask */
        pfr->media_busy &= ~PEER_FIND_ROLE_CALL_ACTIVE;
    }
    DEBUG_LOG("peerFindRole_UpdateMediaBusyFlagForActiveCall. post update media_busy %d",pfr->media_busy);
}

/*!
    \brief Component commits to the specified role.

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

    \note Update the media_busy flag to reflect the current state of A2DP media streaming 
    or active call following standard handover.
    media_busy flag is updated at PRIMARY and SECONDARY regardles of the role.

*/
static void peerFindRole_Commit(bool is_primary)
{
    UNUSED(is_primary);
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    peerFindRole_UpdateMediaBusyFlagForAudioStreaming();
    peerFindRole_UpdateMediaBusyFlagForActiveCall();

    DEBUG_LOG("peerFindRole_Commit. post update media_busy %d",pfr->media_busy);
}

#endif /* INCLUDE_MIRRORING */
