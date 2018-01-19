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
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

/* External Library Headers */
#include "btrCore_priv.h"

/* Local Headers */
#include "btrCore_le.h"
#include "btrCore_bt_ifce.h"


#define MAX_NUMBER_GATT_SERVICES 2
#define MAX_GATT_SERVICE_ID_ARRAY 4
#define MAX_GATT_CHAR_ARRAY 4
#define MAX_GATT_DESC_ARRAY 4
#define MAX_DESC_PROP_UUID_SIZE 9

/* GattDescriptor1 Properties */
typedef struct _stBTRGattDesc {
    unsigned short uni16NumberofGattDesc;                      /* Number of Gatt Descriptor */
    unsigned char  ui8GattDescPath[BTRCORE_MAX_STR_LEN];       /* Object Path */
    unsigned char  ui8Uuid[MAX_DESC_PROP_UUID_SIZE];           /* 128-bit service UUID */
    unsigned char  ui8Value[BTRCORE_STR_LEN];                  /* array{byte} Value [read-only, optional] */
}stBTRGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTRGattChar {
    unsigned short uni16NumberofGattChar;                      /* Number of Gatt Character */
    unsigned char  ui8GattCharPath[BTRCORE_MAX_STR_LEN];       /* Object Path */
    unsigned char  ui8Uuid[MAX_DESC_PROP_UUID_SIZE];           /* 128-bit service UUID */
    unsigned char  ui8Value[BTRCORE_STR_LEN];                  /* array{byte} Value [read-only, optional] */
    stBTRGattDesc  astBTRGattDesc[MAX_GATT_DESC_ARRAY];        /* Max of 4 Gatt Descriptor array */
}stBTRGattChar;


/* GattService Path and Properties */
typedef struct _stBTRGattService {
    unsigned short uni16NumberofGattServiceID;                 /* Number of Gatt Service ID */
    unsigned char  ui8GattServicePath[BTRCORE_MAX_STR_LEN];    /* Object Path */
    unsigned char  ui8Uuid[MAX_DESC_PROP_UUID_SIZE];           /* 128-bit service UUID */
    unsigned short ui16Primary;                                /* boolean Primary [read-only] [0/1] */
    stBTRGattChar astBTRGattChar[MAX_GATT_CHAR_ARRAY];         /* Max of 4 Gatt Charactristic array */
}stBTRGattService;


typedef struct _stBTRCoreGattProfile {
    stBTRGattService arstBTRGattService[MAX_NUMBER_GATT_SERVICES];
} stBTRCoreGattProfile;



#if 0
typedef struct _stBTRCoreLeGattService {
    short numberofgattservices;
    //array enum of service ids maximum 4
    stBTRGattServiceID gattSerID[MAX_GATT_SERVICE_ID];
    //array containing Primary & device per service {Properties of Gatt service} maximum 4
   stBTRGattChar[MAX_GATT_CHAR_ARRAY] stGattChar;
} stBTRCoreLeGattService;


typedef struct _stBTRCoreLeGattCharacteristic {
    enum of service id
    short numberofgattcharacteristics;
    array enum of characterisitics
    array containing Properties of the number of characteristic 
}

typedef struct _stBTRCoreLeGattDescriptor {
    enum of characteristic
    numberofgattdescriptor
    array enum of descriptors
    array containing Properties of the number of descriptors
}
#endif




typedef struct _stBTRCoreLeHdl {

    stBTRCoreGattProfile stBTRGattProfile;

} stBTRCoreLeHdl;



typedef struct _stBTRCoreLeStatusUserData {
    void*        apvLeUserData;
    const char*  apcLeDevAddress;
} stBTRCoreLeStatusUserData;

/* Static Function Prototypes */


/* Callbacks */
static const char* btrCore_LE_GattInfoCb (enBTOpIfceType enBtOpIfceType, const char* apBtGattPath, void* apUserData );



