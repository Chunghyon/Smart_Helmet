/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_info.h
\brief      File consists of function declaration for Fast Pair Device Information Message Stream.
*/
#ifndef FASTPAIR_MSG_STREAM_DEV_INFO_H
#define FASTPAIR_MSG_STREAM_DEV_INFO_H

/********* MESSAGE STREAM PROTOCOL **************/
/*
Octet   Data Type      Description               Mandatory?
0       uint8          Message group             Mandatory
1       uint8          Message code              Mandatory
2 - 3   uint16         Additional data length    Mandatory
4 - n                  Additional data           Optional
The additional data length and additional data fields should be big endian.
*/
/* The present incoming data does not contain message group. */

#define FASTPAIR_DEVINFO_CODE_INDEX  0
#define FASTPAIR_DEVINFO_ADD_DATA_LEN_UPPER_INDEX  1
#define FASTPAIR_DEVINFO_ADD_DATA_LEN_LOWER_INDEX  2
#define FASTPAIR_DEVINFO_ADD_DATA_INDEX 3

#define FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN 0
#define FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_RSP_ADD_DATA_LEN 1
#define FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN 1
#define FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN 2

#define FASTPAIR_DEV_INFO_MODEL_ID_ADD_DATA_LEN (3)
#define FASTPAIR_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN (6)
#define FASTPAIR_DEV_INFO_SESSION_NONCE_LEN (8)

#define FASTPAIR_LEFT_RIGHT_ACTIVE      (0x03)
#define FASTPAIR_SINGLE_ACTIVE          (0x01)

/* Message Code for Device information event group */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_MODEL_ID = 0x01,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BLE_ADDRESS_UPDATED = 0x02,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BATTERY_UPDATED = 0x03,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_REMAINING_BATTERY_TIME = 0x04,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_REQ = 0x05,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_RSP = 0x06,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES = 0x07,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE = 0x08,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_SESSION_NOUNCE = 0x0A
} FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE;

/* Device Info data structure*/
typedef struct{
    uint8 dev_info_capabilities;
}fast_pair_msg_stream_dev_info;

/*! \brief  Initialize fast pair Device info message stream.
*/
void fastPair_MsgStreamDevInfo_Init(void);

/* Device capaility Bits */
#define FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_SILENCE_MODE_SUPPORTED (0x01)
#define FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_COMPANION_APP_INSTALLED (0x02)

/*! \brief  Get the device information

  \return Returns the device info
*/
fast_pair_msg_stream_dev_info fastPair_MsgStreamDevInfo_Get(void);

/*! \brief  Set the device information

    \param dev_info Device Information
*/
void fastPair_MsgStreamDevInfo_Set(fast_pair_msg_stream_dev_info dev_info);

#endif /* FASTPAIR_MSG_STREAM_DEV_INFO_H */

