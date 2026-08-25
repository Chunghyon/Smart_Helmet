/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Header file for mapping between GAIA Context IDs and UI Providers and Contexts
*/

#include <csrtypes.h>

#include "ui_user_config.h"
#include "ui_inputs.h"

/*! \brief Lookup the UI Provider and provider contexts associated with a Context ID.

    \note This function allocates an array to return the provider contexts. It is up
          to the caller to free this array when it has finished with it.

    \param context_id - The context ID to look up the UI Provider and provider contexts for.
    \param provider - The UI Provider registered for this Context ID.
    \param context - An array of provider contexts which match the specifed Context ID
    \param num_contexts - the number of provider contexts in the array
    \return TRUE if the context ID was found in the map
*/
bool UiUserConfig_LookUpUiProviderAndContexts(
        ui_user_config_context_id_t context_id,
        ui_providers_t * provider,
        uint8 **context,
        uint8 *num_contexts);

/*! \brief Lookup the Context ID asociated with a UI Provider and provider context.

    \param provider - The UI Provider to match.
    \param context - The provider context to match.

    \return The context ID which matches the UI Provider and context specified. If no
            match can be found, this function will assign the value ui_context_passthrough.
*/
ui_user_config_context_id_t UiUserConfig_LookupContextId(
        ui_providers_t provider,
        uint8 context);

/*! \brief Add a set of Context ID to UI Provider and context mappings to the UI domain.

    \param provider - The UI Provider the mappings are for.
    \param map - The mappings of Context ID to UI provider context for this Provider.
    \param map_length - The number of mappings in the map for this UI provider.
*/
void UiUserConfig_AddProviderMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map_t * map,
        uint8 map_length);

/*! \brief Clear the Context ID to UI Provider and provider contexts mappings.
*/
void UiUserConfig_ResetMap(void);

/*! \brief Examines the configured Context ID to UI Provider and provider context
           mappings. Return a bit array describing which Context IDs are present
           in the map.

    \param supported_bit_array - this is a bit array with 128 addressable bits,
                                 each corresponding to a Context ID value.

    \note This is used to allo the GAIA smortphone app to discover the supported
          contexts in the embedded device at run-time.
*/
void UiUserConfig_GetSupportedContextsBitArray(uint8 supported_bit_array[16]);
