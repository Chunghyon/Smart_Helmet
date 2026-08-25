/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_data.h
\brief      Header file for the dfu file parser of the dfu_case.
*/

#ifndef DFU_CASE_DATA_H_
#define DFU_CASE_DATA_H_

#ifdef INCLUDE_DFU_CASE

#include <upgrade_protocol.h>

/*! \brief Initialize the dfu_case_data FSM
*/
bool DfuCase_DataInit(void);

/*! \brief start data transfer for new DFU
*/
void dfuCase_DataStartDataTransfer(void);

/*! \brief parse and handle the data received from the host

    \param data buffer containing the data
    \param data_len size of the data to parse
    \return error code if any error occurs during the parsing
*/
UpgradeHostErrorCode dfuCase_DataParse(const uint8 *data, uint16 data_len);

/*! \brief fetch and reset the value of the next request size

    \return data size to request
*/
uint32 DfuCase_DataGetNextReqSize(void);

/*! \brief offset from which next data needs to be requested next

    \return offset to request
*/
uint32 DfuCase_DataGetNextOffset(void);

/*! \brief clear the request size and offset set for the last request

        This function should be called only once we are sure that host has received the request.
*/
void DfuCase_DataClearLastRequest(void);

/*! \brief case has sent the READY message, handle this event
*/
void DfuCase_DataHandleCaseReady(void);

/*! \brief calculate dfu file offset to resume data transfer
*/
void dfuCase_DataCalculateResumeOffset(void);

/*! \brief Clean up after DFU comlpletes or aborts
*/
void DfuCase_DataCleanUp(void);

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_DATA_H_ */
