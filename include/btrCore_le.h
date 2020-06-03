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
    enBTRCoreLEGPropDesc,
    enBTRCoreLEGPropDescValue,
    enBTRCoreLEGPropUnknown
} enBTRCoreLEGattProp;

typedef enum _enBTRCoreLEGattOp {
    enBTRCoreLEGOpReady,
    enBTRCoreLEGOpReadValue,
    enBTRCoreLEGOpWriteValue,
    enBTRCoreLEGOpStartNotify,
    enBTRCoreLEGOpStopNotify,
    enBTRCoreLEGOpUnknown
} enBTRCoreLEGattOp;

typedef enum _enBTRCoreLEAdvProp {
    enBTRCoreLEAdvTypeProp,
    enBTRCoreLEServUUIDProp,
    enBTRCoreLENumServicesProp,
    enBTRCoreLEManfData,
    enBTRCoreLEServiceData,
    enBTRCoreLESolictiUUIDProp,
    enBTRCoreLETxPowerProp,
    enBTRCoreLEUnknown
} enBTRCoreLEAdvProp;

typedef struct _stBTRCoreLeGattInfo {
    enBTRCoreLEGattOp    enLeOper;
    enBTRCoreLEGattProp  enLeProp;
    char*                pui8Uuid;
    char*                pui8Value;
} stBTRCoreLeGattInfo;

/* @} */ // End of group BLUETOOTH_TYPES

/**
 * @addtogroup BLUETOOTH_APIS
 * @{
 */


