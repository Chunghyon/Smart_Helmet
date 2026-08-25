/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage audio use case sources

*/

#include "audio_use_case_source.h"
#include "audio_use_case_instance.h"
#include <logging.h>
#include <stdlib.h>

typedef struct
{
    generic_source_t source;
    audio_use_case_instance_t instance;
} source_map_t;

static struct
{
    unsigned length;
    source_map_t *array;
} source_map = {0};

static audio_use_case_instance_t audioUseCase_GetInstanceForSource(generic_source_t source)
{
    unsigned i;
    for(i = 0; i < source_map.length; i++)
    {
        if (GenericSource_IsSame(source_map.array[i].source, source))
        {
            return source_map.array[i].instance;
        }
    }
    return NULL;
}

static inline void audioUseCase_AssertNewSource(generic_source_t source)
{
    audio_use_case_instance_t instance = audioUseCase_GetInstanceForSource(source);
    if (instance)
    {
        DEBUG_LOG_PANIC("audioUseCase_AssertNewSource: Preexisting instance %p for source enum:source_type_t:%d %d", instance, source.type, source.u);
    }
}

void AudioUseCase_AssociateSourceWithInstance(generic_source_t source, audio_use_case_instance_t instance)
{
    audioUseCase_AssertNewSource(source);
    source_map.array = PanicNull(realloc(source_map.array, sizeof(source_map.array[0]) * (source_map.length + 1)));
    source_map.array[source_map.length].source = source;
    source_map.array[source_map.length].instance = instance;
    source_map.length++;
}

audio_use_case_instance_t AudioUseCase_GetInstanceForSource(generic_source_t source)
{
    audio_use_case_instance_t instance = audioUseCase_GetInstanceForSource(source);
    if (instance == NULL)
    {
        DEBUG_LOG_PANIC("AudioUseCase_GetInstanceForSource: No instance for source enum:source_type_t:%d %d", source.type, source.u);
    }
    return instance;
}

#ifdef HOSTED_TEST_ENVIRONMENT
void AudioUseCase_TestResetSource(void)
{
    free(source_map.array);
    source_map.array = NULL;
    source_map.length = 0;
}
#endif
