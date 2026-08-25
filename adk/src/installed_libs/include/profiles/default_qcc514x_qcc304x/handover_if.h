/****************************************************************************
Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.


FILE NAME
    handover_if.h

DESCRIPTION
    Header file for the TWS Mirroring Handover interface library.

*/

/*!
\defgroup twsm twsm
\ingroup vm_libs

\brief  Interface TWS Mirroring Handover API.

\section Handover_Interface_intro INTRODUCTION

        Libraries that will be marshalled should internally
        implement the interface functions defined below
        for veto, veto_link, marshal, unmarshal, commit, complete and abort.
        These are then exposed through the handover_interface
        struct.
        Transport type supported shall be mentioned by libraries to handle
        marshal/unmarshal/commit specific to ACL transport of handover device.
        Library/client can use the MAKE_BLE_HANDOVER_IF/
        MAKE_BREDR_HANDOVER_IF/MAKE_HANDOVER_IF macros to construct
        interface definition which handles corresponding handover devices.
        If the library wants to implement veto on a per-link basis, it may
        choose to implement the handover_veto_link function instead of, or as
        well as the handover_veto function. The _VPL macros may then be used
        to create the interface structure.

\example Usage example

* A library that needs to implement the handover interface
* should declare and define the necessary functions as
* specified by the function typedefs below.  They should
* be declared internal to the library and not exposed
* by the external API.
*
* Also the library shall mention transport type of ACL which
* library handover interface is interested or able to handle by
* marshal/unmarshal/commit functions. Client/library can use
* MAKE_BLE_HANDOVER_IF/MAKE_BREDR_HANDOVER_IF/MAKE_HANDOVER_IF
* to construct handover interface definition.
*
* During handover procedure this supported transport type will be used
* to invoke marshal/unmarshal/commit interface functions.
* However veto/complete/abort will always be invoked irrespective
* of transport type supported by an interface.
*
* Pointers to these functions are then held in an instance of
* the handover_interface struct, this struct is then exposed
* by the library API.

* library_handover.c

static bool libraryVeto(void);
static bool libraryMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);
static bool libraryUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);
static void libraryHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole);
static void libraryHandoverComplete( const bool newRole );
static void libraryHandoverAbort( void );

* Example of interface definition which handles both BREDR/BLE handover device.
const handover_interface library_handover_if =
        MAKE_HANDOVER_IF(&libraryVeto,
                         &libraryMarshal,
                         &libraryUnMarshal,
                         &libraryHandoverCommit,
                         &libraryHandoverComplete,
                         &libraryHandoverAbort);

* library.h - main library header file
*
extern const handover_interface library_handover_if;

@{

*/


#ifndef HANDOVER_IF_H_
#define HANDOVER_IF_H_

#include <bdaddr_.h>
#include <library.h>
#include <message.h>

/*! Macro to indicate that all ACL transports are supported for handover \ref TRANSPORT_T */
#define HANDOVER_SUPPORT_ALL TRANSPORT_NONE

typedef TRANSPORT_T handover_transport_type_t;

/*! Type of handover in progress, from the perspective of the device on which
    the handover_Gettype() is called.
*/
typedef enum
{
    /*! Handover is not active. */
    handover_type_not_in_progress,

    /*! Handover of all links from primary to secondary, roles are swapped,
        and this device is the initiator of the handover.
        No non-peer links will exist on this device following handover completion. */
    handover_type_standard_initiator,

    /*! Handover of all links from the primary to secondary, roles are swapped,
        and this device is the acceptor of the handover.
        Any handed over links will exist on this device following handover
        completion. */
    handover_type_standard_acceptor,

} handover_type_t;

/*!
    \brief Handover veto check.

    Each component has a veto option over the handover process. Prior
    to handover commencing each component's veto function is called and
    it should check its internal state to determine if the
    handover should proceed.

    This function should check if common state (not specific to a
    single link) should veto the handover. Per-link veto checks should
    be performed in handover_veto_link().
    
    If the component does not implement handover_veto_link(), this
    function may also check for link specific reasons to veto.

    \return TRUE if the component vetoes the handover attempt.

*/
typedef bool (*handover_veto)( void );

/*!
    \brief Handover veto check for a specific link.

    Each component has a veto option over the handover process. Prior
    to handover commencing this function is called for each link being
    handed-over. The component should check its internal state for the
    specified link to determine if the handover should proceed.

    Implementation of this function is optional, it may be used for
    convenience instead of having to implement a link iterator in the
    main handover_veto function.
*/
typedef bool (*handover_veto_link)(const tp_bdaddr *tp_bd_addr);

