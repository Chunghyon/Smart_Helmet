/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of metadata specific avrcp functionality

*/

/* Only compile if INCLUDE_AVRCP_METADATA defined */
#ifdef INCLUDE_AVRCP_METADATA

#include "avrcp_profile_metadata.h"
#include <av.h>
#include <stream.h>
#include <panic.h>
#include <logging.h>
#include <avrcp_profile_abstraction.h>

#define GET_ELEMENT_LARGE_ATT_RESPONSE_LENGTH 528
#define GET_ELEMENT_LARGE_ATT_NUMBER_OF_ITEMS 1

#define GET_ELEMENT_ATT_RESPONSE_LENGTH 64
#define GET_ELEMENT_ATT_NUMBER_OF_ITEMS 2

typedef struct
{
    bool is_track_selected;
    uint8* cleanup_data;
    bool use_large_metadata;

} avrcp_metadata_data_t;

avrcp_metadata_data_t avrcp_metadata_data =
{
    .is_track_selected = FALSE,
    .cleanup_data = NULL,
    .use_large_metadata = FALSE,
};

#define AvrcpMetadata_GetTaskData() (&avrcp_metadata_data)


static void avrcpMetadata_CleanUp(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    switch (id)
    {
    case MESSAGE_SOURCE_EMPTY:
    {
        /* Free the previously stored data ptr. */
        if (AvrcpMetadata_GetTaskData()->cleanup_data)
        {
            free(AvrcpMetadata_GetTaskData()->cleanup_data);
        }
        AvrcpMetadata_GetTaskData()->cleanup_data = NULL;
    }
        break;
    default:
        break;
    }
}

TaskData metadata_cleanup_task = {avrcpMetadata_CleanUp};

void AvrcpMetadata_SetTrackSelected(bool is_selected)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_SetTrackSelected is_selected:%d", is_selected);

    AvrcpMetadata_GetTaskData()->is_track_selected = is_selected;
}

bool AvrcpMetadata_SendTrackChange(const bdaddr* bt_addr, uint32 high_index, uint32 low_index)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_SendTrackChange");

    avInstanceTaskData * av_instance;
    av_instance = appAvInstanceFindFromBdAddr(bt_addr);
    if (av_instance == NULL)
    {
        DEBUG_LOG_VERBOSE("AvrcpMetadata_SendTrackChange av instance cannot be NULL");
        return FALSE;
    }
    if (av_instance->avrcp.avrcp == NULL)
    {
        return FALSE;
    }

    AvrcpProfileAbstract_EventTrackChangedResponse(av_instance, avctp_response_changed, 0, high_index, low_index);

    return TRUE;
}

void AvrcpMetadata_HandleGetElementAttributesInd(avInstanceTaskData *the_inst, AVRCP_GET_ELEMENT_ATTRIBUTES_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_HandleGetElementAttributesInd");

    uint8* attr_data;
    uint16 attr_length = 0;
    uint16 num_of_attributes = 0;   
    avrcp_response_type response = avctp_response_stable;
    const char csr_attribute[] = "CSR"; 
    const char qualification_attribute[] = "PTS-QUALIFICATION-FOR-TG-FRAGMENTATION-TEST";

    PanicNull(the_inst);
    
    attr_length = AvrcpMetadata_GetTaskData()->use_large_metadata? GET_ELEMENT_LARGE_ATT_RESPONSE_LENGTH : GET_ELEMENT_ATT_RESPONSE_LENGTH;
    
    /* Allocate memory for report */
    attr_data = PanicUnlessMalloc(attr_length);

    if (AvrcpMetadata_GetTaskData()->use_large_metadata)
    {
        num_of_attributes = GET_ELEMENT_LARGE_ATT_NUMBER_OF_ITEMS;
        attr_data[3] =0x1; /* Attribute ID */
        attr_data[5] = 0x6A; /* Character Set - UTF-8 */
        attr_data[6] = 0x02;/* Attribute length 520 */
        attr_data[7] = 0x08;
        int increment;
        for(increment = 8; increment < GET_ELEMENT_LARGE_ATT_RESPONSE_LENGTH; increment++)
        {
            attr_data[increment] = 0x17 /* UTF-8 "#" */;
        }
    }
    else
    {
        num_of_attributes = GET_ELEMENT_ATT_NUMBER_OF_ITEMS;
        attr_data[3] =0x1; /* Attribute ID */
        attr_data[5] = 0x6A; /* Character Set - UTF-8 */
        attr_data[7] = 0x04;/* Attribute length */
        strncpy((char*)&attr_data[8], csr_attribute, sizeof(csr_attribute));

        attr_data[15] =0x1; /* Attribute ID */
        attr_data[17] = 0x6A; /* Character Set - UTF-8 */
        attr_data[19] = 0x2c;/* Attribute length 44  bytes*/
        strncpy((char*)&attr_data[20], qualification_attribute, sizeof(qualification_attribute));
    }

    /* The clean-up task clears this so should be NULL at this point */
    PanicNotNull(AvrcpMetadata_GetTaskData()->cleanup_data);

    /* Register a task for freeing the data and store a ptr to it */
    AvrcpMetadata_GetTaskData()->cleanup_data = attr_data;

    AvrcpProfileAbstract_GetElementAttributesResponse(the_inst, response,
        num_of_attributes, attr_length, attr_data, &metadata_cleanup_task, ind);
}

void AvrcpMetadata_HandleEventTrackChanged(avInstanceTaskData *the_inst, avrcp_response_type response, uint32 msgid)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_HandleEventTrackChanged");
    if (AvrcpMetadata_GetTaskData()->is_track_selected)
    {
        AvrcpProfileAbstract_EventTrackChangedResponse(the_inst, response, 0, 0, msgid);
    }
    else
    {
        AvrcpProfileAbstract_EventTrackChangedResponse(the_inst, response, UINT32_MAX, UINT32_MAX, msgid);
    }
}

void AvrcpMetadata_HandleGetPlayStatusInd(avInstanceTaskData *the_inst, AVRCP_GET_PLAY_STATUS_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_HandleGetPlayStatusInd(%p)", the_inst);

    AvrcpProfileAbstract_GetPlayStatusResponse(the_inst, avctp_response_stable, ind);
}

void AvrcpMetadata_SetPlayStatus(avInstanceTaskData *the_inst, avrcp_play_status play_status)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_SetPlayStatus(%p): Status:enum:avrcp_play_status:%d", the_inst, play_status);

    PanicNull(the_inst);

    the_inst->avrcp.play_status = play_status;
}

void AvrcpMetadata_SetLargeMetadata(bool use_large_metadata)
{
    DEBUG_LOG_VERBOSE("AvrcpMetadata_SetLargeMetadata: %d", use_large_metadata);

    AvrcpMetadata_GetTaskData()->use_large_metadata = use_large_metadata;
}

#endif /* INCLUDE_AVRCP_METADATA */
