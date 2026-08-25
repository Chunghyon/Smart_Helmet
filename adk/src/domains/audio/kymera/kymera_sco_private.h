/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera private header for sco audio use case handler
*/

#ifndef KYMERA_SCO_PRIVATE_H_
#define KYMERA_SCO_PRIVATE_H_

#include "kymera.h"
#include "kymera_internal_msg_ids.h"

/*! \brief The KYMERA_INTERNAL_SCO_START message content. */
typedef struct
{
    /*! The SCO audio sink. */
    Sink audio_sink;
    /*! Pointer to SCO chain information. */
    const appKymeraScoChainInfo *sco_info;
    /*! The link Wesco. */
    uint8 wesco;
    /*! The starting volume. */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself before starting kymera SCO. */
    uint8 pre_start_delay;
    /* If TRUE, the chain will be started muted. It will unmute at the time set
    by the function #Kymera_ScheduleScoSyncUnmute, or after a timeout if that
    function isn't called. */
    bool synchronised_start;
    /*! Function to call when SCO chain is started */
    Kymera_ScoStartedHandler started_handler;
} KYMERA_INTERNAL_SCO_START_T;

/*! \brief Init SCO component
 */
void Kymera_ScoInit(void);

/*! \brief Handle request to start SCO (prepare + start SCO).
    \param msg Parameters needed for SCO start
 */
void Kymera_ScoHandleInternalStart(const KYMERA_INTERNAL_SCO_START_T *msg);

/*! \brief Handle request to prepare SCO.
 */
void Kymera_ScoHandlePrepareReq(Sink sco_snk, const appKymeraScoChainInfo *info, uint8 wesco, bool synchronised_start);

/*! \brief Handle request to start SCO (Has to be prepared first).
 */
bool Kymera_ScoHandleStartReq(Kymera_ScoStartedHandler started_handler, int16 volume_in_db);

/*! \brief Handle request to stop SCO.
 */
void Kymera_ScoHandleInternalStop(void);

/*! \brief Handle request to set SCO volume.
    \param volume_in_db The requested volume.
*/
void Kymera_ScoHandleInternalSetVolume(int16 volume_in_db);

/*! \brief Handle request to mute the SCO microphone.
    \param mute Set to TRUE to mute the microphone.
*/
void Kymera_ScoHandleInternalMicMute(bool mute);

/*! \brief Returns the SCO chain where Cvc operator is present
    \return Active SCO chain chandle
*/
kymera_chain_handle_t Kymera_ScoGetCvcChain(void);

/*! \brief Shows if the use case is active (therefore DSP clock needs to be adjusted accordingly)
    \return TRUE when active, FALSE otherwise
*/
bool Kymera_ScoIsActive(void);

#ifdef INCLUDE_CVC_DEMO
/*! \brief Updates CVC mode and passthrough settings for SCO call
*   \param mode full processing or pass-through mode for cvc send and cvc receive operator
*          passthrough_mic microphone to pass through in case of pass-through mode
    \return TRUE if CVC mode settings are updated
*/
bool Kymera_UpdateScoCvcSendSetting(kymera_cvc_mode_t mode, uint8 passthrough_mic);
#endif /* INCLUDE_CVC_DEMO */

#endif /* KYMERA_SCO_PRIVATE_H_ */
