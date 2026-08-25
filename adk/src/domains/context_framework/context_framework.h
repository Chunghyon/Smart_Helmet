/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The context framework provides a framework for abstracting general context information from the components that provide 
            it and a lightweight interface for its distribution to interested consumers. 
*/

#ifndef CONTEXT_FRAMEWORK_H_
#define CONTEXT_FRAMEWORK_H_

#include "context_types.h"

/*! \brief Context Provider callback */
typedef bool (*context_provider_callback_t)(unsigned * context_data, uint8 context_data_size);

/*! \brief Register a context provider callback.

    This API is called by the various context Providers in the system to register their
    get context callbacks with the context framework.

    These get context callbacks are used by the context framework to query a providers
    current context.

    \param context_item - The context item for which a provider is being registered.
    \param context_provider_callback - A function pointer to the get context callback.
*/
void ContextFramework_RegisterContextProvider(context_item_t context_item, context_provider_callback_t context_provider_callback);

/*! \brief Unregister all previously registered context providers

*/
void ContextFramework_UnregisterContextProviders(void);

/*! \brief Notify registered consumers of a context item update, consumers can then choose to get the updated context
     through ContextFramework_GetContextItem(..)

    \param context_item - The context item that has been updated.
*/
void ContextFramework_NotifyContextUpdate(context_item_t context_item);

/*! \brief Get context item

    \param context_item - The context item being requested.
    \param context_data - the data buffer where the context data gets copied into
    \param context_data_size - the size of the provided context data buffer
*/
bool ContextFramework_GetContextItem(context_item_t context_item, unsigned * context_data, uint8 context_data_size);

/*! \brief Register a context consumer.

    This API can be used by context consumers in order to receive context change notifications.

    \param context_item - the context item to register updates from
    \param context_consumer_task - The context consumers task handler to receive context change notifications.
*/
void ContextFramework_RegisterContextConsumer(context_item_t context_item, Task context_consumer_task);

/*! \brief Unregister all previously registered context consumers

*/
void ContextFramework_UnregisterContextConsumers(void);


#endif /* CONTEXT_FRAMEWORK_H_ */
