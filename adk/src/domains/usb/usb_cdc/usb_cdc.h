/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Communications Devices Class driver
*/

#ifndef USB_CDC_H_
#define USB_CDC_H_

#include <usb_device.h>
#include <usb_device_utils.h>

#include "usb_cdc_descriptors.h"

/*! Handler for USB CDC data coming from the host
 * \param data Data received from the host (not valid after the handler returns).
 * \param size Data size in bytes.
 */
typedef void (*usb_cdc_handler_t)(const uint8 *data, uint16 size);

/*! Register handler for data coming from the host
 * \param handler Handler for USB CDC data coming from the host */
void UsbCdc_RegisterHandler(usb_cdc_handler_t handler);

/*! Deregister previously registered handler
 * \param handler Previously registered handler */
void UsbCdc_UnregisterHandler(usb_cdc_handler_t handler);

/*! Send CDC data to the host
 * \param data CDC data
 * \param data_size size of data to send
 * \return USB_RESULT_OK if data was successfully sent.
 * USB_RESULT_NO_SPACE if not enough space in the TX buffer.
 * USB_RESULT_NOT_FOUND if active USB application does not have
 * USB CDC datalink class. */
usb_result_t UsbCdc_SendData(const uint8 *data,
                             uint16 data_size);

/*! USB CDC interface
 *
 * Custom configuration parameters shall be supplied in a configuration
 * structure of type "usb_cdc_config_params_t". */
extern const usb_class_interface_cb_t UsbCdc_Callbacks;

#endif /* USB_CDC_H_ */

