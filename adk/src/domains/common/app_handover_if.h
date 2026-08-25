/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       app_handover_if.h
\defgroup   app_handover App Handover
\ingroup    common_domain
\brief      Exposes interface to Application for relevant components to implement
            it for Handover operation.

\section    Handover_Interface_intro INTRODUCTION

        Components that will be marshalled should internally
        implement the interface functions defined below
        for veto, marshal, unmarshal, and commit.
        These are then exposed through the app_handover_interface
        struct

\subsection usage-reg-bredr Usage of Handover registration-BREDR

* A Component that needs to implement the BREDR application handover
* interface should declare and define the necessary functions
* as specified by the function typedefs below. These functions 
* are private to the component and are exposed in the form of 
* app_handover_if only.
*
* Pointers to these functions are then held in an instance of
* the app_handover_if struct.
\code
static marshal_type_list_t component_TypeList;
static bool component_Veto(void);
static bool component_Marshal(const bdaddr *bd_addr,
                         marshal_type_t type,
                         void **marshal_data);
static void component_Unmarshal(const bdaddr *bd_addr,
                         marshal_type_t type,
                         void *unmarshal_data);
static void component_Commit(bool newRole);
\endcode
* A Component shall use REGISTER_HANDOVER_INTERFACE as follows:
*
* REGISTER_HANDOVER_INTERFACE(component_Name, component_TypeList, component_Veto,
* component_Marshal, component_Unmarshal, component_Commit);
*
* If there is no requirement of marshal/unmarshal for a component can use the following registration:
* REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(component_Name, component_Veto, component_Commit);
*
* If the component wants to veto per-link, it can use the _VPL macro variants.

\subsection usage-reg-ble Usage of Handover registration-BLE


* A Component that needs to implement the BLE application handover
* interface should declare and define the necessary functions
* as specified by the function typedefs below. These functions
* are private to the component and are exposed in the form of
* app_handover_if only.
*
* Pointers to these functions are then held in an instance of
* the app_handover_if struct.
\code
static marshal_type_list_t component_TypeList;
static bool component_Veto(void);
static bool component_Marshal_LE(const typed_bdaddr *taddr,
                         marshal_type_t type,
                         void **marshal_data);
static void component_Unmarshal_LE(const typed_bdaddr *taddr,
                         marshal_type_t type,
                         void *unmarshal_data);
static void component_Commit(bool newRole);
\endcode
* A Component shall use REGISTER_HANDOVER_INTERFACE_LE as follows:
*
* REGISTER_HANDOVER_INTERFACE_LE(component_Name, component_TypeList, component_Veto,
* component_Marshal_LE, component_Unmarshal_LE, component_Commit);
*
* If there is no requirement of marshal/unmarshal for a component can use the following registration:
* REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING_LE(component_Name, component_Veto, component_Commit);
*
* Note: the taddr in component_Marshal_LE/component_Unmarshal_LE represents resolved public address of bonded LE device
* or unresolved randon address of non-bonded LE device
*
* If the component wants to veto per-link, it can use the _VPL macro variants.
*
*/


#ifndef APP_HANDOVER_IF_H_
#define APP_HANDOVER_IF_H_

/*! @{ */

#include <bdaddr_.h>
#include <hydra_macros.h>
#include <app/marshal/marshal_if.h>

/*! \brief Return codes for unmarshalling interface */
typedef enum {
    /*! unmarshalling has failed. */
    UNMARSHAL_FAILURE,
    /*! unmarshalling is successfull and object can be freed. */
    UNMARSHAL_SUCCESS_FREE_OBJECT,
    /*! unmarshalling is successfull and object is in use. Don't free the object. */
    UNMARSHAL_SUCCESS_DONT_FREE_OBJECT,
}app_unmarshal_status_t;

/*! \brief Enum for defining the marshal type category.
 * If Marshal type is device specific, use MARSHAL_TYPE_CATEGORY_PER_INSTANCE
 * If Marshal type non-device specific(common data), use MARSHAL_TYPE_CATEGORY_GENERIC
 */
typedef enum{
    MARSHAL_TYPE_CATEGORY_GENERIC,
    MARSHAL_TYPE_CATEGORY_PER_INSTANCE
}marshal_type_category_t;

/*! \brief Enum for defining the type of handover interface.
 * The values are derived from TRANSPORT_T.
 */
typedef enum{
    INTERFACE_TYPE_BREDR = TRANSPORT_BREDR_ACL,
    INTERFACE_TYPE_BLE = TRANSPORT_BLE_ACL
}interface_type_t;

