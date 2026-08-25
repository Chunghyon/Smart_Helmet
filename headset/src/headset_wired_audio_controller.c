/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_wired_audio_controller.c
\brief      Controller which helps to switch between bluetooth and wired audio sources.
*/

#include "headset_wired_audio_controller.h"
#include "wired_audio_source.h"
#include "headset_usb.h"
#include "usb_device.h"
#include "logging.h"
#include "headset_sm.h"
#include "audio_sources.h"
#include "audio_sources_observer_interface.h"

#include <bt_device.h>
#include <panic.h>

/*! headset wired audio controller task data */
typedef struct
{
   TaskData task;
   bool pause_wired_controller;
   bool wired_connected;
}headsetWiredAudioControllerTaskData;

/*! headset wired audio controller message handler */
static void headsetWiredAudioController_HandleMessage(Task task, MessageId id, Message message);

static void headsetWiredAudioController_AudioRouted(audio_source_t source, audio_routing_change_t change);

/*! Initialise the task data */
headsetWiredAudioControllerTaskData headset_wired_audio_controller_taskdata = {.task = headsetWiredAudioController_HandleMessage};

#define HeadsetWiredAudioControllerGetTask()      (&headset_wired_audio_controller_taskdata.task)
#define HeadsetWiredAudioControllerGetTaskData()  (&headset_wired_audio_controller_taskdata)
#define IsHeadsetWiredAudioControllerPaused()     (headset_wired_audio_controller_taskdata.pause_wired_controller)


/* audio source observer interface */
static const audio_source_observer_interface_t wired_observer_interface =
{
    .OnVolumeChange = NULL,
    .OnAudioRoutingChange = headsetWiredAudioController_AudioRouted
};

/*! \brief handler for handling notifications from audio sources */
static void headsetWiredAudioController_AudioRouted(audio_source_t source, audio_routing_change_t change)
{
   DEBUG_LOG_DEBUG("headsetWiredAudioController_AudioRouted, source=%d routed=%d", source, change);
}

/*! \brief Routine to check the present wired audio status */
static void headsetWiredAudioController_HandleCurrentWiredStatus(void)
{
   headsetWiredAudioControllerTaskData* sp = HeadsetWiredAudioControllerGetTaskData();
#ifdef ALLOW_USB_BT_COEXISTENCE
   bool is_wired_connected = WiredAudioSource_IsAudioAvailable(audio_source_line_in);
#else
   bool is_wired_connected = (WiredAudioSource_IsAudioAvailable(audio_source_line_in) || HeadsetUsb_IsAudioConnected());
#endif

   DEBUG_LOG_VERBOSE("headsetWiredAudioController_HandleCurrentWiredStatus");

   /* Wired audio controller records the wired audio state when it was paused.
    * At the time of resume, if the state of the wired audio has changed, goahead
    * and update the headset state machine.
    */
   if(is_wired_connected != sp->wired_connected)
   {
      if(is_wired_connected)
      {
         headsetSmWiredAudioConnected();
      }
      else
      {
         headsetSmWiredAudioDisconnected();
      }
   }
}

/*! \brief Message handler for headset wired audio controller */
static void headsetWiredAudioController_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    /* If wired audio controller is paused, do not process any wired events */
    if(IsHeadsetWiredAudioControllerPaused())
       return;

    switch (id)
    {
        case WIRED_AUDIO_DEVICE_CONNECT_IND:
        {
           DEBUG_LOG_VERBOSE("headsetWiredAudioController_HandleMessage Wired Source Connected"); 
           headsetSmWiredAudioConnected();
        }
        break;
        
#ifndef ALLOW_USB_BT_COEXISTENCE
        case USB_DEVICE_ENUMERATED:
        {
           DEBUG_LOG_VERBOSE("headsetWiredAudioController_HandleMessage USB Source enumerated"); 

           if(HeadsetUsb_IsAudioEnabled())
           {
              headsetSmWiredAudioConnected();
           }
        }
        break;
        
        case USB_DEVICE_DECONFIGURED:
        {
           DEBUG_LOG_VERBOSE("headsetWiredAudioController_HandleMessage USB Source de-configured");

           if(HeadsetUsb_IsAudioEnabled())
           {
              if(!WiredAudioSource_IsAudioAvailable(audio_source_line_in) && !HeadsetUsb_IsAudioConnected()) 
              { 
                 headsetSmWiredAudioDisconnected();
              }           
           }
        }
        break;
#endif /*#ifndef ALLOW_USB_BT_COEXISTENCE */

        case WIRED_AUDIO_DEVICE_DISCONNECT_IND:
        {
           DEBUG_LOG_VERBOSE("headsetWiredAudioController_HandleMessage Wired Source disconnected"); 
           
           if(!WiredAudioSource_IsAudioAvailable(audio_source_line_in) && !HeadsetUsb_IsAudioConnected()) 
           {
              headsetSmWiredAudioDisconnected();
           }
        }
        break;
        
        default:
        break;
    }
}

/*! \brief Headset Wired audio controller initialisation routine.*/
void HeadsetWiredAudioController_Init(void)
{
    headsetWiredAudioControllerTaskData* sp = HeadsetWiredAudioControllerGetTaskData();

    /* Wired audio controller is active by default */
    sp->pause_wired_controller = FALSE;

    /* Register for wired audio source messages */
    WiredAudioSource_ClientRegister(HeadsetWiredAudioControllerGetTask());
    /* Register for audio sources notifications for Line_in */
    AudioSources_RegisterObserver(audio_source_line_in, &wired_observer_interface);
#ifndef ALLOW_USB_BT_COEXISTENCE
    /* Register for USB messages */
    UsbDevice_ClientRegister(HeadsetWiredAudioControllerGetTask());
    /* Register for audio sources notifications for USB */    
    AudioSources_RegisterObserver(audio_source_usb, &wired_observer_interface);
#endif /*#ifndef ALLOW_USB_BT_COEXISTENCE */
}

/*! \brief Headset Wired audio controller pause.*/
void HeadsetWiredAudioController_Disable(void)
{
    DEBUG_LOG_VERBOSE("HeadsetWiredAudioController_Disable called");
    headsetWiredAudioControllerTaskData* sp = HeadsetWiredAudioControllerGetTaskData();

    /* Record the present state(connected/disconnected) of the wired audio devices */
#ifdef ALLOW_USB_BT_COEXISTENCE
    sp->wired_connected = WiredAudioSource_IsAudioAvailable(audio_source_line_in);
#else
    sp->wired_connected = (WiredAudioSource_IsAudioAvailable(audio_source_line_in) || HeadsetUsb_IsAudioConnected());
#endif
    sp->pause_wired_controller = TRUE;
}

/*! \brief Headset Wired audio controller resume.*/
void HeadsetWiredAudioController_Enable(void)
{
    DEBUG_LOG_VERBOSE("HeadsetWiredAudioController_Enable called");
    headsetWiredAudioControllerTaskData* sp = HeadsetWiredAudioControllerGetTaskData();

	/* Resume wired audio controller only when it was paused before */
    if(sp->pause_wired_controller)
    {
        /* Check the current state of the wired audio */
        headsetWiredAudioController_HandleCurrentWiredStatus();
        sp->pause_wired_controller = FALSE;
    }
}
