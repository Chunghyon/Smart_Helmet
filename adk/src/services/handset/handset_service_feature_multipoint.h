/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    handset_service
\brief      Handset service multipoint feature Id registration and handling.

The multipoint feature Id is started when the handset service is configured
to allow mulitple BR/EDR ACL connections and stopped when only a single BR/EDR
ACL connection is allowed.

Generally multipoint will be a lower priority feature Id in a feature
priority list. The intention behind this is that the multipoint functionality
can be temporarily disabled while a higher priority, transient, feature is
active. For example LE Broadcast audio is active and to protect the audio
quality the handset multipoint is disabled to simplify the BT topology.

When the handset multipoint feature is suspended by a higher priority feature:
* The maximum number of BR/EDR ACLs that can connect is restricted to 1.
  This overrides the value that is set in the current handset config.
* If more than one BR/EDR ACL is connected the least recently used (LRU) one is
disconnected.

When the handset multipoint feature is resumed after a higher priority feature
stops:
* The maximum number of BR/EDR ACLs is un-restricted and will be whatever is
  set in the current handset config.
* If > 1 BR/EDR ACL is allowed then the LRU handset will be reconnected.

*/

#ifndef HANDSET_SERVICE_FEATURE_MULTIPOINT_H
#define HANDSET_SERVICE_FEATURE_MULTIPOINT_H

#ifdef ENABLE_LE_AUDIO_RESTRICTED_MULTIPOINT

#include "handset_service_config.h"

/*! \brief Initialise the handset multipoint feature handler. */
void HandsetService_FeatureMultipointInit(void);

/*! \brief Update the state of the handset multipoint feature.

    This should be called whenever the handset config is updated.

    \param config Current handset service config data.
*/
void HandsetService_FeatureMultipointUpdateState(const handset_service_config_t *config);

/*! \brief Check whether handset BR/EDR multipoint is allowed based on the multipoint feature.

    \return TRUE if the multipoint feature is active, FALSE if it is idle or suspended.
*/
bool HandsetService_FeatureMultipointAllowed(void);

#else

#define HandsetService_FeatureMultipointInit()

#define HandsetService_FeatureMultipointUpdateState(config) (UNUSED(config))

#define HandsetService_FeatureMultipointAllowed() (TRUE)

#endif /* ENABLE_LE_AUDIO_RESTRICTED_MULTIPOINT */

#endif /* HANDSET_SERVICE_FEATURE_MULTIPOINT_H */
