/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of the dev_cfg_fs commands for device test service
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_data.h"
#include "device_test_service_commands_helper.h"
#include "device_test_parse.h"
#include <file/file_if.h>
#include "stream.h"
#include "sink.h"
#include <util.h>
#include <logging.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <panic.h>

#if defined(INCLUDE_DEVICE_TEST_SERVICE_DEV_CFG_FS)

/*! Guard timeout to protect against the host not sending commands for too much time */
#define deviceTestServiceDevCfgTimeoutMs()   (D_SEC(1))

/*! Messages used internally for the device config file system commands */
typedef enum
{
    /*! Timeout for host requesting next portion when reading */
    DTS_DEV_CFG_RX_TIMEOUT_MSG = INTERNAL_MESSAGE_BASE,
    /*! Timeout for host sending next portion when writing */
    DTS_DEV_CFG_TX_TIMEOUT_MSG
} dts_dev_cfg_msg_t;

static void deviceTestService_DevCfgMessageHandler(Task task, MessageId id, Message message);

static struct
{
                        /*!  Task used by the dev_cfg file system commands */
    TaskData internal_task;
    uint16 fs_size;     /*!< Size of file system. Provided by the command in the update case */
    uint16 rx_bytes;    /*!< Number of bytes received so far, for a complete update */
    Sink fs_sink;       /*!< Sink used for updating file system */
    uint16 tx_bytes;    /*!< Total number of bytes sent so far, when reading file system */
    Source fs_source;   /*!< Source used when reading file syste, */
                        /*!  Task supplied when sending a command, needed for the response
                             in case of a timeout */
    Task timeout_response_task;
} dev_cfg_update_context = {.fs_size = 0,
                            .rx_bytes = 0,
                            .fs_sink = 0,
                            .tx_bytes = 0,
                            .fs_source = 0,
                            .internal_task = {.handler = deviceTestService_DevCfgMessageHandler},
                            .timeout_response_task = NULL};

static void deviceTestService_DevCfgCtxCleanup(void)
{
    dev_cfg_update_context.fs_sink = 0;
    dev_cfg_update_context.fs_source = 0;
    dev_cfg_update_context.fs_size = 0;
    dev_cfg_update_context.rx_bytes = 0;
    dev_cfg_update_context.tx_bytes = 0;
    dev_cfg_update_context.timeout_response_task = NULL;
}

static void deviceTestService_DevCfgHandleDevCfgRxTimeout(void)
{
    char response[] = "+DEVCFGUPD: ERR TOUT";

    /* this timeout should never fire if the sink is closed */
    PanicZero(dev_cfg_update_context.fs_sink);
    PanicZero(dev_cfg_update_context.timeout_response_task);

    DEBUG_LOG("deviceTestService_DevCfgHandleDevCfgRxTimeout, timeout, error");
    SinkClose(dev_cfg_update_context.fs_sink);
    DeviceTestService_CommandResponse(dev_cfg_update_context.timeout_response_task,
                                      response, sizeof(response));
    deviceTestService_DevCfgCtxCleanup();
}

static void deviceTestService_DevCfgHandleDevCfgTxTimeout(void)
{
    char response[] = "+DEVCFGREAD: ERR TOUT";

    /* this timeout should never fire if the source is closed */
    PanicZero(dev_cfg_update_context.fs_source);
    PanicZero(dev_cfg_update_context.timeout_response_task);

    DEBUG_LOG("deviceTestService_DevCfgHandleDevCfgTxTimeout, timeout, error");
    SourceClose(dev_cfg_update_context.fs_source);
    DeviceTestService_CommandResponse(dev_cfg_update_context.timeout_response_task,
                                      response, sizeof(response));
    deviceTestService_DevCfgCtxCleanup();
}

/*! \brief internal task used to handle the timeout messages */
static void deviceTestService_DevCfgMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case DTS_DEV_CFG_RX_TIMEOUT_MSG:
            deviceTestService_DevCfgHandleDevCfgRxTimeout();
            break;
        case DTS_DEV_CFG_TX_TIMEOUT_MSG:
            deviceTestService_DevCfgHandleDevCfgTxTimeout();
            break;
        default:
            break;
    }
}

static void deviceTestService_DevCfgStartReadTimer(Task task)
{
    dev_cfg_update_context.timeout_response_task = task;

    MessageSendLater(&dev_cfg_update_context.internal_task,
                     DTS_DEV_CFG_TX_TIMEOUT_MSG, NULL, 
                     deviceTestServiceDevCfgTimeoutMs());
}

