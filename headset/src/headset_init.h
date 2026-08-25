/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_init.h
\brief      Header file for initialisation module
*/


#ifndef HEADSET_INIT_H
#define HEADSET_INIT_H

 /*! \brief Initialisation module data */
typedef struct
{
    TaskData task;              /*!< Init's local task */
} initData;

/*!< Structure used while initialising */
extern initData    headset_init;

/*! Get pointer to init data structure */
#define InitGetTaskData()        (&headset_init)


/*! \brief Initialise init module
    This function is the start of all initialisation.

    When the initialisation sequence has completed #INIT_CFM will
    be sent to the main app Task.
*/
void HeadsetInit_StartInitialisation(void);


/*! Let the application initialisation function know that initialisation is complete

    This should be called on receipt of the #APPS_COMMON_INIT_CFM message.
 */
void HeadsetInit_CompleteInitialisation(void);


#endif /* HEADSET_INIT_H */

