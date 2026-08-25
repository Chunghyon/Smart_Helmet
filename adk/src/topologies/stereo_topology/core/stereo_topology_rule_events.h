/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Definition of events which can initiate stereo topology rule processing.
*/

#ifndef STEREO_TOPOLOGY_RULE_EVENTS_H_
#define STEREO_TOPOLOGY_RULE_EVENTS_H_

#define STEREOTOP_RULE_EVENT_START                                (1ULL << 0)
#define STEREOTOP_RULE_EVENT_HANDSET_LINKLOSS                     (1ULL << 1)

#define STEREOTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET          (1ULL << 2)
#define STEREOTOP_RULE_EVENT_STOP                                 (1ULL << 3)
#define STEREOTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET         (1ULL << 4)
#define STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS (1ULL << 5)
#define STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET  (1ULL << 6)

#endif /* STEREO_TOPOLOGY_RULE_EVENTS_H_ */
