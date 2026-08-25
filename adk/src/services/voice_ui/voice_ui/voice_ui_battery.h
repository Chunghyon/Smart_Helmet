/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_battery.h
\brief      API of the voice UI battery handling.
*/

#ifndef VOICE_UI_BATTERY_H_
#define VOICE_UI_BATTERY_H_

#ifdef HAVE_NO_BATTERY

#define VoiceUi_BatteryInit()
#define VoiceUi_GetBatteryUpdate()

#else

/*! \brief Initialisation of Voice UI battery handling
*/
void VoiceUi_BatteryInit(void);

/*! \brief Force a battery update 
*/
void VoiceUi_GetBatteryUpdate(void);


#endif /* !HAVE_NO_BATTERY */

#endif  /* VOICE_UI_BATTERY_H_ */
