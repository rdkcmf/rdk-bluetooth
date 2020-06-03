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


#define BTR_MAX_GATT_PROFILE                            16
#define BTR_MAX_GATT_SERVICE                            BT_MAX_NUM_GATT_SERVICE
#define BTR_MAX_GATT_CHAR                               BT_MAX_NUM_GATT_CHAR
#define BTR_MAX_GATT_DESC                               BT_MAX_NUM_GATT_DESC


#define BTR_MAX_GATT_CHAR_FLAGS                         BT_MAX_NUM_GATT_CHAR_FLAGS
#define BTR_MAX_GATT_DESC_FLAGS                         BT_MAX_NUM_GATT_DESC_FLAGS
#define BTR_MAX_NUMBER_OF_UUID                          32


/* Characteristic Property bit field and Characteristic Extented Property bit field Values */
#define BTR_GATT_CHAR_FLAG_READ                         BT_GATT_CHAR_FLAG_READ
#define BTR_GATT_CHAR_FLAG_WRITE                        BT_GATT_CHAR_FLAG_WRITE
#define BTR_GATT_CHAR_FLAG_ENCRYPT_READ                 BT_GATT_CHAR_FLAG_ENCRYPT_READ
#define BTR_GATT_CHAR_FLAG_ENCRYPT_WRITE                BT_GATT_CHAR_FLAG_ENCRYPT_WRITE
#define BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ   BT_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ
#define BTR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE  BT_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE
#define BTR_GATT_CHAR_FLAG_SECURE_READ                  BT_GATT_CHAR_FLAG_SECURE_READ                  /* Server Mode only */
#define BTR_GATT_CHAR_FLAG_SECURE_WRITE                 BT_GATT_CHAR_FLAG_SECURE_WRITE                 /* Server Mode only */
#define BTR_GATT_CHAR_FLAG_NOTIFY                       BT_GATT_CHAR_FLAG_NOTIFY
#define BTR_GATT_CHAR_FLAG_INDICATE                     BT_GATT_CHAR_FLAG_INDICATE
#define BTR_GATT_CHAR_FLAG_BROADCAST                    BT_GATT_CHAR_FLAG_BROADCAST
#define BTR_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE       BT_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE
#define BTR_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES  BT_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES
#define BTR_GATT_CHAR_FLAG_RELIABLE_WRITE               BT_GATT_CHAR_FLAG_RELIABLE_WRITE
#define BTR_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES         BT_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES

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


/* Immediate Alert Service UUID */
#define BTR_GATT_LE_IAS_UUID                "00001802-0000-1000-8000-00805f9b34fb"
#define BTR_LE_IAS_ALERT_LEVEL_CHR_UUID     "00002a06-0000-1000-8000-00805f9b34fb"

/* Random UUID for testing purpose */
#define BTR_LE_IAS_RW_DESCRIPTOR_UUID       "8260c653-1a54-426b-9e36-e84c238bc669"

/* Advertisement data structure */
typedef struct _stBTRCoreLeManfData {
    unsigned short  ManfID;                                                     /* Manufacturer ID Key */
    unsigned int    lenManfData;                                                /* Length of data associated with the manufacturer ID */
    unsigned char   data[BT_MAX_GATT_OP_DATA_LEN];                              /* Data associated with the manufacturer ID */
} stBTRCoreLeManfData;

typedef struct _stBTRCoreLeServData {
    char    UUID[BT_MAX_STR_LEN];                                               /* UUID of the service data - Key */
    uint8_t data[BT_MAX_GATT_OP_DATA_LEN];                                      /* Data associated with the service UUID */
} stBTRCoreLeServData;

typedef struct _stBTRCoreLeCustomAdv {
    char                    AdvertisementType[BT_MAX_STR_LEN];                          /* Type of advertising packet : "Broadcast"/"Peripheral" */
    char                    ServiceUUID[BT_MAX_NUM_GATT_SERVICE][BT_MAX_STR_LEN];       /* List of the UUIDs of the services supported by the device */
    int                     numServiceUUID;                                             /* Number of Services supported by the device */
    char                    SolicitUUID[BT_MAX_NUM_GATT_SERVICE][BT_MAX_STR_LEN];       /* Services the Peripheral device maybe interested in when it is a Gatt client too */
    int                     numSolicitUUID;                                             /* Number of solicit services of the device */
    stBTRCoreLeManfData     ManfData;                                                   /* Manufacturer Id and the data assosciated */
    stBTRCoreLeServData     ServiceData;                                                /* Arbitary data associated with the UUID */
    unsigned char           bTxPower;                                                   /* Includes Tx power in advertisement */
} stBTRCoreLeCustomAdv;

/* GattDescriptor1 Properties */
typedef struct _stBTRCoreLeGattDesc {
    char                    descPath[BTRCORE_MAX_STR_LEN];                       /* Descriptor Path */
    char                    descUuid[BT_MAX_UUID_STR_LEN];                       /* 128-bit service UUID */
    unsigned short          descFlags;                                           /* Descriptor Flags - bit field values */
    char                    propertyValue[BT_MAX_GATT_OP_DATA_LEN];              /* value of the descriptor */
    void*                   parentChar;                                          /* ptr to parent Characteristic  */
} stBTRCoreLeGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTRCoreLeGattChar {
    char                    charPath[BTRCORE_MAX_STR_LEN];                       /* Characteristic Path */
    char                    charUuid[BT_MAX_UUID_STR_LEN];                       /* 128-bit service UUID */
    stBTRCoreLeGattDesc     atBTRGattDesc[BTR_MAX_GATT_DESC];                    /* Max of 8 Gatt Descriptor array */
    unsigned short          ui16NumberOfGattDesc;                                /* Number of Gatt Service ID */
    unsigned short          charFlags;                                           /* Characteristic Flags - bit field values */
    char                    value[BT_MAX_GATT_OP_DATA_LEN];                      /* value of the characteristic */
    void*                   parentService;                                       /* ptr to parent Service */
} stBTRCoreLeGattChar;

/* GattService Path and Properties */
typedef struct _stBTRCoreLeGattService {
    BOOLEAN                 serviceType;                                         /* Primary(True) or secondary(False) gatt service*/
    char                    servicePath[BTRCORE_MAX_STR_LEN];                    /* Service Path */
    char                    serviceUuid[BT_MAX_UUID_STR_LEN];                    /* 128-bit service UUID */
    stBTRCoreLeGattChar     astBTRGattChar[BTR_MAX_GATT_CHAR];                   /* Max of 6 Gatt Charactristic array */
    unsigned short          ui16NumberOfGattChar;                                /* Number of Gatt Charactristics */
    void*                   parentProfile;                                       /* ptr to parent Device Profile */
} stBTRCoreLeGattService;

/* GattProfile coresponds to a Device's Gatt Services */
typedef struct _stBTRCoreLeGattProfile {
    tBTRCoreDevId           deviceID;                                           /* TODO To be generated from a common btrCore if */
    char                    devicePath[BTRCORE_MAX_STR_LEN];                    /* Object Path */
    char                    i8LeGattOpReady;
    stBTRCoreLeGattService  astBTRGattService[BTR_MAX_GATT_SERVICE];            /* Max of 4 Gatt Service array */
    unsigned short          ui16NumberOfGattService;                            /* Number of Gatt Service ID */
} stBTRCoreLeGattProfile;

typedef struct _stBTRCoreLeUUID {
    unsigned short  flags;
    char            uuid[BT_MAX_UUID_STR_LEN];
} stBTRCoreLeUUID;

typedef struct _stBTRCoreLeUUIDList {
    unsigned short   numberOfUUID;
    stBTRCoreLeUUID  uuidList[BTR_MAX_NUMBER_OF_UUID];
} stBTRCoreLeUUIDList;


typedef struct _stBTRCoreLeHdl {

    void*                           btIfceHdl;

    stBTRCoreLeGattProfile          astBTRGattProfile[BTR_MAX_GATT_PROFILE];
    unsigned short                  ui16NumberOfGattProfile;

    fPtr_BTRCore_LeStatusUpdateCb   fpcBTRCoreLeStatusUpdate;
    void*                           pvBtLeStatusUserData;

    stBTRCoreLeCustomAdv            stCustomAdv;
    /* Local gatt services */
    stBTRCoreLeGattService          stBTRLeGattService[BTR_MAX_GATT_SERVICE];
    unsigned short                  ui16NumOfLocalGattServices;

} stBTRCoreLeHdl;


/* Static Function Prototypes */
static tBTRCoreDevId btrCore_LE_BTGenerateUniqueDeviceID (const char* apcDeviceAddress);

static stBTRCoreLeGattProfile* btrCore_LE_FindGattProfile (stBTRCoreLeHdl *pstBTRCoreLeHdl, tBTRCoreDevId aBtrDeviceID);
static stBTRCoreLeGattService* btrCore_LE_FindGattService (stBTRCoreLeGattProfile  *pstGattProfile, const char *pService);
static stBTRCoreLeGattChar*    btrCore_LE_FindGattCharacteristic (stBTRCoreLeGattService *pstService, const char *pChar);
static stBTRCoreLeGattDesc*    btrCore_LE_FindGattDescriptor (stBTRCoreLeGattChar *pstChar, const char *desc);

static BOOLEAN  btrCore_LE_isServiceSupported (char* apUUID);

