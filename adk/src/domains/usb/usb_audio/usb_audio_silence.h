/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       usb_audio_silence.h
\brief      USB Audio Silence Generator for unrouted sink endpoints.

            Whenever the host activates a USB-IN audio endpoint, it expects to receive
            data from the endpoint upon each poll, according to the defined interface settings.

            However, the application isn't always ready to route data into an endpoint --
            for instance, it could be waiting for a SCO connection to complete
            before it connects a voice chain to the endpoint.

            During this wait interval which can be several seconds long, the host
            ends up with empty packets whenever it polls the device, which has been
            observed to cause errors in host applications and even kernel panics.

            Thus, this component is intended to stream silence into the endpoint,
            until the application starts a DSP chain connected to this endpoint.
            If/whenever the chain is ever torn down, this component will pick up the slack,
            and continue streaming silence, ensuring the host is never starved of data.
*/

#ifndef USB_AUDIO_SILENCE_H
#define USB_AUDIO_SILENCE_H

#ifndef DISABLE_USB_AUDIO_SILENCE

#include "usb_audio_fd.h"

/*! \brief Start or Stop idle/silence stream for a specified USB Audio device.
 *
 *         When started, silence will be streamed to the Headset MIC sink/endpoint of this USB Audio device,
 *         whenever a DSP chain isn't connected to this sink.

 *  \param usb_audio    USB Audio device which will receive silence stream
 *  \param start        Whether to start or stop the silence stream
 *  \return TRUE if the start/stop operation succeeded, else FALSE
 */
bool UsbAudioSilence_SetStreamState(usb_audio_info_t * usb_audio, bool start);

#else
#define UsbAudioSilence_SetStreamState(usb_audio, start)    do {UNUSED(usb_audio);UNUSED(start);} while(0)
#endif

#endif // USB_AUDIO_SILENCE_H
