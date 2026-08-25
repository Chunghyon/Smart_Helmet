/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This file abstracts the API calls for Synergy and ADK libraries and 
*           exposes common interface to application. Presently only a subset of the APIs
*           (browsing and metadata) are abstracted however other AVRCP APIs can also be 
*           abstracted in the future.
*/
#ifdef INCLUDE_AV
#include <logging.h>
#include <avrcp.h>
#include <stream.h>
#include "avrcp_profile_abstraction.h"

/***************************** function defs ****************************************/

#ifdef INCLUDE_AVRCP_METADATA
void AvrcpProfileAbstract_EventTrackChangedResponse(avInstanceTaskData *the_inst,
                                                avrcp_response_type    response,
                                                uint32                 msgid,
                                                uint32              track_index_high,
                                                uint32              track_index_low)
{
    AvrcpEventTrackChangedResponse(the_inst->avrcp.avrcp, response, track_index_high, track_index_low);
    UNUSED(msgid);
}

void AvrcpProfileAbstract_GetPlayStatusResponse(avInstanceTaskData *the_inst,
                                            avrcp_response_type response,
                                            const AVRCP_GET_PLAY_STATUS_IND_T *ind)
{
    AvrcpGetPlayStatusResponse(the_inst->avrcp.avrcp, response,
                        UINT32_MAX, 0, the_inst->avrcp.play_status);
    UNUSED(ind);
}

void AvrcpProfileAbstract_GetElementAttributesResponse(avInstanceTaskData *the_inst,
                                            avrcp_response_type response,
                                            uint16 num_of_attributes,
                                            uint16 attr_length,
                                            uint8 *attr_data,
                                            TaskData *cleanup_task,
                                            const AVRCP_GET_ELEMENT_ATTRIBUTES_IND_T *ind)
{

    Source src_pdu = 0;

    UNUSED(ind);
    /* Create a source from the data */
    src_pdu = StreamRegionSource(attr_data, attr_length);
    MessageStreamTaskFromSink(StreamSinkFromSource(src_pdu), cleanup_task);

    AvrcpGetElementAttributesResponse(the_inst->avrcp.avrcp, response,
        num_of_attributes, attr_length, src_pdu);
}
#endif /* INCLUDE_AVRCP_METADATA */

#ifdef INCLUDE_AVRCP_BROWSING
void AvrcpProfileAbstract_EventAddressedPlayerChangedResponse(avInstanceTaskData *the_inst,
                            avrcp_response_type response,
                            uint32                 msgid)
{
    AvrcpEventAddressedPlayerChangedResponse(the_inst->avrcp.avrcp, response, 1, 0);
    UNUSED(msgid);
}

void AvrcpProfileAbstract_BrowseGetFolderItemsResponse(avInstanceTaskData *the_inst,
                                        avrcp_response_type response,
                                        uint16              uid_counter, 
                                        uint16              num_items,  
                                        uint16              item_list_size,
                                        uint8 *item, 
                                        TaskData *cleanup_task,
                                        const AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *ind)
{
    Source src_pdu=0;
    /* Create a source from the data */
    src_pdu = StreamRegionSource(item, item_list_size);
    MessageStreamTaskFromSink(StreamSinkFromSource(src_pdu), cleanup_task);
    AvrcpBrowseGetFolderItemsResponse(the_inst->avrcp.avrcp, response, uid_counter, num_items, item_list_size, src_pdu);
    UNUSED(ind);
}

void AvrcpProfileAbstract_SetAddressedPlayerResponse(avInstanceTaskData *the_inst,
                                                const AVRCP_SET_ADDRESSED_PLAYER_IND_T *ind)
{
    avrcp_response_type res_type = avrcp_response_rejected_invalid_player_id;
    uint32  player_id;
    player_id = ind->player_id;
   if(player_id != 0xffffu)
   {
       DEBUG_LOG("AvrcpCalSetAddressedPlayerResponse: Request for AVRCP Addressed Played id %d\n", player_id);
       res_type = avctp_response_accepted;
   }

    AvrcpSetAddressedPlayerResponse(the_inst->avrcp.avrcp, res_type);
}

void AvrcpProfileAbstract_BrowseGetNumberOfItemsResponse( avInstanceTaskData         * the_inst  ,
                                        avrcp_response_type response,
                                        uint16              uid_counter,
                                        uint32              num_items,
                                        const AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *ind)

{
    AvrcpBrowseGetNumberOfItemsResponse(ind->avrcp, response, uid_counter, num_items);
    UNUSED(the_inst);
}

void AvrcpProfileAbstract_BrowseConnectResponse( avInstanceTaskData *the_inst,
                                 uint16 connection_id,
                                 uint16 signal_id,
                                 bool accept)
{

    AvrcpBrowseConnectResponse(the_inst->avrcp.avrcp, connection_id, signal_id, accept);
}
#endif /* INCLUDE_AVRCP_BROWSING */
#else
static const int compiler_happy;
#endif  /* INCLUDE_AV */

