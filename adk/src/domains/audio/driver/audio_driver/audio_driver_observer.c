/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Audio Driver core observer implementation

*/

#include "audio_driver_observer.h"
#include "audio_driver_indications.h"
#include <audio_use_case_instance.h>
#include <logging.h>
#include <stdlib.h>

#define FOR_ALL_OBSERVERS_CALL(instance, callback_name, ...) \
    do { \
        size_t i; \
        audio_use_case_t use_case = AudioUseCase_GetUseCaseFromInstance(instance); \
        for(i = 0; i < observers.array_length; i++) \
        { \
            if (audioDriver_IsInterestedInUseCase(&observers.array[i], use_case) && observers.array[i].interface->callback_name) \
            { \
                observers.array[i].interface->callback_name(__VA_ARGS__); \
            } \
        } \
    } while(0)

typedef struct
{
    const audio_driver_observer_if *interface;
    audio_use_case_t use_case;
} observer_t;

static struct
{
    size_t array_length;
    observer_t *array;
} observers;

static inline bool audioDriver_IsInterestedInUseCase(const observer_t *observer, audio_use_case_t use_case)
{
    return (observer->use_case == audio_use_case_all) || (observer->use_case == use_case);
}

void AudioDriver_RegisterObserver(audio_use_case_t use_case, const audio_driver_observer_if *observer_if)
{
    observers.array_length++;
    observers.array = realloc(observers.array, observers.array_length * sizeof(*observers.array));
    if (observers.array == NULL)
    {
        DEBUG_LOG_PANIC("AudioDriver_RegisterObserver: Failed to allocate memory for %u observers", observers.array_length);
    }
    observers.array[observers.array_length - 1].interface = observer_if;
    observers.array[observers.array_length - 1].use_case = use_case;
}

void AudioDriver_PreparingInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, PreparingInd, instance);
}

void AudioDriver_PreparedInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, PreparedInd, instance);
}

void AudioDriver_StartingInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, StartingInd, instance);
}

void AudioDriver_StartedInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, StartedInd, instance);
}

void AudioDriver_StoppingInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, StoppingInd, instance);
}

void AudioDriver_StoppedInd(audio_use_case_instance_t instance)
{
    FOR_ALL_OBSERVERS_CALL(instance, StoppedInd, instance);
}

void AudioDriver_BeginIoctlInd(audio_use_case_instance_t instance, unsigned ioctl_id)
{
    FOR_ALL_OBSERVERS_CALL(instance, BeginIoctlInd, instance, ioctl_id);
}

void AudioDriver_EndIoctlInd(audio_use_case_instance_t instance, unsigned ioctl_id)
{
    FOR_ALL_OBSERVERS_CALL(instance, EndIoctlInd, instance, ioctl_id);
}

#ifdef HOSTED_TEST_ENVIRONMENT
void AudioDriver_ObserverTestReset(void)
{
    free(observers.array);
    observers.array = NULL;
    observers.array_length = 0;
}
#endif
