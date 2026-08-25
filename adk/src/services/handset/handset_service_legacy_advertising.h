/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the handset legacy advertising set.
*/

#ifndef HANDSET_SERVICE_LEGACY_ADVERTISING_H_
#define HANDSET_SERVICE_LEGACY_ADVERTISING_H_


/*! \brief Initialise the handset legacy advertising module.
*/
void HandsetService_LegacyAdvertisingInit(void);

/*! \brief Update the handset legacy advertising state

    This function will select or release the handset legacy advertising set
    based on the state of the main handset service.

    \return TRUE if the state of the handset legacy advertising will change,
            FALSE otherwise.
*/
bool HandsetService_UpdateLegacyAdvertisingData(void);

#endif /* HANDSET_SERVICE_LEGACY_ADVERTISING_H_ */
