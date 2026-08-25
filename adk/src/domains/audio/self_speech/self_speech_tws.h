/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support for Self speech use cases
*/


#ifndef ANC_TWS_H
#define ANC_TWS_H

/*! \brief Check if primary device
*/
#ifdef ENABLE_SELF_SPEECH
bool SelfSpeechTws_IsPrimary(void);
#else
#define SelfSpeechTws_IsPrimary() (FALSE)
#endif

#endif /* ANC_TWS_H */
