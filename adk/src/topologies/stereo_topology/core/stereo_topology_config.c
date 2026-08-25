/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Configuration / parameters used by the topology.
*/

#include "stereo_topology_config.h"
#include "rtime.h"

static const bredr_scan_manager_scan_parameters_set_t stereo_inquiry_scan_params_set[] =
{
    {
        {
            [SCAN_MAN_PARAMS_TYPE_SLOW] = { .interval = US_TO_BT_SLOTS(2560000), .window = US_TO_BT_SLOTS(11250) },
            [SCAN_MAN_PARAMS_TYPE_FAST] = { .interval = US_TO_BT_SLOTS(320000),  .window = US_TO_BT_SLOTS(11250) },
        },
    },
};

static const bredr_scan_manager_scan_parameters_set_t stereo_page_scan_params_set[] =
{
    {
        {
            [SCAN_MAN_PARAMS_TYPE_SLOW] = { .interval = US_TO_BT_SLOTS(1280000), .window = US_TO_BT_SLOTS(11250), .scan_type = hci_scan_type_interlaced },
            [SCAN_MAN_PARAMS_TYPE_FAST] = { .interval = US_TO_BT_SLOTS(100000),  .window = US_TO_BT_SLOTS(11250), .scan_type = hci_scan_type_interlaced },
            [SCAN_MAN_PARAMS_TYPE_THROTTLE] = { .interval = US_TO_BT_SLOTS(1280000), .window = US_TO_BT_SLOTS(11250), .scan_type = hci_scan_type_standard },
        },
    },
};

const bredr_scan_manager_parameters_t stereo_inquiry_scan_params =
{
    stereo_inquiry_scan_params_set, ARRAY_DIM(stereo_inquiry_scan_params_set)
};

const bredr_scan_manager_parameters_t stereo_page_scan_params =
{
    stereo_page_scan_params_set, ARRAY_DIM(stereo_page_scan_params_set)
};