/*! Command handler for AT + DEVCFGREAD ?

    This function returns the dev_cfg_fs (in bytes) to the host and opens
    a FileSystem stream.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleDevCfgRead(Task task)
{
    /* The response has the following format:
     * \r\n +DEVCFGREAD: FS_SIZE \r\n
     * FS_SIZE: an integer representing the total size of the filesystem.
    */
    char response[24];
    uint16 dev_cfg_total_size = 0;
    uint16 src_size, response_size;

    DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgRead");

    /* proceed only if DTS mode is active and authenticated */
    if (!(DeviceTestServiceIsActive() && DeviceTestServiceIsAuthenticated()))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* if this command comes with the sink or the source open, return ERROR response */
    if(dev_cfg_update_context.fs_sink || dev_cfg_update_context.fs_source)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, FS source (%d) or sink (%d) not zero",
                         dev_cfg_update_context.fs_source, dev_cfg_update_context.fs_sink);
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* open the dev_cfg_fs stream source to compute the size of the FS in bytes */
    Source source = StreamFilesystemSource(FILESYSTEM_ID_DEVICE_RO_FS);
    while ((src_size = SourceBoundary(source)) != 0)
    {
        SourceDrop(source, src_size);
        dev_cfg_total_size += src_size;
    }
    SourceClose(source);

    /* the \n\r chars will be added by the DeviceTestService_CommandResponse */
    response_size = sprintf(response, "+DEVCFGREAD:%d", dev_cfg_total_size);

    DeviceTestService_CommandResponse(task, response, response_size);

    /* open the FS source */
    dev_cfg_update_context.fs_source = StreamFilesystemSource(FILESYSTEM_ID_DEVICE_RO_FS);
    dev_cfg_update_context.fs_size = dev_cfg_total_size;
    dev_cfg_update_context.tx_bytes = 0;

    /* Send the OK response */
    DeviceTestService_CommandResponseOk(task);

    deviceTestService_DevCfgStartReadTimer(task);
}


/*! Command handler for AT + DEVCFGREADPART = fs_chunk_size

    fs_chunk_size: an integer representing the number of bytes that the host
    wants to read.
    The return is a string starting with +DEVCFGREADPART: and followed
    by an ASCII string with the bytes in hexadecimal format and not
    separed by space.

    \param[in] task The task to be used in command responses
    \param[in] params The parameters from the received command (fs_chunk_size)
 */
void DeviceTestServiceCommand_HandleDevCfgReadPart(Task task,
                                                   const struct DeviceTestServiceCommand_HandleDevCfgReadPart *params)
{
    uint16 size_to_tx = 0, target_size_to_tx = params->size;
    uint16 src_size;
    uint8* base;
    char *next_char;
    char response_head[] = "\n\r+DEVCFGREADPART:";

    /* cancel any guard timeout */
    MessageCancelAll(&dev_cfg_update_context.internal_task, DTS_DEV_CFG_TX_TIMEOUT_MSG);

    /* If filesystem source is not opened here, then it is an ERROR */
    if(dev_cfg_update_context.fs_source == 0)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, FS source is zero");
        DeviceTestService_CommandResponseError(task);
        deviceTestService_DevCfgCtxCleanup();
        return;
    }

    /* return ERROR is the host is claiming more bytes than the expected */
    if(dev_cfg_update_context.tx_bytes +  target_size_to_tx > dev_cfg_update_context.fs_size)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgReadPart, requested more than available");
        DeviceTestService_CommandResponseError(task);
        deviceTestService_DevCfgCtxCleanup();
        return;
    }

    /* copy the command header into the RFC sink. Add NULL de-ref check, but assume unlikely */
    if (DeviceTestService_CommandResponseClaimRaw(task, sizeof(response_head)-1, &base))
    {
        memcpy(base, response_head, sizeof(response_head)-1);
        DeviceTestService_CommandResponseFlushRaw(task, FALSE);
    }

    /* target_size_to_tx is the number of residual bytes to transmit */
    while(target_size_to_tx)
    {
        /* read the FS data */
        const uint8 *src_data = SourceMap(dev_cfg_update_context.fs_source);

        /* get the number of bytes in the FS stream */
        src_size = SourceBoundary(dev_cfg_update_context.fs_source);

        if(src_size == 0)
        {
            DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgReadPart, error, source has 0 bytes");
            /* the source should have something, otherwise return error */
            DeviceTestService_CommandResponseError(task);
            SourceClose(dev_cfg_update_context.fs_source);
            deviceTestService_DevCfgCtxCleanup();
            return;
        }

        /* we want to TX either the entire src size or the request */
        size_to_tx = MIN(target_size_to_tx, src_size);

        if (!DeviceTestService_CommandResponseClaimRaw(task, 2*size_to_tx, &base))
        {
            DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgReadPart, Unable to claim output space");
            /* the source should have something, otherwise return error */
            DeviceTestService_CommandResponseError(task);
            SourceClose(dev_cfg_update_context.fs_source);
            deviceTestService_DevCfgCtxCleanup();
            return;
        }
        next_char = (char*)base;

        DEBUG_LOG_V_VERBOSE("size = %d, size_to_tx = %d, target_size_to_tx = %d, tot_tx_bytes= %d",
                            src_size, size_to_tx,
                            target_size_to_tx, dev_cfg_update_context.tx_bytes);

        /* convert the data into ASCII with HEX encoding, 2 hex digits per byte and without spaces */
        for(uint16 i=0; i < size_to_tx; i++)
        {
            next_char += sprintf(next_char,"%02X",src_data[i]);
        }

        /* advance the source data */
        SourceDrop(dev_cfg_update_context.fs_source, size_to_tx);

        /* flush the DTS sink */
        DeviceTestService_CommandResponseFlushRaw(task, FALSE);

        /* keep a total size of TX bytes */
        dev_cfg_update_context.tx_bytes += size_to_tx;

        target_size_to_tx -= size_to_tx;
    }

    /* add the \n\r terminators. 
       Dont report an error, but include check to avoid NULL deref in case of error */
    if (DeviceTestService_CommandResponseClaimRaw(task, 2, &base))
    {
        base[0] = '\n';
        base[1] = '\r';
        DeviceTestService_CommandResponseFlushRaw(task, TRUE);
    }

    /* check if this is the last transfer */
    if(dev_cfg_update_context.tx_bytes == dev_cfg_update_context.fs_size)
    {
        /* if yes, finish and close the source */
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgReadPart, closing FS source");
        SourceClose(dev_cfg_update_context.fs_source);
        deviceTestService_DevCfgCtxCleanup();
    }
    else
    {
        deviceTestService_DevCfgStartReadTimer(task);
    }
}


