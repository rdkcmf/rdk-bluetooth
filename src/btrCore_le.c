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
 * btrCore_le.c
 * Implementation of Gatt functionalities of Bluetooth
 * Here is hiearchy of Gatt profile.
 * -> /com/example/service1
 *   |   - org.freedesktop.DBus.Properties
 *   |   - org.bluez.GattService1 
              - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100 (0100..n)
                   |  - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100/char01100
                   |  - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100/char01200
                   |  - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100/char01300
                             |  - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100/char01300/desc0100
                             |  - /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX/service0100/char01300/desc0200
 *   |
 *   -> /com/example/service1/char0
 *       - org.freedesktop.DBus.Properties
 *       - org.bluez.GattCharacteristic1
 *
 */

/* System Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* External Library Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrCore_logger.h"

/* Local Headers */
#include "btrCore_le.h"
#include "btrCore_service.h"

#include "btrCore_bt_ifce.h"


#define BTR_MAX_GATT_PROFILE                            10
#define BTR_MAX_GATT_SERVICE                            2
#define BTR_MAX_GATT_CHAR                               5
#define BTR_MAX_GATT_DESC                               2
#define BTR_MAX_GATT_CHAR_FLAGS                         15
#define BTR_MAX_GATT_DESC_FLAGS                         8
#define BTR_MAX_NUMBER_OF_UUID                          32


/* Characteristic Property bit field and Characteristic Extented Property bit field Values */
#define BTR_GATT_CHAR_FLAG_READ                         1 << 0
#define BTR_GATT_CHAR_FLAG_WRITE                        1 << 1
#define BTR_GATT_CHAR_FLAG_ENCRYPT_READ                 1 << 2
#define BTR_GATT_CHAR_FLAG_ENCRYPT_WRITE                1 << 3
#define BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ   1 << 4
#define BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE  1 << 5
#define BTR_GATT_CHAR_FLAG_SECURE_READ                  1 << 6          /* Server Mode only */
#define BTR_GATT_CHAR_FLAG_SECURE_WRITE                 1 << 7          /* Server Mode only */
#define BTR_GATT_CHAR_FLAG_NOTIFY                       1 << 8
#define BTR_GATT_CHAR_FLAG_INDICATE                     1 << 9
#define BTR_GATT_CHAR_FLAG_BROADCAST                    1 << 10
#define BTR_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE       1 << 11
#define BTR_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES  1 << 12
#define BTR_GATT_CHAR_FLAG_RELIABLE_WRITE               1 << 13
#define BTR_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES         1 << 14

#if 0
/* Descriptor Property bit field Values */
#define BTR_GATT_DESC_FLAG_READ                         BTR_GATT_CHAR_FLAG_READ
#define BTR_GATT_DESC_FLAG_WRITE                        BTR_GATT_CHAR_FLAG_WRITE
#define BTR_GATT_DESC_FLAG_ENCRYPT_READ                 BTR_GATT_CHAR_FLAG_ENCRYPT_READ
#define BTR_GATT_DESC_FLAG_ENCRYPT_WRITE                BTR_GATT_CHAR_FLAG_ENCRYPT_WRITE
#define BTR_GATT_DESC_FLAG_ENCRYPT_AUTHENTICATED_READ   BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ
#define BTR_GATT_DESC_FLAG_ENCRYPT_AUTHENTICATED_WRITE  BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE
#define BTR_GATT_DESC_FLAG_SECURE_READ                  BTR_GATT_CHAR_FLAG_SECURE_READ    /* Server Mode only */
#define BTR_GATT_DESC_FLAG_SECURE_WRITE                 BTR_GATT_CHAR_FLAG_SECURE_WRITE   /* Server Mode only */
#endif

/* GattDescriptor1 Properties */
typedef struct _stBTRCoreLeGattDesc {
    char                    descPath[BTRCORE_MAX_STR_LEN];                       /* Descriptor Path */
    char                    descUuid[BT_MAX_UUID_STR_LEN];                       /* 128-bit service UUID */
    unsigned short          descFlags;                                           /* Descriptor Flags - bit field values */
    void*                   parentChar;                                          /* ptr to parent Characteristic  */
} stBTRCoreLeGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTRCoreLeGattChar {
    char                    charPath[BTRCORE_MAX_STR_LEN];                       /* Characteristic Path */
    char                    charUuid[BT_MAX_UUID_STR_LEN];                       /* 128-bit service UUID */
    stBTRCoreLeGattDesc     atBTRGattDesc[BTR_MAX_GATT_DESC];                    /* Max of 4 Gatt Descriptor array */
    unsigned short          ui16NumberOfGattDesc;                                /* Number of Gatt Service ID */
    unsigned short          charFlags;                                           /* Characteristic Flags - bit field values */
    void*                   parentService;                                       /* ptr to parent Service */
} stBTRCoreLeGattChar;

/* GattService Path and Properties */
typedef struct _stBTRCoreLeGattService {
    char                    servicePath[BTRCORE_MAX_STR_LEN];                    /* Service Path */
    char                    serviceUuid[BT_MAX_UUID_STR_LEN];                    /* 128-bit service UUID */
    stBTRCoreLeGattChar     astBTRGattChar[BTR_MAX_GATT_CHAR];                   /* Max of 4 Gatt Charactristic array */
    unsigned short          ui16NumberOfGattChar;                                /* Number of Gatt Charactristics */
    void*                   parentProfile;                                       /* ptr to parent Device Profile */
}stBTRCoreLeGattService;

/* GattProfile coresponds to a Device's Gatt Services */
typedef struct _stBTRCoreLeGattProfile {
    tBTRCoreDevId           deviceID;                                           /* TODO To be generated from a common btrCore if */
    char                    devicePath[BTRCORE_MAX_STR_LEN];                    /* Object Path */
    stBTRCoreLeGattService  astBTRGattService[BTR_MAX_GATT_SERVICE];            /* Max of 2 Gatt Service array */
    unsigned short          ui16NumberOfGattService;                            /* Number of Gatt Service ID */
} stBTRCoreLeGattProfile;