//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_LE_Init (
    tBTRCoreLeHdl*      phBTRCoreLe,
    void*               apBtConn,
    const char*         apBtAdapter
) {

    if (!apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }
   enBTRCoreRet            lenBTRCoreRet     = enBTRCoreSuccess;
   stBTRCoreLeHdl*    pstlhBTRCoreLe = NULL;
   BTRCORELOG_ERROR ("**********Inside ********\n");
   // pstlhBTRCoreLe = (stBTRCoreLeHdl*)malloc(sizeof(stBTRCoreLeHdl));
   // if (!pstlhBTRCoreLe)
   //     return enBTRCoreInitFailure;

   // memset(pstlhBTRCoreLe, 0, sizeof(stBTRCoreLeHdl));



    //if (!lBtAVMediaTransportPRet)
    //    lBTAVMediaPlayerPRet = BtrCore_BTRegisterMediaPlayerPathcB(apBtConn,
      BtrCore_BTRegisterLEGattInfoCb (apBtConn,
                                      apBtAdapter,
                                      &btrCore_LE_GattInfoCb,
                                      pstlhBTRCoreLe);
                                       
#if 0
    if (lenBTRCoreRet != enBTRCoreSuccess) {
        free(pstlhBTRCoreLe);
        pstlhBTRCoreLe = NULL;
    }

    *phBTRCoreLe  = (tBTRCoreLeHdl)pstlhBTRCoreLe;
#endif
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

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_LeDataPath (
    tBTRCoreLeHdl       hBTRCoreLe,
    void*               apBtConn,
    const char*         apBtDevAddr,
    int*                apDataPath,
    int*                apDataReadMTU,
    int*                apDataWriteMTU
) {
    stBTRCoreLeHdl*    pstlhBTRCoreLe = NULL;
    //int                     lBtAVMediaRet = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;

    if (!hBTRCoreLe || !apBtConn || !apBtDevAddr) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
    (void) pstlhBTRCoreLe;

    //if ((lBtAVMediaRet = BtrCore_BTGetProp(apBtConn, pstlhBTRCoreLe->pcAVMediaTransportPath, enBTMediaTransport, lunBtOpMedTProp, &ui16Delay)))
      //  lenBTRCoreRet = enBTRCoreFailure;


    return lenBTRCoreRet;
}


//Get Gatt info in GetProperty handling 
enBTRCoreRet
BTRCore_LE_GetGattProperty (
    tBTRCoreLeHdl       hBTRCoreLe,
    void*               apBtConn,
    const char*         apBtDevAddr,
    const char*         lePropertyKey,
    void*               lePropertyValue
) {
   // stBTRCoreLeHdl*         pstlhBTRCoreLe = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreLe || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    //pstlhBTRCoreLe = (stBTRCoreLeHdl*)hBTRCoreLe;
#if 0
    if (NULL == pstlhBTRCoreLe->pcAVMediaPlayerPath) {
       if (NULL == (pstlhBTRCoreLe->pcAVMediaPlayerPath = BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevAddr))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    if (BtrCore_BTGetMediaPlayerProperty(apBtConn, pstlhBTRCoreLe->pcAVMediaPlayerPath, mediaPropertyKey, mediaPropertyValue)) {
       BTRCORELOG_ERROR ("Failed to get Media Property : %s!!!",mediaPropertyKey);
       lenBTRCoreRet = enBTRCoreFailure;
    }
#endif
    return lenBTRCoreRet;
}


static const char*
btrCore_LE_GattInfoCb (
    enBTOpIfceType enBtOpIfceType,
    const char* apBtGattPath,
    void*       apUserData
) {
  //unsigned char uai8UUID[MAX_DESC_PROP_UUID_SIZE];

  //stBTRCoreLeHdl*    pstlhBTRCoreLe = NULL;
  BTRCORELOG_DEBUG("apBtGattPath : %s\n", apBtGattPath);
 // if (!BtrCore_BTGetProp(apBtConn, apBtGattPath,enBTGattService, enBtGattServiceProp, uai8UUID))
 //       lenBTRCoreRet = enBTRCoreFailure;
  return NULL;
}


