/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB CDC default descriptors
*/

#include "usb_cdc_descriptors.h"

/* See the USB Class Definitions for Communications Devices, revision 1.2,
 * Communications Class Functional Descriptors */
static const uint8 interface_descriptor_cdc[] =
{
  0x05,         /* bLength */
  CDC_CS_DESC_INTERFACE, /* bDescriptorType = CS_INTERFACE */
  0x00,         /* bDescriptorSubType = HEADER */
  0x10, 0x01,   /* bcdCDC = Communication Device Class v1.10 */

  0x05,         /* bLength */
  CDC_CS_DESC_INTERFACE, /* bDescriptorType = CS_INTERFACE */
  0x01,         /* bDescriptorSubType = CALL MANAGEMENT */
  0x03,         /* bmCapabilities:
                      bit 1: Device can send/receive call management over Data Class interface
                      bit 0: Device handles call management itself */
  0x01,         /* bDataInterface: number of the Data Interface */

  0x04,         /* bLength */
  CDC_CS_DESC_INTERFACE, /* bDescriptorType = CS_INTERFACE */
  0x02,         /* bDescriptorSubType = ABSTRACT CONTROL MANAGEMENT */
  0x06,         /* bmCapabilities:
                      bit 3: Device supports Network_Connection notification
                      bit 2: Device supports request Send_Break
                      bit 1: Device supports requests Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding and the notification Serial_State
                      bit 0: Device supports requests Set_Comm_Feature, Clear_Comm_Feature and Get_Comm_Feature */

  0x05,         /* bLength */
  CDC_CS_DESC_INTERFACE, /* bDescriptorType = CS_INTERFACE */
  0x06,         /* bDescriptorSubType = UNION */
  0x00,         /* master interface - control */
  0x01          /* slave interface - data */
};

const usb_cdc_class_desc_t usb_cdc_class_desc = {
  .descriptor = interface_descriptor_cdc,
  .size_descriptor = sizeof(interface_descriptor_cdc)
};

const usb_cdc_endpoint_desc_t usb_cdc_endpoints[] = {
  {
    .attr = end_point_attr_int,
    .is_to_host = TRUE,
    .wMaxPacketSize = 8,
    .bInterval = 1
  },
  {
    .attr = end_point_attr_bulk,
    .is_to_host = FALSE,
    .wMaxPacketSize = 64,
    .bInterval = 1
  },
  {
    .attr = end_point_attr_bulk,
    .is_to_host = TRUE,
    .wMaxPacketSize = 64,
    .bInterval = 1
  }
};

/* USB Communications Class Subclass Specification for PSTN Devices,
 * 6.3.11 GetLineCoding */
const uint8 cdc_line_coding[CDC_LINE_CODING_SIZE] = {
  0x80, 0x25, 0, 0,   /* 9600 baud */
  0x00,   /* 1 stop bit */
  0x00,   /* no parity */
  0x08   /* 8 bits per character */
};

const usb_cdc_config_params_t usb_cdc_config = {
  .class_desc = &usb_cdc_class_desc,
  .endpoints = usb_cdc_endpoints,
  .num_endpoints = ARRAY_DIM(usb_cdc_endpoints),
  .line_coding = cdc_line_coding
};
