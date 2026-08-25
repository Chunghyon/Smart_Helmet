/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functionality to include GAIA in LE advertising data
*/

#ifndef GAIA_FRAMEWORK_ADVERTISING_H_
#define GAIA_FRAMEWORK_ADVERTISING_H_

/*! \brief Setup the LE advertising data for the GAIA framework

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool GaiaFramework_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for the sGAIA framework

    \returns TRUE if the update was successful, else FALSE
 */
bool GaiaFramework_UpdateLeAdvertisingData(void);

#endif /* GAIA_FRAMEWORK_ADVERTISING_H_ */
