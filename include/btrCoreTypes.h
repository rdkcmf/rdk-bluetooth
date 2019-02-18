/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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

/* @file Bluetooth Core Types Header file */
#ifndef __BTR_CORE_TYPES_H__
#define __BTR_CORE_TYPES_H__

/**
 * @addtogroup BLUETOOTH_TYPES
 * @{
 */

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif


#define BTRCORE_MAX_STR_LEN     256
#define BTRCORE_STR_LEN         32


typedef enum _BOOLEAN {
    FALSE = 0,
    TRUE
} BOOLEAN;


typedef void* tBTRCoreHandle;

typedef unsigned long long int tBTRCoreDevId;

typedef enum _enBTRCoreRet {
    enBTRCoreFailure, 
    enBTRCoreInitFailure, 
    enBTRCoreNotInitialized, 
    enBTRCoreInvalidAdapter, 
    enBTRCorePairingFailed,
    enBTRCoreDiscoveryFailure, 
    enBTRCoreDeviceNotFound, 
    enBTRCoreInvalidArg, 
    enBTRCoreSuccess
} enBTRCoreRet;

/* @} */ // End of group BLUETOOTH_TYPES

#endif // __BTR_CORE_TYPES_H__