typedef struct _stBTRCoreLeHdl {

    stBTRCoreLeGattProfile  astBTRGattProfile[BTR_MAX_GATT_PROFILE];
    unsigned short          ui16NumberOfGattProfile;

} stBTRCoreLeHdl;



typedef struct _stBTRCoreLeUUID {
    unsigned short  flags;
    char            uuid[BT_MAX_UUID_STR_LEN];
} stBTRCoreLeUUID;

typedef struct _stBTRCoreLeUUIDList {
    unsigned short   numberOfUUID;
    stBTRCoreLeUUID  uuidList[BTR_MAX_NUMBER_OF_UUID];
} stBTRCoreLeUUIDList;

/* Static Function Prototypes */
static tBTRCoreDevId btrCore_LE_BTGenerateUniqueDeviceID (const char* apcDeviceAddress);

static stBTRCoreLeGattProfile* btrCore_LE_FindGattProfile (stBTRCoreLeHdl *pstBTRCoreLeHdl, tBTRCoreDevId aBtrDeviceID);
static stBTRCoreLeGattService* btrCore_LE_FindGattService (stBTRCoreLeGattProfile  *pstGattProfile, const char *pService);
static stBTRCoreLeGattChar*    btrCore_LE_FindGattCharacteristic (stBTRCoreLeGattService *pstService, const char *pChar);
static stBTRCoreLeGattDesc*    btrCore_LE_FindGattDescriptor (stBTRCoreLeGattChar *pstChar, const char *desc);

static enBTRCoreRet  btrCore_LE_GetGattCharacteristicUUIDList (stBTRCoreLeGattService *pstService, void* uuidList);
static enBTRCoreRet  btrCore_LE_GetGattDescriptorUUIDList (stBTRCoreLeGattChar *pstChar, void* uuidList);
static enBTRCoreRet  btrCore_LE_GetDataPath (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, tBTRCoreDevId  atBTRCoreDevId,
                                             const char* apBtLeUuid, char* rpBtLePath, enBTOpIfceType* renBTOpIfceType);

static unsigned short  btrCore_LE_GetAllowedGattFlagValues (char (*flags)[BT_MAX_UUID_STR_LEN], enBTOpIfceType aenBtOpIfceType);
static inline BOOLEAN  btrCore_LE_isServiceSupported (char* apUUID);

/* Callbacks */
static int  btrCore_LE_GattInfoCb (enBTOpIfceType enBtOpIfceType, const char* apBtGattPath, const char* aBtdevAddr,
                                   enBTDeviceState aenBTDeviceState, void* apConnHdl, void* apUserData );


/* static function definitions */
/* To move this implementation to a common file accessible for BTRCore moduels */
static tBTRCoreDevId
btrCore_LE_BTGenerateUniqueDeviceID (
    const char* apcDeviceAddress
) {
    unsigned long long int  lBTRCoreDevId   = 0;
    char                    lcDevHdlArr[13] = {'\0'};

    if (apcDeviceAddress && (strlen(apcDeviceAddress) >= 17)) {
        lcDevHdlArr[0]  = apcDeviceAddress[0];
        lcDevHdlArr[1]  = apcDeviceAddress[1];
        lcDevHdlArr[2]  = apcDeviceAddress[3];
        lcDevHdlArr[3]  = apcDeviceAddress[4];
        lcDevHdlArr[4]  = apcDeviceAddress[6];
        lcDevHdlArr[5]  = apcDeviceAddress[7];
        lcDevHdlArr[6]  = apcDeviceAddress[9];
        lcDevHdlArr[7]  = apcDeviceAddress[10];
        lcDevHdlArr[8]  = apcDeviceAddress[12];
        lcDevHdlArr[9]  = apcDeviceAddress[13];
        lcDevHdlArr[10] = apcDeviceAddress[15];
        lcDevHdlArr[11] = apcDeviceAddress[16];

        lBTRCoreDevId = (tBTRCoreDevId) strtoll(lcDevHdlArr, NULL, 16);
    }
    return lBTRCoreDevId;
}


