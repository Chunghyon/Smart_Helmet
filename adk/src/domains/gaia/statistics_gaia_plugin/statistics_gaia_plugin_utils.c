/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the statistics framework plugin utilities
*/

#include "statistics_gaia_plugin_utils.h"
#include <panic.h>

#if defined(INCLUDE_STATISTICS)

size_t StatisticsGaiaPluginUtils_ReturnUInt8(uint8 local_value,uint8 *return_value_location, size_t max_length)
{
    PanicNull(return_value_location);
    size_t return_size = sizeof(local_value);
    if (return_size > max_length)
    {
        return 0;
    }

    return_value_location[0] = local_value;
    return return_size;
}

size_t StatisticsGaiaPluginUtils_ReturnUInt16(uint16 local_value,uint8 *return_value_location, size_t max_length)
{
    PanicNull(return_value_location);
    size_t return_size = sizeof (local_value);
    if (return_size > max_length)
    {
        return 0;
    }
    return_value_location[0] = local_value >> 8;
    return_value_location[1] = local_value;
    return return_size;
}

size_t StatisticsGaiaPluginUtils_ReturnInt16(int16 local_value,uint8 *return_value_location, size_t max_length)
{
    return StatisticsGaiaPluginUtils_ReturnUInt16((uint16)local_value, return_value_location, max_length);
}

size_t StatisticsGaiaPluginUtils_ReturnUInt32(uint32 local_value,uint8 *return_value_location, size_t max_length)
{
    PanicNull(return_value_location);
    size_t return_size = sizeof (local_value);
    if (return_size > max_length)
    {
        return 0;
    }
    return_value_location[0] = local_value >> 24;
    return_value_location[1] = local_value >> 16;
    return_value_location[2] = local_value >> 8;
    return_value_location[3] = local_value;
    return return_size;
}

size_t StatisticsGaiaPluginUtils_ReturnInt32(int32 local_value,uint8 *return_value_location, size_t max_length)
{
    return StatisticsGaiaPluginUtils_ReturnUInt32((uint32)local_value, return_value_location, max_length);
}

#endif // INCLUDE_STATISTICS
