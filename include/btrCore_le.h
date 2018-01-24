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
 * btrCore_le.h
 * Includes information for LE functionality over BT.
 */

#ifndef __BTR_CORE_LE_H__
#define __BTR_CORE_LE_H__

#include "btrCoreTypes.h"

#define BTRCORE_MAX_STR_LEN 256
#define BTRCORE_STR_LEN     32

typedef void* tBTRCoreLeHdl;


/* Enum Types */
typedef enum _enBTRCoreLEGattProp {
    enBTRCoreLEGSPropUUID,
    enBTRCoreLEGSPropPrimary,
    enBTRCoreLEGSPropDevice,
    enBTRCoreLEGCPropUUID,
    enBTRCoreLEGCPropService,
    enBTRCoreLEGCPropValue,
    enBTRCoreLEGCPropNotifying,
    enBTRCoreLEGCPropFlags,
    enBTRCoreLEGDPropUUID,
    enBTRCoreLEGDPropChar,
    enBTRCoreLEGDPropValue,
    enBTRCoreLEGDPropFlags,
    enBTRCoreLEGPropUnknown
} enBTRCoreLEGattProp;

typedef enum _enBTRCoreLEGattOp {
    enBTRCoreLEGCOpReadValue,
    enBTRCoreLEGCOpWriteValue,
    enBTRCoreLEGCOpStartNotify,
    enBTRCoreLEGCOpStopNotify,
    enBTRCoreLEGDOpReadValue,
    enBTRCoreLEGDOpWriteValue,
    enBTRCoreLEGOpUnknown
} enBTRCoreLEGattOp;


enBTRCoreRet BTRCore_LE_Init (tBTRCoreLeHdl* phBTRCoreLe, void* apBtConn, const char* apBtAdapter);
enBTRCoreRet BTRCore_LE_DeInit (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

/* Should be called by BTRCore_GetSupportedServices */
enBTRCoreRet BTRCore_LE_GetAvailableServicesGatt (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevAddr, const char* lePropertyKey, void* lePropertyValue); // Maps to Gatt service
enBTRCoreRet BTRCore_LE_GetAvailablePropertiesGatt (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevAddr, const char* lePropertyKey, void* lePropertyValue); // Maps to Gatt Characteristic/Descriptor i.e if argument is Service then give back available characteristics, if argument is characteristics give back available descriptors
/* Should be called by BTRCore_GetLEProperty */
enBTRCoreRet BTRCore_LE_GetGattProperty (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevPath, const char* apBtUuid, enBTRCoreLEGattProp aenBTRCoreLEGattProp, void* apBtPropValue);

enBTRCoreRet BtrCore_LE_PerformGattMethodOp (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevPath, const char* apBtUuid, enBTRCoreLEGattOp aenBTRCoreLEGattOp);
//enBTRCoreRet BTRCore_LE_GetPropertyValueGatt (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevAddr, const char* lePropertyKey, void* lePropertyValue); // Maps to the value of the Descritpor (May be we can use for different properties on Service/Characteristic/Descriptor)

//enBTRCoreRet BTRCore_PerformLEOp (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, char* uuid, eBTRCoreDevLeOp  aeBTRCoreDevLeOp)
/* Will call BtrCore_BTPerformLeGattMethodOp (*/
/* Should be called by BTRCore_PerformLEOp */

#endif // __BTR_CORE_LE_H__
