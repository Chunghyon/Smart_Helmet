/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       hidd_profile_private.h
\brief      HID Profile private interface
*/
#ifndef HIDD_PROFILE_PRIVATE_H
#define HIDD_PROFILE_PRIVATE_H
#include "hidd_profile.h"
#include "hidd_profile_typedef.h"
#define HiddProfileGetTaskData() (&hiddInstance)

/*! Get HIDD ProfileState */
#define HiddProfile_GetState() (hiddInstance.state)

extern hiddInstanceTaskData hiddInstance;

/*! \brief Hidd task data */
typedef struct {
    Task client_task;
        /*! List of tasks requiring confirmation of HIDD connect requests */
    task_list_with_data_t connect_request_clients;
    /*! List of tasks requiring confirmation of HIDD disconnect requests */
    task_list_with_data_t disconnect_request_clients;
}hiddTaskData;

extern hiddTaskData hidd_profile_task_data;

/*! \brief Internal message IDs */
enum hidd_profile_internal_messages
{
    HIDD_INTERNAL_REACTIVATE_REQ = INTERNAL_MESSAGE_BASE,
    HIDD_INTERNAL_DELAYED_CONNECT_IND,
    HIDD_INTERNAL_MESSAGE_END
};

hiddInstanceTaskData * HiddProfile_GetInstanceForBdaddr(const bdaddr *addr);
hiddInstanceTaskData * HiddProfile_CreateInstance(void);

#endif // HIDD_PROFILE_PRIVATE_H
