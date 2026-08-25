/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       pddu_map.h
\defgroup   pddu_map PDDU Map
\ingroup    common_domain
\brief      List of PDDUs
 
Single list to avoid accidental duplicates.
*/

#ifndef PDDU_MAP_H_
#define PDDU_MAP_H_

/*! @{ */

/*! \brief PDDU ID Map */
typedef enum
{
    PDDU_ID_BT_DEVICE = 0,            /*!< BT Device */
    PDDU_ID_USER_ACCOUNTS,            /*!< User Account Keys */
    PDDU_ID_FAST_PAIR,                /*!< Fast Pair */
    PDDU_ID_DEVICE_PSKEY,             /*!< Devcie PS Key */
    PDDU_ID_PACS,                     /*!< PACS */
    PDDU_ID_LEA_BROADCAST_MANAGER,    /*!< LE Audio Broadcast Manager */
    PDDU_ID_LEA_UNICAST_MANAGER,      /*!< LE Audio Unicast Manager */
    PDDU_ID_LEA_TMAP_CLIENT,          /*!< LE Audio Tmap Client */
    PDDU_ID_LEA_CALL_CLIENT_CONTROL,  /*!< LE Audio Call Client Control */
    PDDU_ID_LEA_MEDIA_CLIENT_CONTROL, /*!< LE Audio Media Client Control */
    PDDU_ID_CSIP_SET_MEMBER,          /*!< CSOP Set Member */
    PDDU_ID_UI_USER_CONFIG,           /*!< UI User Config */
	PDDU_ID_HANDSET_SERVICE,          /*!< Handset Service Configuration */
} pddu_id_map_t;

/*! @} */

#endif /* PDDU_MAP_H_ */
