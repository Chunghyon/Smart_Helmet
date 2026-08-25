/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage audio use case handlers

*/

#include "audio_use_case_handler.h"
#include "audio_use_case_instance.h"
#include <logging.h>
#include <stdlib.h>

typedef struct
{
    audio_use_case_t use_case;
    const audio_use_case_handler_if *handler;
} use_case_map_t;

static struct
{
    unsigned length;
    use_case_map_t *array;
} use_case_map = {0};

static const audio_use_case_handler_if * audioUseCase_GetHandler(audio_use_case_t use_case)
{
    unsigned i;
    for(i = 0; i < use_case_map.length; i++)
    {
        if (use_case_map.array[i].use_case == use_case)
            return use_case_map.array[i].handler;
    }
    return NULL;
}

static inline void audioUseCase_AssertNewUseCase(audio_use_case_t use_case)
{
    const audio_use_case_handler_if * handler = audioUseCase_GetHandler(use_case);
    if (handler)
    {
        DEBUG_LOG_PANIC("audioUseCase_AssertNewUseCase: Preexisting handler %p for enum:audio_use_case_t:%d", handler, use_case);
    }
}

static inline void audioUseCase_AssertValidHandler(const audio_use_case_handler_if *handler)
{
    if (handler == NULL)
    {
        DEBUG_LOG_PANIC("audioUseCase_AssertValidHandler: Invalid handler %p", handler);
    }
}

void AudioUseCase_RegisterHandler(audio_use_case_t use_case, const audio_use_case_handler_if *handler)
{
    audioUseCase_AssertNewUseCase(use_case);
    audioUseCase_AssertValidHandler(handler);
    use_case_map.array = PanicNull(realloc(use_case_map.array, sizeof(use_case_map.array[0]) * (use_case_map.length + 1)));
    use_case_map.array[use_case_map.length].use_case = use_case;
    use_case_map.array[use_case_map.length].handler = handler;
    use_case_map.length++;
}

const audio_use_case_handler_if * AudioUseCase_GetInstanceHandler(audio_use_case_instance_t instance)
{
    audio_use_case_t use_case = AudioUseCase_GetUseCaseFromInstance(instance);
    const audio_use_case_handler_if *handler = audioUseCase_GetHandler(use_case);
    if (handler == NULL)
    {
        DEBUG_LOG_PANIC("AudioUseCase_GetInstanceHandler: No handler found for enum:audio_use_case_t:%d", use_case);
    }
    return handler;
}

#ifdef HOSTED_TEST_ENVIRONMENT
void AudioUseCase_TestResetHandler(void)
{
    free(use_case_map.array);
    use_case_map.array = NULL;
    use_case_map.length = 0;
}
#endif
