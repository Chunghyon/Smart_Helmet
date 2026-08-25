/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio and HID Descriptors for the USB Source LE Audio application
*/

#include "usb_audio.h"
#include "usb_audio_class_10_default_descriptors.h"
#include "usb_hid_consumer_transport_descriptors.h"

#define USB_SOURCE_AUDIO_SPKR_SUPPORTED_FREQUENCIES   1
#define USB_SOURCE_VOICE_MIC_SUPPORTED_FREQUENCIES    1

#define USB_SOURCE_MIC_CHANNELS        USB_AUDIO_CHANNELS_STEREO
#define USB_SOURCE_SPKR_CHANNELS       USB_AUDIO_CHANNELS_STEREO

#define USB_SOURCE_AUDIO_SAMPLE_SIZE                  USB_SAMPLE_SIZE_24_BIT

#if USB_SOURCE_MIC_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define USB_SOURCE_MIC_CHANNEL_CONFIG   0x03
#elif USB_SOURCE_MIC_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define USB_SOURCE_MIC_CHANNEL_CONFIG   0x01
#else
#error NOT_SUPPORTED
#endif

#if USB_SOURCE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define USB_SOURCE_SPKR_CHANNEL_CONFIG  0x03
#elif USB_SOURCE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define USB_SOURCE_SPKR_CHANNEL_CONFIG  0x01
#else
#error NOT_SUPPORTED
#endif

