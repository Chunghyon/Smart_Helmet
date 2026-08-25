/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		kymera_output_ultra_quiet_dac.h
\addtogroup	kymera
\brief      The Kymera output ultra quite dac feature manager

*/

#ifndef KYMERA_OUTPUT_ULTRA_QUIET_DAC_H
#define KYMERA_OUTPUT_ULTRA_QUIET_DAC_H

#include "kymera_data.h"
#include "kymera_output_if.h"

/* output_chain is not active but Dac is still active in AANC standalone usecase*/
#define ULTRA_QUIET_DAC_VALID_USER (output_user_none)

/*! \brief Ultra Quiet Dac Initialisation function.
 *  gets Called during the init phase.
 */
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
void Kymera_UltraQuietDacInit(void);
#else
#define Kymera_UltraQuietDacInit() ((void)(0))
#endif

/*! \brief Enable Ultra quiet DAC mode when no primary playback audio source is active.
    \param void
    \return void
*/
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
void Kymera_RequestUltraQuietDac(void);
#else
#define Kymera_RequestUltraQuietDac() ((void)(0))
#endif

/*! \brief Check whether Ultra quiet DAC mode enabled or not.
    \param void
    \return TRUE if UltraQuietDac enable is requested otherwise return FALSE.
*/
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
bool Kymera_IsUltraQuietDacEnabled(void);
#else
#define Kymera_IsUltraQuietDacEnabled() ((void)(0))
#endif

/*! \brief Check whether Ultra quiet DAC mode enable requested or not.
    \param void
    \return TRUE if UltraQuietDac enable is requested otherwise return FALSE.
*/
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
bool Kymera_IsUltraQuiteDacRequested(void);
#else
#define Kymera_IsUltraQuiteDacRequested() ((void)(0)
#endif

/*! \brief Disable Ultra quiet DAC mode.
    \param void
    \return void
*/
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
void Kymera_CancelUltraQuietDac(void);
#else
#define Kymera_CancelUltraQuietDac() ((void)(0))
#endif

#endif /* KYMERA_OUTPUT_ULTRA_QUIET_DAC_H */