static stBTRCoreLeGattProfile*
btrCore_LE_FindGattProfile (
    stBTRCoreLeHdl*        pstBTRCoreLeHdl,
    tBTRCoreDevId          aBtrDeviceID
) {
    unsigned short ui16LoopIdx          = 0;
    stBTRCoreLeGattProfile *pstProfile  = NULL;

    if (pstBTRCoreLeHdl) {
        for (; ui16LoopIdx < pstBTRCoreLeHdl->ui16NumberOfGattProfile; ui16LoopIdx++) {
            if (pstBTRCoreLeHdl->astBTRGattProfile[ui16LoopIdx].deviceID == aBtrDeviceID) {
                pstProfile = &pstBTRCoreLeHdl->astBTRGattProfile[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Profile for Device %llu Found", aBtrDeviceID);
                break;
            }
        }
    }
    return pstProfile;
}


static stBTRCoreLeGattService*
btrCore_LE_FindGattService (
    stBTRCoreLeGattProfile* pstProfile,
    const char*             pService
) {
    unsigned short ui16LoopIdx          = 0;
    stBTRCoreLeGattService *pstService  = NULL;

    if (pstProfile) {
        for (; ui16LoopIdx < pstProfile->ui16NumberOfGattService; ui16LoopIdx++) {
            if (!strcmp(pstProfile->astBTRGattService[ui16LoopIdx].servicePath, pService)) {
                pstService = &pstProfile->astBTRGattService[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Service %s Found", pService);
                break;
            }
        }
    }
    return pstService;
}


static stBTRCoreLeGattChar*
btrCore_LE_FindGattCharacteristic (
    stBTRCoreLeGattService* pstService,
    const char*             pChar
) {
    unsigned short ui16LoopIdx      = 0;
    stBTRCoreLeGattChar *pstChar    = NULL;

    if (pstService) {
        for (; ui16LoopIdx < pstService->ui16NumberOfGattChar; ui16LoopIdx++) {
            if (!strcmp(pstService->astBTRGattChar[ui16LoopIdx].charPath, pChar)) {
                pstChar = &pstService->astBTRGattChar[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Char %s Found", pChar);
                break;
            }
        }
    }
    return pstChar;
}


static stBTRCoreLeGattDesc*
btrCore_LE_FindGattDescriptor (
    stBTRCoreLeGattChar*    pstChar,
    const char*             pDesc
) {
    unsigned short ui16LoopIdx      = 0;
    stBTRCoreLeGattDesc *pstDesc    = NULL;

    if (pstChar) {
        for (; ui16LoopIdx < pstChar->ui16NumberOfGattDesc; ui16LoopIdx++) {
            if (!strcmp(pstChar->atBTRGattDesc[ui16LoopIdx].descPath, pDesc)) {
                pstDesc = &pstChar->atBTRGattDesc[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Descriptor %s Found", pDesc);
                break;
            }
        }
    }
    return pstDesc; 
}

// Could make this to a common layer as btrcore.c also uses similar api
static inline BOOLEAN
btrCore_LE_isServiceSupported (
    char*           apUUID
) {
    BOOLEAN isSupported = FALSE;
    char lUUID[8];

    if (!apUUID) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    lUUID[0] = '0';
    lUUID[1] = 'x';
    lUUID[2] = apUUID[4];
    lUUID[3] = apUUID[5];
    lUUID[4] = apUUID[6];
    lUUID[5] = apUUID[7];
    lUUID[6] = '\0';
    
    if (!strcmp(lUUID, BTR_CORE_GATT_TILE_1) ||
        !strcmp(lUUID, BTR_CORE_GATT_TILE_2) ){
        isSupported = TRUE;
    } /* - Add further supported Services
    else if {
    }
    else if {
    } */

    return isSupported;
}

static enBTRCoreRet
btrCore_LE_GetGattCharacteristicUUIDList (
    stBTRCoreLeGattService* pstService,
    void*                   uuidList
) {
    stBTRCoreLeUUIDList* lstBTRCoreLeUUIDList = (stBTRCoreLeUUIDList*)uuidList;
    unsigned short ui16LoopIdx = 0;

    if (!pstService || !uuidList) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    for (; ui16LoopIdx < pstService->ui16NumberOfGattChar; ui16LoopIdx++) {
        strncpy(lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].uuid, pstService->astBTRGattChar[ui16LoopIdx].charUuid, BT_MAX_UUID_STR_LEN-1);
        lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].flags = pstService->astBTRGattChar[ui16LoopIdx].charFlags;
    }
    lstBTRCoreLeUUIDList->numberOfUUID = ui16LoopIdx;

    return enBTRCoreSuccess;
}


static enBTRCoreRet
btrCore_LE_GetGattDescriptorUUIDList (
    stBTRCoreLeGattChar*    pstChar,
    void*                   uuidList
) {
    stBTRCoreLeUUIDList* lstBTRCoreLeUUIDList = (stBTRCoreLeUUIDList*)uuidList;
    unsigned short ui16LoopIdx = 0;

    if (!pstChar || !uuidList) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    for (; ui16LoopIdx < pstChar->ui16NumberOfGattDesc; ui16LoopIdx++) {
        strncpy(lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].uuid, pstChar->atBTRGattDesc[ui16LoopIdx].descUuid, BT_MAX_UUID_STR_LEN-1);
        lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].flags = pstChar->atBTRGattDesc[ui16LoopIdx].descFlags;
    }
    lstBTRCoreLeUUIDList->numberOfUUID = ui16LoopIdx;

    return enBTRCoreSuccess;
}



