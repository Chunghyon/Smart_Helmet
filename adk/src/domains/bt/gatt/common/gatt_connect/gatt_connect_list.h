/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for GATT connect list
*/

#ifndef GATT_CONNECT_LIST_H_
#define GATT_CONNECT_LIST_H_


typedef struct
{
    unsigned cid;
    unsigned mtu;
    unsigned pending_disconnects;
    tp_bdaddr tpaddr;
    uint16 disconnect_reason_code;
} gatt_connection_t;

/*! @brief Find a GATT connection using GATT connection ID

    \param cid          The GATT connection ID
*/
gatt_connection_t* GattConnect_FindConnectionFromCid(unsigned cid);

/*! @brief Create a new GATT connection with GATT connection ID

    \param cid          The GATT connection ID
*/
gatt_connection_t* GattConnect_CreateConnection(unsigned cid);

/*! @brief Destroy a GATT connection using GATT connection ID

    \param cid          The GATT connection ID
*/
void GattConnect_DestroyConnection(unsigned cid);

/*! @brief Initialise gatt_connect_list
*/
void GattConnect_ListInit(void);

/*! @brief Find a GATT connection using tp_bdaddr

    \param tpaddr_in     The tp_bdaddr address to find 
*/
gatt_connection_t* GattConnect_FindConnectionFromTpaddr(const tp_bdaddr *tpaddr_in);

/*! @brief Find a tp_bdaddr using a CID

    \param cid        The GATT connection ID
    \param tpaddr     The tp_bdaddr address found from the connection ID
    
    \return TRUE if the tp_bdaddr could be found from the cid. FALSE otherwise.
*/
bool GattConnect_FindTpaddrFromCid(unsigned cid, tp_bdaddr * tpaddr);

/*! @brief Get the pending disconnect from the list

    \return TRUE if pending diconnect exists in the list, FALSE otherwise.
*/
bool GattConnect_IsDisconnectPending(void);

/*! @brief Get gatt connection count

    \return Gatt connection count.
*/
uint8 GattConnect_GetConnectionCount(void);

/*! @brief This function compares the supplied tp_bdaddr with the tp_bdaddr in the GATT connection list and returns
           the GATT connection, if there is a match. If either the supplied tp_bdaddr or tp_bdaddr in GATT connection
           list is of type random, it will be resolved to public address before comparison.

    \param in_tp_bd_addr   The tp_bdaddr for which the connection has to be found.
    
    \return The gatt connection information.
*/
gatt_connection_t* GattConnect_ResolveAndFindConnection(const tp_bdaddr *in_tp_bd_addr);

#endif 
