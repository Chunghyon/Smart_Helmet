/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      To control USB application.
*/

#include "headset_usb.h"
#include "usb_application.h"
#include "usb_app_default.h"
#include "usb_app_audio.h"
#include "usb_app_voice.h"
#include "usb_app_voice_nb.h"
#include "usb_app_voice_wb.h"
#include "usb_app_audio_voice_2af.h"
#include "usb_app_audio_voice_1af.h"
#include "usb_device.h"

#include "logging.h"
#include <panic.h>
#include <stdlib.h>

#ifdef INCLUDE_USB_DEVICE

/*
 * Not enabled by default yet, as ANC tuning is still using legacy
 * usb_device_class library.
*/
#if defined INCLUDE_USB_AUDIO_VOICE_1AF || (defined INCLUDE_USB_AUDIO && defined INCLUDE_USB_VOICE)
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#elif defined INCLUDE_USB_AUDIO_VOICE_2AF
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#elif defined INCLUDE_USB_AUDIO
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_AUDIO
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#elif defined INCLUDE_USB_VOICE
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_VOICE
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#elif defined INCLUDE_USB_DEFAULT
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_DEFAULT
#else
#define USB_APP_ACTIVE  HEADSET_USB_APP_TYPE_NULL
#define USB_APP_INACTIVE  HEADSET_USB_APP_TYPE_NULL
#endif

static headset_usb_app_type_t active_usb_app = USB_APP_ACTIVE;
static headset_usb_app_type_t inactive_usb_app = USB_APP_INACTIVE;

static bool usb_audio_enable = FALSE;

/*! To check whether the given usb application is supported by headset */
static bool headsetUsb_IsAppSupported(headset_usb_app_type_t usb_app_type)
{
    bool status = FALSE;

    switch(usb_app_type)
    {
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF:
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF:
        case HEADSET_USB_APP_TYPE_NULL:
        case HEADSET_USB_APP_TYPE_DEFAULT:
        case HEADSET_USB_APP_TYPE_AUDIO:
        case HEADSET_USB_APP_TYPE_VOICE:
#ifdef INCLUDE_USB_NB_WB_TEST
        case HEADSET_USB_APP_TYPE_VOICE_NB:
        case HEADSET_USB_APP_TYPE_VOICE_WB:
#endif/* INCLUDE_USB_NB_WB_TEST */
            status = TRUE;
            break;
        default:
            break;
    }

    return status;
}

/*! To check whether the given usb application supports audio streaming */
static bool headsetUsb_IsAppSupportAudio(headset_usb_app_type_t usb_app_type)
{
    bool status = FALSE;

    switch(usb_app_type)
    {
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF:
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF:
        case HEADSET_USB_APP_TYPE_AUDIO:
        case HEADSET_USB_APP_TYPE_VOICE:
#ifdef INCLUDE_USB_NB_WB_TEST
        case HEADSET_USB_APP_TYPE_VOICE_NB:
        case HEADSET_USB_APP_TYPE_VOICE_WB:
#endif/* INCLUDE_USB_NB_WB_TEST */
            status = TRUE;
            break;
        default:
            break;
    }

    return status;
}

bool HeadsetUsb_Init(Task init_task)
{
    usb_audio_enable = FALSE;

    UsbDevice_Init(init_task);

    if(inactive_usb_app == HEADSET_USB_APP_TYPE_DEFAULT)
    {
        UsbApplication_Open(&usb_app_default);
    }

    return TRUE;
}

bool HeadsetUsb_SetActiveAppType(headset_usb_app_type_t usb_app_type)
{
    if(usb_audio_enable)
    {
        /*! headset_sm behavior depends on HeadsetUsb_IsAudioEnabled() & events from USB Device framework.
            Changing active_usb_app while headset_sm is active state would cause undesired behavior  */
        DEBUG_LOG_ALWAYS("HeadsetUsb_SetActiveAppType is supported only if headset is not in an active state (usb_audio_enable == FLASE)");
        return FALSE;
    }

    if(!headsetUsb_IsAppSupported(usb_app_type))
    {
        DEBUG_LOG_ALWAYS("HeadsetUsb_SetActiveAppType: usb_app_type is not supported");
        return FALSE;
    }

    active_usb_app = usb_app_type;
    DEBUG_LOG_INFO("HeadsetUsb_SetActiveAppType:: active_usb_app %d  inactive_usb_app %d", active_usb_app,inactive_usb_app);

    return TRUE;
}

