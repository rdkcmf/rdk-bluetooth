/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*
 * @file btrCore_le.h
 * Includes information for LE functionality over BT.
 */


#ifndef __BTR_CORE_LE_H__
#define __BTR_CORE_LE_H__

#include "btrCoreTypes.h"

/**
 * @addtogroup BLUETOOTH_TYPES
 * @{
 */

#define BTRCORE_MAX_STR_LEN 256
#define BTRCORE_STR_LEN     32

typedef void* tBTRCoreLeHdl;

/* Enum Types */
typedef enum _enBTRCoreLEGattProp {
    enBTRCoreLEGPropUUID,
    enBTRCoreLEGPropPrimary,
    enBTRCoreLEGPropDevice,
    enBTRCoreLEGPropService,
    enBTRCoreLEGPropValue,
    enBTRCoreLEGPropNotifying,
    enBTRCoreLEGPropFlags,
    enBTRCoreLEGPropChar,
    enBTRCoreLEGPropUnknown
} enBTRCoreLEGattProp;

typedef enum _enBTRCoreLEGattOp {
    enBTRCoreLEGOpReadValue,
    enBTRCoreLEGOpWriteValue,
    enBTRCoreLEGOpStartNotify,
    enBTRCoreLEGOpStopNotify,
    enBTRCoreLEGOpUnknown
} enBTRCoreLEGattOp;

/* @} */ // End of group BLUETOOTH_TYPES

/**
 * @addtogroup BLUETOOTH_APIS
 * @{
 */


/* Fptr callback types */
    //typedef enBTRCoreRet (*fPtr_BTRCore_LeDevStatusUpdateCb) ( DevType, LeInfo, void* apUserData);
    // can it be mapped to btrCore_BTDeviceStatusUpdateCb()?


/* Interfaces */
/**
 * @brief   This API registers the callback function that has to be called when the LE device are added or removed.
 *
 * @param[in]  phBTRCoreLe              Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection.
 * @param[in]  apBtAdapter              Bluetooth adapter address.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_Init (tBTRCoreLeHdl* phBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/**
 * @brief   This API deinitializes the LE device.
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection.
 * @param[in]  apBtAdapter               Bluetooth adapter address.

 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_DeInit (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API fetches the GATT property value that is supported.
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection.
 * @param[in]  apBtDevPath              Device Id of the remote device.
 * @param[in]  apBtUuid                 UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLEGattProp     Indicates the operation to be performed.
 * @param[out] apBtPropValue            Property values.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_GetGattProperty (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, tBTRCoreDevId atBTRCoreDevId,
                                         const char* apBtUuid, enBTRCoreLEGattProp aenBTRCoreLEGattProp, void* apBtPropValue);

/**
 * @brief  This API is used to perform read, write, notify operations on LE devices.
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection.
 * @param[in]  apBtDevPath              Device Id of the remote device.
 * @param[in]  apBtUuid                 UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLEGattOp       Indicates the operation to be performed.
 * @param[out] rpLeOpRes                                Indicates the result of the operation.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BtrCore_LE_PerformGattOp (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, tBTRCoreDevId atBTRCoreDevId,
                                       const char* apBtUuid, enBTRCoreLEGattOp aenBTRCoreLEGattOp, void* rpLeOpRes);

/* Outgoing callbacks Registration Interfaces */
/* @} */ // End of group BLUETOOTH_APIS

#endif // __BTR_CORE_LE_H__
