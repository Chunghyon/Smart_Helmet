/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI audio private header with APIs that handle requests that should be done asynchronously (via messages)
*/

#ifndef VOICE_UI_ASYNC_REQ_H_
#define VOICE_UI_ASYNC_REQ_H_

#include <bdaddr.h>

void VoiceUi_SendLinkPolicyUpdateReq(const bdaddr *address);

#endif /* VOICE_UI_ASYNC_REQ_H_ */
