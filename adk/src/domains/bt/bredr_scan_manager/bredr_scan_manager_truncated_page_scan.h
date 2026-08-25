/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief	    Interface to control BREDR truncated scan functionality
*/

extern TaskData truncated_scan_task_data;

#define BredrScanManager_TruncatedPageScanGetTask() (&truncated_scan_task_data)

enum
{
    BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST

}BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_T;

typedef struct
{
    Task client;
    bredr_scan_manager_scan_type_t type;

}BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST_T;
