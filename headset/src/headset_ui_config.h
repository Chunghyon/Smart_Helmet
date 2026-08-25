/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       headset_ui_config.h
\brief      Application specific ui configuration
*/

#ifndef HEADSET_UI_CONFIG_H_
#define HEADSET_UI_CONFIG_H_

#include "ui.h"

/*! \brief Return the ui configuration table for the headset application.

    The configuration table can be passed directly to the ui component in
    domains.

    \param table_length - used to return the number of rows in the config table.

    \return Application specific ui configuration table.
*/
const ui_config_table_content_t* HeadsetUi_GetConfigTable(unsigned* table_length);

/*! \brief Configures the Focus Select module in the framework with the
    source prioritisation for the Headset Application.
*/
void HeadsetUi_ConfigureFocusSelection(void);

/*! \brief Is this Logical Input one that should be screened when the Headset
           is in the Limbo state?

    \param logical_input - the logical input to check for

    \return bool, TRUE if the Logical Input is one that is screened in the Limbo state
*/
bool HeadsetUi_IsLogicalInputScreenedInLimboState(unsigned logical_input);

#endif /* HEADSET_UI_CONFIG_H_ */