static const uint8 control_intf_desc_voice_mic[] =
{
    /* Microphone IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    UAC1D_MIC_VOICE_IT,                 /* bTerminalID */
    UAC_TRM_BIDI_HEADSET & 0xFF,        /* wTerminalType = Microphone*/
    UAC_TRM_BIDI_HEADSET >> 8,
    0x00,                               /* bAssocTerminal = none */
    USB_SOURCE_MIC_CHANNELS,            /* bNrChannels */
    USB_SOURCE_MIC_CHANNEL_CONFIG,      /* wChannelConfig  */
    USB_SOURCE_MIC_CHANNEL_CONFIG >> 8, /* wChannelConfig	*/
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* Microphone Features */
    UAC_FU_DESC_SIZE(USB_SOURCE_MIC_CHANNELS, UAC1D_FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    UAC1D_MIC_VOICE_FU,                 /* bUnitId*/
    UAC1D_MIC_VOICE_IT,                 /* bSourceId - Microphone IT*/
    UAC1D_FU_DESC_CONTROL_SIZE,         /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE,                /* bmaControls[0] (Mute on Master Channel)*/
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[1] (Left Front)*/
#if USB_SOURCE_MIC_CHANNELS == 2
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[2] (Right Front)*/
#endif
    0x00,                               /*iFeature = same as USB product string*/

    /* Microphone OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    UAC1D_MIC_VOICE_OT,                 /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    UAC1D_MIC_VOICE_FU,                 /* bSourceID - Microphone Features */
    0x00,                               /* iTerminal = same as USB product string */
};

/** Default USB streaming interface descriptors for mic */
static const uint8 streaming_intf_desc_voice_mic[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    UAC1D_MIC_VOICE_OT,                 /* bTerminalLink = Microphone OT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(USB_SOURCE_VOICE_MIC_SUPPORTED_FREQUENCIES), /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    USB_SOURCE_MIC_CHANNELS,            /* bNumberOfChannels */
    USB_SAMPLE_SIZE_16_BIT,             /* bSubframeSize = 2 bytes */
    USB_SAMPLE_SIZE_16_BIT * 8,         /* bBitsResolution */
    USB_SOURCE_VOICE_MIC_SUPPORTED_FREQUENCIES, /* bSampleFreqType */
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,

    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency contro */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

static const uac_control_config_t usb_dongle_voice_control_mic_desc = {
    control_intf_desc_voice_mic,
    sizeof(control_intf_desc_voice_mic)
};

static const uac_endpoint_config_t usb_dongle_voice_mic_endpoint = {
    .is_to_host = 1,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

static const uac_streaming_config_t usb_dongle_voice_streaming_mic_desc[] = {
    {
        &usb_dongle_voice_mic_endpoint,
        streaming_intf_desc_voice_mic,
        sizeof(streaming_intf_desc_voice_mic)
    }
};

static const uint8 control_intf_desc_audio_spkr[] =
{
    /* ALT_Speaker IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    UAC1D_SPKR_AUDIO_IT,                /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    USB_SOURCE_SPKR_CHANNELS,           /* bNrChannels */
    USB_SOURCE_SPKR_CHANNEL_CONFIG,     /* wChannelConfig	*/
    USB_SOURCE_SPKR_CHANNEL_CONFIG >> 8,
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* ALT_Speaker Features */
    UAC_FU_DESC_SIZE(USB_SOURCE_SPKR_CHANNELS, UAC1D_FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    UAC1D_SPKR_AUDIO_FU,                /* bUnitId*/
    UAC1D_SPKR_AUDIO_IT,                /* bSourceId - Speaker IT*/
    UAC1D_FU_DESC_CONTROL_SIZE,         /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE | UAC_FU_CONTROL_VOLUME, /* bmaControls[0] (Mute & Vol on Master Channel)*/
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[1] (Left Front)*/
    #if USB_SOURCE_SPKR_CHANNELS == 2
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[2] (Right Front)*/
    #endif
    0x00,                               /* iFeature = same as USB product string*/

    /* ALT_Speaker OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    UAC1D_SPKR_AUDIO_OT,                /* bTerminalID */
    UAC_TRM_BIDI_HEADSET & 0xFF,        /* wTerminalType = Speaker*/
    UAC_TRM_BIDI_HEADSET >> 8,
    0x00,                               /* bAssocTerminal = none */
    UAC1D_SPKR_AUDIO_FU,                /* bSourceID - Speaker Features*/
    0x00                                /* iTerminal = same as USB product string */
};

/** Default USB streaming interface descriptors for alt speaker */
static const uint8 streaming_intf_desc_audio_spkr[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    UAC1D_SPKR_AUDIO_IT,                /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(USB_SOURCE_AUDIO_SPKR_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    USB_SOURCE_SPKR_CHANNELS,           /* bNumberOfChannels */
    USB_SOURCE_AUDIO_SAMPLE_SIZE,       /* bSubframeSize = 3 bytes */
    USB_SOURCE_AUDIO_SAMPLE_SIZE * 8,   /* bBitsResolution */
    USB_SOURCE_AUDIO_SPKR_SUPPORTED_FREQUENCIES,/* bSampleFreqType = 3 discrete sampling frequencies */
    SAMPLE_RATE_48K & 0xFF,             /* tSampleFreq = 48000*/
    (SAMPLE_RATE_48K >> 8) & 0xFF,
    (SAMPLE_RATE_48K >> 16) & 0xFF,

    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency control */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/* Audio Speaker interface descriptors */
static const uac_control_config_t usb_source_music_control_spkr_desc = {
    control_intf_desc_audio_spkr,
    sizeof(control_intf_desc_audio_spkr)
};

static const uac_endpoint_config_t usb_source_music_spkr_endpoint = {
    .is_to_host = 0,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

static const uac_streaming_config_t usb_source_music_streaming_spkr_desc[] = {
    {
        &usb_source_music_spkr_endpoint,
        streaming_intf_desc_audio_spkr,
        sizeof(streaming_intf_desc_audio_spkr)
    }
};

static const usb_audio_interface_config_t usb_source_le_audio_intf_list[] =
{
    {
        .control_desc =   &usb_dongle_voice_control_mic_desc,
        .streaming_config = usb_dongle_voice_streaming_mic_desc,
        .alt_settings_count = ARRAY_DIM(usb_dongle_voice_streaming_mic_desc),
        .type = USB_AUDIO_DEVICE_TYPE_VOICE_MIC,
    },
    {
        .control_desc =   &usb_source_music_control_spkr_desc,
        .streaming_config = usb_source_music_streaming_spkr_desc,
        .alt_settings_count = ARRAY_DIM(usb_source_music_streaming_spkr_desc),
        .type = USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER,
    }
};

const usb_audio_interface_config_list_t usb_source_le_audio_interfaces =
{
    .intf = usb_source_le_audio_intf_list,
    .num_interfaces = ARRAY_DIM(usb_source_le_audio_intf_list)
};

/* HID Report Descriptor - Consumer Transport Control Device */
static const uint8 hid_report_descriptor[] =
{
    0x05, 0x0C,                  /* USAGE_PAGE (Consumer Devices) */
    0x09, 0x01,                  /* USAGE (Consumer Control) */
    0xA1, 0x01,                  /* COLLECTION (Application) */

    0x85, USB_HID_CONSUMER_TRANSPORT_REPORT_ID, /*   REPORT_ID (1) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xCD,                  /*   USAGE (Play/Pause - OSC) */
    0x09, 0xB5,                  /*   USAGE (Next Track - OSC) */
    0x09, 0xB6,                  /*   USAGE (Previous Track - OSC) */
    0x09, 0xB7,                  /*   USAGE (Stop - OSC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xB0,                  /*   USAGE (Play - OOC) */
    0x09, 0xB1,                  /*   USAGE (Pause - OOC) */
    0x09, 0xB3,                  /*   USAGE (Fast Forward -OOC) */
    0x09, 0xB4,                  /*   USAGE (Rewind - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x22,                  /*   INPUT (Data,Var,Abs,NoPref) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xE9,                  /*   USAGE (Volume Increment - RTC) */
    0x09, 0xEA,                  /*   USAGE (Volume Decrement - RTC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x02,                  /*   REPORT_COUNT (2) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs,Bit Field) */

    0x09, 0xE2,                  /*   USAGE (Mute - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x01,                  /*   REPORT_COUNT (1) */
    0x81, 0x06,                  /*   INPUT (Data,Var,Rel,Bit Field) */

    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x05,                  /*   REPORT_COUNT (5) */
    0x81, 0x01,                  /*   INPUT (Const) */

    0xC0,                        /* END_COLLECTION */
};

/* USB HID class descriptor - Consumer Transport Control Device*/
static const uint8 hid_interface_descriptor[] =
{
    HID_DESCRIPTOR_LENGTH,              /* bLength */
    B_DESCRIPTOR_TYPE_HID,              /* bDescriptorType */
    0x11, 0x01,                         /* bcdHID */
    0,                                  /* bCountryCode */
    1,                                  /* bNumDescriptors */
    B_DESCRIPTOR_TYPE_HID_REPORT,       /* bDescriptorType */
    sizeof(hid_report_descriptor),      /* wDescriptorLength */
    0                                   /* wDescriptorLength */
};

static const usb_hid_class_desc_t usb_hid_class_descriptor = {
    .descriptor = hid_interface_descriptor,
    .size_descriptor = sizeof(hid_interface_descriptor)
};

static const usb_hid_report_desc_t usb_hid_report_descriptor = {
    .descriptor = hid_report_descriptor,
    .size_descriptor = sizeof(hid_report_descriptor)
};

static const usb_hid_endpoint_desc_t usb_hid_endpoint_descriptor[] = {
    {
        .is_to_host = TRUE,
        .bInterval = 8,
        .wMaxPacketSize = 64
    }
};

const usb_hid_config_params_t usb_source_le_hid_config = {
    .class_desc = &usb_hid_class_descriptor,
    .report_desc = &usb_hid_report_descriptor,
    .endpoints = usb_hid_endpoint_descriptor,
    .num_endpoints = ARRAY_DIM(usb_hid_endpoint_descriptor)
};
