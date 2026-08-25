/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partition_reader.h

DESCRIPTION

NOTES

*/

#ifndef UPGRADE_PARTITION_READER_H_
#define UPGRADE_PARTITION_READER_H_

#include <string.h>
#include <stdlib.h>

#include <boot.h>
#include <message.h>
#include <byte_utils.h>
#include <print.h>
#include <panic.h>

#include "upgrade_protocol.h"
#include "upgrade.h"

/**
 * This file allows building of a packet for the VM upgrade as defined in the 
 * VM upgrade documentation. The VM upgrade packet is composed as follows:
 *      0 bytes   1        2         3         4        length+3
 *      +---------+---------+---------+ +---------+---------+
 *      | OPCODE* |      LENGTH*      | |      DATA...      |
 *      +---------+---------+---------+ +---------+---------+
 */

#define UPGRADE_PACKET_HEADER                                  (3)

/**
 * Structure of UPGRADE_IS_VALIDATION_DONE_CFM message.
 *      0 bytes  1        2
 *      +--------+--------+
 *      |   WAITING TIME  |
 *      +--------+--------+
 */
#define UPGRADE_VAIDATION_DONE_CFM_DATA_LENGTH                      (2)

/**
 * Structure of UPGRADE_SYNC_REQ message.
 *      0 bytes  1        2        3        4
 *      +--------+--------+--------+--------+
 *      |      IN PROGRESS IDENTIFIER       |
 *      +--------+--------+--------+--------+
 */
#define UPGRADE_SYNC_REQ_DATA_LENGTH                                (4)

/**
 * Structure of UPGRADE_DATA_BYTES_REQ message.
 *      0 bytes  1        2        3        4        5        6        7        8
 *      +--------+--------+--------+--------+--------+--------+--------+--------+
 *      |     NUMBER OF BYTES REQUESTED     |         FILE START OFFSET         |
 *      +--------+--------+--------+--------+--------+--------+--------+--------+
 */
#define UPGRADE_DATA_BYTES_REQ_DATA_LENGTH                          (8)

/**
 * Structure of UPGRADE_START_CFM UPGRADE_START_CFM message.
 *      0 bytes  1        2        3
 *      +--------+--------+--------+
 *      | STATUS |  BATTERY LEVEL  |
 *      +--------+--------+--------+
 */
#define UPRAGE_HOST_START_CFM_DATA_LENGTH                           (3)

/**
 * Structure of UPGRADE_SYNC_CFM UPGRADE_SYNC_CFM message.
 *      0 bytes            1        2        3        4        5                  6
 *      +------------------+--------+--------+--------+--------+------------------+
 *      |   RESUME POINT   |       IN PROGRESS IDENTIFIER      | PROTOCOL VERSION |
 *      +------------------+--------+--------+--------+--------+------------------+
 */
#define UPGRADE_SYNC_CFM_DATA_LENGTH                                (6)

/**
 * Structure of UPGRADE_TRANSFER_COMPLETE_RES message.
 *      0 bytes    1
 *      +----------+
 *      |  ACTION  |
 *      +----------+
 */
#define UPGRADE_TRANSFER_COMPLETE_RES_DATA_LENGTH                   (1)

/**
 * Structure of UPGRADE_IN_PROGRESS_RES message.
 * <blockquote><pre>
 *      0 bytes    1
 *      +----------+
 *      |  ACTION  |
 *      +----------+
 * </pre></blockquote>
 */
#define UPGRADE_IN_PROGRESS_DATA_LENGTH                             (1)

/**
 * Structure of UPGRADE_DATA UPGRADE_DATA message.
 * .</p>
 * <blockquote><pre>
 *      0 bytes       1       ...       n
 *      +-------------+--------+--------+
 *      | LAST PACKET |    DATA...
 *      +-------------+--------+--------+
 * </pre></blockquote>
 */
#define UPGRADE_DATA_MIN_DATA_LENGTH                                (1)

/**
 * Structure of UPGRADE_COMMIT_CFM message.
 * <blockquote><pre>
 *      0 bytes    1
 *      +----------+
 *      |  ACTION  |
 *      +----------+
 * </pre></blockquote>
 */
#define UPGRADE_COMMIT_CFM_DATA_LENGTH                              (1)