static void deviceTestService_DevCfgStartUpdateTimer(Task task)
{
    dev_cfg_update_context.timeout_response_task = task;
    MessageSendLater(&dev_cfg_update_context.internal_task,
                     DTS_DEV_CFG_RX_TIMEOUT_MSG, NULL, deviceTestServiceDevCfgTimeoutMs());
}

/*! Command handler for AT + DEVCFGUPD = fs_size

    fs_size: an integer representing the number of bytes of the filesystem.
    The host will send these using one or more AT + DEVCFGUPDPART commands.

    An error will be reported if the passed fs_size is 0 or if one of the
    read or update procedure was already in progress.

    \param[in] task The task to be used in command responses
    \param[in] params The parameters from the received command (fs_size)
 */
void DeviceTestServiceCommand_HandleDevCfgUpdate(Task task,
                                                 const struct DeviceTestServiceCommand_HandleDevCfgUpdate *params)
{
    /* proceed only if DTS mode is active and authenticated */
    if (!(DeviceTestServiceIsActive() && DeviceTestServiceIsAuthenticated()))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* don't accept size = 0 */
    if(params->size == 0)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, size 0, error");
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* if this command comes with the sink or the source open, return ERROR response */
    if(dev_cfg_update_context.fs_sink || dev_cfg_update_context.fs_source)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, FS source (%d) or sink (%d) not zero",
                         dev_cfg_update_context.fs_source, dev_cfg_update_context.fs_sink);
        DeviceTestService_CommandResponseError(task);
        return;
    }

    dev_cfg_update_context.fs_sink = StreamFilesystemSink(FILESYSTEM_ID_DEVICE_RO_FS);
    dev_cfg_update_context.fs_size = params->size;
    dev_cfg_update_context.rx_bytes = 0;

    DeviceTestService_CommandResponseOk(task);

    deviceTestService_DevCfgStartUpdateTimer(task);

    DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, size %d", dev_cfg_update_context.fs_size);

}

/*! Command handler for AT + DEVCFGUPDPART = fs_chunk_string

    fs_chunk_string: a string containing the FS chunk to be written.
    The format should be an ASCII string with the bytes in hex and
    not separed by space.

    An error will be reported if the bytes string is malformed
    (i.e. not containing hexadecimals) or if the host sent more
    bytes than expected.

    \param[in] task The task to be used in command responses
    \param[in] params The parameters from the received command (fs_chunk_string)
 */
