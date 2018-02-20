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

#include "btrCore_bt_ifce.h"


#define BTRCORE_MAX_NUMBER_GATT_SERVICES    10
#define BTRCORE_MAX_GATT_CHAR_ARRAY         6
#define BTRCORE_MAX_GATT_DESC_ARRAY         2
#define BTRCORE_MAX_UUID_SIZE               64
#define BTRCORE_GATT_CHAR_FLAGS             16
#define BTRCORE_GATT_DESC_FLAGS             8

#define BTRCORE_GATT_TILE_UUID_1            "feed"
#define BTRCORE_GATT_TILE_UUID_2            "feec"

/* GattDescriptor1 Properties */
typedef struct _stBTRGattDesc {
    char           dPath[BTRCORE_MAX_STR_LEN];                          /* Descriptor Path */
    char           descUuid[BTRCORE_MAX_UUID_SIZE];                     /* 128-bit service UUID */
    char           gattCharPath[BTRCORE_MAX_STR_LEN];                   /* Object Path */
}stBTRGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTRGattChar {
    char           cPath[BTRCORE_MAX_STR_LEN];                          /* Characteristic Path */
    char           charUuid[BTRCORE_MAX_UUID_SIZE];                     /* 128-bit service UUID */
    char           gattServicePath[BTRCORE_MAX_STR_LEN];                /* Object Path */
    stBTRGattDesc  astBTRGattDesc[BTRCORE_MAX_GATT_DESC_ARRAY];         /* Max of 4 Gatt Descriptor array */
    unsigned short ui16NumberofGattDesc;                                /* Number of Gatt Service ID */
}stBTRGattChar;


/* GattService Path and Properties */
typedef struct _stBTRGattService {
    tBTRCoreDevId  deviceID;
    char           sPath[BTRCORE_MAX_STR_LEN];                          /* Service Path */
    char           serviceUuid[BTRCORE_MAX_UUID_SIZE];                  /* 128-bit service UUID */
    char           gattDevicePath[BTRCORE_MAX_STR_LEN];                 /* Object Path */
    stBTRGattChar  astBTRGattChar[BTRCORE_MAX_GATT_CHAR_ARRAY];         /* Max of 4 Gatt Charactristic array */
    unsigned short ui16NumberofGattChar;                                /* Number of Gatt Charactristics */
}stBTRGattService;


typedef struct _stBTRCoreGattProfile {
    stBTRGattService    astBTRGattService[BTRCORE_MAX_NUMBER_GATT_SERVICES];
    unsigned short      ui16NumberofGattService;                        /* Number of Gatt Service ID */
} stBTRCoreGattProfile;





typedef struct _stBTRCoreLeHdl {

    stBTRCoreGattProfile stBTRGattProfile;


//    fpcBTRCoreLeDevStatusUpdate;
//    pvBtLeDevStatusUserData;

} stBTRCoreLeHdl;



/* Static Function Prototypes */
static stBTRGattService* btrCore_LE_FindGattService (stBTRCoreGattProfile  *pGattProfile, tBTRCoreDevId aBtrDeviceID, const char *pService);
static stBTRGattChar* btrCore_LE_FindGattCharacteristic (stBTRCoreGattProfile  *pGattProfile, tBTRCoreDevId aBtrDeviceID, const char *pChar);
static stBTRGattDesc* btrCore_LE_FindGattDescriptor (stBTRCoreGattProfile  *pGattProfile, tBTRCoreDevId aBtrDeviceID, const char *desc);

static char* btrCore_LE_GetGattCharacteristicUUID (tBTRCoreLeHdl  hBTRCoreLe, void* apvBtConn,  const char*  apcUuid);


static enBTRCoreRet btrCore_LE_GetDataPath (tBTRCoreLeHdl hBTRCoreLe, void* apBtConn, const char* apBtDevPath, const char* apBtLeUuid, char** rpBtLePath, enBTOpIfceType *renBTOpIfceType);



/* Callbacks */
static int btrCore_LE_GattInfoCb (enBTOpIfceType enBtOpIfceType, const char* apBtGattPath, enBTDeviceState aenBTDeviceState, void* apConnHdl, tBTRCoreDevId   aBtdevId, void* apUserData );


