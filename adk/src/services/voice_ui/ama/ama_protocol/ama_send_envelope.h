/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_envelope.h
\brief  Definition of the APIs to pack and send data to the phone
*/

#ifndef _AMA_SEND_ENVELOPE_H
#define _AMA_SEND_ENVELOPE_H

#include "accessories.pb-c.h"

/*! \brief Send control envelope to transport
 *  \param control_envelope_out Control envelope to be packed and sent
 */
void AmaSendEnvelope_Send(ControlEnvelope * control_envelope_out);

#endif /* _AMA_SEND_ENVELOPE_H */
