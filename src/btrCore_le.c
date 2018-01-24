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
#include <glib.h>

/* External Library Headers */

/* Local Headers */
#include "btrCore_priv.h"
#include "btrCore_le.h"
#include "btrCore_bt_ifce.h"


#define MAX_NUMBER_GATT_SERVICES    2
#define MAX_GATT_SERVICE_ID_ARRAY   4
#define MAX_GATT_CHAR_ARRAY         4
#define MAX_GATT_DESC_ARRAY         4
#define MAX_UUID_SIZE               64
#define GATT_CHAR_FLAGS             16
#define GATT_DESC_FLAGS             8

/* GattDescriptor1 Properties */
typedef struct _stBTRGattDesc {
    int            descId;  /* really if it is usefull? */              /* Descriptor ID */
    char           dPath[BTRCORE_MAX_STR_LEN];                          /* Descriptor Path */
    char           descUuid[MAX_UUID_SIZE];                             /* 128-bit service UUID */
    char           gattCharPath[BTRCORE_MAX_STR_LEN];                   /* Object Path */
}stBTRGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTRGattChar {
    int            charId;                                              /* Characteristic ID */
    char           cPath[BTRCORE_MAX_STR_LEN];                          /* Characteristic Path */
    char           charUuid[MAX_UUID_SIZE];                             /* 128-bit service UUID */
    char           gattServicePath[BTRCORE_MAX_STR_LEN];                /* Object Path */
    stBTRGattDesc  astBTRGattDesc[MAX_GATT_DESC_ARRAY];                 /* Max of 4 Gatt Descriptor array */
    unsigned short ui16NumberofGattDesc;                                /* Number of Gatt Service ID */
}stBTRGattChar;


/* GattService Path and Properties */
typedef struct _stBTRGattService {
    int            serviceId;                                           /* Service ID */
    char           sPath[BTRCORE_MAX_STR_LEN];                          /* Service Path */
    char           serviceUuid[MAX_UUID_SIZE];                          /* 128-bit service UUID */
    char           gattDevicePath[BTRCORE_MAX_STR_LEN];                 /* Object Path */
    stBTRGattChar  astBTRGattChar[MAX_GATT_CHAR_ARRAY];                 /* Max of 4 Gatt Charactristic array */
    unsigned short ui16NumberofGattChar;                                /* Number of Gatt Charactristics */
}stBTRGattService;


typedef struct _stBTRCoreGattProfile {
    stBTRGattService    astBTRGattService[MAX_NUMBER_GATT_SERVICES];
    unsigned short      ui16NumberofGattService;                        /* Number of Gatt Service ID */
} stBTRCoreGattProfile;





typedef struct _stBTRCoreLeHdl {

    stBTRCoreGattProfile stBTRGattProfile;

} stBTRCoreLeHdl;



/* Static Function Prototypes */
static inline int btrCore_LE_GetGattServiceId (const char* apGattPath);
static inline int btrCore_LE_GetGattCharId (const char* apGattPath);
static inline int btrCore_LE_GetGattDescId (const char* apGattPath);

static stBTRGattService* btrCore_LE_FindGattService (stBTRCoreGattProfile  *pGattProfile, int serviceId);
static stBTRGattChar* btrCore_LE_FindGattCharacteristic (stBTRGattService  *pService, int charId);
static stBTRGattDesc* btrCore_LE_FindGattDescriptor (stBTRGattChar *pChar, int descId);

static enBTRCoreRet btrCore_LE_GetDataPath (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevPath, const char* apBtLeUuid, char** rpBtLePath);



/* Callbacks */
static const char* btrCore_LE_GattInfoCb (enBTOpIfceType enBtOpIfceType, const char* apBtGattPath, enBTDataMode  aenBTDataMode, void* apConnHdl, void* apUserData );


/* static function definitions */
/* Macro define Hardcoded Values */
static inline int
btrCore_LE_GetGattServiceId (
    const char*   apGattPath
) {
    return (int) strtol (&apGattPath[(strstr(apGattPath, "service") - apGattPath) + 7], NULL, 10);
}

static inline int
btrCore_LE_GetGattCharId (
    const char*   apGattPath
) {
    return (int) strtol (&apGattPath[(strstr(apGattPath, "char") - apGattPath) + 4], NULL, 10);
}

static inline int
btrCore_LE_GetGattDescId (
    const char*    apGattPath
) {
    return (int) strtol (&apGattPath[(strstr(apGattPath, "desc") - apGattPath) + 4], NULL, 10);
}


