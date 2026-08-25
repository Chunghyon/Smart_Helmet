/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Core procedures used by the handover profile to perform a handover.
*/
#ifndef HANDOVER_PROFILE_PROCEDURES_H_
#define HANDOVER_PROFILE_PROCEDURES_H_

#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"

/* \brief Boost the CPU */
handover_profile_status_t handoverProfile_PerformanceRequest(void);

/* \brief Unboost the CPU */
handover_profile_status_t handoverProfile_PerformanceRelinquish(void);

/* \brief Halt the ACL link */
handover_profile_status_t handoverProfile_HaltLink(handover_device_t *device);

/* \brief Resume the ACL link */
handover_profile_status_t handoverProfile_ResumeLink(handover_device_t *device);

/* \brief Prepare the ACL for handover */
handover_profile_status_t handoverProfile_PrepareLink(handover_device_t *device);

/* \brief Cancel the prepare for handover for the ACL */
handover_profile_status_t handoverProfile_CancelPrepareLink(handover_device_t *device);

/* \brief Wait for data to be transmitted to the peer */
handover_profile_status_t handoverProfile_ClearPendingPeerData(void);

/* \brief Panic */
handover_profile_status_t handoverProfile_Panic(void);

#endif
#endif