/*!
    \brief Marshal the data associated with the specified connection

    \param tp_bd_addr Bluetooth address of the link to be marshalled
    \param buf Address to which the marshaller will write the marshalled byte
           stream.
    \param length space in the marshal byte stream buffer.
    \param[out] written number of bytes written to the buffer.
    \return TRUE if component marshalling complete, otherwise FALSE

*/
typedef bool (*handover_marshal)(const tp_bdaddr *tp_bd_addr,
                                 uint8 *buf,
                                 uint16 length,
                                 uint16 *written);



/*!
    \brief Unmarshal the data associated with the specified connection

    \param tp_bd_addr Bluetooth address of the link to be unmarshalled
    \param buf Address of the byte stream to be unmarshalled.
    \param length amount of data in the marshal byte stream buffer.
    \param[out] consumed the number of bytes written to the buffer
    \return TRUE if component unmarshalling complete, otherwise FALSE

*/
typedef bool (*handover_unmarshal)(const tp_bdaddr *tp_bd_addr,
                                  const uint8 *buf,
                                  uint16 length,
                                  uint16 *consumed);

/*!
    \brief Module performs time-critical actions to commit to the specified role.

    This function would be invoked once for each connected device.
    The library should perform time-critical actions to commit to the new role.

    \param tp_bd_addr Bluetooth address of the connected device.
    \param is_primary TRUE if TWS primary role requested, else
                      secondary

    \return void

*/
typedef void (*handover_commit)(const tp_bdaddr *tp_bd_addr, const bool is_primary);

/*!
    \brief Module performs pending actions and completes the transition to 
    the specified new role.

    This function will be invoked only once during the handover procedure.
    The library should perform pending actions and transition to the new role.

    \param is_primary TRUE if TWS primary role requested, else
                      secondary

    \return void

*/
typedef void (*handover_complete)( const bool is_primary );

/*!
    \brief Abort the Handover process

    Module should abort the handover process and free any memory
    associated with the marshalling process.

    The abort operation applies to all connections being marshalled
    (i.e. the abort operation is not per-device)

    \return void

*/
typedef void (*handover_abort)( void );


/*! \brief Find the type of handover which is currently being performed.
    \return handover_type_t Type of handover. 
    
    This API should only be called from one of the handover interface functions
    defined in this API, and is therefore only valid to be called during a
    handover.
*/
handover_type_t handover_getType(void);

/*!
    @brief Structure of handover interface function pointers

    Each component should expose a const version of this
    struct in it's main header file.  This negates the need to
    expose each of the individual handover interface functions.

*/
typedef struct
{
    handover_transport_type_t     supportedType;      /*!< Type of ACL transport(BREDR/BLE) that the handover interface support */
    handover_veto       pFnVeto;        /*!< Pointer to the component's handover_veto function */
    handover_veto_link  pFnVetoLink;    /*!< Pointer to the component's handover_veto_link function */
    handover_marshal    pFnMarshal;     /*!< Pointer to the component's handover_marshal function */
    handover_unmarshal  pFnUnmarshal;   /*!< Pointer to the component's handover_unmarshal function */
    handover_commit     pFnCommit;      /*!< Pointer to the component's handover_commit function */
    handover_complete   pFnComplete;    /*!< Pointer to the component's handover_complete function */
    handover_abort      pFnAbort;       /*!< Pointer to the component's handover_abort function */
} handover_interface;

/*! Macro to define structure of handover interface that support only BR/EDR ACL handover */
#define MAKE_BREDR_HANDOVER_IF(VETO, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {TRANSPORT_BREDR_ACL, VETO, NULL, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}

/*! Macro to define structure of handover interface that support only BLE ACL handover */
#define MAKE_BLE_HANDOVER_IF(VETO, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {TRANSPORT_BLE_ACL, VETO, NULL, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}

/*! Macro to define structure of handover interface that support all(BREDR/BLE) ACL handover */
#define MAKE_HANDOVER_IF(VETO, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {HANDOVER_SUPPORT_ALL, VETO, NULL, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}

/*! Macro to define structure of handover interface that support all(BREDR/BLE) ACL handover with a veto per link */
#define MAKE_HANDOVER_IF_VPL(VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {HANDOVER_SUPPORT_ALL, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}

/*! Macro to define structure of handover interface that support only BR/EDR ACL handover with a veto per link */
#define MAKE_BREDR_HANDOVER_IF_VPL(VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {TRANSPORT_BREDR_ACL, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}

/*! Macro to define structure of handover interface that support only BLE ACL handover with a veto per link */
#define MAKE_BLE_HANDOVER_IF_VPL(VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT) \
    {TRANSPORT_BLE_ACL, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT, COMPLETE, ABORT}


#endif /* HANDOVER_IF_H_ */
