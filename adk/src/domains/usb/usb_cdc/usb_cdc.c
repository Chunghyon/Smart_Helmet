/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Communications Devices Class interface
*/

#include "usb_cdc.h"

#include "logging.h"

#include <csrtypes.h>

#include <usb_device.h>
#include <usb_device_utils.h>
#include <usb.h>

#include <message.h>

#include <panic.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <stream.h>
#include <stdlib.h>


#ifndef USB_DT_INTERFACE_ASSOCIATION
#define USB_DT_INTERFACE_ASSOCIATION 11
#endif

#ifndef USB_DT_INTERFACE_ASSOCIATION_SIZE
#define USB_DT_INTERFACE_ASSOCIATION_SIZE 8
#endif

TaskData cdc_task;

/*! USB CDC class context data structure */
typedef struct usb_cdc_data_t
{
    Sink class_sink;
    Source class_source;
    Sink ep_sink;
    Source ep_source;

    uint8 usb_iad_descriptor[USB_DT_INTERFACE_ASSOCIATION_SIZE];

    const usb_cdc_config_params_t *config;
} usb_cdc_data_t;

static usb_cdc_data_t *cdc_data;

usb_cdc_handler_t cdc_data_handler;

void UsbCdc_RegisterHandler(usb_cdc_handler_t handler)
{
    cdc_data_handler = handler;
}

void UsbCdc_UnregisterHandler(usb_cdc_handler_t handler)
{
    if (cdc_data_handler == handler)
    {
        cdc_data_handler = NULL;
    }
}

usb_result_t UsbCdc_SendData(const uint8 *data,
                             uint16 data_size)
{
    if (!cdc_data || !cdc_data->ep_sink)
    {
        return USB_RESULT_NOT_FOUND;
    }

    uint8 *sink_data = SinkMapClaim(cdc_data->ep_sink, data_size);

    if (!sink_data)
    {
        DEBUG_LOG_ERROR("UsbCdc: SendData - cannot claim sink space\n");
        return USB_RESULT_NO_SPACE;
    }

    /* copy report data, truncate if data_size > report_size */
    memcpy(sink_data, data, data_size);

    if (!SinkFlush(cdc_data->ep_sink, data_size))
    {
        DEBUG_LOG_ERROR("UsbCdc: SendData - failed to send data\n");
        Panic();
    }

    return USB_RESULT_OK;
}

static void usbCdc_DataReceived(Source ep_source)
{
    uint16 data_size;

    while ((data_size = SourceBoundary(ep_source)) != 0)
    {
        const uint8 *data = SourceMap(ep_source);

        if (cdc_data_handler)
        {
            cdc_data_handler(data, data_size);
        }

        SourceDrop(ep_source, data_size);
    }
}

static void usbCdc_ClassRequest(Source source)
{
    uint16 data_size;
    while ((data_size = SourceBoundary(source)) != 0)
    {
        UsbResponse resp;
        /* copy the original request to the response */
        memcpy(&resp.original_request, SourceMapHeader(source), sizeof(UsbRequest));
        /* Set the response fields to default values to make the code below simpler */
        resp.success = FALSE;
        resp.data_length = 0;
        switch (resp.original_request.bRequest) 
        {
            case (CDC_GET_LINE_CODING):
            {
                uint8 *out;
                DEBUG_LOG_INFO("UsbCdc: CDC_GET_LINE_CODING");
                if ((out = SinkMapClaim(cdc_data->class_sink, CDC_LINE_CODING_SIZE)) != 0)
                {
                    int i;
                    for (i=0; i < CDC_LINE_CODING_SIZE; i++)
                    {
                        out[i] = cdc_data->config->line_coding[i];
                    }
                    resp.success = TRUE;
                    resp.data_length = CDC_LINE_CODING_SIZE;
                }
                break;
            }

            case (CDC_SET_LINE_CODING):
            {
                const uint8 *in = SourceMap(source);
                uint16 rate = in[0] + (in[1] << 8) + (in[2] << 16) + (in[3] << 24);
                uint8 stop_bits = in[4] + 1;
                uint8 parity = in[5];
                uint8 data_bits = in[6];
                DEBUG_LOG_INFO("UsbCdc: CDC_SET_LINE_CODING rate %d, stop bits %d, parity %d, data bits %d",
                        rate, stop_bits, parity, data_bits);
                resp.success = TRUE;
                break;
            }

            case (CDC_SET_CONTROL_LINE_STATE):
            {
                DEBUG_LOG_INFO("UsbCdc: SET_CONTROL_LINE_STATE RTS %d, DTR %d",
                        (resp.original_request.wValue >> 1) & 1,
                        (resp.original_request.wValue) & 1);
                resp.success = TRUE;
                break;
            }

            default:
            {
                DEBUG_LOG_ERROR("UsbCdc: unhandled request: %04X\n",
                        resp.original_request.bRequest);
                break;
            }
        }
        /* Send response */
        if (resp.data_length)
        {
            (void)SinkFlushHeader(cdc_data->class_sink,
                                  resp.data_length,
                                  (uint16 *)&resp, sizeof(UsbResponse));
        }
        else
        {
            /* Sink packets can never be zero-length, so flush a dummy byte */
            (void) SinkClaim(cdc_data->class_sink, 1);
            (void) SinkFlushHeader(cdc_data->class_sink, 1, (uint16 *) &resp, sizeof(UsbResponse));
        }   
        /* Discard the original request */
        SourceDrop(source, data_size);
    }
}

