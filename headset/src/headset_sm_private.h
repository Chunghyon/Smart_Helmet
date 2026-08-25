/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_sm_private.h
\brief      Internal interface for SM components.
*/

#ifndef HEADSET_SM_PRIVATE_H_
#define HEADSET_SM_PRIVATE_H_

/*! \brief Application state machine internal message IDs */
enum sm_internal_message_ids
{
    SM_INTERNAL_PAIR_HANDSET,                       /*!< Start pairing with a handset. */
    SM_INTERNAL_DELETE_HANDSETS,                    /*!< Delete all paired handsets. */
    SM_INTERNAL_FACTORY_RESET,                      /*!< Reset device to factory defaults. */
    SM_INTERNAL_LINK_DISCONNECTION_COMPLETE,        /*!< All links are now disconnected */
    SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION,         /*!< Timeout if link disconnection takes too long. */
    SM_INTERNAL_POWER_OFF,                          /*!< power OFF headset */
    SM_INTERNAL_TIMEOUT_IDLE,                       /*!< Timeout when idle and no bt connection */
    SM_INTERNAL_TIMEOUT_LIMBO,                      /*!< Timeout when in limbo state and no charger connected*/
};

#endif /* HEADSET_SM_PRIVATE_H_ */
