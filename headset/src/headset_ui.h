/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_ui.h
\brief      Header file for the application User Interface
*/
#ifndef HEADSET_UI_H
#define HEADSET_UI_H

#include "domain_message.h"
#include "headset_led.h"

/*! brief Initialise indicator module */
bool HeadsetUi_Init(Task init_task);

#endif // HEADSET_UI_H
