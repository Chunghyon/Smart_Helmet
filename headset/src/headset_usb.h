/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB application control
*/

#ifndef HEADSET_USB_H
#define HEADSET_USB_H

/* USB application types */
typedef enum
{
    HEADSET_USB_APP_TYPE_NULL,
    HEADSET_USB_APP_TYPE_DEFAULT,
    HEADSET_USB_APP_TYPE_VOICE,
    HEADSET_USB_APP_TYPE_AUDIO,
    HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF,
    HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF,

#ifdef INCLUDE_USB_NB_WB_TEST
    /*! Only supported for testing */
    HEADSET_USB_APP_TYPE_VOICE_NB,
    HEADSET_USB_APP_TYPE_VOICE_WB,
#endif/* INCLUDE_USB_NB_WB_TEST */

} headset_usb_app_type_t;


/*! Init USB application with inactive configuration
    \param init_task Unused
    \return always returns TRUE
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_Init(Task init_task);
#else
#define HeadsetUsb_Init(init_task) (FALSE)
#endif

/*! \brief To set active application type.
    Must be called only when USB application is in inactive state
    \param USB application type.
    \return Returns TRUE if operation is success else return FALSE.
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_SetActiveAppType(headset_usb_app_type_t usb_app_type);
#else
#define HeadsetUsb_SetActiveAppType(usb_app_type) (FALSE)
#endif

/*! \brief To set inactive application type.
    Must be called only when USB application is in inactive state
    \param USB application type.
    \return Returns TRUE if operation is success else return FALSE.
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_SetInactiveAppType(headset_usb_app_type_t usb_app_type);
#else
#define HeadsetUsb_SetInactiveAppType(usb_app_type) (FALSE)
#endif

/*! Enble USB Audio/voice.
    Active USB application will be opened and active application is configured by 
    build time macros and HeadsetUsb_SetActiveAppType()

    \param requesting_task Unused
    \return always returns TRUE 
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_AudioEnable(Task requesting_task);
#else
#define HeadsetUsb_AudioEnable(requesting_task) ((void)(0))
#endif

/*! Disable USB Audio/Voice
    Inactive USB application will be opened and active application is configured by build time macros.
    \param requesting_task Unused
    \return always returns TRUE
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_AudioDisable(Task requesting_task);
#else
#define HeadsetUsb_AudioDisable(requesting_task) ((void)(0))
#endif

/*! To check whether USB Audio/Voice is enabled.
    Result depends on usb_audio_is_enabled & usb_active_app_type
    \return returns TRUE if USB Audio/Voice is enabled otherwise FALSE
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_IsAudioEnabled(void);
#else
#define HeadsetUsb_IsAudioEnabled() (FALSE)
#endif

/*! To check whether USB Audio/Voice is connected.
    \return returns TRUE if USB Audio/Voice is enabled & connected to host otherwise FALSE
*/
#ifdef INCLUDE_USB_DEVICE
bool HeadsetUsb_IsAudioConnected(void);
#else
#define HeadsetUsb_IsAudioConnected() (FALSE)
#endif


#endif /* HEADSET_USB_H */

