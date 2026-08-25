/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_device_domain Device
\ingroup    focus_domain
\brief      Focus interface definition for instantiating a module which shall
            return the focussed remote device.
*/
#ifndef FOCUS_DEVICE_H
#define FOCUS_DEVICE_H

#include "focus_types.h"

#include <device.h>
#include <ui_inputs.h>
#include <bdaddr.h>

/*! @{ */

/*! \brief Focus interface callback used by Focus_GetDeviceForContext API */
typedef bool (*focus_device_for_context_t)(ui_providers_t provider, device_t* device);

/*! \brief Focus interface callback used by Focus_GetDeviceForUiInput API */
typedef bool (*focus_device_for_ui_input_t)(ui_input_t ui_input, device_t* device);

/*! \brief Focus interface callback used by Focus_GetFocusForDevice API */
typedef focus_t (*focus_for_device_t)(const device_t device);

/*! \brief Structure used to configure the focus interface callbacks to be used
           to access the focussed device. And also to add/remove device to/from Exclude List. */
typedef struct
{
    focus_device_for_context_t for_context;
    focus_device_for_ui_input_t for_ui_input;
    focus_for_device_t focus;

} focus_device_t;

/*! \brief Configure a set of function pointers to use for retrieving the focussed device

    \param a structure containing the functions implementing the focus interface for retrieving
           the focussed device.
*/
void Focus_ConfigureDevice(focus_device_t const * focus_device);

/*! \brief Get the focussed device for which to query the context of the specified UI Provider

    \param provider - a UI Provider
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForContext(ui_providers_t provider, device_t* device);

/*! \brief Get the focussed device that should consume the specified UI Input

    \param ui_input - the UI Input that shall be consumed
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForUiInput(ui_input_t ui_input, device_t* device);

/*! \brief Get the current focus status for the specified device

    \param device - the device_t handle
    \return the focus status of the specified device
*/
focus_t Focus_GetFocusForDevice(const device_t device);

/*! \brief Configure override function pointers to use for retrieving the focussed device.

    Clients can register the override functions that can return the focussed device in some scenarios
    that client is interested in. Override functions to be executed first to check for focussed device, 
    if no device is found, then logic to find focussed device would fall-back on functions registered 
    through Focus_ConfigureDevice().

    \param A structure containing the function pointers implementing the focus interface for retrieving
           the focussed device for the specified client configuration.
*/
void Focus_ConfigureDeviceOverride(focus_device_t const * focus_device);

/*! @} */

#endif /* FOCUS_DEVICE_H */