/* static function definitions */
static stBTRGattService*
btrCore_LE_FindGattService (
    stBTRCoreGattProfile   *pGattProfile,
    tBTRCoreDevId          aBtrDeviceID,
    const char             *pService
) {
    int i32LoopIdx = 0;
    stBTRGattService *pstService = NULL;
        if (pGattProfile) {
       for (; i32LoopIdx < pGattProfile->ui16NumberofGattService; i32LoopIdx++) {
           if (pGattProfile->astBTRGattService[i32LoopIdx].deviceID == aBtrDeviceID &&
               !strcmp(pGattProfile->astBTRGattService[i32LoopIdx].sPath, pService) ){
              pstService = &pGattProfile->astBTRGattService[i32LoopIdx];
              BTRCORELOG_DEBUG ("Gatt Service %s Found", pService);
              break;
           }
       }
    }
    return pstService;
}


static stBTRGattChar*
btrCore_LE_FindGattCharacteristic (
    stBTRCoreGattProfile   *pGattProfile,
    tBTRCoreDevId          aBtrDeviceID,
    const char             *pChar
) {
    int i32LoopIdx = 0;
    stBTRGattChar *pstChar = NULL;
    if (pGattProfile) {
       for (; i32LoopIdx < pGattProfile->ui16NumberofGattService; i32LoopIdx++) {
           if (pGattProfile->astBTRGattService[i32LoopIdx].deviceID == aBtrDeviceID) {
              int i32LoopIdx2  = 0;
              stBTRGattService *pstService = &pGattProfile->astBTRGattService[i32LoopIdx];
              for (; i32LoopIdx2 < pstService->ui16NumberofGattChar; i32LoopIdx2++) {
                  if (!strcmp(pstService->astBTRGattChar[i32LoopIdx2].cPath, pChar)) {
                     pstChar = &pstService->astBTRGattChar[i32LoopIdx2];
                     BTRCORELOG_DEBUG ("Gatt Char %s Found", pChar);
                     break;
                  }
              }
           }
       }
    }
    return pstChar;
}


static stBTRGattDesc*
btrCore_LE_FindGattDescriptor (
    stBTRCoreGattProfile   *pGattProfile,
    tBTRCoreDevId          aBtrDeviceID,
    const char             *pDesc
) {
    int i32LoopIdx = 0;
    stBTRGattDesc *pstDesc = NULL;
    if (pGattProfile) {
       for (; i32LoopIdx < pGattProfile->ui16NumberofGattService; i32LoopIdx++) {
           if (pGattProfile->astBTRGattService[i32LoopIdx].deviceID == aBtrDeviceID) {
              int i32LoopIdx2  = 0;
              stBTRGattService *pstService = &pGattProfile->astBTRGattService[i32LoopIdx];
              for (; i32LoopIdx2 < pstService->ui16NumberofGattChar; i32LoopIdx2++) {
                  int i32LoopIdx3  = 0;
                  stBTRGattChar *pstChar = &pstService->astBTRGattChar[i32LoopIdx2];
                  for (; i32LoopIdx3 < pstChar->ui16NumberofGattDesc; i32LoopIdx3++) {
                      if (!strcmp(pstChar->astBTRGattDesc[i32LoopIdx3].dPath, pDesc)) {
                         pstDesc = &pstChar->astBTRGattDesc[i32LoopIdx3];
                         BTRCORELOG_DEBUG ("Gatt Descriptor %s Found", pDesc);
                         break;
                      }
                  }
              }
           }
       }
    }
    return pstDesc; 
}


