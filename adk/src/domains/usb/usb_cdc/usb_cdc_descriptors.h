/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for using default descriptors of USB Communications Devices Class
*/

#ifndef USB_CDC_DESCRIPTORS_H
#define USB_CDC_DESCRIPTORS_H

#include <usb_hub.h>

/*! Table 2: Communications Device Class Code */
#define CDC_DEVICE_CLASS_CODE 0x02

/*! Table 3: Communications Interface Class Code */
#define CDC_B_INTERFACE_CLASS   0x02
/*! Table 4 Class Subclass Code */
#define CDC_B_INTERFACE_SUB_CLASS_ABSTRACT_CONTROL 0x02
/*! Table 5 Communications Interface Class Control Protocol Codes */
#define CDC_B_INTERFACE_PROTOCOL_NON_SPECIFIC 0x00
#define CDC_B_INTERFACE_PROTOCOL_AT_V250 0x01

/*! Table 6: Data Interface Class Code */
#define CDC_B_INTERFACE_DATA_CLASS 0x0a

/*! USB Communications Class Subclass Specification for PSTN Devices,
 * Table 17: Line Coding Structure*/
#define CDC_LINE_CODING_SIZE 7

/*! USB CDC spec, Table 19: Class-Specific Request Codes */
typedef enum
{
    CDC_GET_LINE_CODING = 0x21,
    CDC_SET_LINE_CODING = 0x20,
    CDC_SET_CONTROL_LINE_STATE = 0x22
} cdc_class_request_t;


/*! USB Class Definitions for Communications Devices, revision 1.2
 * Table 12: Type Values for the bDescriptorType Field */
typedef enum
{
    CDC_CS_DESC_INTERFACE           = 0x24,
    CDC_CS_DESC_ENDPOINT            = 0x25
} usb_cdc_cs_descriptor_type_t;

/*! Class-specific CDC interface descriptor */
typedef struct
{
    const uint8*        descriptor;
    uint16              size_descriptor;
} usb_cdc_class_desc_t;

/*! CDC endpoint settings */
typedef struct
{
    /* Endpoint type */
    EndPointAttr attr;

    /*! Direction - "1": to_host or "0": from_host */
    uint8 is_to_host;

    /*! Polling interval */
    uint8 bInterval;

    /*! Maximum packet size in bytes */
    uint16 wMaxPacketSize;
} usb_cdc_endpoint_desc_t;

/*! CDC interface configuration */
typedef struct
{
    /*! Class-specific CDC functional interface descriptor */
    const usb_cdc_class_desc_t   *class_desc;

    /*! CDC Endpoints list */
    const usb_cdc_endpoint_desc_t  *endpoints;

    /*! Number of CDC endpoints */
    int num_endpoints;

    /*! Data to send in CDC_GET_LINE_CODING response */
    const uint8 *line_coding;
} usb_cdc_config_params_t;

/*! Default USB CDC class descriptor */
extern const usb_cdc_class_desc_t usb_cdc_class_desc;
/*! Default USB CDC endpoint config */
extern const usb_cdc_endpoint_desc_t usb_cdc_endpoint;
/*! Default USB CDC configuration */
extern const usb_cdc_config_params_t usb_cdc_config;

#endif /* USB_CDC_DESCRIPTORS_H */
