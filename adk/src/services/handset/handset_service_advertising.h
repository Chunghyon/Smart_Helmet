/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef HANDSET_SERVICE_ADVERTISING_H_
#define HANDSET_SERVICE_ADVERTISING_H_

/*! \brief Update the decision to advertise based on the current
     and max number of LE connections

    \return TRUE if the update was successful, else FALSE
*/
bool HandsetService_UpdateAdvertising(void);

#endif /* HANDSET_SERVICE_ADVERTISING_H_ */
