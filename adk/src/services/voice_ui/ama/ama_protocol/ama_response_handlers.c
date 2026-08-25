/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_response_handlers.c
\brief  Implementation of the APIs to handle AMA responses from the phone
*/

#ifdef INCLUDE_AMA

#include "ama_response_handlers.h"

#include "ama_data.h"
#include "ama_notify_app_msg.h"

#include <logging.h>

void AmaProtocol_HandleStartSpeechResponse(ControlEnvelope *control_envelope_in)
{
    Response * response = control_envelope_in->u.response;
    Response__PayloadCase response_case = response->payload_case;
    Dialog * dialog = response->u.dialog;

    DEBUG_LOG("AMA COMMAND__START_SPEECH Response: error_code=enum:ErrorCode:%d, response_case=enum:Response__PayloadCase:%d",
        response->error_code, response->payload_case);

    if(response->error_code == ERROR_CODE__SUCCESS)
    {
        switch(response_case)
        {
            case RESPONSE__PAYLOAD__NOT_SET:
                DEBUG_LOG("AMA COMMAND__START_SPEECH Response: No payload");
                break;

            case RESPONSE__PAYLOAD_DIALOG:
            {
                DEBUG_LOG("AMA COMMAND__START_SPEECH Response: Dialog ID %d", response->u.dialog->id);
                if (AmaData_IsStoppingSession())
                {
                    DEBUG_LOG("AMA COMMAND__START_SPEECH Response: Stopping session");
                    AmaData_SetStoppingSession(FALSE);
                    AmaProtocol_NotifyAppStopSpeech(dialog->id);
                }
             }
             break;

            default:
                DEBUG_LOG_ERROR("AMA COMMAND__START_SPEECH Response: Unhandled response_case %d, Dialog ID %d", response_case, response->u.dialog->id);
                break;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("AMA COMMAND__START_SPEECH Response: Error!! enum:ErrorCode:%d", response->error_code);
        AmaProtocol_NotifyAppStopSpeech(dialog->id);
    }
}

void AmaProtocol_HandleGetCentralInformationResponse(ControlEnvelope *control_envelope_in)
{   /* Check this. constants.ph-c.h says only the phone can send COMMAND__GET_CENTRAL_INFORMATION */
    /* and we have nothing to send the command??? */
#ifdef DEBUG_AMA_LIB
    Response* response = control_envelope_in->u.response;

    DEBUG_LOG("AMA COMMAND__GET_CENTRAL_INFORMATION response");
    if(NULL != response)
    {
        Response__PayloadCase respCase = response->payload_case;
        CentralInformation * centralInformation = response->u.central_information;
        DEBUG_LOG("AMA COMMAND__GET_CENTRAL_INFORMATION response: case enum:Response__PayloadCase:%d, code enum:ErrorCode:%d", respCase, response->error_code);
        if(NULL != centralInformation)
        {
            AmaLog_LogVaArg("AMA COMMAND__GET_CENTRAL_INFORMATION response: name %s, platform enum:Platform:%d\n", centralInformation->name, centralInformation->platform);
        }
    }
#else
    UNUSED(control_envelope_in);
#endif /* DEBUG_AMA_LIB */

}

void AmaProtocol_HandleNotHandledResponse(ControlEnvelope *control_envelope_in)
{
#ifdef DEBUG_AMA_LIB
    Command command = control_envelope_in->command;
    Response* response = control_envelope_in->u.response;
    Response__PayloadCase response_case = response->payload_case;
    DEBUG_LOG("AMA Reponse Not handled command = enum:Command:%d, case enum:Response__PayloadCase:%d, error code enum:ErrorCode:d", command, response_case, response->error_code);
#else
    UNUSED(control_envelope_in);
#endif /* DEBUG_AMA_LIB */
}

#endif /* INCLUDE_AMA */
