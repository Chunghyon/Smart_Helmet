/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage audio use case instances

*/

#include "audio_use_case_instance.h"
#include "audio_use_case_source.h"
#include <logging.h>
#include <stdlib.h>

struct audio_use_case_instance_tag
{
    audio_use_case_t use_case;
};

static struct
{
    size_t length;
    struct audio_use_case_instance_tag *array;
} instances = {0};

static inline struct audio_use_case_instance_tag audioUseCase_GetNewInstance(audio_use_case_t use_case)
{
    struct audio_use_case_instance_tag instance = {0};
    instance.use_case = use_case;
    return instance;
}

static audio_use_case_instance_t audioUseCase_NewInstance(audio_use_case_t use_case)
{
    instances.array = PanicNull(realloc(instances.array, sizeof(instances.array[0]) * (instances.length + 1)));
    instances.array[instances.length] = audioUseCase_GetNewInstance(use_case);
    instances.length++;
    // NULL should be invalid, therefore start handle values from 1
    return (audio_use_case_instance_t)instances.length;
}

static inline audio_use_case_instance_t audioUseCase_GetInstanceFromIndex(size_t instance_index)
{
    // NULL should be invalid, therefore start instance handle values from 1
    return (audio_use_case_instance_t)(instance_index + 1);
}

static inline size_t audioUseCase_GetInstanceIndex(audio_use_case_instance_t instance)
{
    return (size_t)instance - 1;
}

static void audioUseCase_AssertValidInstance(audio_use_case_instance_t instance)
{
    size_t index = audioUseCase_GetInstanceIndex(instance);
    if (index >= instances.length)
    {
        DEBUG_LOG_PANIC("audioUseCase_AssertValidInstance: Invalid instance %p", instance);
    }
}

static audio_use_case_instance_t audioUseCase_GetUniqueInstanceFromUseCase(audio_use_case_t use_case)
{
    size_t i;
    audio_use_case_instance_t instance = NULL;
    for(i = 0; i < instances.length; i++)
    {
        if (instances.array[i].use_case == use_case)
        {
            if (instance)
            {
                DEBUG_LOG_PANIC("audioUseCase_GetUniqueInstanceFromUseCase: Second instance found for use_case = enum:audio_use_case_t:%d, first = %p second = %p",
                        use_case, instance, audioUseCase_GetInstanceFromIndex(i));
            }
            else
            {
                instance = audioUseCase_GetInstanceFromIndex(i);
            }
        }
    }
    return instance;
}

static void audioUseCase_AssertNoInstance(audio_use_case_t use_case)
{
    audio_use_case_instance_t instance = audioUseCase_GetUniqueInstanceFromUseCase(use_case);
    if (instance)
    {
        DEBUG_LOG_PANIC("audioUseCase_AssertNoInstance: Existing instance %p found for use_case = enum:audio_use_case_t:%d", instance, use_case);
    }
}

audio_use_case_instance_t AudioUseCase_CreateInstance(audio_use_case_t use_case)
{
    audioUseCase_AssertNoInstance(use_case);
    return audioUseCase_NewInstance(use_case);
}

audio_use_case_instance_t AudioUseCase_CreateInstanceForSource(audio_use_case_t use_case, generic_source_t source)
{
    audio_use_case_instance_t instance = audioUseCase_NewInstance(use_case);
    AudioUseCase_AssociateSourceWithInstance(source, instance);
    return instance;
}

audio_use_case_instance_t AudioUseCase_GetInstanceForUseCase(audio_use_case_t use_case)
{
    audio_use_case_instance_t instance = audioUseCase_GetUniqueInstanceFromUseCase(use_case);
    if (instance == NULL)
    {
        DEBUG_LOG_PANIC("AudioUseCase_GetInstanceFromUseCase: No instance found for use_case = enum:audio_use_case_t:%d", use_case);
    }
    return instance;
}

audio_use_case_t AudioUseCase_GetUseCaseFromInstance(audio_use_case_instance_t instance)
{
    audioUseCase_AssertValidInstance(instance);
    return instances.array[audioUseCase_GetInstanceIndex(instance)].use_case;
}

#ifdef HOSTED_TEST_ENVIRONMENT
void AudioUseCase_TestResetInstance(void)
{
    free(instances.array);
    instances.array = NULL;
    instances.length = 0;
}
#endif
