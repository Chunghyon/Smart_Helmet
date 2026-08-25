/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_response_handlers.h
\brief  Definition of the APIs to handle AMA responses from the phone
*/

#ifndef AMA_RESPONSE_HANDLERS_H
#define AMA_RESPONSE_HANDLERS_H

#include "accessories.pb-c.h"

/*! \brief Handler function for COMMAND__START_SPEECH response
 *  \param control_envelope_in The unpacked command from the phone
 */
void AmaProtocol_HandleStartSpeechResponse(ControlEnvelope * control_envelope_in);

/*! \brief Handler function for COMMAND__GET_CENTRAL_INFORMATION response
 *  \param control_envelope_in The unpacked command from the phone
 */
void AmaProtocol_HandleGetCentralInformationResponse(ControlEnvelope * control_envelope_in);

/*! \brief Handler function for unrecognised responses
 *  \param control_envelope_in The unpacked command from the phone
 */
void AmaProtocol_HandleNotHandledResponse(ControlEnvelope * control_envelope_in);

#endif // AMA_RESPONSE_HANDLERS_H
