/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_envelope.c
\brief  Implementation of the APIs to pack and send data to the phone
*/

#ifdef INCLUDE_AMA
#include <panic.h>
#include <stdlib.h>
#include "logging.h"
#include "ama_notify_app_msg.h"
#include "ama_send_envelope.h"
#include "ama_log.h"
#include "ama_transport_version.h"
#include "ama_transport.h"

void AmaSendEnvelope_Send(ControlEnvelope* control_envelope_out)
{    
    size_t envelope_size = control_envelope__get_packed_size(control_envelope_out);
    uint8 *packed_envelope = AmaTransport_AllocatePacketData(envelope_size);

    if(control_envelope__pack(control_envelope_out, packed_envelope) != envelope_size)
    {
        DEBUG_LOG("AMA Error building packed envelope %d", envelope_size);
    }

    AmaLog_ControlEnvelope(AMA_LOG_SENDING, control_envelope_out, packed_envelope, envelope_size);
    AmaTransport_TransmitData(ama_stream_control, packed_envelope, envelope_size);

    AmaTransport_FreePacketData(packed_envelope, envelope_size);
}

#endif /* INCLUDE_AMA */
