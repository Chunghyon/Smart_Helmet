/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_init.c
\brief      The initialisation and shutdown phases of rafs

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_init.h"
#include "rafs_fat.h"
#include "rafs_directory.h"
#include "rafs_message.h"
#include "rafs_utils.h"

rafs_errors_t Rafs_DoInit(void)
{
    Rafs_CreateInstanceStructure();
    Rafs_MessageInit();

    return RAFS_OK;
}

rafs_errors_t Rafs_DoShutdown(void)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();

    if( rafs_self->partition )
    {
        result = RAFS_STILL_MOUNTED;
    }
    else
    {
        Rafs_MessageShutdown();
        free(rafs_self);
        rafs_self = NULL;
        Rafs_SetTaskData(rafs_self);
    }

    return result;
}
