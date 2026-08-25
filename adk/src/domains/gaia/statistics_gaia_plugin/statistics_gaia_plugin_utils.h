/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the statistics framework plugin utilities
*/

#ifndef STATISTICS_GAIA_PLUGIN_UTILS_H
#define STATISTICS_GAIA_PLUGIN_UTILS_H

#if defined(INCLUDE_STATISTICS)

/*! \brief Helper function used to write UInt8 value into return payload.
    \param  local_value             The value to be returned.
    \param  return_value_location   Pointer to location in payload.
    \param  max_length              Maximum number of bytes allowed in the payload for the value.

    \return length successfully copied to the payload. 0 if local_value size exceeds max_length
*/
size_t StatisticsGaiaPluginUtils_ReturnUInt8(uint8 local_value,uint8 *return_value_location, size_t max_length);

/*! \brief Helper function used to write UInt16 value into return payload.
    \param  local_value             The value to be returned.
    \param  return_value_location   Pointer to location in payload.
    \param  max_length              Maximum number of bytes allowed in the payload for the value.

    \return true if successfully copied to the payload. False if local_value size exceeds max_length
*/
size_t StatisticsGaiaPluginUtils_ReturnUInt16(uint16 local_value,uint8 *return_value_location, size_t max_length);

/*! \brief Helper function used to write Int16 value into return payload.
    \param  local_value             The value to be returned.
    \param  return_value_location   Pointer to location in payload.
    \param  max_length              Maximum number of bytes allowed in the payload for the value.

    \return true if successfully copied to the payload. False if local_value size exceeds max_length
*/
size_t StatisticsGaiaPluginUtils_ReturnInt16(int16 local_value,uint8 *return_value_location, size_t max_length);

/*! \brief Helper function used to write UInt32 value into return payload.
    \param  local_value             The value to be returned.
    \param  return_value_location   Pointer to location in payload.
    \param  max_length              Maximum number of bytes allowed in the payload for the value.

    \return true if successfully copied to the payload. False if local_value size exceeds max_length
*/
size_t StatisticsGaiaPluginUtils_ReturnUInt32(uint32 local_value,uint8 *return_value_location, size_t max_length);

/*! \brief Helper function used to write Int32 value into return payload.
    \param  local_value             The value to be returned.
    \param  return_value_location   Pointer to location in payload.
    \param  max_length              Maximum number of bytes allowed in the payload for the value.

    \return true if successfully copied to the payload. False if local_value size exceeds max_length
*/
size_t StatisticsGaiaPluginUtils_ReturnInt32(int32 local_value,uint8 *return_value_location, size_t max_length);

#endif // INCLUDE_STATISTICS

#endif // STATISTICS_GAIA_PLUGIN_UTILS_H