static enBTRCoreRet
btrCore_LE_GetDataPath (
    tBTRCoreLeHdl       hBTRCoreLe,
    void*               apBtConn,
    tBTRCoreDevId       atBTRCoreDevId,
    const char*         apBtLeUuid,
    char*               rpBtLePath,
    enBTOpIfceType*     renBTOpIfceType
) {
    unsigned short ui16PLoopindex       = 0;
    char* retLeDataPath                 = NULL;
    stBTRCoreLeGattProfile *pProfile    = NULL;

    if (!hBTRCoreLe || !apBtConn || !atBTRCoreDevId || !apBtLeUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    stBTRCoreLeHdl*  pstlhBTRCoreLe   = (stBTRCoreLeHdl*)hBTRCoreLe;

    if (!pstlhBTRCoreLe->ui16NumberOfGattProfile) {
        BTRCORELOG_ERROR ("No Gatt Profile Exists!!!!\n");
        return enBTRCoreFailure;
    }

    for (; ui16PLoopindex < pstlhBTRCoreLe->ui16NumberOfGattProfile; ui16PLoopindex++) {
        pProfile = &pstlhBTRCoreLe->astBTRGattProfile[ui16PLoopindex];

        if (atBTRCoreDevId == pProfile->deviceID) {
            unsigned short ui16SLoopindex       = 0;
            stBTRCoreLeGattService *pService    = NULL;

            BTRCORELOG_DEBUG ("Profile Matched for Device %llu \n", atBTRCoreDevId);

            if (pProfile->ui16NumberOfGattService == 0) {
                BTRCORELOG_ERROR ("No Gatt Service Exists!!!\n");
                break; // profile loop
            }

            for (; ui16SLoopindex < pProfile->ui16NumberOfGattService; ui16SLoopindex++) {
                pService  = &pProfile->astBTRGattService[ui16SLoopindex];

                if (strstr(pService->serviceUuid, apBtLeUuid)) {
                    retLeDataPath = pService->servicePath;
                    *renBTOpIfceType = enBTGattService;
                    BTRCORELOG_DEBUG ("UUID matched Service : %s", pService->servicePath);
                    break; // service loop
                }
            }

            if (ui16SLoopindex != pProfile->ui16NumberOfGattService) {
                break; // profile loop
            }

            for (ui16SLoopindex=0; ui16SLoopindex < pProfile->ui16NumberOfGattService; ui16SLoopindex++) {
                unsigned short ui16CLoopindex   = 0;
                stBTRCoreLeGattChar *pChar      = NULL;

                pService  = &pProfile->astBTRGattService[ui16SLoopindex];

                if (pService->ui16NumberOfGattChar == 0) {
                    continue;  /* Service has no Char to loop through */
                }

                for (; ui16CLoopindex < pService->ui16NumberOfGattChar; ui16CLoopindex++) {
                    pChar = &pService->astBTRGattChar[ui16CLoopindex];

                    if (!strcmp(pChar->charUuid, apBtLeUuid)) {
                        retLeDataPath = pChar->charPath;
                        *renBTOpIfceType = enBTGattCharacteristic;
                        BTRCORELOG_DEBUG ("UUID matched Characteristic : %s", pChar->charPath);
                        break; // char loop
                    }
                    else {
                        unsigned short ui16DLoopindex   = 0;
                        stBTRCoreLeGattDesc  *pDesc     = NULL;

                        if (pChar->ui16NumberOfGattDesc == 0) {
                            continue;  /* Char has no desc to loop through */
                        }

                        for (; ui16DLoopindex < pChar->ui16NumberOfGattDesc; ui16DLoopindex++) {
                            pDesc = &pChar->atBTRGattDesc[ui16DLoopindex];

                            if (!strcmp(apBtLeUuid, pDesc->descUuid)) {
                                retLeDataPath = pDesc->descPath;
                                *renBTOpIfceType = enBTGattDescriptor;
                                BTRCORELOG_DEBUG ("UUID matched Descriptor : %s\n", pDesc->descPath);
                                break; // desc loop
                            }
                        }
                        if (ui16DLoopindex != pChar->ui16NumberOfGattDesc) {
                            break; // char loop
                        }
                    }
                }
                if (ui16CLoopindex != pService->ui16NumberOfGattChar) {
                    break; // service loop
                }
            }
            break; // profile loop
        }
    }

    if (ui16PLoopindex == pstlhBTRCoreLe->ui16NumberOfGattProfile) {
        BTRCORELOG_ERROR ("Profile Not Found for Dev : %llu!!!\n", atBTRCoreDevId);
    }

    if (retLeDataPath) {
        strncpy (rpBtLePath, retLeDataPath, BT_MAX_STR_LEN-1);
    } else {
        BTRCORELOG_ERROR ("No match found for UUID : %s !!!\n", apBtLeUuid);
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


static unsigned short
btrCore_LE_GetAllowedGattFlagValues (
    char            (*flags)[BT_MAX_UUID_STR_LEN],
    enBTOpIfceType  aenBtOpIfceType
) {
    unsigned short flagBits = 0;
    unsigned char  u8idx    = 0,
                   maxFlags = 0;

    if (!flags || !flags[0]) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return 0;
    }

    if (aenBtOpIfceType == enBTGattCharacteristic) {
        maxFlags = BTR_MAX_GATT_CHAR_FLAGS;
    } else
    if (aenBtOpIfceType == enBTGattDescriptor) {
        maxFlags = BTR_MAX_GATT_DESC_FLAGS;
    }

    for (; u8idx < maxFlags && flags[u8idx]; u8idx++) {
        if (!strcmp("read", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_READ;
        }
        if (!strcmp("write", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_WRITE;
        }
        if (!strcmp("encrypt-read", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_ENCRYPT_READ;
        }
        if (!strcmp("encrypt-write", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_ENCRYPT_WRITE;
        }
        if (!strcmp("encrypt-authenticated-read", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ;
        }
        if (!strcmp("encrypt-authenticated-write", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE;
        }
        if (!strcmp("secure-read", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_SECURE_READ;
        }
        if (!strcmp("secure-write", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_SECURE_WRITE;
        }
        if (!strcmp("notify", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_NOTIFY;
        }
        if (!strcmp("indicate", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_INDICATE;
        }
        if (!strcmp("broadcast", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_BROADCAST;
        }
        if (!strcmp("write-without-response", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE;
        }
        if (!strcmp("authenticated-signed-writes", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES;
        }
        if (!strcmp("reliable-write", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_RELIABLE_WRITE;
        }
        if (!strcmp("writable-auxiliaries", flags[u8idx])) {
            flagBits |= BTR_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES;
        }
    }
    BTRCORELOG_INFO ("- %d\n", flagBits);

    return flagBits;
}


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_LE_Init (
    tBTRCoreLeHdl*      phBTRCoreLe,
    void*               apBtConn,
    const char*         apBtAdapter
) {
    enBTRCoreRet       lenBTRCoreRet   = enBTRCoreSuccess;
    stBTRCoreLeHdl*    pstlhBTRCoreLe  = NULL;

    if (!phBTRCoreLe || !apBtConn || !apBtAdapter) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    BTRCORELOG_ERROR ("BTRCore_LE_Init\n");

    pstlhBTRCoreLe = (stBTRCoreLeHdl*)malloc(sizeof(stBTRCoreLeHdl));

    if (!pstlhBTRCoreLe) {
        BTRCORELOG_ERROR ("Memory Allocation Failed\n");
        return enBTRCoreInitFailure;
    }

    memset(pstlhBTRCoreLe, 0, sizeof(stBTRCoreLeHdl));


    if (BtrCore_BTRegisterLEGattInfoCb (apBtConn,
                                        apBtAdapter,
                                        &btrCore_LE_GattInfoCb,
                                        pstlhBTRCoreLe)) {
        lenBTRCoreRet = enBTRCoreFailure;
    }
                                       
    if (lenBTRCoreRet != enBTRCoreSuccess) {
        free(pstlhBTRCoreLe);
        pstlhBTRCoreLe = NULL;
    }

    *phBTRCoreLe  = (tBTRCoreLeHdl)pstlhBTRCoreLe;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_LE_DeInit (
    tBTRCoreLeHdl  hBTRCoreLe,
    void*          apBtConn,
    const char*    apBtAdapter
) {
    //stBTRCoreLeHdl*    pstlhBTRCoreLe = NULL;
    enBTRCoreRet       lenBTRCoreRet  = enBTRCoreFailure;

    if (!hBTRCoreLe || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }

    free (hBTRCoreLe);

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_LE_GetGattProperty (
    tBTRCoreLeHdl        hBTRCoreLe,
    void*                apvBtConn,
    tBTRCoreDevId        atBTRCoreDevId,
    const char*          apcBtUuid,
    enBTRCoreLEGattProp  aenBTRCoreLEGattProp,
    void*                apvBtPropValue
) {
    char lpcBtLePath[BT_MAX_STR_LEN] = "\0";
    enBTOpIfceType lenBTOpIfceType   = enBTUnknown;
    //tBTRCoreDevId  ltBTRCoreDevId    = btrCore_LE_BTGenerateUniqueDeviceID (apcBtDevAddr);
    unBTOpIfceProp lunBTOpIfceProp;

    if (!hBTRCoreLe || !apvBtConn || !atBTRCoreDevId || !apcBtUuid)  {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }
  
    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, atBTRCoreDevId, apcBtUuid, lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
        return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath[0]) {
        BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!");
        return enBTRCoreFailure;
    }
     
    switch (aenBTRCoreLEGattProp) {

    case enBTRCoreLEGPropUUID:
        if (lenBTOpIfceType == enBTGattService) {
            stBTRCoreLeHdl*    lpstlhBTRCoreLe  = (stBTRCoreLeHdl*)hBTRCoreLe;

            stBTRCoreLeGattProfile*  pProfile   = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
            stBTRCoreLeGattService*  pService   = btrCore_LE_FindGattService(pProfile, lpcBtLePath);

            if (!pService) {
                BTRCORELOG_ERROR ("Failed to find pProfile|pService|pChar : %p|%p\n", pProfile, pService);
                return enBTRCoreFailure;
            }

            if (btrCore_LE_GetGattCharacteristicUUIDList(pService, apvBtPropValue) != enBTRCoreSuccess) {
                BTRCORELOG_ERROR ("Failed to get Char UUID List for Service %s\n", lpcBtLePath);
                return enBTRCoreFailure;
            }

            return enBTRCoreSuccess;  /* Is it Ok to have return here or return logically at function End? */
        } else
        if (lenBTOpIfceType == enBTGattCharacteristic) {
            char servicePath[BT_MAX_STR_LEN]   = "\0";
            stBTRCoreLeHdl*    lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;

            lunBTOpIfceProp.enBtGattCharProp   = enBTGattCPropService;
            if (BtrCore_BTGetProp (apvBtConn, lpcBtLePath, lenBTOpIfceType, lunBTOpIfceProp, servicePath)) {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get servicePath on %s !!!\n", lpcBtLePath);
                return enBTRCoreFailure;
            }

            stBTRCoreLeGattProfile*  pProfile   = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
            stBTRCoreLeGattService*  pService   = btrCore_LE_FindGattService(pProfile, servicePath);
            stBTRCoreLeGattChar*     pChar      = btrCore_LE_FindGattCharacteristic(pService, lpcBtLePath);

            if (!pChar) {
                BTRCORELOG_ERROR ("Failed to find pProfile|pService|pChar : %p|%p|%p\n", pProfile, pService, pChar);
                return enBTRCoreFailure;
            }

            if (btrCore_LE_GetGattDescriptorUUIDList(pChar, apvBtPropValue) != enBTRCoreSuccess) {
                BTRCORELOG_ERROR ("Failed to get Desc UUID List for Char %s\n", lpcBtLePath);
                return enBTRCoreFailure;
            }

            return enBTRCoreSuccess;  /* Is it Ok to have return here or return logically at function End? */
        } else
        if (lenBTOpIfceType == enBTGattDescriptor) {
            char servicePath[BT_MAX_STR_LEN]    = "\0";
            char charPath[BT_MAX_STR_LEN]       = "\0";
            stBTRCoreLeHdl*         lpstlhBTRCoreLe         = (stBTRCoreLeHdl*)hBTRCoreLe;
            stBTRCoreLeUUIDList*    lstBTRCoreLeUUIDList    = (stBTRCoreLeUUIDList*)apvBtPropValue;
            char retCPath = -1, retSPath = -1;

            lunBTOpIfceProp.enBtGattDescProp                = enBTGattDPropCharacteristic;
            retCPath = BtrCore_BTGetProp (apvBtConn, lpcBtLePath, lenBTOpIfceType, lunBTOpIfceProp, charPath);
            lunBTOpIfceProp.enBtGattCharProp                = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp (apvBtConn, charPath, lenBTOpIfceType, lunBTOpIfceProp, servicePath);

            if (retCPath || retSPath) {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get charPath/servicePath : %s / %s\n", charPath, servicePath);
                return enBTRCoreFailure;
            }

            stBTRCoreLeGattProfile* pProfile   = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
            stBTRCoreLeGattService* pService   = btrCore_LE_FindGattService(pProfile, servicePath);
            stBTRCoreLeGattChar*    pChar      = btrCore_LE_FindGattCharacteristic(pService, charPath);
            stBTRCoreLeGattDesc*    pDesc      = btrCore_LE_FindGattDescriptor(pChar, lpcBtLePath);

            if (pDesc) {
                BTRCORELOG_ERROR ("Failed to find pProfile|pService|pChar|pDesc : %p|%p|%p|%p\n", pProfile, pService, pChar, pDesc);
                return enBTRCoreFailure;
            }

            strncpy(lstBTRCoreLeUUIDList->uuidList[0].uuid, pDesc->descUuid, BT_MAX_UUID_STR_LEN-1);
            lstBTRCoreLeUUIDList->uuidList[0].flags = pDesc->descFlags;

            return enBTRCoreSuccess;  /* Is it Ok to have return here or return logically at function End? */
        }
        break;
    case enBTRCoreLEGPropPrimary:
        lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropPrimary;
        break;
    case enBTRCoreLEGPropDevice:
        lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
        break;
    case enBTRCoreLEGPropService:
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
        break;
    case enBTRCoreLEGPropValue:
        if (lenBTOpIfceType == enBTGattCharacteristic) {
            lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropValue;
        } else if (lenBTOpIfceType == enBTGattDescriptor) {
            lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropValue;
        }
        break;
    case enBTRCoreLEGPropNotifying:
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropNotifying;
        break;
    case enBTRCoreLEGPropFlags:
        if (lenBTOpIfceType == enBTGattCharacteristic) {
            lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
        } else if (lenBTOpIfceType == enBTGattDescriptor) {
            lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropFlags;
        }
        break;
    case enBTRCoreLEGPropChar:
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
        break;
    case enBTRCoreLEGPropUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid enBTRCoreLEGattProp Options %d !!!", aenBTRCoreLEGattProp);
        break;
    }

    if (lenBTOpIfceType == enBTUnknown || BtrCore_BTGetProp (apvBtConn,
                                                             lpcBtLePath,
                                                             lenBTOpIfceType,
                                                             lunBTOpIfceProp,
                                                             apvBtPropValue)) {
        BTRCORELOG_ERROR ("Failed to get Gatt %d Prop for UUID : %s!!!", aenBTRCoreLEGattProp, apcBtUuid);
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

/* Get the UUIDs implicitly based on the Op type ? instead of getting it from the higher layer */
enBTRCoreRet
BtrCore_LE_PerformGattOp (
    tBTRCoreLeHdl      hBTRCoreLe,
    void*              apvBtConn,
    tBTRCoreDevId      atBTRCoreDevId,
    const char*        apcBtUuid,
    enBTRCoreLEGattOp  aenBTRCoreLEGattOp,
    void*              rpLeOpRes
) {
    char lpcBtLePath[BT_MAX_STR_LEN] = "\0";
    char *pDevicePath                = "\0";
    enBTOpIfceType lenBTOpIfceType   = enBTUnknown;
    enBTLeGattOp   lenBTLeGattOp     = enBTLeGattOpUnknown;
    //tBTRCoreDevId  ltBTRCoreDevId    = btrCore_LE_BTGenerateUniqueDeviceID (apcBtDevAddr);

    if (!hBTRCoreLe || !apvBtConn || !atBTRCoreDevId || !apcBtUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }
  
    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, atBTRCoreDevId, apcBtUuid, lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath[0]) {
       BTRCORELOG_ERROR ("LE Path is NULL!!!");
       return enBTRCoreFailure;
    }

    if (lenBTOpIfceType == enBTGattService) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg | %s is a Service UUID...LE Service Ops are not available!!!\n", apcBtUuid);
        return enBTRCoreFailure;
    } else
    if (lenBTOpIfceType == enBTGattCharacteristic ||
        lenBTOpIfceType == enBTGattDescriptor     ){
        stBTRCoreLeHdl*    lpstlhBTRCoreLe  = (stBTRCoreLeHdl*)hBTRCoreLe;
        stBTRCoreLeGattProfile*  pProfile   = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
        pDevicePath = pProfile->devicePath;
    } 

    switch (aenBTRCoreLEGattOp) {

    case enBTRCoreLEGOpReadValue:
        lenBTLeGattOp = enBTLeGattOpReadValue;
        break;
    case enBTRCoreLEGOpWriteValue:
        lenBTLeGattOp = enBTLeGattOpWriteValue;
        break;
    case enBTRCoreLEGOpStartNotify:
        lenBTLeGattOp = enBTLeGattOpStartNotify;
        break;
    case enBTRCoreLEGOpStopNotify:
        lenBTLeGattOp = enBTLeGattOpStopNotify;
        break;
    case enBTRCoreLEGOpUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid enBTRCoreLEGattOp Options %d !!!", aenBTRCoreLEGattOp);
        break;
    }

    if (lenBTLeGattOp == enBTLeGattOpUnknown || BtrCore_BTPerformLeGattOp (apvBtConn,
                                                                           lpcBtLePath,
                                                                           lenBTOpIfceType,
                                                                           lenBTLeGattOp,
                                                                           (void*)pDevicePath,  // for now
                                                                           rpLeOpRes) ) {
       BTRCORELOG_ERROR ("Failed to Perform Le Gatt Op %d for UUID %s  !!!", aenBTRCoreLEGattOp, apcBtUuid);
       return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

// Outgoing callbacks Registration Interfaces
#if 0
enBTRCoreRet
BTRCore_LE_RegisterDevStatusUpdateCb (
    tBTRCoreLeHdl*                      hBTRCoreLe,
    fPtr_BTRCore_LeDevStatusUpdateCb    afpcBTRCoreLeDevStatusUpdate,
    void*                               apvBtLeDevStatusUserData
) {
    stBTRCoreLeHdl* phBTRCoreLe = NULL;

    if (!hBTRCoreLe || !afpcBTRCoreLeDevStatusUpdate)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    phBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;

    phBTRCoreLe->fpcBTRCoreLeDevStatusUpdate = afpcBBTRCoreAVMediaStatusUpdate;
    phBTRCoreLe->pvBtLeDevStatusUserData     = apvBtLeDevStatusUserData;

    return enBTRCoreSuccess;
}
#endif


//incoming callbacks
static int
btrCore_LE_GattInfoCb (
    enBTOpIfceType      aenBtOpIfceType,
    const char*         apBtGattPath,
    const char*         aBtdevAddr,
    enBTDeviceState     aenBTDeviceState,
    void*               apConnHdl,
    void*               apUserData
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreLeGattProfile* pProfile        = NULL;
    tBTRCoreDevId           ltBTRCoreDevId  = btrCore_LE_BTGenerateUniqueDeviceID (aBtdevAddr);

    if (!apBtGattPath || !aBtdevAddr || !apUserData) {
        BTRCORELOG_ERROR("Invalid arguments!!!");
        return -1;
    }
    BTRCORELOG_DEBUG("apBtGattPath : %s\n", apBtGattPath);

    if (aenBTDeviceState == enBTDevStFound) {
        char lBtUuid[BT_MAX_STR_LEN] = "\0";
        unBTOpIfceProp aunBTOpIfceProp;

        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Storing GATT Service Info...\n");
            char lBtDevPath[BT_MAX_STR_LEN] = "\0";
            char retUuid = -1, retDPath = -1;
            stBTRCoreLeGattService *pService = NULL;

            aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
            retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
            retDPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtDevPath);

            if (!retUuid && !retDPath) {
                if (btrCore_LE_isServiceSupported(lBtUuid)) {
                    if (!(pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                        if (lpstlhBTRCoreLe->ui16NumberOfGattProfile < BTR_MAX_GATT_PROFILE) {
                            pProfile = &lpstlhBTRCoreLe->astBTRGattProfile[lpstlhBTRCoreLe->ui16NumberOfGattProfile];
                            strncpy(pProfile->devicePath, lBtDevPath, BTRCORE_MAX_STR_LEN - 1);
                            pProfile->deviceID = ltBTRCoreDevId;
                            lpstlhBTRCoreLe->ui16NumberOfGattProfile++;
                            BTRCORELOG_DEBUG ("Added Profile for Device %llu Successfully.", ltBTRCoreDevId);
                        } else {
                            BTRCORELOG_ERROR ("BTR_MAX_GATT_PROFILE Added. Couldn't add anymore...");           }
                    }
                    if (pProfile->ui16NumberOfGattService < BTR_MAX_GATT_SERVICE) {
                        if (!(pService = btrCore_LE_FindGattService(pProfile, apBtGattPath))) {
                            pService = &pProfile->astBTRGattService[pProfile->ui16NumberOfGattService];
                            strncpy(pService->serviceUuid, lBtUuid, BT_MAX_UUID_STR_LEN - 1);
                            strncpy(pService->servicePath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                            pService->parentProfile = pProfile;
                            pProfile->ui16NumberOfGattService++;
                            BTRCORELOG_DEBUG ("Added Service %s Successfully.", lBtUuid);
                        } else {
                            BTRCORELOG_WARN ("Gatt Service %s already exists...", apBtGattPath);                }
                    } else {
                        BTRCORELOG_WARN ("BTR_MAX_GATT_SERVICE Added. Couldn't add anymore...");                }
                } else {
                    BTRCORELOG_WARN ("Service Not Supported | UUID : %s\n", lBtUuid);                           }
            } else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retDPath : %d",retUuid, retDPath);   }
        } else 
        if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG ("Storing GATT Characteristic Info...\n");
            char lBtSerivcePath[BT_MAX_STR_LEN]  = "\0";
            char retUuid = -1, retSPath = -1;
            stBTRCoreLeGattService *pService = NULL;
            stBTRCoreLeGattChar    *pChar    = NULL;

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;
            retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtSerivcePath);

            if (!retUuid && !retSPath) {
                if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                    if ((pService = btrCore_LE_FindGattService(pProfile,  lBtSerivcePath))) {
                        if ((pService->ui16NumberOfGattChar < BTR_MAX_GATT_CHAR)) {
                            if (!(pChar = btrCore_LE_FindGattCharacteristic(pService, apBtGattPath))) {
                                char cFlags[BTR_MAX_GATT_CHAR_FLAGS][BT_MAX_UUID_STR_LEN];
                                memset (cFlags, 0, sizeof(cFlags));
                                pChar = &pService->astBTRGattChar[pService->ui16NumberOfGattChar];
                                strncpy(pChar->charUuid, lBtUuid, BT_MAX_UUID_STR_LEN - 1 );
                                strncpy(pChar->charPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                                pChar->parentService = pService;
                                aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
                                if (!BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&cFlags)) {
                                    pChar->charFlags = btrCore_LE_GetAllowedGattFlagValues(cFlags, enBTGattCharacteristic);
                                }
                                pService->ui16NumberOfGattChar++;
                                BTRCORELOG_DEBUG ("Added Characteristic %s Successfully.", lBtUuid);
                            } else {
                                BTRCORELOG_WARN ("Gatt Characteristic %s already exists...", apBtGattPath);     }
                        } else {
                            BTRCORELOG_WARN ("BTR_MAX_GATT_CHAR Addedd. Couldn't add anymore...");              }
                    } else {
                        BTRCORELOG_ERROR ("Gatt Service %s not found...", lBtSerivcePath);                      }
                } else {
                    BTRCORELOG_ERROR ("Gatt Profile for device %llu not found...", ltBTRCoreDevId);             }
            } else  {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retSPath : %d",retUuid, retSPath);   }
        } else
        if (aenBtOpIfceType == enBTGattDescriptor) {
            BTRCORELOG_DEBUG ("Storing GATT Descriptor Info...\n");
            char lBtSerivcePath[BT_MAX_STR_LEN]  = "\0";
            char lBtCharPath[BT_MAX_STR_LEN]     = "\0";
            char retUuid = -1, retCPath = -1, retSPath = -1;
            stBTRCoreLeGattService *pService = NULL;
            stBTRCoreLeGattChar    *pChar    = NULL;
            stBTRCoreLeGattDesc    *pDesc    = NULL;

            aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;
            retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
            retCPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtCharPath);

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp(apConnHdl, lBtCharPath, enBTGattCharacteristic, aunBTOpIfceProp, (void*)&lBtSerivcePath);

            if (!retUuid && !retCPath && !retSPath) {
                if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                    if ((pService = btrCore_LE_FindGattService(pProfile,  lBtSerivcePath))) {
                        if ((pChar = btrCore_LE_FindGattCharacteristic(pService, lBtCharPath))) {
                            if (pChar->ui16NumberOfGattDesc < BTR_MAX_GATT_DESC) {
                                if (!(pDesc = btrCore_LE_FindGattDescriptor(pChar, apBtGattPath))) {
                                    char dFlags[BTR_MAX_GATT_DESC_FLAGS][BT_MAX_UUID_STR_LEN];
                                    memset (dFlags, 0, sizeof(dFlags));
                                    pDesc = &pChar->atBTRGattDesc[pChar->ui16NumberOfGattDesc];
                                    strncpy (pDesc->descUuid, lBtUuid, BT_MAX_UUID_STR_LEN - 1);
                                    strncpy(pDesc->descPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                                    pDesc->parentChar = pChar;
                                    aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropFlags;
                                    if (!BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&dFlags)) {
                                        pDesc->descFlags = btrCore_LE_GetAllowedGattFlagValues(dFlags, enBTGattDescriptor);
                                    }
                                    pChar->ui16NumberOfGattDesc++;
                                    BTRCORELOG_DEBUG ("Added Gatt Descriptor %s Successfully.", lBtUuid);
                                } else {
                                    BTRCORELOG_WARN ("Gatt Descriptor %s already exists...", apBtGattPath);     }
                            } else {
                                BTRCORELOG_WARN ("BTR_MAX_GATT_DESC Added. Couldn't add anymore...");           }
                        } else {
                            BTRCORELOG_ERROR ("Gatt Characteristic not found for Desc %s", apBtGattPath);       }
                    } else {
                        BTRCORELOG_ERROR ("Gatt Service not found for Desc %s", apBtGattPath);                  }
                } else {
                    BTRCORELOG_ERROR ("Gatt Profile for device %llu not found...", ltBTRCoreDevId);             }
            } else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retCPath : %d | retSpath : %d !!!", retUuid, retCPath, retSPath);
            }
        }
    } 
    else
    if (aenBTDeviceState == enBTDevStLost) {
        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Freeing Gatt Service %s ", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        } else
        if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG ("Freeing GATT Characteristic %s ", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        } else
        if (aenBtOpIfceType == enBTGattDescriptor) {
            BTRCORELOG_DEBUG ("Freeing GATT Descriptor %s ", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        }
    }
    else
    if (aenBTDeviceState == enBTDevStPropChanged) {
        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Property Changed for Gatt Service %s", apBtGattPath);
        }
        if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG ("Property Changed for Gatt Char %s", apBtGattPath);
        #if 0
            /* To decide if we will require this */
            char lBtSerivcePath[BT_MAX_STR_LEN] = "\0";
            unBTOpIfceProp aunBtOpIfceProp;
            aunBTOpIfceProp.enBtGattCharProp    = enBTGattCPropService;

            if (!BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtSerivcePath)) {
                if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                    if ((pService = btrCore_LE_FindGattService(pProfile,  lBtSerivcePath)))   {
                        if ((pChar = btrCore_LE_FindGattCharacteristic(pService, apBtGattPath))) {

                            if (pChar->charFlags & (BTR_GATT_CHAR_FLAG_READ))) {
                                unsigned char value[BT_MAX_UUID_STR_LEN] = "\0";
                                char byteValue[BT_MAX_STR_LEN]           = "\0";
                                char hexaValue[]                         = "0123456789abcdef";
                                aunBtOpIfceProp.enBtGattCharProp         = enBTGattCPropValue;

                                if (!BtrCore_BTGetProp (apConnHdl, apBtGattPath, enBTGattCharacteristic, aunBtOpIfceProp, (void*)&value)) {
                                    unsigned char u8idx=0, u8idx2=0;
                                    while (value[u8idx]) {
                                          byteValue[u8idx2++] = hexaValue[value[u8idx] >> 4];
                                          byteValue[u8idx2++] = hexaValue[value[u8idx] &  0x0F];
                                          u8idx++;
                                    }
                                    byteValue[u8idx2] = '\0';
                                    BTRCORELOG_DEBUG ("Obtained byteValue : %s", byteValue);
                                    /* -------------Callback to Higher Layers-------------- */
                                } else {
                                   BTRCORE_ERROR ("BtrCore_BTGetProp Failed to get property enBTGattCPropValue");                   }
                            } else {  
                                BTRCORE_ERROR ("BTR_GATT_CHAR_FLAG_READ Operation not permitted in interface %s\n", apBtGattPath);  }
                        } else {
                            BTRCORE_ERROR ("Gatt Char %s Not Found\n", apBtGattPath);                                               }
                    } else {
                        BTRCORE_ERROR ("Gatt Service %s Not Found\n", lBtSerivcePath);                                              }
                } else {
                    BTRCORE_ERROR ("Gatt Profile for Device  %llu Not Found\n", ltBTRCoreDevId);                                    }
            } else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get property enBTGattCPropService\n");
            }
       #endif
       }
       if (aenBtOpIfceType == enBTGattDescriptor) {
          BTRCORELOG_DEBUG ("Property Changed for Gatt Desc %s", apBtGattPath);
       }
    }
    else
    {
       BTRCORELOG_WARN ("Callback for irrelavent DeviceState : %d!!!", aenBTDeviceState);
    }
    return 0;
}


