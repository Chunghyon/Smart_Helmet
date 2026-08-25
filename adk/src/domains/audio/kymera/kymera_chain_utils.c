/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Generic APIs to help configure/check an entire chain

*/

#include "kymera_chain_utils.h"
#include <panic.h>

static const operator_config_t * kymera_GetConfigWithRole(const chain_config_t *chain_definition, unsigned role)
{
    for(unsigned i = 0; i < chain_definition->number_of_operators; i++)
    {
        if (chain_definition->operator_config[i].role == role)
            return &chain_definition->operator_config[i];
    }

    return NULL;
}

void Kymera_ConfigureChain(kymera_chain_handle_t chain, const operator_config_map_t *op_config_map, uint8 maps_length, const void *params)
{
    Operator operator;

    PanicFalse(chain != NULL);
    PanicFalse(op_config_map != NULL);

    for(unsigned i = 0; i < maps_length; i++)
    {
        operator = ChainGetOperatorByRole(chain, op_config_map[i].operator_role);
        if (operator)
        {
            op_config_map[i].ConfigureOperator(operator, params);
        }
    }
}

void Kymera_CheckChainCapabilities(const chain_config_t *chain_definition, const capability_check_map_t *cap_check_map, uint8 maps_length, const void *params)
{
    const operator_config_t *role_config;

    PanicFalse(chain_definition != NULL);
    PanicFalse(cap_check_map != NULL);

    for(unsigned i = 0; i < maps_length; i++)
    {
        role_config = kymera_GetConfigWithRole(chain_definition, cap_check_map[i].capability_role);
        if (role_config)
        {
            cap_check_map[i].CheckCapability(role_config->capability_id, params);
        }
        else if (cap_check_map[i].call_if_not_found)
        {
            cap_check_map[i].CheckCapability(capability_id_none, params);
        }
    }
}
