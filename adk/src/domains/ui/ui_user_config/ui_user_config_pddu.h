/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Header for the Persistent Device Data User (PDDU) implemenatation of the UI User
            Gesture configuration feature.
*/
#ifndef UI_USER_CONFIG_PDDU_H
#define UI_USER_CONFIG_PDDU_H

/*! \brief Register the UI User Config component with the Device Database Serialiser

    Called early in the Application start up to register the UI User Config component with the
    Device Database Serialiser so that this module can the feature can use NVRAM.
*/
void UiUserConfig_RegisterPdduInternal(void);

#endif // UI_USER_CONFIG_PDDU_H
