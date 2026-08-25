/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Data types for describing the current context of a handset device.
*/

#ifndef HANDSET_CONTEXT_H
#define HANDSET_CONTEXT_H

typedef enum
{
    /*! Undefined or no context for the handset. */
    handset_context_none = 0,

    /*! Handset was disconnected. */
    handset_context_disconnected,

    /*! Handset was disconnected due to link loss. */
    handset_context_link_loss,

    /*! Handset is currently reconnecting. */
    handset_context_reconnecting,

    /*! Handset is currently connecting. */
    handset_context_connecting,

    /*! Handset has ACL connected. */
    handset_context_acl_connected,

    /*! Handset has profiles connected. */
    handset_context_profiles_connected,

    /*! Last handset connection attempt failed. */
    handset_context_not_available,

    /*! Handset has initiated barge-in connection. */
    handset_context_barge_in

} handset_context_t;

#endif // HANDSET_CONTEXT_H