/*!
    \brief Component veto the Handover process

    Each component has a veto option over the handover process. Prior
    to handover commencing each component's veto function is called and
    it should check its internal state to determine if the
    handover should proceed.

    The veto check applies to all links

    \return TRUE if the component wishes to veto the handover attempt.

*/
typedef bool (*app_handover_veto_t)(void);

/*!
    \brief Component veto the Handover process for a single BREDR link.
    \param[in] bd_addr Bluetooth address of the BREDR link.
    \return TRUE if the component vetoes the handover for this link.

    This veto check is optional and may be used if the component
    wants to make veto decision on a per link basis.
*/
typedef bool (*app_handover_veto_link_t)(const bdaddr *bd_addr);

/*!
    \brief Component veto the Handover process for a single LE link
    \param[in] bd_addr Bluetooth address of the LE link.
    \return TRUE if the component vetoes the handover for this link.

    This veto check is optional and may be used if the component
    wants to make veto decision on a per link basis.
*/
typedef bool (*app_handover_veto_le_link_t)(const typed_bdaddr *tbdaddr);

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the BREDR link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
typedef bool (*app_handover_marshal_t)(const bdaddr *bd_addr,
                                 marshal_type_t type,
                                 void **marshal_obj);


/*!
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type \ref marshal_type_t

    \param[in] bd_addr      Bluetooth address of the BREDR link to be unmarshalled.
                            \ref bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return UNMARSHAL_FAILURE: Unmarshalling failed
            UNMARSHAL_SUCCESS_FREE_OBJECT: Unmarshalling complete. Caller can free the unmarshal_obj.
            UNMARSHAL_SUCCESS_DONT_FREE_OBJECT: Unmarshalling complete. Caller cannot free the 
                unmarshal_obj, as component is using it.

*/
typedef app_unmarshal_status_t (*app_handover_unmarshal_t)(const bdaddr *bd_addr,
                                  marshal_type_t type,
                                  void *unmarshal_obj);

/*!
    \brief The function shall set marshal_obj to the address of the BLE application specific object to
           be marshalled.

    \param[in] bd_addr      Typed bluetooth address. It represents resolved public address of bonded LE device
                                       or unresolved random address of non-bonded LE device. \ref typed_bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
typedef bool (*app_handover_marshal_le_t)(const typed_bdaddr *tbdaddr,
                                 marshal_type_t type,
                                 void **marshal_obj);


/*!
    \brief The function shall copy the BLE application specific unmarshal_obj associated to specific
            marshal type \ref marshal_type_t.

    \param[in] tbdaddr      Typed bluetooth address. It represents resolved public address for LE bonded device
                                       or unresolved random address for LE non-bonded device. \ref typed_bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return UNMARSHAL_FAILURE: Unmarshalling failed
            UNMARSHAL_SUCCESS_FREE_OBJECT: Unmarshalling complete. Caller can free the unmarshal_obj.
            UNMARSHAL_SUCCESS_DONT_FREE_OBJECT: Unmarshalling complete. Caller cannot free the
                unmarshal_obj, as component is using it.

*/
typedef app_unmarshal_status_t (*app_handover_unmarshal_le_t)(const typed_bdaddr *tbdaddr,
                                  marshal_type_t type,
                                  void *unmarshal_obj);

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary TRUE if device role is primary, else secondary

*/
typedef void (*app_handover_commit_t)( bool is_primary );


typedef struct {
    /*! marshal type */
    uint8 type;
    /*! marshal type category */
    uint8 category;
}marshal_type_info_t;

/*! \brief Structure with list of Marshal types.

    This data is supplied with registered handover interface by components registering
    with handover application.
*/
typedef struct {
    /*! List of marshal types */
    const marshal_type_info_t *types_info_list;
    /*! Size of list */
    uint8 list_size;
} marshal_type_list_t;

