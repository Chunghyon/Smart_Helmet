/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "context_framework.h"

#include <logging.h>
#include <message.h>
#include <stdlib.h>

#define CONTEXT_FRAMEWORK_LOG   DEBUG_LOG_VERBOSE

typedef struct
{
    context_item_t context_item;
    context_provider_callback_t context_provider_callback;
} context_framework_provider_t;

static context_framework_provider_t * registered_context_providers = NULL;
static uint8 number_of_context_providers = 0;

typedef struct
{
    Task consumer_task;
    context_item_t context_item;
} context_framework_consumer_t;

static context_framework_consumer_t * registered_context_consumers = NULL;
static unsigned number_of_context_consumers = 0;

static context_framework_provider_t * contextFramework_GetContextProviderForItem(context_item_t context_item)
{
    context_framework_provider_t * provider = NULL;
    if(registered_context_providers)
    {
        for(int i=0; i < number_of_context_providers; i++)
        {
            if(registered_context_providers[i].context_item == context_item)
            {
                provider = &registered_context_providers[i];
                break;
            }
        }
    }
    return provider;
}

void ContextFramework_RegisterContextProvider(context_item_t context_item, context_provider_callback_t context_provider_callback)
{
    CONTEXT_FRAMEWORK_LOG("ContextFramework_RegisterContextProvider enum:context_item_t:%d", context_item);

    if(context_provider_callback == NULL || context_item >= max_context_items || contextFramework_GetContextProviderForItem(context_item))
    {
        Panic();
    }

    size_t new_size = sizeof(context_framework_provider_t) * (number_of_context_providers + 1);
    registered_context_providers = PanicNull(realloc(registered_context_providers, new_size));

    context_framework_provider_t * new_provider = &registered_context_providers[number_of_context_providers];
    new_provider->context_item = context_item;
    new_provider->context_provider_callback = context_provider_callback;
    number_of_context_providers++;
}

void ContextFramework_NotifyContextUpdate(context_item_t context_item)
{
    CONTEXT_FRAMEWORK_LOG("ContextFramework_NotifyContextUpdate enum:context_item_t:%d", context_item);

    for(int i=0; i < number_of_context_consumers; i++)
    {
        if (registered_context_consumers[i].context_item == context_item &&
                registered_context_consumers[i].consumer_task != NULL)
        {
            MessageSend(registered_context_consumers[i].consumer_task, context_item, NULL);
        }
    }
}

void ContextFramework_UnregisterContextProviders(void)
{
    CONTEXT_FRAMEWORK_LOG("ContextFramework_UnregisterContextProviders");
    free(registered_context_providers);
    registered_context_providers = NULL;
    number_of_context_providers = 0;
}

bool ContextFramework_GetContextItem(context_item_t context_item, unsigned * context_data, uint8 context_data_size)
{
    PanicNull(context_data);
    PanicZero(context_data_size);
    bool context_is_valid = FALSE;
    context_framework_provider_t * provider = contextFramework_GetContextProviderForItem(context_item);
    if(provider)
    {
        context_is_valid = provider->context_provider_callback(context_data, context_data_size);
    }
    return context_is_valid;
}

static bool contextFramework_IsConsumerRegistered(context_item_t context_item, Task context_consumer_task)
{
    bool is_registered = FALSE;
    for(int i=0; i < number_of_context_consumers; i++)
    {
        if (context_item == registered_context_consumers[i].context_item &&
                context_consumer_task == registered_context_consumers[i].consumer_task)
        {
            is_registered = TRUE;
        }
    }
    return is_registered;
}

void ContextFramework_RegisterContextConsumer(context_item_t context_item, Task context_consumer_task)
{
    CONTEXT_FRAMEWORK_LOG("ContextFramework_RegisterContextConsumer item=%d task=%p", context_item, context_consumer_task);
    PanicNull(context_consumer_task);
    PanicZero(context_item < max_context_items);

    if(!contextFramework_IsConsumerRegistered(context_item, context_consumer_task))
    {
        size_t new_size;
        context_framework_consumer_t * new_consumer;

        new_size = sizeof(context_framework_consumer_t) * (number_of_context_consumers + 1);
        registered_context_consumers = PanicNull(realloc(registered_context_consumers, new_size));

        new_consumer = registered_context_consumers + number_of_context_consumers;
        new_consumer->consumer_task = context_consumer_task;
        new_consumer->context_item = context_item;

        number_of_context_consumers++;
    }
    else
    {
        Panic();
    }
}

void ContextFramework_UnregisterContextConsumers(void)
{
    CONTEXT_FRAMEWORK_LOG("ContextFramework_UnregisterContextConsumers");
    free(registered_context_consumers);
    registered_context_consumers = NULL;
    number_of_context_consumers = 0;
}

