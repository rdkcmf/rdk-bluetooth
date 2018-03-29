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


/* Fptr callback types */
    //typedef enBTRCoreRet (*fPtr_BTRCore_LeDevStatusUpdateCb) ( DevType, LeInfo, void* apUserData);
    // can it be mapped to btrCore_BTDeviceStatusUpdateCb()?


/* Interfaces */
enBTRCoreRet BTRCore_LE_Init (tBTRCoreLeHdl* phBTRCoreLe, void* apBtConn, const char* apBtAdapter);

enBTRCoreRet BTRCore_LE_DeInit (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtAdapter);

enBTRCoreRet BTRCore_LE_GetGattProperty (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, tBTRCoreDevId atBTRCoreDevId,
                                         const char* apBtUuid, enBTRCoreLEGattProp aenBTRCoreLEGattProp, void* apBtPropValue);

enBTRCoreRet BtrCore_LE_PerformGattOp (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, tBTRCoreDevId atBTRCoreDevId,
                                       const char* apBtUuid, enBTRCoreLEGattOp aenBTRCoreLEGattOp, void* rpLeOpRes);

/* Outgoing callbacks Registration Interfaces */

#endif // __BTR_CORE_LE_H__
