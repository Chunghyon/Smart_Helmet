/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AVRCP component that abstract the profile APIs based on the 
*           library that was enabled for building the applictaion.
*/

#ifndef AVRCP_PROFILE_ABSTRACTION_H_
#define AVRCP_PROFILE_ABSTRACTION_H_

#include "av_typedef.h"


void AvrcpProfileAbstract_EventTrackChangedResponse(avInstanceTaskData *the_inst, 
                                                avrcp_response_type    response, 
                                                uint32                 msgid,
                                                uint32              track_index_high, 
                                                uint32              track_index_low);

void AvrcpProfileAbstract_EventAddressedPlayerChangedResponse(avInstanceTaskData *the_inst, 
                            avrcp_response_type response, 
                            uint32                 msgid);

void AvrcpProfileAbstract_BrowseGetFolderItemsResponse(avInstanceTaskData *the_inst,
                                        avrcp_response_type response,
                                        uint16              uid_counter, 
                                        uint16              num_items,  
                                        uint16              item_list_size,
                                        uint8 *item, 
                                        TaskData *cleanup_task,
                                        const AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *ind);

void AvrcpProfileAbstract_SetAddressedPlayerResponse(avInstanceTaskData *the_inst, 
                                                const AVRCP_SET_ADDRESSED_PLAYER_IND_T *ind);

void AvrcpProfileAbstract_GetPlayStatusResponse(avInstanceTaskData *the_inst, 
                                            avrcp_response_type response, 
                                            const AVRCP_GET_PLAY_STATUS_IND_T *ind) ; 

void AvrcpProfileAbstract_GetElementAttributesResponse(avInstanceTaskData *the_inst, 
                                            avrcp_response_type response, 
                                            uint16 num_of_attributes, 
                                            uint16 attr_length, 
                                            uint8 *attr_data,
                                            TaskData *cleanup_task, 
                                            const AVRCP_GET_ELEMENT_ATTRIBUTES_IND_T *ind);

void AvrcpProfileAbstract_BrowseGetNumberOfItemsResponse( avInstanceTaskData         * the_inst  ,
                                        avrcp_response_type response,     
                                        uint16              uid_counter, 
                                        uint32              num_items, 
                                        const AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *ind);

void AvrcpProfileAbstract_BrowseConnectResponse( avInstanceTaskData *the_inst, 
                                 uint16 connection_id, 
                                 uint16 signal_id,
                                 bool accept);

#endif /* AVRCP_PROFILE_ABSTRACTION_H_ */