static char*
btrCore_LE_GetGattCharacteristicUUID (
      tBTRCoreLeHdl       hBTRCoreLe,
      void*               apvBtConn,
      const char*         apcUuid
) {
    unBTOpIfceProp lunBTOpIfceProp;
    char apvBtPropValue[8][BT_MAX_UUID_STR_LEN];
    lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
    BOOLEAN readValue=FALSE;

    stBTRCoreLeHdl*      lpstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    stBTRCoreGattProfile   *pGattProfile  = &lpstlhBTRCoreLe->stBTRGattProfile;
    int i32LoopIdx =0;

    if (pGattProfile) {
       for (; i32LoopIdx < pGattProfile->ui16NumberofGattService; i32LoopIdx++) {
           if (strstr(pGattProfile->astBTRGattService[i32LoopIdx].serviceUuid, apcUuid)) {
              int i  = 0;
              stBTRGattService *pstService = &pGattProfile->astBTRGattService[i32LoopIdx];
              for (; i < pstService->ui16NumberofGattChar; i++) {
                  if (pstService->astBTRGattChar[i].charUuid) {
                     if (!BtrCore_BTGetProp (apvBtConn, pstService->astBTRGattChar[i].cPath, enBTGattCharacteristic, lunBTOpIfceProp, (void*)&apvBtPropValue)) {
                        unsigned char u8idx = 0;  
                        for (; u8idx < 8; u8idx++) {
                            if (!strcmp(apvBtPropValue[u8idx], "read")) {
                               readValue = TRUE;
                               break;
                             }
                        }
                     } else {
                        BTRCORELOG_ERROR ("Failed to get Gatt Char property - Flags!!!");
                     }

                     if (readValue) {
                        return pstService->astBTRGattChar[i].charUuid;
                     }
                  }
              }
              break;
           }
        }
     }
     return NULL;
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
    char**              rpBtLePath,
    enBTOpIfceType*     renBTOpIfceType
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
    char* retLeDataPath           = NULL;
    stBTRGattService *pService    = NULL;

    for (; ui16SLoopindex < pGattProfile->ui16NumberofGattService; ui16SLoopindex++) {
        pService  = &pGattProfile->astBTRGattService[ui16SLoopindex];
        if (!strcmp(apBtDevPath, pService->gattDevicePath)) {

           if (strstr(pService->serviceUuid, apBtLeUuid)) {
              retLeDataPath = pService->sPath;
              *renBTOpIfceType = enBTGattService;
              BTRCORELOG_DEBUG ("UUID matched Service : %s", pService->sPath);
              break;
           }
           else {
              if (pService->ui16NumberofGattChar == 0) {
                 continue;  /* Service has no Char to loop through */
              }
              unsigned short ui16CLoopindex = 0;
              stBTRGattChar *pChar          = NULL;

              for (; ui16CLoopindex < pService->ui16NumberofGattChar; ui16CLoopindex++) {
                  pChar = &pService->astBTRGattChar[ui16CLoopindex];
                  if (!strcmp(apBtLeUuid, pChar->charUuid)) {
                     retLeDataPath = pChar->cPath;
                     *renBTOpIfceType = enBTGattCharacteristic;
                     BTRCORELOG_DEBUG ("UUID matched Characteristic : %s", pChar->cPath);
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
                            *renBTOpIfceType = enBTGattDescriptor;
                            BTRCORELOG_DEBUG ("UUID matched Descriptor : %s", pDesc->dPath);
                            break; // desc loop
                         }
                     }
                     if (ui16DLoopindex != pChar->ui16NumberofGattDesc) {
                        break; // char loop
                     }
                 }
              } else {
                 break;  // service loop
              }
           }
        }
    }
    if (ui16SLoopindex == pGattProfile->ui16NumberofGattService) {
       BTRCORELOG_ERROR ("Service Not Found for Dev : %s!!!", apBtDevPath);
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
    enBTOpIfceType lenBTOpIfceType = enBTUnknown;
    unBTOpIfceProp lunBTOpIfceProp;

    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, apcBtDevPath, apcBtUuid, &lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath) {
       BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!");
       return enBTRCoreFailure;
    }
     
    switch (aenBTRCoreLEGattProp) {

    case enBTRCoreLEGPropUUID:
        if (lenBTOpIfceType == enBTGattService) {
           lunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
        } else if (lenBTOpIfceType == enBTGattCharacteristic) {
           lunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;
        } else if (lenBTOpIfceType == enBTGattDescriptor) {
           lunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;
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
    case enBTRCoreLEGPropCharList:
        strncpy((char*)apvBtPropValue, btrCore_LE_GetGattCharacteristicUUID(hBTRCoreLe, apvBtConn, apcBtUuid), BTRCORE_MAX_STR_LEN-1);
        return enBTRCoreSuccess;
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
BtrCore_LE_PerformGattOp (
    tBTRCoreLeHdl      hBTRCoreLe,
    void*              apvBtConn,
    const char*        apcBtDevPath,
    const char*        apcBtUuid,
    enBTRCoreLEGattOp  aenBTRCoreLEGattOp,
    void*              rpLeOpRes
) {

    if (!hBTRCoreLe || !apvBtConn || !apcBtDevPath || !apcBtUuid) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }
  
    char* lpcBtLePath = NULL;
    enBTOpIfceType lenBTOpIfceType = enBTUnknown;
    enBTLeGattOp lenBTLeGattOp = enBTLeGattOpUnknown;

    if (btrCore_LE_GetDataPath(hBTRCoreLe, apvBtConn, apcBtDevPath, apcBtUuid, &lpcBtLePath, &lenBTOpIfceType) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get LE Path for UUID %s !!!", apcBtUuid);
       return enBTRCoreFailure;
    }
    
    if (!lpcBtLePath) {
       BTRCORELOG_ERROR ("Obtained LE Path is NULL!!!");
       return enBTRCoreFailure;
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
                                                                           (void*)apcBtDevPath,  // for now
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
    enBTOpIfceType  aenBtOpIfceType,
    const char*     apBtGattPath,
    enBTDeviceState aenBTDeviceState,
    void*           apConnHdl,
    tBTRCoreDevId   aBtdevId,
    void*           apUserData
) {
    if (!apBtGattPath || !apUserData) {
       BTRCORELOG_ERROR("Invalid arguments!!!");
       return -1;
    }
    stBTRCoreLeHdl*      lpstlhBTRCoreLe = (stBTRCoreLeHdl*)apUserData;
    stBTRCoreGattProfile *pGattProfile   = &(lpstlhBTRCoreLe->stBTRGattProfile);
    BTRCORELOG_DEBUG("apBtGattPath : %s\n", apBtGattPath);

    if (aenBTDeviceState == enBTDevStFound) {
       char lBtUuid[BT_MAX_STR_LEN]="\0";
       unBTOpIfceProp aunBTOpIfceProp;

       if (aenBtOpIfceType == enBTGattService) {
          BTRCORELOG_DEBUG ("Storing GATT Service Info...\n");
          char lBtDevPath[BT_MAX_STR_LEN] = "\0";
          unsigned char retUuid = -1, retDPath = -1;

          stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, aBtdevId, apBtGattPath);

          aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropUUID;
          retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

          aunBTOpIfceProp.enBtGattServiceProp = enBTGattSPropDevice;
          retDPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtDevPath);

          if (!retUuid && !retDPath) {
             if (pGattProfile->ui16NumberofGattService < BTRCORE_MAX_NUMBER_GATT_SERVICES) {
                if (!pService && (strstr(lBtUuid, BTRCORE_GATT_TILE_UUID_1) || strstr(lBtUuid, BTRCORE_GATT_TILE_UUID_2))) {  //TODO api which checks for the allowed UUIDs
                   pService = &pGattProfile->astBTRGattService[pGattProfile->ui16NumberofGattService];
                   pService->deviceID = aBtdevId;
                   strncpy(pService->serviceUuid, lBtUuid, BTRCORE_MAX_UUID_SIZE - 1);
                   strncpy(pService->sPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                   strncpy(pService->gattDevicePath, lBtDevPath, BTRCORE_MAX_STR_LEN - 1);
                   pGattProfile->ui16NumberofGattService++;
                   BTRCORELOG_DEBUG ("Added Service %s Successfully.", lBtUuid);
                } else {
                   BTRCORELOG_WARN ("Gatt Service %s already exists/or unknown UUID : %s...", apBtGattPath, lBtUuid); }
             } else {
                BTRCORELOG_WARN ("BTRCORE_MAX_NUMBER_GATT_SERVICES Added. Couldn't add anymore...");               }
          } else {
             BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retDPath : %d",retUuid, retDPath); }
       } else 
       if (aenBtOpIfceType == enBTGattCharacteristic) {
          BTRCORELOG_DEBUG ("Storing GATT Characteristic Info...\n");
          char lBtSerivcePath[BT_MAX_STR_LEN]  = "\0";
          unsigned char retUuid = -1, retSPath = -1;

          aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropUUID;
          retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

          aunBTOpIfceProp.enBtGattCharProp = enBTGattCPropService;
          retSPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtSerivcePath);

          if (!retUuid && !retSPath) {
             stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, aBtdevId, lBtSerivcePath);
             stBTRGattChar    *pChar = btrCore_LE_FindGattCharacteristic(pGattProfile, aBtdevId, apBtGattPath);

             if (pService) {
                if (pService->ui16NumberofGattChar < BTRCORE_MAX_GATT_CHAR_ARRAY) {
                   if (!pChar) {
                      pChar = &pService->astBTRGattChar[pService->ui16NumberofGattChar];
                      strncpy(pChar->charUuid, lBtUuid, BTRCORE_MAX_UUID_SIZE - 1 );
                      strncpy(pChar->cPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                      strncpy(pChar->gattServicePath, lBtSerivcePath, BTRCORE_MAX_STR_LEN - 1);
                      pService->ui16NumberofGattChar++;
                      BTRCORELOG_DEBUG ("Added Characteristic %s Successfully.", lBtUuid);
                   } else {
                      BTRCORELOG_WARN ("Gatt Characteristic %s already exists...", apBtGattPath);          }
                } else {
                   BTRCORELOG_WARN ("BTRCORE_MAX_GATT_CHAR_ARRAY Addedd. Couldn't add anymore...");                }
             } else {
                BTRCORELOG_WARN ("Gatt Service %s not found...", lBtSerivcePath);                          }
          } else {
             BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retSPath : %d",retUuid, retSPath); }
       } else
       if (aenBtOpIfceType == enBTGattDescriptor) {
          BTRCORELOG_DEBUG ("Storing GATT Descriptor Info...\n");
          char lBtCharPath[BT_MAX_STR_LEN]  = "\0";
          unsigned char retUuid = -1, retCPath = -1;

          aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropUUID;
          retUuid  = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtUuid);

          aunBTOpIfceProp.enBtGattDescProp = enBTGattDPropCharacteristic;
          retCPath = BtrCore_BTGetProp(apConnHdl, apBtGattPath, aenBtOpIfceType, aunBTOpIfceProp, (void*)&lBtCharPath);

          if (!retUuid && !retCPath) {
             stBTRGattChar  *pChar = btrCore_LE_FindGattCharacteristic(pGattProfile, aBtdevId, lBtCharPath);
             stBTRGattDesc  *pDesc = btrCore_LE_FindGattDescriptor(pGattProfile, aBtdevId, apBtGattPath);
             if (pChar) {
                if (pChar->ui16NumberofGattDesc < BTRCORE_MAX_GATT_DESC_ARRAY) {
                   if (!pDesc) {
                      pDesc = &(pChar->astBTRGattDesc[pChar->ui16NumberofGattDesc]);
                      strncpy (pDesc->descUuid, lBtUuid, BTRCORE_MAX_UUID_SIZE - 1);
                      strncpy(pDesc->dPath, apBtGattPath, BTRCORE_MAX_STR_LEN - 1);
                      strncpy(pDesc->gattCharPath, lBtCharPath, BTRCORE_MAX_STR_LEN - 1);
                      pChar->ui16NumberofGattDesc++;
                      BTRCORELOG_DEBUG ("Added Gatt Descriptor %s Successfully.", lBtUuid);
                   } else {
                      BTRCORELOG_WARN ("Gatt Descriptor %s already exists...", apBtGattPath);     }
                } else {
                   BTRCORELOG_WARN ("BTRCORE_MAX_GATT_DESC_ARRAY Added. Couldn't add anymore...");        }
             } else {
                BTRCORELOG_WARN ("Gatt Characteristic not found for Desc %s", apBtGattPath);      }
          } else {
             BTRCORELOG_ERROR ("BtrCore_BTGetProp Failed retUuid : %d | retCPath : %d !!!", retUuid, retCPath);
          }
       }
    } 
    else
    if (aenBTDeviceState == enBTDevStLost) {
       if (aenBtOpIfceType == enBTGattService) {
          BTRCORELOG_DEBUG ("Freeing Gatt Service %s ", apBtGattPath);
          stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, aBtdevId, apBtGattPath);

          if (pGattProfile && pService) {
             memset (pService, 0, sizeof(stBTRGattService));
             pGattProfile->ui16NumberofGattService--;
          } else {
             BTRCORELOG_WARN ("Couldn't Free...pGattProfile %p | pService : %p", pGattProfile, pService);
          }
       } else
       if (aenBtOpIfceType == enBTGattCharacteristic) {
          BTRCORELOG_DEBUG ("Freeing GATT Characteristic %s ", apBtGattPath);
          stBTRGattChar  *pChar = btrCore_LE_FindGattCharacteristic(pGattProfile, aBtdevId, apBtGattPath);
          stBTRGattService *pService = btrCore_LE_FindGattService(pGattProfile, aBtdevId, pChar->gattServicePath);

          if (pChar) {
             memset (pChar, 0, sizeof(stBTRGattChar));
             pService->ui16NumberofGattChar--;
          } else {
             BTRCORELOG_WARN ("Couldn't Free...pService : %p | pChar : %p", pService, pChar);
          }
       } else
       if (aenBtOpIfceType == enBTGattDescriptor) {
          BTRCORELOG_DEBUG ("Freeing GATT Descriptor %s ", apBtGattPath);
          stBTRGattDesc  *pDesc = btrCore_LE_FindGattDescriptor(pGattProfile, aBtdevId, apBtGattPath);
          stBTRGattChar  *pChar = btrCore_LE_FindGattCharacteristic(pGattProfile, aBtdevId, pDesc->gattCharPath);

          if (pDesc) {
             memset (pDesc, 0, sizeof(stBTRGattDesc));
             pChar->ui16NumberofGattDesc--;
          } else {
             BTRCORELOG_WARN ("Couldn't Free...pChar : %p | pDesc : %p", pChar, pDesc);
          }
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
          char arrayOfStr[16][BT_MAX_UUID_STR_LEN];
          char pIface[BT_MAX_STR_LEN] = "\0";
          BOOLEAN readValue = FALSE;
          unBTOpIfceProp aunBtOpIfceProp;

          strncpy(pIface, apBtGattPath, (strlen(apBtGattPath)<BT_MAX_STR_LEN)? strlen(apBtGattPath):(BT_MAX_STR_LEN-1));
          aunBtOpIfceProp.enBtGattCharProp = enBTGattCPropFlags;
          if (!BtrCore_BTGetProp (apConnHdl, pIface, enBTGattCharacteristic, aunBtOpIfceProp, (void*)&arrayOfStr)) {
             unsigned char u8idx = 0;
             for (; u8idx < 16; u8idx++) {
                 if (!strcmp(arrayOfStr[u8idx], "read")) {
                    readValue = TRUE;
                    break;
                 }
             }
          } else {
             BTRCORELOG_ERROR ("Failed to get Gatt Char property - Flags!!!");
          }
          if (readValue) {
             unsigned char value[BT_MAX_UUID_STR_LEN] = "\0";
             char tileID[BT_MAX_STR_LEN] = "\0";
             aunBtOpIfceProp.enBtGattCharProp = enBTGattCPropValue;

             if (!BtrCore_BTGetProp (apConnHdl, pIface, enBTGattCharacteristic, aunBtOpIfceProp, (void*)&value)) {
                BTRCORELOG_ERROR ("<<<<<TILE ID>>>>>");
                unsigned char u8idx=0, u8idx2=0;
                while (value[u8idx]) {
                      BTRCORELOG_ERROR("%x%x ", value[u8idx] >> 4, value[u8idx] & 0x0F);
                      tileID[u8idx2++] = value[u8idx] >> 4;
                      tileID[u8idx2++] = value[u8idx] &  0x0F ;
                      u8idx++;
                 }
                 tileID[u8idx2] = '\0';
                 BTRCORELOG_ERROR ("Obtained TileID for ReadValue : %s", tileID);
                 // callback
             } else {
                 BTRCORELOG_ERROR ("Failed to get Gatt Char property - Value!!!");
             }
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


