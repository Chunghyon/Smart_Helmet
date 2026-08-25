/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_anc.h
\brief      File consists of function decalration for Amazon Voice Service's ANC handling.
*/
#ifndef AMA_ANC_H
#define AMA_ANC_H

/*! \brief Initialize the AMA ANC module.
*/
bool AmaAnc_Init(void);

/*! \brief Update the ANC enabled status
    \param[in] bool Enabled
*/
void AmaAnc_AncEnableUpdate(bool enabled);

/*! \brief Update the Passthrough enabled status
    \param[in] bool Enabled
*/
void AmaAnc_PassthroughEnableUpdate(bool enabled);

/*! \brief Update the Passthrough level
    \param[in] uint8 Passthrough level as a percentage
*/
void AmaAnc_PassthroughLevelUpdate(uint8 level_as_percentage);

#endif

