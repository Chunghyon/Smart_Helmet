/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Generic APIs to help configure/check an entire chain

*/

#ifndef KYMERA_CHAIN_UTILS_H_
#define KYMERA_CHAIN_UTILS_H_

#include <chain.h>

typedef struct
{
    unsigned operator_role;
    void   (*ConfigureOperator)(Operator operator, const void *chain_configure_params);
} operator_config_map_t;

typedef struct
{
    /*! Same as the operator_role in a chain definition */
    unsigned capability_role;
    /*! cap_id will be capability_id_none if the role is not found in the chain */
    void   (*CheckCapability)(capability_id_t cap_id, const void *chain_check_params);
    /*! If TRUE CheckCapability will get called even if its respective role is not found */
    unsigned call_if_not_found:1;
} capability_check_map_t;

/*! \brief Configure operators in chain based on a map between the operator role and the function used to configure such an operator.
    \param chain Chain handle of the chain to be configured.
    \param op_config_map Table that maps each operator role to a function call used to configure it.
    \param maps_length Length of the op_config_map array.
    \param params Argument passed to each of the functions used in the op_config_map table (chain_configure_params).
*/
void Kymera_ConfigureChain(kymera_chain_handle_t chain, const operator_config_map_t *op_config_map, uint8 maps_length, const void *params);

/*! \brief Check capabilities in chain definition based on a map between the capability role (operator role) and the function to run for said capability (operator_config).
    \param chain_definition The chain definition whose capabilities we want to check.
    \param cap_check_map Table that maps each capability role to a function call used to check it.
    \param maps_length Length of the cap_check_map array.
    \param params Argument passed to each of the functions used in the cap_check_map table (chain_check_params).
*/
void Kymera_CheckChainCapabilities(const chain_config_t *chain_definition, const capability_check_map_t *cap_check_map, uint8 maps_length, const void *params);

#endif /* KYMERA_CHAIN_UTILS_H_ */