static stBTRGattService*
btrCore_LE_FindGattService (
    stBTRCoreGattProfile   *pGattProfile,
    int                    serviceId
) {
    int i = 0;
    if (pGattProfile) {
       for (; i < pGattProfile->ui16NumberofGattService; i++) {
           if (pGattProfile->astBTRGattService[i].serviceId == serviceId) {
              BTRCORELOG_DEBUG ("Gatt Service %d Found", serviceId);
              break;
           }
       }
    }
    return (pGattProfile && i != pGattProfile->ui16NumberofGattService)? &(pGattProfile->astBTRGattService[i]) : NULL ;
}


static stBTRGattChar*
btrCore_LE_FindGattCharacteristic (
    stBTRGattService   *pService,
    int                charId
) {
    int i = 0;
    if (pService) {
       for (; i < pService->ui16NumberofGattChar; i++) {
           if (pService->astBTRGattChar[i].charId == charId) {
              BTRCORELOG_DEBUG ("Gatt Characteristic %d Found", charId);
              break;
           }
       }
    }
    return (pService && i != pService->ui16NumberofGattChar)? &(pService->astBTRGattChar[i]) : NULL ;
}


static stBTRGattDesc*
btrCore_LE_FindGattDescriptor (
    stBTRGattChar    *pChar,
    int              descId
) {
    int i = 0;
    if (pChar) {
        for (; i < pChar->ui16NumberofGattDesc; i++) {
            if (pChar->astBTRGattDesc[i].descId == descId) {
               BTRCORELOG_DEBUG ("Gatt Descriptor %d Found", descId);
               break;
            }
        }
    }
    return (pChar && i != pChar->ui16NumberofGattDesc)? &(pChar->astBTRGattDesc[i]) : NULL ;
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

    if (!phBTRCoreLe || !apBtConn || !apBtAdapter) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    enBTRCoreRet       lenBTRCoreRet   = enBTRCoreSuccess;
    stBTRCoreLeHdl*    pstlhBTRCoreLe  = NULL;

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
btrCore_LE_GetDataPath (
    tBTRCoreLeHdl       hBTRCoreLe,
    void*               apBtConn,
    const char*         apBtDevPath,
    const char*         apBtLeUuid,
    char**              rpBtLePath
) {

    if (!hBTRCoreLe || !apBtConn || !apBtDevPath || !apBtLeUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");       
       return enBTRCoreInvalidArg;
    }

    stBTRCoreLeHdl*      pstlhBTRCoreLe   = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreGattProfile *pGattProfile    = &pstlhBTRCoreLe->stBTRGattProfile;

    if (pGattProfile->ui16NumberofGattService == 0) {
       BTRCORELOG_ERROR ("No Gatt Service Exists!!!\n");
       return enBTRCoreFailure;
    }

    unsigned short ui16SLoopindex = 0;
    stBTRGattService *pService    = NULL;

    for (; ui16SLoopindex < pGattProfile->ui16NumberofGattService; ui16SLoopindex++) {
        pService  = &pGattProfile->astBTRGattService[ui16SLoopindex];
        if (!strcmp(apBtDevPath, pService->gattDevicePath)) {
           BTRCORELOG_DEBUG ("Found Service : %d", pService->serviceId);
           break;
        }
    }

    if (ui16SLoopindex == pGattProfile->ui16NumberofGattService) {
       BTRCORELOG_ERROR ("Service Not Found for Dev : %s!!!", apBtDevPath);
       return enBTRCoreFailure;
    }

    char* retLeDataPath = NULL;

    if (!strcmp(apBtLeUuid, pService->serviceUuid)) {
       retLeDataPath = pService->sPath;
       BTRCORELOG_DEBUG ("UUID matched Service : %d", pService->serviceId);
    }
    else {
       if (pService->ui16NumberofGattChar == 0) {
          BTRCORELOG_ERROR ("No match found for UUID : %s !!!", apBtLeUuid);
          return enBTRCoreFailure;
       }
       unsigned short ui16CLoopindex = 0;
       stBTRGattChar *pChar          = NULL;

       for (; ui16CLoopindex < pService->ui16NumberofGattChar; ui16CLoopindex++) {
           pChar = &pService->astBTRGattChar[ui16CLoopindex];
           if (!strcmp(apBtLeUuid, pChar->charUuid)) {
              retLeDataPath = pChar->cPath;
              BTRCORELOG_DEBUG ("UUID matched Characteristic : %d", pChar->charId);
              break;
           }
       }
       if (ui16CLoopindex == pService->ui16NumberofGattChar) {
          unsigned short ui16DLoopindex = 0;
          stBTRGattDesc  *pDesc = NULL;

          for (ui16CLoopindex=0; ui16CLoopindex < pService->ui16NumberofGattChar; ui16CLoopindex++) {
              pChar = &pService->astBTRGattChar[ui16CLoopindex];

              if (pChar->ui16NumberofGattDesc == 0) {
                 continue;  /* Char has no desc to loop through */
              }

              for (ui16DLoopindex=0; ui16DLoopindex < pChar->ui16NumberofGattDesc; ui16DLoopindex++) {
                  pDesc = &pChar->astBTRGattDesc[ui16DLoopindex];
                  if (!strcmp(apBtLeUuid, pDesc->descUuid)) {
                     retLeDataPath = pDesc->dPath;
                     BTRCORELOG_DEBUG ("UUID matched Descriptor : %d", pDesc->descId);
                     break;
                  }
              }
          }
       }
    }
    if (retLeDataPath) {
       *rpBtLePath = retLeDataPath;
    } else {
       BTRCORELOG_ERROR ("No match found for UUID : %s !!!", apBtLeUuid);
       return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


//Get Gatt info in GetProperty handling 
enBTRCoreRet
BTRCore_LE_GetGattProperty (
    tBTRCoreLeHdl        hBTRCoreLe,
    void*                apvBtConn,
    const char*          apcBtDevPath,
    const char*          apcBtUuid,
    enBTRCoreLEGattProp  aenBTRCoreLEGattProp,
    void*                apvBtPropValue
) {
    if (!hBTRCoreLe || !apvBtConn || !apcBtDevPath || !apcBtUuid)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }
  
    char* lpcBtLePath = NULL;

    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, apcBtDevPath, apcBtUuid, &lpcBtLePath) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath) {
       BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!");
       return enBTRCoreFailure;
    }
    
    enBTOpIfceType lenBTOpIfceType = enBTUnknown;
    unBTOpIfceProp lunBTOpIfceProp;
    
    switch (aenBTRCoreLEGattProp) {

    case enBTRCoreLEGSPropUUID:
        lenBTOpIfceType = enBTGattService;
        lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
        break;
    case enBTRCoreLEGSPropPrimary:
        lenBTOpIfceType = enBTGattService;
        lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropPrimary;
        break;
    case enBTRCoreLEGSPropDevice:
        lenBTOpIfceType = enBTGattService;
        lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
        break;
    case enBTRCoreLEGCPropUUID:
        lenBTOpIfceType = enBTGattCharacteristic;
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;
        break;
    case enBTRCoreLEGCPropService:
        lenBTOpIfceType = enBTGattCharacteristic;
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
        break;
    case enBTRCoreLEGCPropValue:
        lenBTOpIfceType = enBTGattCharacteristic;
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropValue;
        break;
    case enBTRCoreLEGCPropNotifying:
        lenBTOpIfceType = enBTGattCharacteristic;
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropNotifying;
        break;
    case enBTRCoreLEGCPropFlags:
        lenBTOpIfceType = enBTGattCharacteristic;
        lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
        break;
    case enBTRCoreLEGDPropUUID:
        lenBTOpIfceType = enBTGattDescriptor;
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;
        break;
    case enBTRCoreLEGDPropChar:
        lenBTOpIfceType = enBTGattDescriptor;
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
        break;
    case enBTRCoreLEGDPropValue:
        lenBTOpIfceType = enBTGattDescriptor;
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropValue;
        break;
    case enBTRCoreLEGDPropFlags:
        lenBTOpIfceType = enBTGattDescriptor;
        lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropFlags;
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


enBTRCoreRet
BtrCore_LE_PerformGattMethodOp (
    tBTRCoreLeHdl      hBTRCoreLe,
    void*              apvBtConn,
    const char*        apcBtDevPath,
    const char*        apcBtUuid,
    enBTRCoreLEGattOp  aenBTRCoreLEGattOp
) {

    if (!hBTRCoreLe || !apvBtConn || !apcBtDevPath || !apcBtUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }
  
    char* lpcBtLePath = NULL;

    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, apcBtDevPath, apcBtUuid, &lpcBtLePath) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath) {
       BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!");
       return enBTRCoreFailure;
    }

    enBTLeGattOp lenBTLeGattOp = enBTLeGattOpUnknown;

    switch (aenBTRCoreLEGattOp) { 

    case enBTRCoreLEGCOpReadValue:
        lenBTLeGattOp = enBTLeGattCharOpReadValue;
        break;
    case enBTRCoreLEGCOpWriteValue:
        lenBTLeGattOp = enBTLeGattCharOpWriteValue;
        break;
    case enBTRCoreLEGCOpStartNotify:
        lenBTLeGattOp = enBTLeGattCharOpStartNotify;
        break;
    case enBTRCoreLEGCOpStopNotify:
        lenBTLeGattOp = enBTLeGattCharOpStopNotify;
        break;
    case enBTRCoreLEGDOpReadValue:
        lenBTLeGattOp = enBTLeGattDescOpReadValue;
        break;
    case enBTRCoreLEGDOpWriteValue:
        lenBTLeGattOp = enBTLeGattDescOpWriteValue;
        break;
    case enBTRCoreLEGOpUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid enBTRCoreLEGattOp Options %d !!!", aenBTRCoreLEGattOp);
        break;
    }

    if (lenBTLeGattOp == enBTLeGattOpUnknown || BtrCore_BTPerformLeGattMethodOp (apvBtConn,
                                                                                 lpcBtLePath,
                                                                                 lenBTLeGattOp,
                                                                                 NULL) ){
       BTRCORELOG_ERROR ("Failed to Perform Le Gatt Op %d for UUID %s  !!!", aenBTRCoreLEGattOp, apcBtUuid);
       return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}



static const char*
btrCore_LE_GattInfoCb (
    enBTOpIfceType aenBtOpIfceType,
    const char*    apBtGattPath,
    enBTDataMode   aenBTDataMode,
    void*          apConnHdl,
    void*          apUserData
) {

    if (!apBtGattPath || !apUserData) {
       BTRCORELOG_ERROR("Invalid arguments!!!");
       return NULL;
    }

    stBTRCoreLeHdl*      lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreGattProfile *pGattProfile   = &(lpstlhBTRCoreLe->stBTRGattProfile);
    BTRCORELOG_DEBUG("apBtGattPath : %s\n", apBtGattPath);

    char lBtUuid[BT_MAX_STR_LEN]="\0";
    unBTOpIfceProp aunBTOpIfceProp;

    if (aenBtOpIfceType == enBTGattService) {
       int sId = btrCore_LE_GetGattServiceId(apBtGattPath);
             
       stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, sId);

       if (aenBTDataMode == enBTDMStore) {
          BTRCORELOG_DEBUG ("Storing GATT Service Info...\n");
          char lBtDevPath[BT_MAX_STR_LEN] = "\0";
          unsigned char retUuid = -1, retDPath = -1;

          aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
          retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

          aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
          retDPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtDevPath);

          if (!retUuid && !retDPath) {
             if (pGattProfile->ui16NumberofGattService < MAX_NUMBER_GATT_SERVICES) {
                if (!pService) {
                   pService = &pGattProfile->astBTRGattService[pGattProfile->ui16NumberofGattService];
                   strncpy(pService->serviceUuid, lBtUuid, MAX_UUID_SIZE - 1);
                   strncpy(pService->sPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                   strncpy(pService->gattDevicePath, lBtDevPath, BTRCORE_MAX_STR_LEN - 1);
                   pService->serviceId = sId;
                   pGattProfile->ui16NumberofGattService++;
                   BTRCORELOG_DEBUG ("Added Service %s Successfully.", lBtUuid);
                } else {
                   BTRCORELOG_WARN ("Gatt Service %s already exists...", apBtGattPath);       }
             } else {
                BTRCORELOG_WARN ("MAX_NUMBER_GATT_SERVICES Added. Couldn't add anymore...");  }
          } else {
             BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get GattService!!!");             }
       } else
       if (aenBTDataMode == enBTDMRelease) {
          BTRCORELOG_DEBUG ("Freeing Gatt Service %s ", apBtGattPath);

          if (pService) {
             memset (pService, 0, sizeof(stBTRGattService));
             pGattProfile->ui16NumberofGattService--;
          } else {
             BTRCORELOG_WARN ("Gatt Service %s not found...", apBtGattPath);
          }
       } 
    }
    else if (aenBtOpIfceType == enBTGattCharacteristic) {
       char gattPath[BTRCORE_MAX_STR_LEN] = "\0";
       int sId = 0, cId = 0;

       strncpy (gattPath, apBtGattPath, (strlen(apBtGattPath)<BTRCORE_MAX_STR_LEN)?strlen(apBtGattPath):(BTRCORE_MAX_STR_LEN-1) );
       cId = btrCore_LE_GetGattCharId(gattPath);
       gattPath[strstr(gattPath, "/char") - gattPath] = '\0';
       sId = btrCore_LE_GetGattServiceId(gattPath);

       stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, sId);
       stBTRGattChar    *pChar = btrCore_LE_FindGattCharacteristic(pService, cId);

       if (pService) {
          if (aenBTDataMode == enBTDMStore) {
             BTRCORELOG_DEBUG ("Storing GATT Characteristic Info...\n");
             aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;

             if (!BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid)) {
                if (pService->ui16NumberofGattChar < MAX_GATT_CHAR_ARRAY) {
                   if (!pChar) {
                      pChar = &(pService->astBTRGattChar[pService->ui16NumberofGattChar]);
                      strncpy(pChar->charUuid, lBtUuid, MAX_UUID_SIZE - 1 );
                      strncpy(pChar->cPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                      pChar->charId = cId;
                      pService->ui16NumberofGattChar++;
                      BTRCORELOG_DEBUG ("Added Characteristic %s Successfully.", lBtUuid);
                   } else {
                      BTRCORELOG_WARN ("Gatt Characteristic %s already exists...", apBtGattPath);  }
                } else {
                   BTRCORELOG_WARN ("MAX_GATT_CHAR_ARRAY Addedd. Couldn't add anymore...");        }   
             } else {
                BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get Characteristic!!!");            }
          } else
          if (aenBTDataMode == enBTDMRelease) {
             BTRCORELOG_DEBUG ("Freeing GATT Characteristic %s ", apBtGattPath);

             if (pChar) {
                memset (pChar, 0, sizeof(stBTRGattChar));
                pService->ui16NumberofGattChar--;
             } else {
                BTRCORELOG_WARN ("Gatt Characteristic %s not found...", apBtGattPath);
             }
          }
       } else {
          BTRCORELOG_WARN ("Gatt Service %s not found...", gattPath);
       }
    }
    else if (aenBtOpIfceType == enBTGattDescriptor) {
       char gattPath[BTRCORE_MAX_STR_LEN] = "\0";
       int sId = 0, cId = 0, dId = 0;

       strncpy (gattPath, apBtGattPath, (strlen(apBtGattPath)<BTRCORE_MAX_STR_LEN)?strlen(apBtGattPath):(BTRCORE_MAX_STR_LEN-1) );
       dId = btrCore_LE_GetGattDescId(gattPath);
       gattPath[strstr(gattPath, "/desc") - gattPath] = '\0';
       cId = btrCore_LE_GetGattCharId(gattPath);
       gattPath[strstr(gattPath, "/char") - gattPath] = '\0';
       sId = btrCore_LE_GetGattServiceId(gattPath);

       stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, sId);
       stBTRGattChar    *pChar = btrCore_LE_FindGattCharacteristic(pService, cId);
       stBTRGattDesc    *pDesc = btrCore_LE_FindGattDescriptor(pChar, dId);

       if (pService) {
          if (pChar) {
             if (aenBTDataMode == enBTDMStore) {
                BTRCORELOG_DEBUG ("Storing GATT Descriptor Info...\n");
                aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;

                if (!BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid)) {
                   if (pChar->ui16NumberofGattDesc < MAX_GATT_DESC_ARRAY) {
                      if (!pDesc) {
                         pDesc = &(pChar->astBTRGattDesc[pChar->ui16NumberofGattDesc]);
                         strncpy (pDesc->descUuid, lBtUuid, MAX_UUID_SIZE - 1);
                         strncpy(pDesc->dPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                         pDesc->descId = dId;
                         pChar->ui16NumberofGattDesc++;
                         BTRCORELOG_DEBUG ("Added Gatt Descriptor %s Successfully.", lBtUuid);
                      } else {
                         BTRCORELOG_WARN ("Gatt Descriptor %s already exists...", apBtGattPath);  }
                   } else {
                      BTRCORELOG_WARN ("MAX_GATT_DESC_ARRAY Added. Couldn't add anymore...");     }
                } else {
                   BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed to get Descriptor!!!");            }
             } else
             if (aenBTDataMode == enBTDMRelease) {
                BTRCORELOG_DEBUG ("Freeing GATT Descriptor %s ", apBtGattPath);

                if (pDesc) {
                   memset (pDesc, 0, sizeof(stBTRGattDesc));
                   pChar->ui16NumberofGattDesc--;
                } else {
                   BTRCORELOG_WARN ("Gatt Descriptor %s not found", apBtGattPath);
                }
             }
          } else {
             BTRCORELOG_WARN ("Gatt Characteristic %s/char%04d not found...", gattPath, cId);  }
       } else {
          BTRCORELOG_WARN ("Gatt Service %s not found...", gattPath);
       }
    }

    return NULL;
}


