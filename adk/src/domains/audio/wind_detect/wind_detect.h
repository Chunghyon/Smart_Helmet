/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       wind_detect.h
\defgroup   wind_detect Wind Detect
\ingroup    audio_domain
\brief      Wind noise detection and anc gain reduction implementation header file.
*/
#ifndef WIND_DETECT_H
#define WIND_DETECT_H

/*! @{ */
typedef enum
{
    stage1_wind_released,
    stage2_wind_released,
    stage1_wind_detected,
    stage2_wind_detected,
}windDetectStatus_t;

typedef enum
{
    wind_detect_state_idle,
    wind_detect_state_analysing,
    wind_detect_state_windy,
    wind_detect_state_disable /*No wind is processed in this state*/
}windDetectState_t;

/*!
    \brief API identifying wind detection support
    \param None
    \return TRUE for feature supported otherwise FALSE
*/
#ifdef ENABLE_WIND_DETECT
#define WindDetect_IsSupported() (TRUE)
#else
#define WindDetect_IsSupported() (FALSE)
#endif

/*!
    \brief API returns status of wind noise reduction enable
    \param None
    \return TRUE for Wind noise reduction enabled otherwise FALSE
*/
#ifdef ENABLE_WIND_DETECT
bool WindDetect_IsWindNoiseReductionEnabled(void);
#else
#define WindDetect_IsWindNoiseReductionEnabled() (FALSE)
#endif

/*!
    \brief API to take kymera action of putting Wind detect capability into Wind processing mode or Standby mode
    \param  Enable/Disable Wind detect
    \return None
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_Enable(bool enable);
#else
#define WindDetect_Enable(enable) (UNUSED(enable))
#endif

/*!
    \brief API returns status of local device wind noise detection flags
    \param None
    \return Wind noise detection flag status
*/
#ifdef ENABLE_WIND_DETECT
windDetectStatus_t WindDetect_GetLocalAttackStatus(void);
#else
#define WindDetect_GetLocalAttackStatus() FALSE
#endif

/*!
    \brief API returns status of remote device wind noise detection flags
    \param None
    \return Wind noise detection flag status
*/
#ifdef ENABLE_WIND_DETECT
windDetectStatus_t WindDetect_GetRemoteAttackStatus(void);
#else
#define WindDetect_GetRemoteAttackStatus() FALSE
#endif

/*!
    \brief API used for setting the remote device wind noise status
    \param wind_released, stage1_wind_detected or stage2_wind_detected are the valid inputs
    \return None
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_SetRemoteAttackStatus(windDetectStatus_t status);
#else
#define WindDetect_SetRemoteAttackStatus(x) (UNUSED(x))
#endif

/*!
    \brief API used for setting the local device wind noise status.
    \param wind_released, stage1_wind_detected or stage2_wind_detected are the valid inputs
    \return None
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_SetLocalAttackStatus(windDetectStatus_t status);
#else
#define WindDetect_SetLocalAttackStatus(x) (UNUSED(x))
#endif

/*!
    \brief API used for handling wind detect instruction from ANC state manager.
    \param None
    \return None
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_HandleWindDetect(void);
#else
#define WindDetect_HandleWindDetect() ((void)(0))
#endif


/*!
    \brief API used for handling wind release intruction from ANC state manager.
    \param None
    \return None
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_HandleWindRelease(void);
#else
#define WindDetect_HandleWindRelease() ((void)(0))
#endif


/*!
    \brief API used wind detect feature initialization.
    \param initialization Task.
    \return Returns true when the initialization is successful.
*/
#ifdef ENABLE_WIND_DETECT
bool WindDetect_Init(Task init_task);
#else
#define WindDetect_Init(x) FALSE
#endif

/*!
    \brief API used for reseting wind detect feature flags.
    \param None.
    \return None.
*/
#ifdef ENABLE_WIND_DETECT
void WindDetect_Reset(void);
#else
#define WindDetect_Reset() ((void)(0))
#endif

/*! @} */

#endif /* WIND_DETECT_H */