void DeviceTestServiceCommand_HandleDevCfgUpdatePart(Task task,
                                                 const struct DeviceTestServiceCommand_HandleDevCfgUpdatePart *params)
{
    unsigned string_len = params->value.length;
    const uint8 *input = params->value.data;
    unsigned nibbles = 0;
    uint8 value = 0;
    uint8 *next_byte;
    bool error = FALSE;
    unsigned chunk_len = 0, chunk_len_aux = 0;

    /* cancel any guard timeout */
    MessageCancelAll(&dev_cfg_update_context.internal_task, DTS_DEV_CFG_RX_TIMEOUT_MSG);

    /* if at this point the fs_sink is 0 it means that it was closed or never opened
        Answer with an ERROR */
    if(dev_cfg_update_context.fs_sink == 0)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, FS sink is zero");
        deviceTestService_DevCfgCtxCleanup();
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* calculate how many bytes are in the string so that
        we can claim the space in the sink before starting
        to store the data */
    for(unsigned i=0; i<string_len; i++)
    {
        if (isxdigit((char)input[i]))
        {
            chunk_len_aux++;
        }
    }
    chunk_len_aux/=2;

    uint16 offset = SinkClaim(dev_cfg_update_context.fs_sink, chunk_len_aux);
    uint8 *base = SinkMap(dev_cfg_update_context.fs_sink);
    next_byte = base + offset;

    /* conversion from ascii to bin */
    while (string_len--)
    {
        char ch = *input++;

        if (!isxdigit(ch))
        {
            if (ch == ' ')
            {
                if (nibbles == 0)
                {
                    continue;
                }
            }
            error = TRUE;
            break;
        }
        value = (value << 4) + deviceTestService_HexToNumber(ch);
        if (++nibbles == 2)
        {
            *next_byte++ = value;
            value = 0;
            nibbles = 0;
            chunk_len++;
        }
    }

    PanicFalse(chunk_len == chunk_len_aux);

    dev_cfg_update_context.rx_bytes += chunk_len;

    if(dev_cfg_update_context.rx_bytes > dev_cfg_update_context.fs_size)
    {
        DEBUG_LOG("DeviceTestServiceCommand_HandleDevCfgUpdate, error:"
                  " the number of received bytes is more than the size previously set");
        error = TRUE;
    }

    if(!error)
    {
        uint8* chunk_to_store = base+offset;

        DEBUG_LOG_V_VERBOSE("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                         chunk_to_store[0], chunk_to_store[1], chunk_to_store[2], chunk_to_store[3],
                chunk_to_store[4], chunk_to_store[5], chunk_to_store[6], chunk_to_store[7]);
        DEBUG_LOG_V_VERBOSE("rx = %d/%d", dev_cfg_update_context.rx_bytes, dev_cfg_update_context.fs_size);

        /* check if this is the last transfer */
        if(dev_cfg_update_context.rx_bytes == dev_cfg_update_context.fs_size)
        {
            /* if yes, flush and close the sink */
            if (!SinkFlushBlocking(dev_cfg_update_context.fs_sink, dev_cfg_update_context.rx_bytes))
            {
                DEBUG_LOG_V_VERBOSE("DeviceTestServiceCommand_HandleDevCfgUpdate, error: FS flush failed");
                /* Send ERROR as the flush validation failed */
                DeviceTestService_CommandResponseError(task);
            }
            else
            {
                DEBUG_LOG_V_VERBOSE("DeviceTestServiceCommand_HandleDevCfgUpdate, flush successful");
                DeviceTestService_CommandResponseOk(task);
            }
            SinkClose(dev_cfg_update_context.fs_sink);
            deviceTestService_DevCfgCtxCleanup();
        }
        else
        {
            deviceTestService_DevCfgStartUpdateTimer(task);
            DeviceTestService_CommandResponseOk(task);
        }
    }
    else
    {
        /* falling here means error, so close the sink */
        SinkClose(dev_cfg_update_context.fs_sink);
        deviceTestService_DevCfgCtxCleanup();
        DeviceTestService_CommandResponseError(task);
    }

}

#else /* ! INCLUDE_DEVICE_TEST_SERVICE_DEV_CFG_FS */

void DeviceTestServiceCommand_HandleDevCfgRead(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleDevCfg. Dev Cfg Commands not supported");

    DeviceTestService_CommandResponseError(task);
}

void DeviceTestServiceCommand_HandleDevCfgReadPart(Task task,
                                                   const struct DeviceTestServiceCommand_HandleDevCfgReadPart *params)
{
    UNUSED(params);

    DeviceTestServiceCommand_HandleDevCfgRead(task);
}

void DeviceTestServiceCommand_HandleDevCfgUpdate(Task task,
                                                 const struct DeviceTestServiceCommand_HandleDevCfgUpdate *params)
{
    UNUSED(params);

    DeviceTestServiceCommand_HandleDevCfgRead(task);
}

void DeviceTestServiceCommand_HandleDevCfgUpdatePart(Task task,
                                                 const struct DeviceTestServiceCommand_HandleDevCfgUpdatePart *params)
{
    UNUSED(params);

    DeviceTestServiceCommand_HandleDevCfgRead(task);
}

#endif /* INCLUDE_DEVICE_TEST_SERVICE_DEV_CFG_FS */

/*! @} End of group documentation */