static enBTRCoreRet  btrCore_LE_GetGattCharacteristicUUIDList (stBTRCoreLeGattService *pstService, void* uuidList);
static enBTRCoreRet  btrCore_LE_GetGattDescriptorUUIDList (stBTRCoreLeGattChar *pstChar, void* uuidList);
static enBTRCoreRet  btrCore_LE_GetDataPath (stBTRCoreLeHdl* pstBTRCoreLeHdl, tBTRCoreDevId  atBTRCoreDevId,
                                             const char* apBtLeUuid, char* rpBtLePath, enBTOpIfceType* renBTOpIfceType);
static unsigned short  btrCore_LE_GetAllowedGattFlagValues (char (*flags)[BT_MAX_UUID_STR_LEN], enBTOpIfceType aenBtOpIfceType);
static enBTRCoreRet btrCore_LE_GetGattInfo(tBTRCoreLeHdl hBTRCoreLe, char* aUUID, enBTRCoreLEGattProp aGattProp, void *aValue);

static enBTRCoreRet btrCore_LE_UpdateLocalGattInfoCb(enBTOpIfceType aenBtOpIfceType, enBTLeGattOp aenBtLeGattOp, const char* aBtrAddr, const char* apBtGattPath, char* apValue, void* apUserData);
static enBTRCoreRet btrCore_LE_LocalGattServerInfoCb( const char* apBtAdvPath, const char* apcBtDevAddr, stBTLeGattService **apstBTRCoreLeGattService, int *aNumOfGattServices, void* apUserData);