static void usbCdc_Handler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (!cdc_data)
    {
        return;
    }

    if (id == MESSAGE_MORE_DATA)
    {
        const MessageMoreData *msg = (const MessageMoreData *)message;
        Source source = msg->source;

        if (source == cdc_data->class_source)
        {
            usbCdc_ClassRequest(source);
        }
        else if (source == cdc_data->ep_source)
        {
            usbCdc_DataReceived(source);
        }
    }
}

static const uint16 iad_string[] =
{ 0x0055, /* U */
  0x0053, /* S */
  0x0042, /* B */
  0x0020, /*  */
  0x0043, /* C */
  0x0044, /* D */
  0x0043, /* C */
  0x0000
};

static usb_class_context_t usbCdc_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes ctrl_codes, data_codes;
    UsbInterface control_intf, data_intf;
    uint8 control_endpoint = 0, source_endpoint = 0, sink_endpoint = 0;
    EndPointInfo ep_info[2];

    usb_cdc_data_t *data;

    const usb_cdc_config_params_t *config = (const usb_cdc_config_params_t *)config_data;

    DEBUG_LOG_INFO("UsbCdc: create");

    if (cdc_data)
    {
        DEBUG_LOG_ERROR("UsbCdc: ERROR - class already present");
        Panic();
    }

    if (!config ||
            !config->class_desc ||
            !config->endpoints || config->num_endpoints != 3
            || !config->line_coding)
    {
        DEBUG_LOG_ERROR("UsbCdc: ERROR - configuration not provided or incorrect");
        Panic();
    }

    /* Communication interface class, abstract control model, V.25ter (AT commands) */

    ctrl_codes.bInterfaceClass    = CDC_B_INTERFACE_CLASS;
    ctrl_codes.bInterfaceSubClass = CDC_B_INTERFACE_SUB_CLASS_ABSTRACT_CONTROL;
    ctrl_codes.bInterfaceProtocol = CDC_B_INTERFACE_PROTOCOL_AT_V250;
    ctrl_codes.iInterface         = 0;

    control_intf = UsbAddInterface(&ctrl_codes, CDC_CS_DESC_INTERFACE,
                           config->class_desc->descriptor,
                           config->class_desc->size_descriptor);

    if (control_intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbCdc: UsbAddInterface ERROR");
        Panic();
    }

    /* find the interrupt endpoint */
    int ep_index;
    for (ep_index = 0; ep_index < config->num_endpoints; ep_index++)
    {
        const usb_cdc_endpoint_desc_t *ep_config = &config->endpoints[ep_index];

        if (ep_config->attr == end_point_attr_int)
        {
            uint8 endpoint = UsbDevice_AllocateEndpointAddress(dev_index, ep_config->is_to_host);
            if (!endpoint)
            {
                DEBUG_LOG_ERROR("UsbCdc: UsbDevice_AllocateEndpointAddress ERROR");
                Panic();
            }

            ep_info[0].bEndpointAddress = endpoint;
            ep_info[0].bmAttributes = ep_config->attr;
            ep_info[0].wMaxPacketSize = ep_config->wMaxPacketSize;
            ep_info[0].bInterval = ep_config->bInterval;
            ep_info[0].extended = NULL;
            ep_info[0].extended_length = 0;

            control_endpoint = endpoint;
            break;
        }
    }

    if (!control_endpoint)
    {
        DEBUG_LOG_ERROR("UsbCdc: ERROR - configuration must have an interrupt endpoint");
        Panic();
    }

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(control_intf, 1, ep_info))
    {
        DEBUG_LOG_ERROR("UsbCdc: UsbAddEndPoints ERROR");
        Panic();
    }

    /* Data interface class */
    data_codes.bInterfaceClass    = CDC_B_INTERFACE_DATA_CLASS;
    data_codes.bInterfaceSubClass = 0;
    data_codes.bInterfaceProtocol = 0;
    data_codes.iInterface         = 0;

    /* CDC data interface does not have class descriptors */
    data_intf = UsbAddInterface(&data_codes, 0, NULL, 0);

    if (data_intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbCdc: UsbAddInterface ERROR");
        Panic();
    }

    /* find 2x bulk endpoints */
    for (ep_index = 0; ep_index < config->num_endpoints; ep_index++)
    {
        const usb_cdc_endpoint_desc_t *ep_config = &config->endpoints[ep_index];

        if (ep_config->attr == end_point_attr_bulk)
        {
            uint8 endpoint = UsbDevice_AllocateEndpointAddress(dev_index, ep_config->is_to_host);
            if (!endpoint)
            {
                DEBUG_LOG_ERROR("UsbCdc: UsbDevice_AllocateEndpointAddress ERROR");
                Panic();
            }

            if (ep_config->is_to_host)
            {
                sink_endpoint = endpoint;
            }
            else
            {
                source_endpoint = endpoint;
            }

            int ep_info_index = (ep_config->is_to_host) ? 0 : 1;

            ep_info[ep_info_index].bEndpointAddress = endpoint;
            ep_info[ep_info_index].bmAttributes = ep_config->attr;
            ep_info[ep_info_index].wMaxPacketSize = ep_config->wMaxPacketSize;
            ep_info[ep_info_index].bInterval = ep_config->bInterval;
            ep_info[ep_info_index].extended = NULL;
            ep_info[ep_info_index].extended_length = 0;
        }
    }

    if (!sink_endpoint || !source_endpoint)
    {
        DEBUG_LOG_ERROR("UsbCdc: ERROR - configuration must have 2x bulk endpoints");
        Panic();
    }

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(data_intf, 2, ep_info))
    {
        DEBUG_LOG_ERROR("UsbCdc: UsbAddEndPoints ERROR");
        Panic();
    }

    uint8 i_ad_string;
    if (UsbDevice_AddStringDescriptor(dev_index, iad_string, &i_ad_string) != USB_RESULT_OK)
    {
        DEBUG_LOG_ERROR("UsbCdc: UsbDevice_AddStringDescriptor ERROR");
        Panic();
    }

    data = (usb_cdc_data_t *)PanicUnlessMalloc(sizeof(usb_cdc_data_t));
    memset(data, 0, sizeof(usb_cdc_data_t));

    data->usb_iad_descriptor[0] = USB_DT_INTERFACE_ASSOCIATION_SIZE;      /* bLength */
    data->usb_iad_descriptor[1] = USB_DT_INTERFACE_ASSOCIATION; /* bDescriptorType = INTERFACE_ASSOCIATION */
    data->usb_iad_descriptor[2] = (uint8)control_intf; /* bFirstInterface */
    data->usb_iad_descriptor[3] = 0x02;      /* bInterfaceCount */
    data->usb_iad_descriptor[4] = ctrl_codes.bInterfaceClass; /* bFunctionClass */
    data->usb_iad_descriptor[5] = ctrl_codes.bInterfaceSubClass; /* bFunctionSubClass */
    data->usb_iad_descriptor[6] = ctrl_codes.bInterfaceProtocol; /* bFunctionProtocol */
    data->usb_iad_descriptor[7] = i_ad_string; /* iFunction */

    UsbAddInterfaceAssociationDescriptor(control_intf, data->usb_iad_descriptor,
                                         USB_DT_INTERFACE_ASSOCIATION_SIZE);

    cdc_task.handler = usbCdc_Handler;

    data->class_sink = StreamUsbClassSink(control_intf);
    data->class_source = StreamSourceFromSink(data->class_sink);

    MessageStreamTaskFromSink(data->class_sink, &cdc_task);
    (void)SinkConfigure(data->class_sink, VM_SINK_MESSAGES, VM_MESSAGES_SOME);

    data->ep_sink = StreamUsbEndPointSink(sink_endpoint);
    MessageStreamTaskFromSink(data->ep_sink, &cdc_task);
    (void)SinkConfigure(data->ep_sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE);

    data->ep_source = StreamUsbEndPointSource(source_endpoint);
    MessageStreamTaskFromSource(data->ep_source, &cdc_task);
    (void)SourceConfigure(data->ep_source,
                          VM_SOURCE_MESSAGES, VM_MESSAGES_SOME);

    data->config = config;

    cdc_data = data;

    return (usb_class_context_t)cdc_data;
}

static usb_result_t usbCdc_Destroy(usb_class_context_t context)
{
    if (!cdc_data ||
            (usb_class_context_t)cdc_data != context)
    {
        return USB_RESULT_NOT_FOUND;
    }

    free(cdc_data);
    cdc_data = NULL;

    DEBUG_LOG_INFO("UsbCdc: closed");

    return USB_RESULT_OK;
}

const usb_class_interface_cb_t UsbCdc_Callbacks =
{
        .Create = usbCdc_Create,
        .Destroy = usbCdc_Destroy,
        .SetInterface = NULL
};