/* Fptr callback types */
//TODO:  char* apBtLeInfo ->  apstBtLeInfo | define a struct based on future need
typedef enBTRCoreRet (*fPtr_BTRCore_LeStatusUpdateCb) (stBTRCoreLeGattInfo* apstBtrLeInfo, const char* apBtDevAddr, void* apUserData);


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
 * @param[in]  apBtAdapter              Bluetooth adapter address.

 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_DeInit (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API fetches the GATT property value that is supported.
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtDevPath              Device Id of the remote device.
 * @param[in]  apBtUuid                 UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLEGattProp     Indicates the operation to be performed.
 * @param[out] apBtPropValue            Property values.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_GetGattProperty (tBTRCoreLeHdl hBTRCoreLe, tBTRCoreDevId atBTRCoreDevId,
                                         const char* apBtUuid, enBTRCoreLEGattProp aenBTRCoreLEGattProp, void* apBtPropValue);

/**
 * @brief  This API is used to perform read, write, notify operations on LE devices.
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtDevPath              Device Id of the remote device.
 * @param[in]  apBtUuid                 UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLEGattOp       Indicates the operation to be performed.
 * @param[out] rpLeOpRes                Indicates the result of the operation.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BtrCore_LE_PerformGattOp (tBTRCoreLeHdl hBTRCoreLe, tBTRCoreDevId atBTRCoreDevId,
                                       const char* apBtUuid, enBTRCoreLEGattOp aenBTRCoreLEGattOp, char* apLeOpArg, char* rpLeOpRes);

/**
 * @brief  This API is used to invoke method calls to RegisterAdvertisement and RegisterApplication to 
 *         begin LE advertising
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection..
 * @param[in]  apBtAdapter              Bluetooth adapter address.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_StartAdvertisement(tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API is used to store the advertisement type supported by device 
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aAdvtType                Advertisement type 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_SetAdvertisementType(tBTRCoreLeHdl hBTRCoreLe, char* aAdvtType);

/**
 * @brief  This API is used to store the UUIDs that would be advertised by the device during the advertisement
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aUUID                    Service UUID
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_SetServiceUUIDs(tBTRCoreLeHdl hBTRCoreLe, char* aUUID);

/**
 * @brief  This API is used to store the manufacturer data to be sent with the advertisement
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aManfId                  Manufacturer ID
 * @param[in]  aDeviceDetails           Manufacturer device details
 * @param[in]  aLenManfData             Length of manufacturer data
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_SetManufacturerData(tBTRCoreLeHdl hBTRCoreLe, unsigned short aManfId, unsigned char* aDeviceDetails, int aLenManfData);

/**
 * @brief  This API is used to enable/disable sending tranmission power with the advertisement data

 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aTxPower                 Enable or disable sending Tx power
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_SetEnableTxPower(tBTRCoreLeHdl hBTRCoreLe, BOOLEAN aTxPower);

/**
 * @brief  This API is used to invoke method calls to UnRegisterAdvertisement and UnRegisterApplication to
 *         stop LE advertising
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  apBtConn                 Dbus connection..
 * @param[in]  apBtAdapter              Bluetooth adapter address.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_StopAdvertisement(tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API is used to add service info for the advertisement
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aBtdevAddr               BT Address of advertising device
 * @param[in]  aUUID                    UUID of the service
 * @param[in]  aServiceType             Indicates Primary or secondary service
 * @param[out] aNumGattServices         Returns number of gatt services added
 *
 * @return  Returns the pointer to the service added
 * @retval  Returns NULL on failure, appropiate address otherwise.
 */
int* BTRCore_LE_AddGattServiceInfo(tBTRCoreLeHdl hBTRCoreLe, const char* apBtAdapter, char* aBtdevAddr, char* aUUID, BOOLEAN aServiceType, int *aNumGattServices);

/**
 * @brief  This API is used to add gatt characteristic info for the advertisement
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aBtdevAddr               BT Address of advertising device
 * @param[in]  aParentUUID              Service the characteristic belongs to
 * @param[in]  aUUID                    UUID of the service
 * @param[in]  aCharFlags               Bit field to indicate usage of characteristic
 * @param[in]  aValue                   Value of the characteristic if applicable
 *
 * @return  Returns the pointer to the characteristic added
 * @retval  Returns NULL on failure, appropiate address otherwise.
 */
int* BTRCore_LE_AddGattCharInfo(tBTRCoreLeHdl hBTRCoreLe, const char* apBtAdapter, char* aBtdevAddr, char* aParentUUID, char* aUUID, unsigned short aCharFlags, char* aValue);

/**
 * @brief  This API is used to add gatt descriptor info for the advertisement
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aBtdevAddr               BT Address of advertising device
 * @param[in]  aParentUUID              Characteristic the descriptor belongs to
 * @param[in]  aUUID                    UUID of the service
 * @param[in]  aDescFlags               Bit field to indicate usage of characteristic
 * @param[out] aValue                   Value of the descriptor if applicable
 *
 * @return  Returns the pointer to the characteristic added
 * @retval  Returns NULL on failure, appropiate address otherwise.
 */
int* BTRCore_LE_AddGattDescInfo(tBTRCoreLeHdl hBTRCoreLe, const char* apBtAdapter, char* aBtdevAddr, char* aParentUUID, char* aUUID, unsigned short aDescFlags, char* aValue);

/**
 * @brief  This API Returns the specified property value associated with the UUID
 *
 * @param[in]  hBTRCoreLe               Handle to bluetooth low energy device interface.
 * @param[in]  aUUID                    UUID of the Gatt element
 * @param[in]  aGattProp                Gatt property to be fetched
 * @param[out] aValue                   Value of the property
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_LE_SetPropertyValue(tBTRCoreLeHdl hBTRCoreLe, char *aUUID, char *aValue, enBTRCoreLEGattProp aElement);

/* Outgoing callbacks Registration Interfaces */
/* BTRCore_LE_RegisterStatusUpdateCb - Callback for LE dev notifications and state changes */
enBTRCoreRet BTRCore_LE_RegisterStatusUpdateCb (tBTRCoreLeHdl hBTRCoreLe, fPtr_BTRCore_LeStatusUpdateCb afPtr_BTRCore_LeStatusUpdateCb, void* apUserData);

/* @} */ // End of group BLUETOOTH_APIS

#endif // __BTR_CORE_LE_H__