/* Callbacks */
static int btrCore_LE_GattInfoCb (enBTOpIfceType enBtOpIfceType, enBTLeGattOp aenGattOp, const char* apBtGattPath, const char* aBtdevAddr, enBTDeviceState aenBTDeviceState, void* apCbInfo, void* apUserData );
static int btrCore_LE_AdvInfoCb(const char* apBtAdvPath, stBTLeCustomAdv** appstBtLeCustomAdv, void* apUserData);


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
    unsigned short          ui16LoopIdx = 0;
    stBTRCoreLeGattProfile* pstProfile  = NULL;

    if (pstBTRCoreLeHdl) {
        for (ui16LoopIdx = 0; ui16LoopIdx < pstBTRCoreLeHdl->ui16NumberOfGattProfile; ui16LoopIdx++) {
            if (pstBTRCoreLeHdl->astBTRGattProfile[ui16LoopIdx].deviceID == aBtrDeviceID) {
                pstProfile = &pstBTRCoreLeHdl->astBTRGattProfile[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Profile for Device %llu Found.\n", aBtrDeviceID);
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
    unsigned short          ui16LoopIdx = 0;
    stBTRCoreLeGattService* pstService  = NULL;

    if (pstProfile) {
        for (ui16LoopIdx = 0; ui16LoopIdx < pstProfile->ui16NumberOfGattService; ui16LoopIdx++) {
            if (!strcmp(pstProfile->astBTRGattService[ui16LoopIdx].servicePath, pService)) {
                pstService = &pstProfile->astBTRGattService[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Service %s Found.\n", pService);
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
    unsigned short          ui16LoopIdx = 0;
    stBTRCoreLeGattChar*    pstChar     = NULL;

    if (pstService) {
        for (ui16LoopIdx = 0; ui16LoopIdx < pstService->ui16NumberOfGattChar; ui16LoopIdx++) {
            if (!strcmp(pstService->astBTRGattChar[ui16LoopIdx].charPath, pChar)) {
                pstChar = &pstService->astBTRGattChar[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Char %s Found.\n", pChar);
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
    unsigned short          ui16LoopIdx = 0;
    stBTRCoreLeGattDesc*    pstDesc     = NULL;

    if (pstChar) {
        for (ui16LoopIdx = 0; ui16LoopIdx < pstChar->ui16NumberOfGattDesc; ui16LoopIdx++) {
            if (!strcmp(pstChar->atBTRGattDesc[ui16LoopIdx].descPath, pDesc)) {
                pstDesc = &pstChar->atBTRGattDesc[ui16LoopIdx];
                BTRCORELOG_DEBUG ("Gatt Descriptor %s Found.\n", pDesc);
                break;
            }
        }
    }

    return pstDesc; 
}

// Could make this to a common layer as btrcore.c also uses similar api
static BOOLEAN
btrCore_LE_isServiceSupported (
    char*   apUUID
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
        !strcmp(lUUID, BTR_CORE_GATT_TILE_2) ||
        !strcmp(lUUID, BTR_CORE_GATT_TILE_3) ||
        !strcmp(lUUID, BTR_CORE_GEN_ATRIB)) {
        isSupported = TRUE;
    } /* - Add further supported Services */

    return isSupported;
}

static enBTRCoreRet
btrCore_LE_GetGattCharacteristicUUIDList (
    stBTRCoreLeGattService* pstService,
    void*                   uuidList
) {
    stBTRCoreLeUUIDList* lstBTRCoreLeUUIDList = (stBTRCoreLeUUIDList*)uuidList;
    unsigned short       ui16LoopIdx = 0;

    if (!pstService || !uuidList) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    for (ui16LoopIdx = 0; ui16LoopIdx < pstService->ui16NumberOfGattChar; ui16LoopIdx++) {
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
    unsigned short       ui16LoopIdx = 0;

    if (!pstChar || !uuidList) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    for (ui16LoopIdx = 0; ui16LoopIdx < pstChar->ui16NumberOfGattDesc; ui16LoopIdx++) {
        strncpy(lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].uuid, pstChar->atBTRGattDesc[ui16LoopIdx].descUuid, BT_MAX_UUID_STR_LEN-1);
        lstBTRCoreLeUUIDList->uuidList[ui16LoopIdx].flags = pstChar->atBTRGattDesc[ui16LoopIdx].descFlags;
    }

    lstBTRCoreLeUUIDList->numberOfUUID = ui16LoopIdx;


    return enBTRCoreSuccess;
}


static enBTRCoreRet
btrCore_LE_GetDataPath (
    stBTRCoreLeHdl*     pstBTRCoreLeHdl,
    tBTRCoreDevId       atBTRCoreDevId,
    const char*         apBtLeUuid,
    char*               rpBtLePath,
    enBTOpIfceType*     renBTOpIfceType
) {
    unsigned short          ui16PLoopindex = 0;
    char*                   retLeDataPath  = NULL;
    stBTRCoreLeGattProfile* pProfile       = NULL;

    if (!pstBTRCoreLeHdl || !atBTRCoreDevId || !apBtLeUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    if (!pstBTRCoreLeHdl->ui16NumberOfGattProfile) {
        BTRCORELOG_ERROR ("No Gatt Profile Exists!!!!\n");
        return enBTRCoreFailure;
    }

    for (ui16PLoopindex = 0; ui16PLoopindex < pstBTRCoreLeHdl->ui16NumberOfGattProfile; ui16PLoopindex++) {
        pProfile = &pstBTRCoreLeHdl->astBTRGattProfile[ui16PLoopindex];

        if (atBTRCoreDevId == pProfile->deviceID) {
            unsigned short          ui16SLoopindex  = 0;
            stBTRCoreLeGattService *pService        = NULL;

            BTRCORELOG_DEBUG ("Profile Matched for Device %llu \n", atBTRCoreDevId);

            if (pProfile->ui16NumberOfGattService == 0) {
                BTRCORELOG_ERROR ("No Gatt Service Exists!!!\n");
                break; // profile loop
            }

            for (ui16SLoopindex = 0; ui16SLoopindex < pProfile->ui16NumberOfGattService; ui16SLoopindex++) {
                pService  = &pProfile->astBTRGattService[ui16SLoopindex];

                if (strstr(pService->serviceUuid, apBtLeUuid)) {
                    retLeDataPath = pService->servicePath;
                    *renBTOpIfceType = enBTGattService;
                    BTRCORELOG_DEBUG ("UUID matched Service : %s.\n", pService->servicePath);
                    break; // service loop
                }
            }

            if (ui16SLoopindex != pProfile->ui16NumberOfGattService) {
                break; // profile loop
            }

            for (ui16SLoopindex = 0; ui16SLoopindex < pProfile->ui16NumberOfGattService; ui16SLoopindex++) {
                unsigned short       ui16CLoopindex = 0;
                stBTRCoreLeGattChar* pChar          = NULL;

                pService  = &pProfile->astBTRGattService[ui16SLoopindex];

                if (pService->ui16NumberOfGattChar == 0) {
                    continue;  /* Service has no Char to loop through */
                }

                for (ui16CLoopindex = 0; ui16CLoopindex < pService->ui16NumberOfGattChar; ui16CLoopindex++) {
                    pChar = &pService->astBTRGattChar[ui16CLoopindex];

                    if (!strcmp(pChar->charUuid, apBtLeUuid)) {
                        retLeDataPath = pChar->charPath;
                        *renBTOpIfceType = enBTGattCharacteristic;
                        BTRCORELOG_DEBUG ("UUID matched Characteristic : %s.\n", pChar->charPath);
                        break; // char loop
                    }
                    else {
                        unsigned short       ui16DLoopindex = 0;
                        stBTRCoreLeGattDesc* pDesc          = NULL;

                        if (pChar->ui16NumberOfGattDesc == 0) {
                            continue;  /* Char has no desc to loop through */
                        }

                        for (ui16DLoopindex = 0; ui16DLoopindex < pChar->ui16NumberOfGattDesc; ui16DLoopindex++) {
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

    if (ui16PLoopindex == pstBTRCoreLeHdl->ui16NumberOfGattProfile) {
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
    unsigned char  u8idx    = 0;
    unsigned char  maxFlags = 0;

    if (!flags || !flags[0]) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return 0;
    }

    if (aenBtOpIfceType == enBTGattCharacteristic) {
        maxFlags = BTR_MAX_GATT_CHAR_FLAGS;
    }
    else if (aenBtOpIfceType == enBTGattDescriptor) {
        maxFlags = BTR_MAX_GATT_DESC_FLAGS;
    }

    for (u8idx = 0; u8idx < maxFlags && flags[u8idx]; u8idx++) {
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

static enBTRCoreRet
btrCore_LE_GetGattInfo (
    tBTRCoreLeHdl           hBTRCoreLe,
    char*                   aUUID,
    enBTRCoreLEGattProp     aGattProp,
    void*                   aValue
) {
    stBTRCoreLeHdl*         pstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreSuccess;
    int                     lNumGattServices = pstlhBTRCoreLe->ui16NumOfLocalGattServices;
    stBTRCoreLeGattDesc*    lpstBTRCoreLeGattDesc = NULL;
    stBTRCoreLeGattChar*    lpstBTRCoreLeGattChar = NULL;
    stBTRCoreLeGattService* lCurrGattService = NULL, *lpstBTRCoreLeGattService = NULL;
    BOOLEAN                 lbFoundUUID = FALSE;

    lpstBTRCoreLeGattService = pstlhBTRCoreLe->stBTRLeGattService;
    for (int ServiceIndex = 0; (ServiceIndex < lNumGattServices) && (FALSE == lbFoundUUID); ServiceIndex++) {
        lCurrGattService = &lpstBTRCoreLeGattService[ServiceIndex];
        if (!strncmp(aUUID, lCurrGattService->serviceUuid, BT_MAX_STR_LEN)) {
            lbFoundUUID = TRUE;
            break;
        }

        for (int index = 0; (index < lCurrGattService->ui16NumberOfGattChar) && (FALSE == lbFoundUUID); index++) {
            lpstBTRCoreLeGattChar = &lCurrGattService->astBTRGattChar[index];
            if (!strncmp(aUUID, lpstBTRCoreLeGattChar->charUuid, BT_MAX_STR_LEN)) {
                lbFoundUUID = TRUE;
                break;
            }

            for (int descIndex = 0; (descIndex < lpstBTRCoreLeGattChar->ui16NumberOfGattDesc); descIndex++) {
                lpstBTRCoreLeGattDesc = &lpstBTRCoreLeGattChar->atBTRGattDesc[descIndex];
                if (!strncmp(aUUID, lpstBTRCoreLeGattDesc->descUuid, BT_MAX_STR_LEN)) {
                    lbFoundUUID = TRUE;
                    break;
                }
            }
        }
    }

    if (TRUE == lbFoundUUID) {
        switch (aGattProp) {
        case enBTRCoreLEGPropService: {
            stBTRCoreLeGattService **lpstGattService = (stBTRCoreLeGattService**)aValue;
            *lpstGattService = lCurrGattService;
            BTRCORELOG_INFO("enBTRCoreLEGPropService UUID is %s\n", (*lpstGattService)->serviceUuid);
        }
            break;
        case enBTRCoreLEGPropChar: {
            stBTRCoreLeGattChar **lpstGattChar = (stBTRCoreLeGattChar**)aValue;
            *lpstGattChar = lpstBTRCoreLeGattChar;
            BTRCORELOG_INFO("enBTRCoreLEGPropChar UUID is %s\n", (*lpstGattChar)->charUuid);
        }
            break;
        case enBTRCoreLEGPropDesc: {
            stBTRCoreLeGattDesc **lpstGattDesc = (stBTRCoreLeGattDesc**)aValue;
            *lpstGattDesc = lpstBTRCoreLeGattDesc;
            BTRCORELOG_INFO("enBTRCoreLEGPropDesc UUID is %s\n", (*lpstGattDesc)->descUuid);
        }
            break;
        case enBTRCoreLEGPropValue: {
            char **lCharPropValue = (char**)aValue;
            *lCharPropValue = lpstBTRCoreLeGattChar->value;
            BTRCORELOG_INFO("Characteristic value for UUID is %s\n", *lCharPropValue);
        }
            break;
        case enBTRCoreLEGPropDescValue: {
            char **lDescPropValue = (char**)aValue;
            *lDescPropValue = lpstBTRCoreLeGattDesc->propertyValue;
            BTRCORELOG_INFO("Descriptor value for UUID is %s\n", *lDescPropValue);
        }
            break;
        default: {
            lenBTRCoreRet = enBTRCoreFailure;
        }
            break;
        }
    }
    else {
        BTRCORELOG_ERROR("UUID %s could not be found\n", aUUID);
        lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


static enBTRCoreRet
btrCore_LE_LocalGattServerInfoCb(
    const char*         apBtAdvPath,
    const char*         apcBtDevAddr,
    stBTLeGattService** appstBTRCoreLeGattService,
    int*                aNumOfGattServices,
    void*               apUserData
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTLeGattService*      apstBTRCoreLeGattService = *appstBTRCoreLeGattService;
    stBTRCoreLeGattService* lpstBTRLeGattService = lpstlhBTRCoreLe->stBTRLeGattService;

    for (unsigned short lui16serviceIndex = 0; lui16serviceIndex < lpstlhBTRCoreLe->ui16NumOfLocalGattServices; lui16serviceIndex++) {
        apstBTRCoreLeGattService->serviceType = lpstBTRLeGattService->serviceType;
        apstBTRCoreLeGattService->ui16NumberOfGattChar = lpstBTRLeGattService->ui16NumberOfGattChar;
        strncpy(apstBTRCoreLeGattService->servicePath, lpstBTRLeGattService->servicePath, BT_MAX_STR_LEN - 1);
        strncpy(apstBTRCoreLeGattService->serviceUuid, lpstBTRLeGattService->serviceUuid, BT_MAX_UUID_STR_LEN - 1);

        stBTLeGattChar*      apstBtLeGattChar = apstBTRCoreLeGattService->astBTRGattChar;
        stBTRCoreLeGattChar* lpstBTRLeGattChar = lpstBTRLeGattService->astBTRGattChar;
        for (unsigned short lui16charIndex = 0; lui16charIndex < lpstBTRLeGattService->ui16NumberOfGattChar; lui16charIndex++) {
            apstBtLeGattChar->charFlags = lpstBTRLeGattChar->charFlags;
            apstBtLeGattChar->ui16NumberOfGattDesc = lpstBTRLeGattChar->ui16NumberOfGattDesc;
            strncpy(apstBtLeGattChar->charPath, lpstBTRLeGattChar->charPath, BT_MAX_STR_LEN - 1);
            strncpy(apstBtLeGattChar->charUuid, lpstBTRLeGattChar->charUuid, BT_MAX_UUID_STR_LEN - 1);
            strncpy(apstBtLeGattChar->value, lpstBTRLeGattChar->value, BT_MAX_GATT_OP_DATA_LEN - 1);

            stBTLeGattDesc*      apstBtLeGattDesc = apstBtLeGattChar->atBTRGattDesc;
            stBTRCoreLeGattDesc* lpstBTRLeGattDesc = lpstBTRLeGattChar->atBTRGattDesc;
            for (unsigned short lui16descIndex = 0; lui16descIndex < lpstBTRLeGattChar->ui16NumberOfGattDesc; lui16descIndex++) {
                apstBtLeGattDesc->descFlags = lpstBTRLeGattDesc->descFlags;
                strncpy(apstBtLeGattDesc->descPath, lpstBTRLeGattDesc->descPath, BT_MAX_STR_LEN - 1);
                strncpy(apstBtLeGattDesc->descUuid, lpstBTRLeGattDesc->descUuid, BT_MAX_UUID_STR_LEN - 1);
                strncpy(apstBtLeGattDesc->propertyValue, lpstBTRLeGattDesc->propertyValue, BT_MAX_GATT_OP_DATA_LEN - 1);

                apstBtLeGattDesc++;
                lpstBTRLeGattDesc++;
            }

            apstBtLeGattChar++;
            lpstBTRLeGattChar++;
        }

        apstBTRCoreLeGattService++;
        lpstBTRLeGattService++;
    }

    *aNumOfGattServices = lpstlhBTRCoreLe->ui16NumOfLocalGattServices;

    return enBTRCoreSuccess;
}

static enBTRCoreRet
btrCore_LE_UpdateLocalGattInfoCb (
    enBTOpIfceType      aenBtOpIfceType,
    enBTLeGattOp        aenBtLeGattOp,
    const char*         aBtrAddr,
    const char*         apBtGattPath,
    char*               apValue,
    void*               apUserData
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreLeGattInfo     lstBtrLeInfo;
    stBTRCoreLeGattDesc*    lpstBTRCoreLeGattDesc = NULL;
    stBTRCoreLeGattChar*    lpstBTRCoreLeGattChar = NULL;
    stBTRCoreLeGattService* lCurrGattService = NULL, *lpstBTRCoreLeGattService = NULL;
    int                     lNumGattServices = 0;
    BOOLEAN                 lbFoundGattPath = FALSE;
    enBTRCoreRet            lRetValue = enBTRCoreSuccess;

    if (!apBtGattPath || !apUserData) {
        BTRCORELOG_ERROR("Invalid arguments!!!\n");
        return enBTRCoreInvalidArg;
    }

    lpstBTRCoreLeGattService = lpstlhBTRCoreLe->stBTRLeGattService;
    lNumGattServices = lpstlhBTRCoreLe->ui16NumOfLocalGattServices;

    /* Find the data structure pointer corresponding to the path */
    for (int ServiceIndex = 0; ServiceIndex < lNumGattServices; ServiceIndex++) {

        if (FALSE == lbFoundGattPath) {
            lCurrGattService = &lpstBTRCoreLeGattService[ServiceIndex];

            for (int index = 0; index < (lCurrGattService->ui16NumberOfGattChar) && (FALSE == lbFoundGattPath); index++) {
                lpstBTRCoreLeGattChar = &lCurrGattService->astBTRGattChar[index];

                if (!strncmp(apBtGattPath, lpstBTRCoreLeGattChar->charPath, BT_MAX_STR_LEN)) {
                    lbFoundGattPath = TRUE;
                    break;
                }

                for (int descIndex = 0; descIndex < lpstBTRCoreLeGattChar->ui16NumberOfGattDesc; descIndex++) {
                    lpstBTRCoreLeGattDesc = &lpstBTRCoreLeGattChar->atBTRGattDesc[descIndex];

                    if (!strncmp(apBtGattPath, lpstBTRCoreLeGattDesc->descPath, BT_MAX_STR_LEN)) {
                        lbFoundGattPath = TRUE;
                        break;
                    }
                }
            }
        }
    }

    if (TRUE == lbFoundGattPath) {
        switch (aenBtLeGattOp) {
        case enBTLeGattOpReadValue: {
            switch (aenBtOpIfceType) {
            case enBTGattCharacteristic: {
                lstBtrLeInfo.pui8Uuid = lpstBTRCoreLeGattChar->charUuid;
                lstBtrLeInfo.enLeProp = enBTRCoreLEGPropChar;
                /* Update stBTRCoreLeGattInfo and cb to upper layer */
                lstBtrLeInfo.enLeOper = enBTRCoreLEGOpReadValue;
                lstBtrLeInfo.pui8Value = apValue;
                BTRCORELOG_TRACE("Btr address of device reading value is %s\n", aBtrAddr);
                lpstlhBTRCoreLe->fpcBTRCoreLeStatusUpdate(&lstBtrLeInfo, aBtrAddr, lpstlhBTRCoreLe->pvBtLeStatusUserData);
                BTRCORELOG_TRACE("Value of UUID %s\n", apValue);
                strncpy(lpstBTRCoreLeGattChar->value, apValue, BT_MAX_GATT_OP_DATA_LEN - 1);
            }
                break;
            case enBTGattDescriptor: {
                lstBtrLeInfo.pui8Uuid = lpstBTRCoreLeGattDesc->descUuid;
                lstBtrLeInfo.enLeProp = enBTRCoreLEGPropDesc;
                /* Get value from upper layer */
                lstBtrLeInfo.enLeOper = enBTRCoreLEGOpReadValue;
                lstBtrLeInfo.pui8Value = apValue;
                BTRCORELOG_TRACE("Btr address of device reading value is %s\n", aBtrAddr);
                lpstlhBTRCoreLe->fpcBTRCoreLeStatusUpdate(&lstBtrLeInfo, aBtrAddr, lpstlhBTRCoreLe->pvBtLeStatusUserData);
                BTRCORELOG_TRACE("Value of UUID %s\n", apValue);
                strncpy(lpstBTRCoreLeGattDesc->propertyValue, apValue, BT_MAX_GATT_OP_DATA_LEN - 1);
            }
                break;
            default:
                break;
            }
        }
            break;
        case enBTLeGattOpWriteValue: {
            switch (aenBtOpIfceType) {
            case enBTGattCharacteristic: {
                strncpy(lpstBTRCoreLeGattChar->value, apValue, BT_MAX_GATT_OP_DATA_LEN - 1);
                BTRCORELOG_TRACE("Value is %s\n", lpstBTRCoreLeGattChar->value);
                lstBtrLeInfo.pui8Uuid = lpstBTRCoreLeGattChar->charUuid;
                lstBtrLeInfo.pui8Value = lpstBTRCoreLeGattChar->value;
                lstBtrLeInfo.enLeProp = enBTRCoreLEGPropChar;
            }
                break;
            case enBTGattDescriptor: {
                strncpy(lpstBTRCoreLeGattDesc->propertyValue, apValue, BT_MAX_GATT_OP_DATA_LEN - 1);
                lstBtrLeInfo.pui8Uuid = lpstBTRCoreLeGattDesc->descUuid;
                lstBtrLeInfo.pui8Value = lpstBTRCoreLeGattDesc->propertyValue;
                lstBtrLeInfo.enLeProp = enBTRCoreLEGPropDesc;
            }
                break;
            default:
                break;
            }

            /* Update stBTRCoreLeGattInfo and cb to upper layer */
            lstBtrLeInfo.enLeOper = enBTRCoreLEGOpWriteValue;
            BTRCORELOG_TRACE("Btr address of device writing value is %s\n", aBtrAddr);
            lpstlhBTRCoreLe->fpcBTRCoreLeStatusUpdate(&lstBtrLeInfo, aBtrAddr, lpstlhBTRCoreLe->pvBtLeStatusUserData);
        }
            break;
        default:
            break;
        }
    }
    else {
        lRetValue = enBTRCoreFailure;
        BTRCORELOG_ERROR("UUID could not be found\n");
    }

    return lRetValue;
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

    BTRCORELOG_WARN ("BTRCore_LE_Init\n");

    pstlhBTRCoreLe = (stBTRCoreLeHdl*)g_malloc0(sizeof(stBTRCoreLeHdl));

    if (!pstlhBTRCoreLe) {
        BTRCORELOG_ERROR ("Memory Allocation Failed\n");
        return enBTRCoreInitFailure;
    }

    pstlhBTRCoreLe->btIfceHdl = apBtConn;

    if (BtrCore_BTRegisterLEGattInfoCb (apBtConn,
                                        apBtAdapter,
                                        &btrCore_LE_GattInfoCb,
                                        pstlhBTRCoreLe)) {
        lenBTRCoreRet = enBTRCoreFailure;
    }

    if (BtrCore_BTRegisterLEAdvInfoCb(apBtConn,
                                      apBtAdapter,
                                      &btrCore_LE_AdvInfoCb,
                                      pstlhBTRCoreLe)) {
        lenBTRCoreRet = enBTRCoreFailure;
    }

    if (lenBTRCoreRet != enBTRCoreSuccess) {
        g_free(pstlhBTRCoreLe);
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
    stBTRCoreLeHdl*    pstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;;
    enBTRCoreRet       lenBTRCoreRet  = enBTRCoreSuccess;

    if (!hBTRCoreLe || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }

    if (pstlhBTRCoreLe->btIfceHdl != apBtConn) {
        BTRCORELOG_WARN ("Incorrect Argument - btIfceHdl : Continue\n");
    }

    g_free(hBTRCoreLe);

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_LE_StartAdvertisement (
    tBTRCoreLeHdl       hBTRCoreLe,
    void*               apBtConn,
    const char*         apBtAdapter
) {
    enBTRCoreRet       lenBTRCoreRet = enBTRCoreSuccess;

    if (!hBTRCoreLe || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }

    /* Invoke method calls for RegisterAdvertisement and RegisterApplication from here for now */
    if (BtrCore_BTRegisterLeAdvertisement(apBtConn, apBtAdapter)) {
        BTRCORELOG_ERROR("Unable to register advertisement\n");
        lenBTRCoreRet = enBTRCoreFailure;
    }

    if (BtrCore_BTRegisterLeGatt(apBtConn, apBtAdapter)) {
        BTRCORELOG_ERROR("Unable to register application (gatt service)\n");
        lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_LE_StopAdvertisement (
    tBTRCoreLeHdl  hBTRCoreLe,
    void*          apBtConn,
    const char*    apBtAdapter
) {
    stBTRCoreLeHdl*    pstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet       lenBTRCoreRet = enBTRCoreSuccess;

    if (!hBTRCoreLe || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }

    if (pstlhBTRCoreLe->btIfceHdl != apBtConn) {
        BTRCORELOG_WARN("Incorrect Argument - btIfceHdl : Continue\n");
    }

    if (BtrCore_BTUnRegisterLeGatt (apBtConn,
                                    apBtAdapter)) {
        lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}

enBTRCoreRet
BTRCore_LE_SetAdvertisementType (
    tBTRCoreLeHdl   hBTRCoreLe,
    char*           aAdvtType
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    stBTRCoreLeCustomAdv*   lpstBTRCoreLeCustAdv = NULL;

    if (NULL != lpstlhBTRCoreLe) {
        lpstBTRCoreLeCustAdv = &lpstlhBTRCoreLe->stCustomAdv;

        if (!(strncmp(aAdvtType, "peripheral", BTRCORE_MAX_STR_LEN)) ||
            !(strncmp(aAdvtType, "broadcast", BTRCORE_MAX_STR_LEN))) {
            strncpy(lpstBTRCoreLeCustAdv->AdvertisementType, aAdvtType, BTRCORE_MAX_STR_LEN);
            BTRCORELOG_INFO("Adv type : %s\n", lpstBTRCoreLeCustAdv->AdvertisementType);
            lenBTRCoreRet = enBTRCoreSuccess;
        }
        else {
            lenBTRCoreRet = enBTRCoreInvalidArg;
        }
    }

    return lenBTRCoreRet;
}

enBTRCoreRet
BTRCore_LE_SetServiceUUIDs (
    tBTRCoreLeHdl   hBTRCoreLe,
    char*           aUUID
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    stBTRCoreLeCustomAdv*   lpstBTRCoreLeCustAdv = NULL;

    if (NULL != lpstlhBTRCoreLe) {
        lpstBTRCoreLeCustAdv = &lpstlhBTRCoreLe->stCustomAdv;

        if (BTR_MAX_GATT_SERVICE > lpstBTRCoreLeCustAdv->numServiceUUID) {
            strncpy(lpstBTRCoreLeCustAdv->ServiceUUID[lpstBTRCoreLeCustAdv->numServiceUUID], aUUID, BTRCORE_MAX_STR_LEN);
            BTRCORELOG_INFO("Service UUID : %s\n", lpstBTRCoreLeCustAdv->ServiceUUID[lpstBTRCoreLeCustAdv->numServiceUUID]);
            lpstBTRCoreLeCustAdv->numServiceUUID += 1;
            lenBTRCoreRet = enBTRCoreSuccess;
        }
        else {
            lenBTRCoreRet = enBTRCoreInvalidArg;
        }
    }

    return lenBTRCoreRet;
}

enBTRCoreRet
BTRCore_LE_SetManufacturerData (
    tBTRCoreLeHdl   hBTRCoreLe,
    unsigned short  aManfId,
    unsigned char*  aDeviceDetails,
    int             aLenManfData
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    stBTRCoreLeCustomAdv*   lpstBTRCoreLeCustAdv = NULL;
    stBTRCoreLeManfData*    lpstManfData = NULL;

    if (NULL != lpstlhBTRCoreLe) {
        lpstBTRCoreLeCustAdv = &lpstlhBTRCoreLe->stCustomAdv;

        lpstManfData = &lpstBTRCoreLeCustAdv->ManfData;
        lpstManfData->ManfID = aManfId;
        BTRCORELOG_INFO("Manf ID : %x\n", lpstBTRCoreLeCustAdv->ManfData.ManfID);
        lpstManfData->lenManfData = aLenManfData;

        for (int index = 0; index < aLenManfData; index++) {
            lpstManfData->data[index] = aDeviceDetails[index];
            BTRCORELOG_INFO("Manf data %x\n", lpstBTRCoreLeCustAdv->ManfData.data[index]);
        }
        lenBTRCoreRet = enBTRCoreSuccess;
    }

    return lenBTRCoreRet;
}

enBTRCoreRet
BTRCore_LE_SetEnableTxPower(
    tBTRCoreLeHdl   hBTRCoreLe,
    BOOLEAN         aTxPower
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    stBTRCoreLeCustomAdv*   lpstBTRCoreLeCustAdv = NULL;

    if (NULL != lpstlhBTRCoreLe) {
        lpstBTRCoreLeCustAdv = &lpstlhBTRCoreLe->stCustomAdv;

        lpstBTRCoreLeCustAdv->bTxPower = aTxPower;
        BTRCORELOG_INFO("TX power is: %d\n", lpstBTRCoreLeCustAdv->bTxPower);
        lenBTRCoreRet = enBTRCoreSuccess;
    }

    return lenBTRCoreRet;
}

int*
BTRCore_LE_AddGattServiceInfo (
    tBTRCoreLeHdl   hBTRCoreLe,
    const char*     apBtAdapter,
    char*           aBtrDevAddr,
    char*           aUUID,
    BOOLEAN         aServiceType,
    int*            aNumGattServices
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreLeGattService* lpstBTRGattService = NULL;
    int*                    pService = NULL;

    /* Check whether service uuid exists */
    btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aUUID, enBTRCoreLEGPropService, (void*)&pService);
    /* If service does not exist */
    if (NULL == pService) {
        if (BTR_MAX_GATT_SERVICE > lpstlhBTRCoreLe->ui16NumOfLocalGattServices) {
            int                 lIndex = lpstlhBTRCoreLe->ui16NumOfLocalGattServices;
            lpstBTRGattService = &lpstlhBTRCoreLe->stBTRLeGattService[lIndex];
            char lpBtLeGattSrvEpPath[BT_MAX_DEV_PATH_LEN] = "\0";
            char lCurAdapterAddress[BT_MAX_DEV_PATH_LEN] = "\0";

            strncpy(lCurAdapterAddress, aBtrDevAddr, strlen(aBtrDevAddr));

            char *current_pos = strchr(lCurAdapterAddress, ':');
            while (current_pos){
                *current_pos = '_';
                current_pos = strchr(current_pos, ':');
            }

            memset(lpBtLeGattSrvEpPath, '\0', BT_MAX_DEV_PATH_LEN);
            strncpy(lpBtLeGattSrvEpPath, apBtAdapter, strlen(apBtAdapter));
            strncat(lpBtLeGattSrvEpPath, "/dev_", 5);
            strncat(lpBtLeGattSrvEpPath, lCurAdapterAddress, strlen(lCurAdapterAddress));

            /* Set gatt service UUID */
            strncpy(lpstBTRGattService->serviceUuid, aUUID, sizeof(lpstBTRGattService->serviceUuid));
            /* Set primary/secondary gatt service */
            lpstBTRGattService->serviceType = aServiceType;
            /* Set service path */
            snprintf(lpstBTRGattService->servicePath, BT_MAX_STR_LEN - 1, "%s/%s%02d", lpBtLeGattSrvEpPath, "service", lIndex);
            BTRCORELOG_INFO("Service path %s\n", lpstBTRGattService->servicePath);

            lpstlhBTRCoreLe->ui16NumOfLocalGattServices += 1;

            *aNumGattServices = lpstlhBTRCoreLe->ui16NumOfLocalGattServices;
        }
    }

    /* return service pointer */
    return (int*)lpstBTRGattService;
}


int*
BTRCore_LE_AddGattCharInfo (
    tBTRCoreLeHdl   hBTRCoreLe,                            /* Handle to CoreLe */
    const char*     apBtAdapter,
    char*           aBtrDevAddr,                           /* Bt address of advertising device */
    char*           aParentUUID,                           /* Service the characteristic belongs to */
    char*           aUUID,                                 /* UUID of characteristic */
    unsigned short  aCharFlags,                            /* Bit field to indicate usage of characteristic */
    char*           aValue                                 /* Value of the characteristic if applicable */
) {
    stBTRCoreLeHdl* lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreLeGattService* lpstBTRGattService = NULL;
    stBTRCoreLeGattChar* lpstBTRCoreLeGattChar = NULL;
    int* pChar = NULL;
    int* pParent = NULL;

    /* Check whether the char uuid exists */
    btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aUUID, enBTRCoreLEGPropChar, (void*)&pChar);
    if (NULL == pChar) {
        /* Check whether parent service uuid exists */
        btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aParentUUID, enBTRCoreLEGPropService, (void*)&pParent);
        if (NULL != pParent) {
            lpstBTRGattService = (stBTRCoreLeGattService*)pParent;

            if (BTR_MAX_GATT_CHAR > lpstBTRGattService->ui16NumberOfGattChar) {
                int lIndex = lpstBTRGattService->ui16NumberOfGattChar;
                lpstBTRCoreLeGattChar = &lpstBTRGattService->astBTRGattChar[lIndex];

                /* Set gatt char UUID */
                strncpy(lpstBTRCoreLeGattChar->charUuid, aUUID, sizeof(lpstBTRCoreLeGattChar->charUuid));
                /* Set parent service */
                lpstBTRCoreLeGattChar->parentService = pParent;
                /* Set char path */
                snprintf(lpstBTRCoreLeGattChar->charPath, BT_MAX_STR_LEN - 1, "%s/%s%04d", lpstBTRGattService->servicePath, "char", lIndex);
                /* Set char flags */
                lpstBTRCoreLeGattChar->charFlags = aCharFlags;
                /* Set the value of the characteristic after checking it is a characteristic to be read */
                if ((NULL != aValue) && (BTR_GATT_CHAR_FLAG_READ == (BTR_GATT_CHAR_FLAG_READ & aCharFlags))) {
                    strncpy(lpstBTRCoreLeGattChar->value, aValue, sizeof(lpstBTRCoreLeGattChar->value));
                }

                lpstBTRGattService->ui16NumberOfGattChar += 1;
            }
        }
    }

    return (int*)lpstBTRCoreLeGattChar;
}

int*
BTRCore_LE_AddGattDescInfo (
    tBTRCoreLeHdl   hBTRCoreLe,                            /* Handle to CoreLe */
    const char*     apBtAdapter,
    char*           aBtrDevAddr,                           /* Bt address of advertising device */
    char*           aParentUUID,                           /* Char the descriptor belongs to */
    char*           aUUID,                                 /* UUID of descriptor */
    unsigned short  aDescFlags,                            /* Bit field to indicate usage of descriptor */
    char*           aValue                                 /* Value of the descriptor if applicable */
) {
    stBTRCoreLeHdl* lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreLeGattChar *lpstBTRGattChar = NULL;
    stBTRCoreLeGattDesc* lpstBTRGattDesc = NULL;
    int *pDesc = NULL;
    int *pParent = NULL;

    /* Check whether the char uuid exists */
    btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aUUID, enBTRCoreLEGPropDesc, (void*)&pDesc);
    if (NULL == pDesc) {
        /* Check whether parent service uuid exists */
        btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aParentUUID, enBTRCoreLEGPropChar, (void*)&pParent);
        if (NULL != pParent) {
            lpstBTRGattChar = (stBTRCoreLeGattChar*)pParent;
            if (BTR_MAX_GATT_DESC > lpstBTRGattChar->ui16NumberOfGattDesc) {
                int lIndex = lpstBTRGattChar->ui16NumberOfGattDesc;
                lpstBTRGattDesc = &lpstBTRGattChar->atBTRGattDesc[lIndex];

                /* Set desc UUID */
                strncpy(lpstBTRGattDesc->descUuid, aUUID, sizeof(lpstBTRGattDesc->descUuid));
                /* Set desc parent char */
                lpstBTRGattDesc->parentChar = pParent;
                /* Set desc path */
                snprintf(lpstBTRGattDesc->descPath, BT_MAX_STR_LEN - 1, "%s/%s%03d", lpstBTRGattChar->charPath, "desc", lIndex);
                BTRCORELOG_INFO("Desc path %s\n", lpstBTRGattDesc->descPath);
                /* Set desc flags */
                lpstBTRGattDesc->descFlags = aDescFlags;
                /* Set value of desc if it can be read */
                if ((NULL != aValue) && (BTR_GATT_CHAR_FLAG_READ == (BTR_GATT_CHAR_FLAG_READ & aDescFlags))) {
                    strncpy(lpstBTRGattDesc->propertyValue, aValue, sizeof(lpstBTRGattDesc->propertyValue));
                }

                lpstBTRGattChar->ui16NumberOfGattDesc += 1;
            }
        }
    }
    /* return desc pointer */
    return (int*)lpstBTRGattDesc;
}

enBTRCoreRet
BTRCore_LE_SetPropertyValue (
    tBTRCoreLeHdl       hBTRCoreLe,
    char*               aUUID,
    char*               aValue,
    enBTRCoreLEGattProp aGattProp
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreLeGattChar*    lpstBTRGattChar = NULL;
    unBTOpIfceProp          lunBtOpAdapProp;
    //BOOLEAN notifying = FALSE;

    if (!hBTRCoreLe) {
        BTRCORELOG_ERROR("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!aValue) || (!aUUID)) {
        BTRCORELOG_ERROR("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    lunBtOpAdapProp.enBtGattCharProp = enBTGattCPropValue;
    //lunBtOpAdapProp.enBtGattCharProp = enBTGattCPropNotifying;
    /* Check whether service uuid exists */
    btrCore_LE_GetGattInfo(lpstlhBTRCoreLe, aUUID, aGattProp, (void*)&lpstBTRGattChar);
    BTRCORELOG_INFO("Value is %s - \n", aValue);
    if (NULL != lpstBTRGattChar) {
        if (BtrCore_BTSetProp(lpstlhBTRCoreLe->btIfceHdl, lpstBTRGattChar->charPath, enBTGattCharacteristic, lunBtOpAdapProp, (void*)aValue)) {
            BTRCORELOG_ERROR("Set Char Property Value - FAILED\n");
            return enBTRCoreFailure;
        }
    }
   

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_LE_GetGattProperty (
    tBTRCoreLeHdl        hBTRCoreLe,
    tBTRCoreDevId        atBTRCoreDevId,
    const char*          apcBtUuid,
    enBTRCoreLEGattProp  aenBTRCoreLEGattProp,
    void*                apvBtPropValue
) {
    stBTRCoreLeHdl* lpstlhBTRCoreLe  = (stBTRCoreLeHdl*)hBTRCoreLe;
    char            lpcBtLePath[BT_MAX_STR_LEN] = "\0";
    enBTOpIfceType  lenBTOpIfceType   = enBTUnknown;
    unBTOpIfceProp  lunBTOpIfceProp;

    if (!hBTRCoreLe || !atBTRCoreDevId || !apcBtUuid)  {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }
  
    if (btrCore_LE_GetDataPath(lpstlhBTRCoreLe, atBTRCoreDevId, apcBtUuid, lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!\n", apcBtUuid);
        return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath[0]) {
        BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!\n");
        return enBTRCoreFailure;
    }
     
    switch (aenBTRCoreLEGattProp) {

    case enBTRCoreLEGPropUUID:
        if (lenBTOpIfceType == enBTGattService) {
            stBTRCoreLeGattProfile* pProfile   = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
            stBTRCoreLeGattService* pService   = btrCore_LE_FindGattService(pProfile, lpcBtLePath);

            if (!pService) {
                BTRCORELOG_ERROR ("Failed to find pProfile|pService|pChar : %p|%p\n", pProfile, pService);
                return enBTRCoreFailure;
            }

            if (btrCore_LE_GetGattCharacteristicUUIDList(pService, apvBtPropValue) != enBTRCoreSuccess) {
                BTRCORELOG_ERROR ("Failed to get Char UUID List for Service %s\n", lpcBtLePath);
                return enBTRCoreFailure;
            }

            return enBTRCoreSuccess;  /* Is it Ok to have return here or return logically at function End? */
        }
        else if (lenBTOpIfceType == enBTGattCharacteristic) {
            char            servicePath[BT_MAX_STR_LEN]   = "\0";

            lunBTOpIfceProp.enBtGattCharProp   = enBTGattCPropService;
            if (BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, lpcBtLePath, lenBTOpIfceType, lunBTOpIfceProp, servicePath)) {
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
        }
        else if (lenBTOpIfceType == enBTGattDescriptor) {
            char                    servicePath[BT_MAX_STR_LEN]    = "\0";
            char                    charPath[BT_MAX_STR_LEN]       = "\0";
            stBTRCoreLeUUIDList*    lstBTRCoreLeUUIDList    = (stBTRCoreLeUUIDList*)apvBtPropValue;
            char retCPath = -1, retSPath = -1;

            lunBTOpIfceProp.enBtGattDescProp                = enBTGattDPropCharacteristic;
            retCPath = BtrCore_BTGetProp (lpstlhBTRCoreLe->btIfceHdl, lpcBtLePath, lenBTOpIfceType, lunBTOpIfceProp, charPath);

            lunBTOpIfceProp.enBtGattCharProp                = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp (lpstlhBTRCoreLe->btIfceHdl, charPath, lenBTOpIfceType, lunBTOpIfceProp, servicePath);

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
        }
        else if (lenBTOpIfceType == enBTGattDescriptor) {
            lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropValue;
        }
        break;

    case enBTRCoreLEGPropNotifying:
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropNotifying;
        break;

    case enBTRCoreLEGPropFlags:
        if (lenBTOpIfceType == enBTGattCharacteristic) {
            lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
        }
        else if (lenBTOpIfceType == enBTGattDescriptor) {
            lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropFlags;
        }
        break;

    case enBTRCoreLEGPropChar:
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
        break;

    case enBTRCoreLEGPropUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid enBTRCoreLEGattProp Options %d !!!\n", aenBTRCoreLEGattProp);
        break;

    }

    if (lenBTOpIfceType == enBTUnknown || BtrCore_BTGetProp (lpstlhBTRCoreLe->btIfceHdl,
                                                             lpcBtLePath,
                                                             lenBTOpIfceType,
                                                             lunBTOpIfceProp,
                                                             apvBtPropValue)) {
        BTRCORELOG_ERROR ("Failed to get Gatt %d Prop for UUID : %s!!!\n", aenBTRCoreLEGattProp, apcBtUuid);
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

/* Get the UUIDs implicitly based on the Op type ? instead of getting it from the higher layer */
enBTRCoreRet
BtrCore_LE_PerformGattOp (
    tBTRCoreLeHdl      hBTRCoreLe,
    tBTRCoreDevId      atBTRCoreDevId,
    const char*        apcBtUuid,
    enBTRCoreLEGattOp  aenBTRCoreLEGattOp,
    char*              apLeOpArg,
    char*              rpLeOpRes
) {
    stBTRCoreLeHdl* lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    char            lpcBtLePath[BT_MAX_STR_LEN] = "\0";
    char*           lpDevicePath      = "\0";
    enBTOpIfceType  lenBTOpIfceType   = enBTUnknown;
    enBTLeGattOp    lenBTLeGattOp     = enBTLeGattOpUnknown;

    if (!hBTRCoreLe || !atBTRCoreDevId || !apcBtUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }
  
    if (btrCore_LE_GetDataPath(lpstlhBTRCoreLe, atBTRCoreDevId, apcBtUuid, lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!\n", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath[0]) {
       BTRCORELOG_ERROR ("LE Path is NULL!!!\n");
       return enBTRCoreFailure;
    }

    if (lenBTOpIfceType == enBTGattService) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg | %s is a Service UUID...LE Service Ops are not available!!!\n", apcBtUuid);
        return enBTRCoreFailure;
    }
    else if ((lenBTOpIfceType == enBTGattCharacteristic) ||
             (lenBTOpIfceType == enBTGattDescriptor))  {
        stBTRCoreLeGattProfile* pProfile        = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, atBTRCoreDevId);
        lpDevicePath = pProfile->devicePath;
    } 

    // Validate UUID 
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
        BTRCORELOG_ERROR ("Invalid enBTRCoreLEGattOp Options %d !!!.\n", aenBTRCoreLEGattOp);
        break;
    }

    if (lenBTLeGattOp == enBTLeGattOpUnknown || BtrCore_BTPerformLeGattOp (lpstlhBTRCoreLe->btIfceHdl,
                                                                           lpcBtLePath,
                                                                           lenBTOpIfceType,
                                                                           lenBTLeGattOp,
                                                                           lpDevicePath,  // for now
                                                                           apLeOpArg, 
                                                                           rpLeOpRes) ) {
       BTRCORELOG_ERROR ("Failed to Perform Le Gatt Op %d for UUID %s  !!!.\n", aenBTRCoreLEGattOp, apcBtUuid);
       return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

// Outgoing callbacks Registration Interfaces
enBTRCoreRet
BTRCore_LE_RegisterStatusUpdateCb (
    tBTRCoreLeHdl                       hBTRCoreLe,
    fPtr_BTRCore_LeStatusUpdateCb       afpcBTRCoreLeStatusUpdate,
    void*                               apvBtLeStatusUserData
) {
    stBTRCoreLeHdl* phBTRCoreLe = NULL;

    if (!hBTRCoreLe || !afpcBTRCoreLeStatusUpdate)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    phBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;

    phBTRCoreLe->fpcBTRCoreLeStatusUpdate = afpcBTRCoreLeStatusUpdate;
    phBTRCoreLe->pvBtLeStatusUserData     = apvBtLeStatusUserData;

    return enBTRCoreSuccess;
}


//incoming callbacks

static int
btrCore_LE_AdvInfoCb (
    const char*            apBtAdvPath,
    stBTLeCustomAdv**      appstBtLeCustomAdv,
    void*                  apUserData
) {
    stBTRCoreLeHdl*        lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreLeCustomAdv*  lpstCustomAdv = &lpstlhBTRCoreLe->stCustomAdv;
    stBTLeCustomAdv*       lpstBTLeCustomAdv = *appstBtLeCustomAdv;

    strncpy(lpstBTLeCustomAdv->AdvertisementType, lpstCustomAdv->AdvertisementType, (BT_MAX_STR_LEN - 1));
    lpstBTLeCustomAdv->bTxPower = lpstCustomAdv->bTxPower;

    lpstBTLeCustomAdv->numServiceUUID = lpstCustomAdv->numServiceUUID;
    for (int index = 0; index < lpstCustomAdv->numServiceUUID; index++) {
        strncpy(lpstBTLeCustomAdv->ServiceUUID[index], lpstCustomAdv->ServiceUUID[index], (BT_MAX_STR_LEN - 1));
    }

    lpstBTLeCustomAdv->numSolicitUUID = lpstCustomAdv->numSolicitUUID;
    for (int index = 0; index < lpstCustomAdv->numSolicitUUID; index++) {
        strncpy(lpstBTLeCustomAdv->SolicitUUID[index], lpstCustomAdv->SolicitUUID[index], (BT_MAX_STR_LEN - 1));
    }

    lpstBTLeCustomAdv->ManfData.ManfID = lpstCustomAdv->ManfData.ManfID;
    lpstBTLeCustomAdv->ManfData.lenManfData = lpstCustomAdv->ManfData.lenManfData;
    for (int index = 0; index < lpstCustomAdv->ManfData.lenManfData; index++) {
       lpstBTLeCustomAdv->ManfData.data[index] = lpstCustomAdv->ManfData.data[index];
    }

    return 0;
}

static int
btrCore_LE_GattInfoCb (
    enBTOpIfceType      aenBtOpIfceType,
    enBTLeGattOp        aenGattOp,
    const char*         apBtGattPath,
    const char*         aBtdevAddr,
    enBTDeviceState     aenBTDeviceState,
    void*               apLeCbData,
    void*               apUserData
) {
    stBTRCoreLeHdl*         lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreLeGattProfile* pProfile        = NULL;
    tBTRCoreDevId           ltBTRCoreDevId  = -1;

    if (!apBtGattPath || !aBtdevAddr || !apUserData) {
        BTRCORELOG_ERROR("Invalid arguments!!!\n");
        return -1;
    }

    ltBTRCoreDevId  = btrCore_LE_BTGenerateUniqueDeviceID (aBtdevAddr);
    BTRCORELOG_DEBUG("apBtGattPath : %s\n", apBtGattPath);

    if (enBTAdvertisement == aenBtOpIfceType) {
        if (NULL != apLeCbData) {
            BTRCORELOG_TRACE("Inovking btrCore_LE_LocalGattServerInfoCb\n");
            stBTLeGattService* lpstBTLeGattSrv = ((stBTLeGattInfo*)apLeCbData)->astBTRGattService;
            btrCore_LE_LocalGattServerInfoCb(apBtGattPath, aBtdevAddr, &lpstBTLeGattSrv, &((stBTLeGattInfo*)apLeCbData)->nNumGattServices, apUserData);
        }
    }
    else if (aenBTDeviceState == enBTDevStFound) {
        char lBtUuid[BT_MAX_STR_LEN] = "\0";
        unBTOpIfceProp aunBTOpIfceProp;

        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Storing GATT Service Info...\n");
            char lBtDevPath[BT_MAX_STR_LEN] = "\0";
            char retUuid = -1, retDPath = -1;
            stBTRCoreLeGattService *pService = NULL;

            aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
            retUuid  = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            if (!retUuid && btrCore_LE_isServiceSupported(lBtUuid)) {

                aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
                retDPath = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtDevPath);

                if (!retDPath) {
                    if (!(pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {

                        if (lpstlhBTRCoreLe->ui16NumberOfGattProfile < BTR_MAX_GATT_PROFILE) {
                            pProfile = &lpstlhBTRCoreLe->astBTRGattProfile[lpstlhBTRCoreLe->ui16NumberOfGattProfile];
                            strncpy(pProfile->devicePath, lBtDevPath, BTRCORE_MAX_STR_LEN - 1);
                            pProfile->deviceID = ltBTRCoreDevId;
                            lpstlhBTRCoreLe->ui16NumberOfGattProfile++;
                            BTRCORELOG_DEBUG ("Added Profile for Device %llu Successfully.\n", ltBTRCoreDevId);
                        }
                        else {
                            BTRCORELOG_ERROR ("BTR_MAX_GATT_PROFILE Added. Couldn't add anymore...\n");
                        }
                    }

                    if (pProfile->ui16NumberOfGattService < BTR_MAX_GATT_SERVICE) {
                        if (!(pService = btrCore_LE_FindGattService(pProfile, apBtGattPath))) {
                            pService = &pProfile->astBTRGattService[pProfile->ui16NumberOfGattService];
                            strncpy(pService->serviceUuid, lBtUuid, BT_MAX_UUID_STR_LEN - 1);
                            strncpy(pService->servicePath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                            pService->parentProfile = pProfile;
                            pProfile->ui16NumberOfGattService++;
                            BTRCORELOG_DEBUG ("Added Service %s Successfully.\n", lBtUuid);
                        }
                        else {
                            BTRCORELOG_WARN ("Gatt Service %s already exists...\n", apBtGattPath);
                        }
                    }
                    else {
                        BTRCORELOG_WARN ("BTR_MAX_GATT_SERVICE Added. Couldn't add anymore...\n");
                    }
                }
                else {
                    BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retDPath : %d.\n",retUuid, retDPath);
                }
            }
            else {
                BTRCORELOG_WARN ("Service Not Supported | UUID : %s\n", lBtUuid);
            }
        }
        else if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG ("Storing GATT Characteristic Info...\n");
            char lBtSerivcePath[BT_MAX_STR_LEN]  = "\0";
            char retUuid = -1, retSPath = -1;
            stBTRCoreLeGattService *pService = NULL;
            stBTRCoreLeGattChar    *pChar    = NULL;

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;
            retUuid  = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtSerivcePath);

            if (!retUuid && !retSPath) {

                if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                    if ((pService = btrCore_LE_FindGattService(pProfile, lBtSerivcePath))) {

                        if ((pService->ui16NumberOfGattChar < BTR_MAX_GATT_CHAR)) {

                            if (!(pChar = btrCore_LE_FindGattCharacteristic(pService, apBtGattPath))) {
                                char cFlags[BTR_MAX_GATT_CHAR_FLAGS][BT_MAX_UUID_STR_LEN];
                                memset (cFlags, 0, sizeof(cFlags));
                                pChar = &pService->astBTRGattChar[pService->ui16NumberOfGattChar];
                                strncpy(pChar->charUuid, lBtUuid, BT_MAX_UUID_STR_LEN - 1 );
                                strncpy(pChar->charPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                                pChar->parentService = pService;
                                aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
                                if (!BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&cFlags)) {
                                    pChar->charFlags = btrCore_LE_GetAllowedGattFlagValues(cFlags, enBTGattCharacteristic);
                                }
                                pService->ui16NumberOfGattChar++;
                                BTRCORELOG_DEBUG ("Added Characteristic %s Successfully.\n", lBtUuid);
                            }
                            else {
                                BTRCORELOG_WARN ("Gatt Characteristic %s already exists.\n", apBtGattPath);
                            }
                        }
                        else {
                            BTRCORELOG_WARN ("BTR_MAX_GATT_CHAR Addedd. Couldn't add anymore.\n");
                        }
                    }
                    else {
                        BTRCORELOG_ERROR ("Gatt Service %s not found.\n", lBtSerivcePath);
                    }
                }
                else {
                    BTRCORELOG_ERROR ("Gatt Profile for device %llu not found.\n", ltBTRCoreDevId);
                }
            }
            else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retSPath : %d\n",retUuid, retSPath);
            }
        }
        else if (aenBtOpIfceType == enBTGattDescriptor) {
            BTRCORELOG_DEBUG ("Storing GATT Descriptor Info...\n");
            char lBtSerivcePath[BT_MAX_STR_LEN]  = "\0";
            char lBtCharPath[BT_MAX_STR_LEN]     = "\0";
            char retUuid = -1, retCPath = -1, retSPath = -1;
            stBTRCoreLeGattService *pService = NULL;
            stBTRCoreLeGattChar    *pChar    = NULL;
            stBTRCoreLeGattDesc    *pDesc    = NULL;

            aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;
            retUuid  = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

            aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
            retCPath = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtCharPath);

            aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
            retSPath = BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, lBtCharPath, enBTGattCharacteristic, aunBTOpIfceProp, (void*)&lBtSerivcePath);

            if (!retUuid && !retCPath && !retSPath) {

                if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                    if ((pService = btrCore_LE_FindGattService(pProfile, lBtSerivcePath))) {
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
                                    if (!BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&dFlags)) {
                                        pDesc->descFlags = btrCore_LE_GetAllowedGattFlagValues(dFlags, enBTGattDescriptor);
                                    }
                                    pChar->ui16NumberOfGattDesc++;
                                    BTRCORELOG_DEBUG ("Added Gatt Descriptor %s Successfully.\n", lBtUuid);
                                }
                                else {
                                    BTRCORELOG_WARN ("Gatt Descriptor %s already exists.\n", apBtGattPath);
                                }
                            }
                            else {
                                BTRCORELOG_WARN ("BTR_MAX_GATT_DESC Added. Couldn't add anymore.\n");
                            }
                        }
                        else {
                            BTRCORELOG_ERROR ("Gatt Characteristic not found for Desc %s\n", lBtCharPath);
                        }
                    }
                    else {
                        BTRCORELOG_ERROR ("Gatt Service not found for Desc %s.\n", lBtSerivcePath);
                    }
                }
                else {
                    BTRCORELOG_ERROR ("Gatt Profile for device %llu not found...\n", ltBTRCoreDevId);
                }
            }
            else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retCPath : %d | retSpath : %d !!!\n", retUuid, retCPath, retSPath);
            }
        }
    } 
    else if (aenBTDeviceState == enBTDevStLost) {
        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Freeing Gatt Service %s \n", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        }
        else if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG ("Freeing GATT Characteristic %s \n", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        }
        else if (aenBtOpIfceType == enBTGattDescriptor) {
            BTRCORELOG_DEBUG ("Freeing GATT Descriptor %s \n", apBtGattPath);
            /* Decide on this later as, enBTDevStLost won't be called upon a LE Dev disconnect/Lost */ 
        }
    }
    else if (aenBTDeviceState == enBTDevStPropChanged) {
        if (aenBtOpIfceType == enBTGattService) {
            BTRCORELOG_DEBUG ("Property Changed for Gatt Service %s\n", apBtGattPath);
        }
        else if (aenBtOpIfceType == enBTGattCharacteristic) {
            BTRCORELOG_DEBUG("Property Changed for Gatt Char %s\n", apBtGattPath);

            if ((enBTLeGattOpReadValue == aenGattOp) || (enBTLeGattOpWriteValue == aenGattOp)) {
                if (NULL != apLeCbData) {
                    BTRCORELOG_TRACE("Inovking btrCore_LE_UpdateLocalGattInfoCb\n");
                    btrCore_LE_UpdateLocalGattInfoCb(aenBtOpIfceType, aenGattOp, aBtdevAddr, apBtGattPath, (char*)apLeCbData, apUserData);
                }
            }
            else if (enBTLeGattOpUnknown == aenGattOp) {
                char lBtSerivcePath[BT_MAX_STR_LEN] = "\0";
                unBTOpIfceProp aunBtOpIfceProp;
                aunBtOpIfceProp.enBtGattCharProp = enBTGattCPropService;
                stBTRCoreLeGattService *pService = NULL;
                stBTRCoreLeGattChar    *pChar = NULL;

                if (!BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, aenBtOpIfceType, aunBtOpIfceProp, (void*)&lBtSerivcePath)) {
                    if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                        if ((pService = btrCore_LE_FindGattService(pProfile, lBtSerivcePath))) {
                            if ((pChar = btrCore_LE_FindGattCharacteristic(pService, apBtGattPath))) {

                                if ((pChar->charFlags & BTR_GATT_CHAR_FLAG_READ) ||
                                    (pChar->charFlags & BTR_GATT_CHAR_FLAG_NOTIFY)) {
                                    stBTRCoreLeGattInfo lstBtrLeInfo;
                                    char value[BT_MAX_GATT_OP_DATA_LEN];

                                    memset(value, '\0', BT_MAX_GATT_OP_DATA_LEN);
                                    memset(&lstBtrLeInfo, 0, sizeof(stBTRCoreLeGattInfo));


                                    aunBtOpIfceProp.enBtGattCharProp = enBTGattCPropValue;
                                    if (!BtrCore_BTGetProp(lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, enBTGattCharacteristic, aunBtOpIfceProp, (void*)&value)) {
                                        BTRCORELOG_TRACE("Obtained Characteristic Value \"%s\" with len %d\n", value, strlen(value));

                                        if (!strlen(value)) {
                                            lstBtrLeInfo.enLeOper = enBTRCoreLEGOpStartNotify; //TODO: Differentiate between enBTRCoreLEGOpStartNotify & enBTRCoreLEGOpStopNotify
                                        }
                                        else {
                                            lstBtrLeInfo.enLeOper = enBTRCoreLEGOpReadValue; //TODO: Deduce from Bt-Ifce and locally stored information
                                        }
                                        lstBtrLeInfo.enLeProp = enBTRCoreLEGPropValue; //TODO: Deduce from Bt-Ifce and locally stored information
                                        //TODO: The above needs to be changed correctly
                                        lstBtrLeInfo.pui8Value = value;
                                        lstBtrLeInfo.pui8Uuid = pChar->charUuid;

                                        /* -------------Callback to Higher Layers-------------- */
                                        lpstlhBTRCoreLe->fpcBTRCoreLeStatusUpdate(&lstBtrLeInfo, aBtdevAddr, lpstlhBTRCoreLe->pvBtLeStatusUserData);
                                    }
                                    else {
                                        BTRCORELOG_ERROR("BtrCore_BTGetProp Failed to get property enBTGattCPropValue.\n");
                                    }
                                }
                                else {
                                    BTRCORELOG_ERROR("BTR_GATT_CHAR_FLAG_NOTIFY/READ Operation not permitted in interface %s\n", apBtGattPath);
                                }
                            }
                            else {
                                BTRCORELOG_ERROR("Gatt Char %s Not Found\n", apBtGattPath);
                            }
                        }
                        else {
                            BTRCORELOG_ERROR("Gatt Service %s Not Found\n", lBtSerivcePath);
                        }
                    }
                    else {
                        BTRCORELOG_ERROR("Gatt Profile for Device  %llu Not Found\n", ltBTRCoreDevId);
                    }
                }
            }
            else {
            BTRCORELOG_ERROR("BtrCore_BTGetProp Failed to get property enBTGattCPropService\n");
            }
        }
        else if (aenBtOpIfceType == enBTGattDescriptor) {
            BTRCORELOG_DEBUG ("Property Changed for Gatt Desc %s\n", apBtGattPath);
            if ((enBTLeGattOpReadValue == aenGattOp) || (enBTLeGattOpWriteValue == aenGattOp)) {
                if (NULL != apLeCbData) {
                    BTRCORELOG_DEBUG("Inovking btrCore_LE_UpdateLocalGattInfoCb\n");
                    btrCore_LE_UpdateLocalGattInfoCb(aenBtOpIfceType, aenGattOp, aBtdevAddr, apBtGattPath, (char*)apLeCbData, apUserData);
                }
            }
        }
        else if (aenBtOpIfceType == enBTDevice) {
            if ((pProfile = btrCore_LE_FindGattProfile(lpstlhBTRCoreLe, ltBTRCoreDevId))) {
                stBTRCoreLeGattInfo lstBtrLeInfo;
                unBTOpIfceProp      aunBtOpIfceProp;
                int                 i32value = 0;

                memset (&lstBtrLeInfo, 0, sizeof(stBTRCoreLeGattInfo));

                aunBtOpIfceProp.enBtDeviceProp = enBTDevPropSrvRslvd;
                if (!BtrCore_BTGetProp (lpstlhBTRCoreLe->btIfceHdl, apBtGattPath, enBTDevice, aunBtOpIfceProp, (void*)&i32value)) {
                    BTRCORELOG_WARN ("Obtained Device SERVICESRESOLVED Value %d\n", i32value);

                    if (pProfile->i8LeGattOpReady != (char)i32value) {
                        pProfile->i8LeGattOpReady = (char)i32value;

                        if (pProfile->i8LeGattOpReady) {
                            lstBtrLeInfo.enLeOper  = enBTRCoreLEGOpReady;
                            lstBtrLeInfo.pui8Value = &pProfile->i8LeGattOpReady;

                            /* -------------Callback to Higher Layers-------------- */
                            lpstlhBTRCoreLe->fpcBTRCoreLeStatusUpdate (&lstBtrLeInfo, aBtdevAddr, lpstlhBTRCoreLe->pvBtLeStatusUserData);
                        }
                    }
                }
                else {
                   BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get property enBTGattCPropValue.\n");
                }
            }
        }
    }
    else {
        BTRCORELOG_WARN ("Callback for irrelavent DeviceState : %d!!!\n", aenBTDeviceState);
    }

    return 0;
}

