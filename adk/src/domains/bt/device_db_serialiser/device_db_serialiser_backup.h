/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup device_database_serialiser
\brief      Backup of earbud properties.
 
Makes a copy of EARBUD and SELF devices properties and store them in one ps key.
That is to make storing of those properties atomic operation.

Intended use is to call DeviceDbSerialiser_MakeBackup() before the EARBUD or SELF device is serialised.
When the corruption of EARBUD and SELF device keys is detected, then DeviceDbSerialiser_RestoreBackup()
should be called to restore EARBUD and SELF keys from the backup copy.
DeviceDbSerialiser_RestoreBackup() should be called before actual deserialistion of device db happens.

Note that making a backup requires large pmalloc memory buffer, which equals to sum of all EARBUD and SELF properties.
This is because the complete data buffer needs to be in the memory before it can be written to a ps key.

*/

#ifndef DEVICE_DB_SERIALISER_BACKUP_H_
#define DEVICE_DB_SERIALISER_BACKUP_H_

/*! @{ */

/*! \brief Make a backup of EARBUD and SELF ps keys.

    Stores EARBUD and SELF devices properties into the single backup ps key.
*/
void DeviceDbSerialiser_MakeBackup(void);

/*! \brief Restore backup.

    Uses data from the backup key to overwrite device attribute ps keys
    corresponding to EARBUD and SELF.
*/
void DeviceDbSerialiser_RestoreBackup(void);

/*! @} */

#endif /* DEVICE_DB_SERIALISER_BACKUP_H_ */