/*! \brief Structure with handover interfaces for the application components.

    Components which implement the handover interface provide this data while
    registering with handover application. Then, during a handover trigger, the
    handover application uses the registred interfaces and marshal type list provided
    in this structure to perform Veto, Marshal, Unmarshal and Commit operations.
*/
typedef struct {
    /*! Represents the type of handover interface */
    interface_type_t interface_type;
    /*! List of Marshal types marshaled/unmarshaled by this interface */
    const marshal_type_list_t *type_list;
    /*! Veto Interface */
    app_handover_veto_t Veto;
    union app_handover_veto_u {
        /*! Veto link for BREDR */
        app_handover_veto_link_t bredr;
        /*! Veto link for LE */
        app_handover_veto_le_link_t le;
    } VetoLink;
    union {
        /*! Marshaling Interface for BREDR */
        app_handover_marshal_t bredr;
        /*! Marshaling Interface for BLE */
        app_handover_marshal_le_t le;
    } Marshal;
    union {
        /*! Unmarshaling Interface for BREDR*/
        app_handover_unmarshal_t bredr;
        /*! Unmarshaling Interface for BLE*/
        app_handover_unmarshal_le_t le;
    } Unmarshal;
    /*! Commit Interface */
    app_handover_commit_t Commit;
} registered_handover_interface_t;

/*! \brief Macro to mention Marshal type and it's category(see marshal_type_category_t)*/
#define MARSHAL_TYPE_INFO(type_name, type_category) {MARSHAL_TYPE(type_name), type_category}


/*! \brief Private base macro to register the handover interface to be used by BREDR handover application clients */
#define _REGISTER_BREDR_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT) \
_Pragma("datasection handover_interface_registrations") \
const registered_handover_interface_t handover_interface_##NAME = \
{INTERFACE_TYPE_BREDR, TYPELIST, VETO, .VetoLink.bredr = VETO_LINK, .Marshal.bredr = MARSHAL, .Unmarshal.bredr = UNMARSHAL, COMMIT}


/*! \brief Macro to register the handover interface to be used by BREDR handover application clients */
#define REGISTER_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, MARSHAL, UNMARSHAL, COMMIT) \
    _REGISTER_BREDR_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, NULL, MARSHAL, UNMARSHAL, COMMIT)

/*! \brief Macro to register the handover interface to be used by BREDR handover application clients with veto per-link */
#define REGISTER_HANDOVER_INTERFACE_VPL(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT) \
    _REGISTER_BREDR_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT)

/*! \brief Macro to register the handover interface with no marshalling data. */
#define REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(NAME, VETO, COMMIT) \
    _REGISTER_BREDR_HANDOVER_INTERFACE(NAME, NULL, VETO, NULL, NULL, NULL, COMMIT)

/*! \brief Macro to register the handover interface with no marshalling data and with veto per-link. */
#define REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING_VPL(NAME, VETO, VETO_LINK, COMMIT) \
    _REGISTER_BREDR_HANDOVER_INTERFACE(NAME, NULL, VETO, VETO_LINK, NULL, NULL, COMMIT)


/*! \brief Private base macro to register handover interface to be used by BLE handover application clients */
#define _REGISTER_LE_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT) \
_Pragma("datasection handover_interface_registrations") \
const registered_handover_interface_t handover_interface_le_##NAME = \
{INTERFACE_TYPE_BLE, TYPELIST, VETO, .VetoLink.le = VETO_LINK, .Marshal.le = MARSHAL, .Unmarshal.le = UNMARSHAL, COMMIT}

/*! \brief Macro to register the handover interface to be used by BREDR handover application clients */
#define REGISTER_HANDOVER_INTERFACE_LE(NAME, TYPELIST, VETO, MARSHAL, UNMARSHAL, COMMIT) \
    _REGISTER_LE_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, NULL, MARSHAL, UNMARSHAL, COMMIT)

/*! \brief Macro to register the handover interface to be used by BREDR handover application clients with veto per-link */
#define REGISTER_HANDOVER_INTERFACE_LE_VPL(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT) \
    _REGISTER_LE_HANDOVER_INTERFACE(NAME, TYPELIST, VETO, VETO_LINK, MARSHAL, UNMARSHAL, COMMIT)

/*! \brief Macro to register the handover interface with no marshalling data. */
#define REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING_LE(NAME, VETO, COMMIT) \
    _REGISTER_LE_HANDOVER_INTERFACE(NAME, NULL, VETO, NULL, NULL, NULL, COMMIT)

/*! \brief Macro to register the handover interface with no marshalling data and with veto per-link. */
#define REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING_LE_VPL(NAME, VETO, VETO_LINK, COMMIT) \
    _REGISTER_LE_HANDOVER_INTERFACE(NAME, NULL, VETO, VETO_LINK, NULL, NULL, COMMIT)

/*! \brief Linker defined consts referencing the location of the section containing
    the handover interface registrations.
*/
extern const registered_handover_interface_t handover_interface_registrations_begin[];
extern const registered_handover_interface_t handover_interface_registrations_end[];

/*! @} */
#endif /* APP_HANDOVER_IF_H_ */