bool HeadsetUsb_SetInactiveAppType(headset_usb_app_type_t usb_app_type)
{
    if(usb_audio_enable)
    {
        /*! headset_sm behavior depends on HeadsetUsb_IsAudioEnabled() & events from USB Device framework.
            Changing active_usb_app while headset_sm is active state would cause undesired behavior  */
        DEBUG_LOG_ALWAYS("HeadsetUsb_SetInactiveAppType is supported only if headset is not in an active state (usb_audio_enable == FLASE)");
        return FALSE;
    }

    if(inactive_usb_app == usb_app_type)
    {
        DEBUG_LOG_INFO("HeadsetUsb_SetInactiveAppType: inactive_usb_app == usb_app_type");
        return TRUE;
    }

    if(!headsetUsb_IsAppSupported(usb_app_type))
    {
        DEBUG_LOG_ALWAYS("HeadsetUsb_SetInactiveAppType: usb_app_type is not supported");
        return FALSE;
    }

    inactive_usb_app = usb_app_type;
    DEBUG_LOG_INFO("HeadsetUsb_SetInactiveAppType:: active_usb_app %d  inactive_usb_app %d", active_usb_app,inactive_usb_app);

    return TRUE;
}

bool HeadsetUsb_AudioEnable(Task requesting_task)
{
    DEBUG_LOG_INFO("HeadsetUsb_AudioEnable: active_usb_app %d", active_usb_app);
    UNUSED(requesting_task);
    usb_audio_enable = TRUE;

    switch(active_usb_app)
    {
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF:
            UsbApplication_Open(&usb_app_audio_voice_1af);
            break;
        case HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF:
            UsbApplication_Open(&usb_app_audio_voice_2af);
            break;
        case HEADSET_USB_APP_TYPE_AUDIO:
            UsbApplication_Open(&usb_app_audio);
            break;
        case HEADSET_USB_APP_TYPE_VOICE:
            UsbApplication_Open(&usb_app_voice);
            break;
        case HEADSET_USB_APP_TYPE_DEFAULT:
            UsbApplication_Open(&usb_app_default);
            break;
#ifdef INCLUDE_USB_NB_WB_TEST
        case HEADSET_USB_APP_TYPE_VOICE_NB:
            UsbApplication_Open(&usb_app_voice_nb);
            break;
        case HEADSET_USB_APP_TYPE_VOICE_WB:
            UsbApplication_Open(&usb_app_voice_wb);
            break;
#endif/* INCLUDE_USB_NB_WB_TEST */
        default:
            break;
    }

    return TRUE;
}

bool HeadsetUsb_AudioDisable(Task requesting_task)
{
    DEBUG_LOG_INFO("HeadsetUsb_AudioDisable: inactive_usb_app %d", inactive_usb_app);
    UNUSED(requesting_task);
    usb_audio_enable = FALSE;

    if(inactive_usb_app != active_usb_app)
    {
        if(inactive_usb_app == HEADSET_USB_APP_TYPE_DEFAULT)
        {
            UsbApplication_Open(&usb_app_default);
        }
        else
        {
            /*! inactive_usb_app will be either HEADSET_USB_APP_TYPE_DEFAULT or
            *  HEADSET_USB_APP_TYPE_NULL */
            PanicFalse(inactive_usb_app == HEADSET_USB_APP_TYPE_NULL);
            UsbApplication_Close();
        }
    }

    return TRUE;
}

bool HeadsetUsb_IsAudioEnabled(void)
{
    if(usb_audio_enable && headsetUsb_IsAppSupportAudio(active_usb_app))
    {
        return TRUE;
    }

    return FALSE;
}


bool HeadsetUsb_IsAudioConnected(void)
{
    if(HeadsetUsb_IsAudioEnabled() && UsbApplication_IsConnectedToHost())
    {
        return TRUE;
    }

    return FALSE;
}

#endif