/**
 * Structure of UPGRADE_ERROR_IND message.
 *      0 bytes  1        2
 *      +--------+--------+
 *      |   EROR          |
 *      +--------+--------+
 */
#define UPGRADE_ERROR_IND_DATA_LENGTH                      (2)

#define BYTES_TO_WORDS(_b_)       ((_b_+1) >> 1)

#define UPGRADE_HOST_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY UPGRADE_HOST_ERROR_INTERNAL_ERROR_2

typedef struct upgrade_partition_reader_data_ctx_t * upgrade_partition_reader_handle_t;

/*!
    @brief Read data from parititon
    @param handle The handle of the upgrade partition reader instance
    @param data_ptr where partition data is copied.
    @param last_packet TRUE if partition data is last packet.
    @param bytes_requested Amount of parition data is requested.
    @param bytes_read Amount of data is read from partition.
    @param offset Offset from which to read

    Returns status of partition data (SUCCESS or FAILURE).
*/
UpgradeHostErrorCode UpgradePartitionReader_ReadData(upgrade_partition_reader_handle_t handle,
                                                     uint8 * data_ptr,
                                                     bool * last_packet,
                                                     uint32 bytes_requested,
                                                     uint32 * bytes_read,
                                                     uint32 offset);

/*!
    @brief Create instance of Upgrade partition reader

    Returns The handle of the upgrade partition reader instance.
*/
upgrade_partition_reader_handle_t UpgradePartitionReader_CreateInstance(void);

/*!
    @brief Destroy instance of Upgrade partition reader.

    De-init of Upgrade partition reader includes close of any open source
    stream handles & freeing up of heap memory allocated to hold partition reader
    instance.

    @param handle The handle of the upgrade partition reader instance.
    @return None.
*/
void UpgradePartitionReader_DestroyInstance(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the partition number for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The partition number.
*/
uint16 UpgradePartitionReader_GetPartitionNumber(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the partition offset for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The partition offset.
*/
uint32 UpgradePartitionReader_GetPartitionOffset(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the state of the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The state.
*/
UpgradePartitionDataState UpgradePartitionReader_GetState(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the app header length for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The app header length.
*/
uint16 UpgradePartitionReader_GetAppHeaderLength(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the partition data length for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The partition data length.
*/
uint32 UpgradePartitionReader_GetPartitionDataLength(upgrade_partition_reader_handle_t handle);

/*!
    @brief Get the footer length for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @return The footer length.
*/
uint16 UpgradePartitionReader_GetFooterLength(upgrade_partition_reader_handle_t handle);

/*!
    @brief Set the partition number for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param partition_number The partition number to set.
    @return None.
*/
void UpgradePartitionReader_SetPartitionNumber(upgrade_partition_reader_handle_t handle, uint16 partition_number);

/*!
    @brief Set the partition offset for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param partition_offset The partition offset to set.
    @return None.
*/
void UpgradePartitionReader_SetPartitionOffset(upgrade_partition_reader_handle_t handle, uint32 partition_offset);

/*!
    @brief Set the read offset for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param read_offset The read offset to set.
    @return None.
*/
void UpgradePartitionReader_SetReadOffset(upgrade_partition_reader_handle_t handle, uint16 read_offset);

/*!
    @brief Set the pskey for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param pskey The pskey to set.
    @return None.
*/
void UpgradePartitionReader_SetPskey(upgrade_partition_reader_handle_t handle, uint16 pskey);

/*!
    @brief Set the state for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param state The state to set.
    @return None.
*/
void UpgradePartitionReader_SetState(upgrade_partition_reader_handle_t handle, UpgradePartitionDataState state);

/*!
    @brief Set the footer length for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param length The footer length to set.
    @return None.
*/
void UpgradePartitionReader_SetFooterLength(upgrade_partition_reader_handle_t handle, uint16 length);

/*!
    @brief Set the partition data length for the upgrade partition reader instance.

    @param handle The handle of the upgrade partition reader instance.
    @param length The partition data length to set.
    @return None.
*/
void UpgradePartitionReader_SetPartitionDataLength(upgrade_partition_reader_handle_t handle, uint32 length);

#endif /* UPGRADE_PARTITION_READER_H_ */
