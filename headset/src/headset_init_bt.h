/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_init_bt.h
\brief      Header file for initialisation
*/

#ifndef HEADSET_INIT_BT_H
#define HEADSET_INIT_BT_H


#include <message.h>


#define INIT_CL_CFM CL_INIT_CFM
#define INIT_READ_LOCAL_BD_ADDR_CFM CL_DM_LOCAL_BD_ADDR_CFM


bool HeadsetInitBt_ConnectionInit(Task init_task);

TaskData *HeadsetInitBt_GetTask(void);
void HeadsetInitBt_StartBtInit(void);

bool HeadsetInitBt_InitHandleClDmLocalBdAddrCfm(Message message);



#endif // HEADSET_INIT_BT_H
