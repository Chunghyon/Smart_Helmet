/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_setup_tracker.h
\brief      AMA setup tracker APIs
*/
#ifndef AMA_SETUP_TRACKER_H
#define AMA_SETUP_TRACKER_H

/*! \brief Perform any necessary actions when notified that AMA setup on the connected handset has been completed.
*/
void Ama_CompleteSetup(void);

/*! \brief Determine if AMA has completed setup via the connect handset.
    \return TRUE if AMA has completed setup, FALSE otherwise.
*/
bool Ama_IsSetupComplete(void);

#endif // AMA_SETUP_TRACKER_H
