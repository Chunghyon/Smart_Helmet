/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       ps_key_map.h
\defgroup   ps_key_map PS Key Map
\ingroup    common_domain
\brief      Map of user PS Keys.
 
Add a PS Key here if it is going to be used.
If there is possibility that a PS Key would need to be populated during deployment
then it should also be added to subsys7_psflash.htf.
 
*/

#ifndef PS_KEY_MAP_H_
#define PS_KEY_MAP_H_


/*! @{ */

/*!  \brief Macro that applies offset of 100 to user PS Keys 50 to 99.
    
    The second group of user PS Key is not located
    immediately after the first group, hence the offset.
*/
#define PS_MAPPED_USER_KEY(key) (((key)<50)?(key):100+(key))

typedef enum
{
    /*! \brief Reserved PS Key, don't use*/
    PS_KEY_RESERVED = PS_MAPPED_USER_KEY(0),

    /*! \brief HFP volumes
       HFP won't use this key anymore,
       but don't use it for other purposes to not affect upgrade
       from the previous ADKs.
    */
    PS_KEY_HFP_CONFIG = PS_MAPPED_USER_KEY(1),

    /*! \brief Fixed role setting */
    PS_KEY_FIXED_ROLE = PS_MAPPED_USER_KEY(2),

    /*! \brief Device Test Service enable */
    PS_KEY_DTS_ENABLE = PS_MAPPED_USER_KEY(3),

    /*! \brief Reboot action PS Key */
    PS_KEY_REBOOT_ACTION = PS_MAPPED_USER_KEY(4),

    /*! \brief Fast Pair Model Id */
    PS_KEY_FAST_PAIR_MODEL_ID = PS_MAPPED_USER_KEY(5),

    /*! \brief Fast Pair Scrambled ASPK */
    PS_KEY_FAST_PAIR_SCRAMBLED_ASPK = PS_MAPPED_USER_KEY(6),

    /*! \brief Upgrade PS Key */
    PS_KEY_UPGRADE = PS_MAPPED_USER_KEY(7),

    /*! \brief GAA Model Id */
    PS_KEY_GAA_MODEL_ID = PS_MAPPED_USER_KEY(8),

    /*! \brief User EQ selected bank and gains */
    PS_KEY_USER_EQ = PS_MAPPED_USER_KEY(9),

    /*! \brief GAA OTA control */
    PS_KEY_GAA_OTA_CONTROL = PS_MAPPED_USER_KEY(10),

    PS_KEY_BATTERY_STATE_OF_CHARGE = PS_MAPPED_USER_KEY(11),

    /*! \brief ANC session data */
    PS_KEY_ANC_SESSION_DATA = PS_MAPPED_USER_KEY(12),

    /*! \brief PS Key used to store ANC delta gain (in DB) between
     * ANC golden gain configuration and calibrated gain during
     * production test: FFA, FFB and FB
     */
    PS_KEY_ANC_FINE_GAIN_TUNE_KEY = PS_MAPPED_USER_KEY(13),

    PS_KEY_EARBUD_DEVICES_BACKUP = PS_MAPPED_USER_KEY(14),

    /*! \brief PS Key used for various charger comms controls.
     * This key can be extended for any debug, test or general
     * configurations are exposed.
     * Current format is:
     * Index [0]: If set, will enable debug-over-charger-comms and
     *            disable entering dormant mode when in the charging
     *            case. This is only applicable to SchemeB.
     */
    PS_KEY_CHARGER_COMMS_CONTROL = PS_MAPPED_USER_KEY(15),
    
    /*! \brief PS Keys used to store per device data */
    PS_KEY_DEVICE_PS_KEY_FIRST = PS_MAPPED_USER_KEY(20),
    PS_KEY_DEVICE_PS_KEY_LAST = PS_MAPPED_USER_KEY(29),

    /*! \brief Version of PS Key layout */
    PS_KEY_DATA_VERSION = PS_MAPPED_USER_KEY(50),

    /*! \brief Setting for testing AV Codec in test mode */
    PS_KEY_TEST_AV_CODEC = PS_MAPPED_USER_KEY(80),

    /*! \brief Setting for testing HFP Codec in test mode */
    PS_KEY_TEST_HFP_CODEC = PS_MAPPED_USER_KEY(81),
} ps_key_map_t;

/*! @} */

#endif /* PS_KEY_MAP_H_ */
