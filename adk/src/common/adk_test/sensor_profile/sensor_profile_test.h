/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sensor profile test module.
*/

#ifdef INCLUDE_SENSOR_PROFILE

#ifndef SENSOR_PROFILE_TEST_H
#define SENSOR_PROFILE_TEST_H

#include <stdint.h>

/*! \brief Register the Sensor service/app.
    To be called in both Primary and Secondary earbuds.

    \return TRUE if call succeeded; FALSE if it failed.
*/
bool SensorProfileTest_Register(void);

/*! \brief Start the Sensor streaming service.
    To be called only in Primary earbud.

    \return TRUE if call succeeded; FALSE if it failed.
*/
bool SensorProfileTest_Enable(void);

/*! \brief Stop the Sensor streaming service.
    To be called only in Primary earbud.

    \return TRUE if call succeeded; FALSE if it failed.
*/
bool SensorProfileTest_Disable(void);

/*! \brief Start the Peer Sensor streaming service.
    To be called only in Primary earbud.

    \return TRUE if call succeeded; FALSE if it failed.
*/
bool SensorProfileTest_EnablePeer(void);

/*! \brief Stop the Peer Sensor streaming service.
    To be called only in Primary earbud.

    \return TRUE if call succeeded; FALSE if it failed.
*/
bool SensorProfileTest_DisablePeer(void);

/* Test utilities */
/*! \brief Function to query the number of packets this earbud has sent
           to its peer since the client invoked SensorProfile_Test_Enabled.
           Counter is reset to zero when SensorProfile is Connected and
           when SensorProfile_Test_Disabled is invoked.

    \return Number of packets sent to peer since the traffic was Enabled.
            Zero if traffic is Disabled.
*/
uint32_t SensorProfileTest_PacketCounter(void);

/*! \brief Function to query whether or not Sensor Profile is connected.

    \return TRUE if Sensor Profile is connected; FALSE if not.
*/
bool SensorProfileTest_Connected(void);

#endif /* SENSOR_PROFILE_TEST_H */

#endif /* INCLUDE_SENSOR_PROFILE */
