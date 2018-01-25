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
//btrCore.c

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for getpid
#include <sched.h>      //for StopDiscovery test
#include <string.h>     //for strcnp
#include <errno.h>      //for error numbers

#include <glib.h>

#include "btrCore.h"
#include "btrCore_avMedia.h"

#include "btrCore_le.h"
#include "btrCore_bt_ifce.h"

#include "btrCore_service.h"

#include "btrCore_priv.h"

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

/* Local types */
//TODO: Move to a private header
typedef struct _stBTRCoreDevStateInfo {
    enBTRCoreDeviceState    eDevicePrevState;
    enBTRCoreDeviceState    eDeviceCurrState;
} stBTRCoreDevStateInfo;


typedef struct _stBTRCoreHdl {

    tBTRCoreAVMediaHdl              avMediaHdl;
    tBTRCoreLeHdl                   leHdl;

    void*                           connHdl;
    char*                           agentPath;

    unsigned int                    numOfAdapters;
    char*                           adapterPath[BTRCORE_MAX_NUM_BT_ADAPTERS];
    char*                           adapterAddr[BTRCORE_MAX_NUM_BT_ADAPTERS];

    char*                           curAdapterPath;
    char*                           curAdapterAddr;

    unsigned int                    numOfScannedDevices;
    stBTRCoreBTDevice               stScannedDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];
    stBTRCoreDevStateInfo           stScannedDevStInfoArr[BTRCORE_MAX_NUM_BT_DEVICES];

    unsigned int                    numOfPairedDevices;
    stBTRCoreBTDevice               stKnownDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];
    stBTRCoreDevStateInfo           stKnownDevStInfoArr[BTRCORE_MAX_NUM_BT_DEVICES];

    stBTRCoreBTDevice               stFoundDevice;

    stBTRCoreDevStatusCBInfo        stDevStatusCbInfo;

    stBTRCoreMediaStatusCBInfo      stMediaStatusCbInfo;

    stBTRCoreConnCBInfo             stConnCbInfo;

    fPtr_BTRCore_DeviceDiscoveryCb  fpcBBTRCoreDeviceDiscovery;
    fPtr_BTRCore_StatusCb           fpcBBTRCoreStatus;
    fPtr_BTRCore_MediaStatusCb      fpcBBTRCoreMediaStatus;
    fPtr_BTRCore_ConnIntimCb        fpcBBTRCoreConnIntim; 
    fPtr_BTRCore_ConnAuthCb         fpcBBTRCoreConnAuth; 

    void*                           pvcBDevDiscUserData;
    void*                           pvcBStatusUserData;
    void*                           pvcBMediaStatusUserData;
    void*                           pvcBConnIntimUserData;
    void*                           pvcBConnAuthUserData;

    GThread*                        dispatchThread;
    GMutex                          dispatchMutex;
    BOOLEAN                         dispatchThreadQuit;

} stBTRCoreHdl;


/* Static Function Prototypes */
static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevId btrCore_GenerateUniqueDeviceID (const char* apcDeviceAddress);
static enBTRCoreDeviceClass btrCore_MapClassIDtoDeviceType(unsigned int classID);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_SetScannedDeviceInfo (stBTRCoreHdl* apsthBTRCore); 
static const char* btrCore_GetScannedDeviceName (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_AddDeviceToKnownDevicesArr (stBTRCoreHdl* apsthBTRCore, stBTDeviceInfo* apstBTDeviceInfo);
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static void btrCore_MapKnownDeviceListFromPairedDeviceInfo (stBTRCoreBTDevice* knownDevicesArr, stBTPairedDeviceInfo* pairedDeviceInfo);
static const char* btrCore_GetKnownDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceName (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceMac (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_ShowSignalStrength (short strength);
static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut);
static enBTRCoreDeviceState btrCore_BTParseDeviceConnectionState (const char* pcStateValue);

/* Local Op Threads Prototypes */
void* DoDispatch (void* ptr); 

/* Incoming Callbacks Prototypes */
static int btrCore_BTDeviceStatusUpdateCb (enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo,  void* apUserData);
static int btrCore_BTDeviceConnectionIntimationCb (enBTDeviceType  aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, unsigned int aui32devPassKey, void* apUserData);
static int btrCore_BTDeviceAuthenticationCb (enBTDeviceType  aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);

static enBTRCoreRet btrCore_BTMediaStatusUpdateCb (void* apMediaStreamStatus, const char*  apBtdevAddr, void* apUserCbData);


/* Static Function Definition */
static void
btrCore_InitDataSt (
    stBTRCoreHdl*   apsthBTRCore
) {
    int i;

    /* Current Adapter */
    apsthBTRCore->curAdapterAddr = (char*)malloc(sizeof(char) * BD_NAME_LEN);
    memset(apsthBTRCore->curAdapterAddr, '\0', sizeof(char) * BD_NAME_LEN);

    /* Adapters */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_ADAPTERS; i++) {
        apsthBTRCore->adapterPath[i] = (char*)malloc(sizeof(char) * BD_NAME_LEN);
        memset(apsthBTRCore->adapterPath[i], '\0', sizeof(char) * BD_NAME_LEN);

        apsthBTRCore->adapterAddr[i] = (char*)malloc(sizeof(char) * BD_NAME_LEN);
        memset(apsthBTRCore->adapterAddr[i], '\0', sizeof(char) * BD_NAME_LEN);
    }

    /* Scanned Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stScannedDevicesArr[i].tDeviceId = 0;
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceName, '\0', sizeof(BD_NAME));
        apsthBTRCore->stScannedDevicesArr[i].i32RSSI = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].bFound = FALSE;

        apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    apsthBTRCore->numOfScannedDevices = 0;
    apsthBTRCore->numOfPairedDevices  = 0;

    /* Found Device */
    memset (apsthBTRCore->stFoundDevice.pcDeviceAddress, '\0', sizeof(BD_NAME));
    memset (apsthBTRCore->stFoundDevice.pcDeviceName, '\0', sizeof(BD_NAME));
    apsthBTRCore->stFoundDevice.i32RSSI = INT_MIN;
    apsthBTRCore->stFoundDevice.bFound = FALSE;

    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stKnownDevicesArr[i].tDeviceId = 0;
        apsthBTRCore->stKnownDevicesArr[i].bDeviceConnected = 0;
        memset (apsthBTRCore->stKnownDevicesArr[i].pcDevicePath, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stKnownDevicesArr[i].pcDeviceName, '\0', sizeof(BD_NAME));
        apsthBTRCore->stKnownDevicesArr[i].i32RSSI = INT_MIN;
        apsthBTRCore->stKnownDevicesArr[i].bFound = FALSE;

        apsthBTRCore->stKnownDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stKnownDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    /* Callback Info */
    apsthBTRCore->stDevStatusCbInfo.eDevicePrevState = enBTRCoreDevStInitialized;
    apsthBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStInitialized;


    memset(&apsthBTRCore->stConnCbInfo, 0, sizeof(stBTRCoreConnCBInfo));

    apsthBTRCore->fpcBBTRCoreDeviceDiscovery= NULL;
    apsthBTRCore->fpcBBTRCoreStatus         = NULL;
    apsthBTRCore->fpcBBTRCoreMediaStatus    = NULL;
    apsthBTRCore->fpcBBTRCoreConnIntim      = NULL;
    apsthBTRCore->fpcBBTRCoreConnAuth       = NULL;

    apsthBTRCore->pvcBDevDiscUserData       = NULL;
    apsthBTRCore->pvcBStatusUserData        = NULL;
    apsthBTRCore->pvcBMediaStatusUserData   = NULL;
    apsthBTRCore->pvcBConnIntimUserData     = NULL;
    apsthBTRCore->pvcBConnAuthUserData      = NULL;
    /* Always safer to initialze Global variables, init if any left or added */
}


static tBTRCoreDevId
btrCore_GenerateUniqueDeviceID (
    const char* apcDeviceAddress
) {
    tBTRCoreDevId   lBTRCoreDevId = 0;
    char            lcDevHdlArr[13] = {'\0'};

    // MAC Address Format 
    // AA:BB:CC:DD:EE:FF\0
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

static enBTRCoreDeviceClass
btrCore_MapClassIDtoDeviceType (
    unsigned int classID
) {
    enBTRCoreDeviceClass rc = enBTRCore_DC_Unknown;
    BTRCORELOG_DEBUG ("classID = 0x%x\n", classID);

    if ((classID & 0x100u) || (classID & 0x200u) || (classID & 0x400u)) {
        unsigned int ui32DevClassID = classID & 0xFFFu;
        BTRCORELOG_DEBUG ("ui32DevClassID = 0x%x\n", ui32DevClassID);

        if (ui32DevClassID == enBTRCore_DC_Tablet) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Tablet\n");
            rc = enBTRCore_DC_Tablet;
        }
        else if (ui32DevClassID == enBTRCore_DC_SmartPhone) {
            BTRCORELOG_INFO ("enBTRCore_DC_SmartPhone\n");
            rc = enBTRCore_DC_SmartPhone;
        }
        else if (ui32DevClassID == enBTRCore_DC_WearableHeadset) {
            BTRCORELOG_INFO ("enBTRCore_DC_WearableHeadset\n");
            rc = enBTRCore_DC_WearableHeadset;
        }
        else if (ui32DevClassID == enBTRCore_DC_Handsfree) {
            BTRCORELOG_INFO ("enBTRCore_DC_Handsfree\n");
            rc = enBTRCore_DC_Handsfree;
        }
        else if (ui32DevClassID == enBTRCore_DC_Reserved) {
            BTRCORELOG_INFO ("enBTRCore_DC_Reserved\n");
            rc = enBTRCore_DC_Reserved;
        }
        else if (ui32DevClassID == enBTRCore_DC_Microphone) {
            BTRCORELOG_INFO ("enBTRCore_DC_Microphone\n");
            rc = enBTRCore_DC_Microphone;
        }
        else if (ui32DevClassID == enBTRCore_DC_Loudspeaker) {
            BTRCORELOG_INFO ("enBTRCore_DC_Loudspeaker\n");
            rc = enBTRCore_DC_Loudspeaker;
        }
        else if (ui32DevClassID == enBTRCore_DC_Headphones) {
            BTRCORELOG_INFO ("enBTRCore_DC_Headphones\n");
            rc = enBTRCore_DC_Headphones;
        }
        else if (ui32DevClassID == enBTRCore_DC_PortableAudio) {
            BTRCORELOG_INFO ("enBTRCore_DC_PortableAudio\n");
            rc = enBTRCore_DC_PortableAudio;
        }
        else if (ui32DevClassID == enBTRCore_DC_CarAudio) {
            BTRCORELOG_INFO ("enBTRCore_DC_CarAudio\n");
            rc = enBTRCore_DC_CarAudio;
        }
        else if (ui32DevClassID == enBTRCore_DC_STB) {
            BTRCORELOG_INFO ("enBTRCore_DC_STB\n");
            rc = enBTRCore_DC_STB;
        }
        else if (ui32DevClassID == enBTRCore_DC_HIFIAudioDevice) {
            BTRCORELOG_INFO ("enBTRCore_DC_HIFIAudioDevice\n");
            rc = enBTRCore_DC_HIFIAudioDevice;
        }
        else if (ui32DevClassID == enBTRCore_DC_VCR) {
            BTRCORELOG_INFO ("enBTRCore_DC_VCR\n");
            rc = enBTRCore_DC_VCR;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoCamera) {
            BTRCORELOG_INFO ("enBTRCore_DC_VideoCamera\n");
            rc = enBTRCore_DC_VideoCamera;
        }
        else if (ui32DevClassID == enBTRCore_DC_Camcoder) {
            BTRCORELOG_INFO ("enBTRCore_DC_Camcoder\n");
            rc = enBTRCore_DC_Camcoder;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoMonitor) {
            BTRCORELOG_INFO ("enBTRCore_DC_VideoMonitor\n");
            rc = enBTRCore_DC_VideoMonitor;
        }
        else if (ui32DevClassID == enBTRCore_DC_TV) {
            BTRCORELOG_INFO ("enBTRCore_DC_TV\n");
            rc = enBTRCore_DC_TV;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoConference) {
            BTRCORELOG_INFO ("enBTRCore_DC_VideoConference\n");
            rc = enBTRCore_DC_TV;
        }
    }

    return rc;
}

static void
btrCore_ClearScannedDevicesList (
    stBTRCoreHdl* apsthBTRCore
) {
    int i;

    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stScannedDevicesArr[i].tDeviceId = 0;
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceName, '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].pcDeviceName));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress,  '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress));
        apsthBTRCore->stScannedDevicesArr[i].i32RSSI = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].bFound = FALSE;

        apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }
    apsthBTRCore->numOfScannedDevices = 0;
}


static void
btrCore_SetScannedDeviceInfo (
    stBTRCoreHdl*   apsthBTRCore
) {
    int i;

    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (!apsthBTRCore->stScannedDevicesArr[i].bFound) {
            //BTRCORELOG_ERROR ("adding %s at location %d\n",apsthBTRCore->stFoundDevice.device_address,i);
            apsthBTRCore->stScannedDevicesArr[i].bFound = TRUE; //mark the record as found
            strcpy(apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress, apsthBTRCore->stFoundDevice.pcDeviceAddress);
            strcpy(apsthBTRCore->stScannedDevicesArr[i].pcDeviceName, apsthBTRCore->stFoundDevice.pcDeviceName);
            strcpy(apsthBTRCore->stScannedDevicesArr[i].pcDevicePath, apsthBTRCore->stFoundDevice.pcDevicePath);
            apsthBTRCore->stScannedDevicesArr[i].i32RSSI = apsthBTRCore->stFoundDevice.i32RSSI;
            apsthBTRCore->stScannedDevicesArr[i].ui32VendorId = apsthBTRCore->stFoundDevice.ui32VendorId;
            apsthBTRCore->stScannedDevicesArr[i].enDeviceType = apsthBTRCore->stFoundDevice.enDeviceType;
            apsthBTRCore->stScannedDevicesArr[i].tDeviceId = apsthBTRCore->stFoundDevice.tDeviceId;

            /* Copy the profile supports */
            memcpy (&apsthBTRCore->stScannedDevicesArr[i].stDeviceProfile, &apsthBTRCore->stFoundDevice.stDeviceProfile, sizeof(stBTRCoreSupportedServiceList));

            apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState;
            apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStFound;

            apsthBTRCore->numOfScannedDevices++;
            break;
        }
    }
}


static const char*
btrCore_GetScannedDeviceAddress (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int loop = 0;

    if ((!aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfScannedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfScannedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stScannedDevicesArr[loop].tDeviceId)
                return apsthBTRCore->stScannedDevicesArr[loop].pcDeviceAddress;
        }
    }

    return NULL;
}

static const char*
btrCore_GetScannedDeviceName (
   stBTRCoreHdl*    apsthBTRCore,
   tBTRCoreDevId    aBTRCoreDevId
) {
    int loop = 0;

   if ((!aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfScannedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfScannedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stScannedDevicesArr[loop].tDeviceId)
             return apsthBTRCore->stScannedDevicesArr[loop].pcDeviceName;
        }

    }

    return NULL;
}


static void
btrCore_AddDeviceToKnownDevicesArr (
    stBTRCoreHdl*   apsthBTRCore,
    stBTDeviceInfo* apstBTDeviceInfo
) {
    tBTRCoreDevId   ltDeviceId;
    int             i32LoopIdx      = 0;
    int             i32KnownDevIdx  = -1;
    int             i32ScannedDevIdx= -1;

    if (!apsthBTRCore || !apstBTDeviceInfo)
        return;

    ltDeviceId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

    for (i32LoopIdx = 0; i32LoopIdx < apsthBTRCore->numOfPairedDevices; i32LoopIdx++) {
        if (ltDeviceId == apsthBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
            i32KnownDevIdx = i32LoopIdx;
            break;
        }
    }

    if (i32KnownDevIdx != -1) {
        BTRCORELOG_DEBUG ("Already Present in stKnownDevicesArr - DevID = %lld\n", ltDeviceId);
        return;
    }

    if (apsthBTRCore->numOfPairedDevices >= BTRCORE_MAX_NUM_BT_DEVICES) {
        BTRCORELOG_ERROR ("No Space in stKnownDevicesArr - DevID = %lld\n", ltDeviceId);
        return;
    }

    for (i32LoopIdx = 0; i32LoopIdx < apsthBTRCore->numOfScannedDevices; i32LoopIdx++) {
        if (ltDeviceId == apsthBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
            i32ScannedDevIdx = i32LoopIdx;
            break;
        }
    }

    if (i32ScannedDevIdx == -1) {
        BTRCORELOG_DEBUG ("Not Present in stScannedDevicesArr - DevID = %lld\n", ltDeviceId);
        return;
    }


    i32KnownDevIdx = apsthBTRCore->numOfPairedDevices;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId          = apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].tDeviceId;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType       = apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].enDeviceType;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].i32RSSI            = apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].i32RSSI;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].ui32VendorId       = apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].ui32VendorId;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].bFound             = apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].bFound;
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected   = apstBTDeviceInfo->bConnected;

    strncpy(apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceName,    apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].pcDeviceName,    BD_NAME_LEN - 1);
    strncpy(apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceAddress, apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].pcDeviceAddress, BD_NAME_LEN - 1);
    strncpy(apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDevicePath,    apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].pcDevicePath,    BD_NAME_LEN - 1);

    memcpy (&apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].stDeviceProfile, &apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx].stDeviceProfile, sizeof(stBTRCoreSupportedServiceList));

    apsthBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = apsthBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState;
    apsthBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = apsthBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;

    BTRCORELOG_TRACE ("Added in stKnownDevicesArr - DevID = %lld\n", ltDeviceId);
}

static void
btrCore_MapKnownDeviceListFromPairedDeviceInfo (
    stBTRCoreBTDevice*      knownDevicesArr,
    stBTPairedDeviceInfo*   pairedDeviceInfo
) {
    unsigned char i_idx = 0;
    unsigned char j_idx = 0;
  
    for (i_idx = 0; i_idx < pairedDeviceInfo->numberOfDevices; i_idx++) {
        knownDevicesArr[i_idx].ui32VendorId           = pairedDeviceInfo->deviceInfo[i_idx].ui16Vendor;
        knownDevicesArr[i_idx].tDeviceId              = btrCore_GenerateUniqueDeviceID(pairedDeviceInfo->deviceInfo[i_idx].pcAddress);
        knownDevicesArr[i_idx].enDeviceType           = btrCore_MapClassIDtoDeviceType(pairedDeviceInfo->deviceInfo[i_idx].ui32Class);
        strncpy(knownDevicesArr[i_idx].pcDeviceName, pairedDeviceInfo->deviceInfo[i_idx].pcName, BD_NAME_LEN);
        strncpy(knownDevicesArr[i_idx].pcDeviceAddress, pairedDeviceInfo->deviceInfo[i_idx].pcAddress, BD_NAME_LEN);
        strncpy(knownDevicesArr[i_idx].pcDevicePath, pairedDeviceInfo->devicePath[i_idx], BD_NAME_LEN);
   
        BTRCORELOG_DEBUG ("Unique DevID = %lld\n", knownDevicesArr[i_idx].tDeviceId);

        for (j_idx = 0; j_idx < BT_MAX_DEVICE_PROFILE; j_idx++) {
            if (pairedDeviceInfo->deviceInfo[i_idx].aUUIDs[j_idx][0] == '\0')
                break;
            else
                knownDevicesArr[i_idx].stDeviceProfile.profile[j_idx].uuid_value = btrCore_BTParseUUIDValue (
                                                                                        pairedDeviceInfo->deviceInfo[i_idx].aUUIDs[j_idx],
                                                                                        knownDevicesArr[i_idx].stDeviceProfile.profile[j_idx].profile_name);
        }
        knownDevicesArr[i_idx].stDeviceProfile.numberOfService   =  j_idx;

        if (knownDevicesArr[i_idx].enDeviceType == enBTRCore_DC_Unknown) {
            for (j_idx = 0; j_idx < knownDevicesArr[i_idx].stDeviceProfile.numberOfService; j_idx++) {
                if (knownDevicesArr[i_idx].stDeviceProfile.profile[j_idx].uuid_value == strtol(BTR_CORE_A2SNK, NULL, 16)) {
                    knownDevicesArr[i_idx].enDeviceType = enBTRCore_DC_Loudspeaker;
                }
            }
        }
    }
}
  
 
static enBTRCoreRet
btrCore_PopulateListOfPairedDevices (
    stBTRCoreHdl*   apsthBTRCore,
    const char*     pAdapterPath
) {
    unsigned char           i_idx = 0;
    unsigned char           j_idx = 0;
    enBTRCoreRet            retResult = enBTRCoreSuccess;
    stBTPairedDeviceInfo    pairedDeviceInfo;
    stBTRCoreBTDevice       knownDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];

    memset (&pairedDeviceInfo, 0, sizeof(pairedDeviceInfo));

    if (BtrCore_BTGetPairedDeviceInfo (apsthBTRCore->connHdl, pAdapterPath, &pairedDeviceInfo) != 0) {
        BTRCORELOG_ERROR ("Failed to populate List Of Paired Devices\n");
        return enBTRCoreFailure;
    }


    memset (knownDevicesArr, 0, sizeof(knownDevicesArr));
    btrCore_MapKnownDeviceListFromPairedDeviceInfo (knownDevicesArr, &pairedDeviceInfo); 

    /* Initially stBTRCoreKnownDevice List is populated from pairedDeviceInfo(bluez i/f) directly *********/  
    if (!apsthBTRCore->numOfPairedDevices) { 
        apsthBTRCore->numOfPairedDevices = pairedDeviceInfo.numberOfDevices;
        memcpy (apsthBTRCore->stKnownDevicesArr, knownDevicesArr, sizeof(stBTRCoreBTDevice) * apsthBTRCore->numOfPairedDevices);

        for (i_idx = 0; i_idx < pairedDeviceInfo.numberOfDevices; i_idx++) {
            apsthBTRCore->stKnownDevStInfoArr[i_idx].eDevicePrevState = enBTRCoreDevStInitialized;
            apsthBTRCore->stKnownDevStInfoArr[i_idx].eDeviceCurrState = enBTRCoreDevStPaired;
        }
    } 
    else {
        /**************************************************************************************************
        stBTRCoreKnownDevice in stBTRCoreHdl is handled seperately, instead of populating directly from bluez i/f
        pairedDeviceInfo list as it causes inconsistency in indices of stKnownDevStInfoArr during pair and unpair
        of devices.
        This case of the addition of Dev6, Dev7 and removal of Dev5 in stBTRCoreKnownDevice list using index
        arrays shows the working of the below algorithm.

         stBTPairedDeviceInfo List from bluez i/f               stBTRCoreKnownDevice List maintained in BTRCore
        +------+------+------+------+------+------+                   +------+------+------+------+------+
        |      |      |      |      |      |      |                   |      |      |      |      |      |      
        | Dev7 | Dev1 | Dev2 | Dev4 | Dev3 | Dev6 |                   | Dev3 | Dev1 | Dev4 | Dev5 | Dev2 |
        |      |      |      |      |      |      |                   |      |      |      |      |      |   
        +------+------+------+------+------+------+                   +------+------+------+------+------+
        +------+------+------+------+------+------+                   +------+------+------+------+------+
        |  0   |  1   |  1   |  1   |  1   |  0   |                   |  1   |  1   |  1   |  0   |  1   |   
        +------+------+------+------+------+------+                   +------+------+------+------+------+
          |                                  |                          |      |      |             |    
          +---------------------------+      |      +-------------------+      |      |             | 
                                      |      |      |      +-------------------+      |             |
                                      |      |      |      |      +-------------------+             |
                                      |      |      |      |      |      +--------------------------+                              
                                      |      |      |      |      |      |
                                   +------+------+------+------+------+------+
                                   |      |      |      |      |      |      |
                                   | Dev7 | Dev6 | Dev3 | Dev1 | Dev4 | Dev2 |
                                   |      |      |      |      |      |      |
                                   +------+------+------+------+------+------+       
                                   -----Updated stBTRCoreKnownDevice List-----
        Now as the change of indexes is known, stKnownDevStInfoArr is also handled  accordingly.******************/
        
        unsigned char count = 0;
        unsigned char k_idx = 0;
        unsigned char numOfDevices = 0;
        unsigned char knownDev_index_array[BTRCORE_MAX_NUM_BT_DEVICES];
        unsigned char pairedDev_index_array[BTRCORE_MAX_NUM_BT_DEVICES];

        memset (knownDev_index_array,  0, sizeof(knownDev_index_array));
        memset (pairedDev_index_array, 0, sizeof(pairedDev_index_array));
        memcpy (knownDevicesArr, apsthBTRCore->stKnownDevicesArr,  sizeof(apsthBTRCore->stKnownDevicesArr)); 
        memset (apsthBTRCore->stKnownDevicesArr,               0,  sizeof(apsthBTRCore->stKnownDevicesArr));

        /*Loops through to mark the new added and removed device entries in the list              */
        for (i_idx = 0, j_idx = 0; i_idx < pairedDeviceInfo.numberOfDevices && j_idx < apsthBTRCore->numOfPairedDevices; j_idx++) {
            if (btrCore_GenerateUniqueDeviceID(pairedDeviceInfo.deviceInfo[i_idx].pcAddress) == knownDevicesArr[j_idx].tDeviceId) {
                knownDev_index_array[j_idx] = 1;
                pairedDev_index_array[i_idx]= 1;
                i_idx++;
            }
            else {
                for (k_idx = i_idx + 1; k_idx < pairedDeviceInfo.numberOfDevices; k_idx++) {
                    if (btrCore_GenerateUniqueDeviceID(pairedDeviceInfo.deviceInfo[k_idx].pcAddress) == knownDevicesArr[j_idx].tDeviceId) {
                        knownDev_index_array[j_idx] = 1;
                        pairedDev_index_array[k_idx]= 1;
                        break;
                    }
                }
            }
        }

        numOfDevices = apsthBTRCore->numOfPairedDevices;

        /*Loops through to check for the removal of Device entries from the list during Unpairing */ 
        for (i_idx = 0; i_idx < numOfDevices; i_idx++) {
            if (knownDev_index_array[i_idx]) {
                memcpy (&apsthBTRCore->stKnownDevicesArr[i_idx - count], &knownDevicesArr[i_idx], sizeof(stBTRCoreBTDevice));
                apsthBTRCore->stKnownDevStInfoArr[i_idx - count].eDevicePrevState = apsthBTRCore->stKnownDevStInfoArr[i_idx].eDevicePrevState;
                apsthBTRCore->stKnownDevStInfoArr[i_idx - count].eDeviceCurrState = apsthBTRCore->stKnownDevStInfoArr[i_idx].eDeviceCurrState;
            }
            else {
                count++; 
                apsthBTRCore->numOfPairedDevices--;
            }
        }
        btrCore_MapKnownDeviceListFromPairedDeviceInfo (knownDevicesArr, &pairedDeviceInfo);

        /*Loops through to checks for the addition of Device entries to the list during paring     */ 
        for (i_idx = 0; i_idx < pairedDeviceInfo.numberOfDevices; i_idx++) {
            if (!pairedDev_index_array[i_idx]) {
                memcpy(&apsthBTRCore->stKnownDevicesArr[apsthBTRCore->numOfPairedDevices], &knownDevicesArr[i_idx], sizeof(stBTRCoreBTDevice));
                if (apsthBTRCore->stKnownDevStInfoArr[apsthBTRCore->numOfPairedDevices].eDeviceCurrState != enBTRCoreDevStConnected) {
                    apsthBTRCore->stKnownDevStInfoArr[apsthBTRCore->numOfPairedDevices].eDevicePrevState = enBTRCoreDevStInitialized;
                    apsthBTRCore->stKnownDevStInfoArr[apsthBTRCore->numOfPairedDevices].eDeviceCurrState = enBTRCoreDevStPaired;
                    apsthBTRCore->stKnownDevicesArr[apsthBTRCore->numOfPairedDevices].bDeviceConnected   = FALSE;
                }
                apsthBTRCore->numOfPairedDevices++;
            }
        }
    } 
    

    return retResult;
}


static const char*
btrCore_GetKnownDeviceAddress (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int loop = 0;

    if ((!aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].tDeviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].pcDevicePath;
        }
    }

    return NULL;
}

static const char*
btrCore_GetKnownDeviceName (
   stBTRCoreHdl*    apsthBTRCore,
   tBTRCoreDevId    aBTRCoreDevId
) {
    int loop = 0;

   if ((!aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].tDeviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].pcDeviceName;
        }

    }

    return NULL;
}

static const char*
btrCore_GetKnownDeviceMac (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int loop = 0;

    if ((!aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].tDeviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].pcDeviceAddress;
        }
    }

    return NULL;
}

static void 
btrCore_ShowSignalStrength (
    short strength
) {
    short pos_str;

    pos_str = 100 + strength;//strength is usually negative with number meaning more strength

    BTRCORELOG_INFO (" Signal Strength: %d dbmv  ", strength);

    if (pos_str > 70) {
        BTRCORELOG_INFO ("++++\n");
    }

    if ((pos_str > 50) && (pos_str <= 70)) {
        BTRCORELOG_INFO ("+++\n");
    }

    if ((pos_str > 37) && (pos_str <= 50)) {
        BTRCORELOG_INFO ("++\n");
    }

    if (pos_str <= 37) {
        BTRCORELOG_INFO ("+\n");
    } 
}


static unsigned int
btrCore_BTParseUUIDValue (
    const char* pUUIDString,
    char*       pServiceNameOut
) {
    char aUUID[8];
    unsigned int uuid_value = 0;


    if (pUUIDString) {
        /* Arrive at short form of UUID */
        aUUID[0] = '0';
        aUUID[1] = 'x';
        aUUID[2] = pUUIDString[4];
        aUUID[3] = pUUIDString[5];
        aUUID[4] = pUUIDString[6];
        aUUID[5] = pUUIDString[7];
        aUUID[6] = '\0';

        uuid_value = strtol(aUUID, NULL, 16);

        /* Have the name by list comparision */
        if (!strcasecmp(aUUID, BTR_CORE_SP))
            strcpy(pServiceNameOut, BTR_CORE_SP_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HEADSET))
            strcpy(pServiceNameOut, BTR_CORE_HEADSET_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_A2SRC))
            strcpy(pServiceNameOut, BTR_CORE_A2SRC_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_A2SNK))
            strcpy(pServiceNameOut, BTR_CORE_A2SNK_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_AVRTG))
            strcpy(pServiceNameOut, BTR_CORE_AVRTG_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_AAD))
            strcpy(pServiceNameOut, BTR_CORE_AAD_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_AVRCT))
            strcpy(pServiceNameOut, BTR_CORE_AVRCT_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_AVREMOTE))
            strcpy(pServiceNameOut, BTR_CORE_AVREMOTE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HS_AG))
            strcpy(pServiceNameOut, BTR_CORE_HS_AG_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HANDSFREE))
            strcpy(pServiceNameOut, BTR_CORE_HANDSFREE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HAG))
            strcpy(pServiceNameOut, BTR_CORE_HAG_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HEADSET2))
            strcpy(pServiceNameOut, BTR_CORE_HEADSET2_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_GEN_AUDIO))
            strcpy(pServiceNameOut, BTR_CORE_GEN_AUDIO_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_PNP))
            strcpy(pServiceNameOut, BTR_CORE_PNP_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_GEN_ATRIB))
            strcpy(pServiceNameOut, BTR_CORE_GEN_ATRIB_TEXT);

        else
            strcpy (pServiceNameOut, "Not Identified");
    }
    else
        strcpy (pServiceNameOut, "Not Identified");

    return uuid_value;
}


static enBTRCoreDeviceState
btrCore_BTParseDeviceConnectionState (
    const char* pcStateValue
) {
    enBTRCoreDeviceState rc = enBTRCoreDevStInitialized;

    if ((pcStateValue) && (pcStateValue[0] != '\0')) {
        BTRCORELOG_DEBUG ("Current State of this connection is @@%s@@\n", pcStateValue);

        if (!strcasecmp("disconnected", pcStateValue)) {
            rc = enBTRCoreDevStDisconnected;
        }
        else if (!strcasecmp("connecting", pcStateValue)) {
            rc = enBTRCoreDevStConnecting;
        }
        else if (!strcasecmp("connected", pcStateValue)) {
            rc = enBTRCoreDevStConnected;
        }
        else if (!strcasecmp("playing", pcStateValue)) {
            rc = enBTRCoreDevStPlaying;
        }
    }

    return rc;
}


/*  Local Op Threads */
void*
DoDispatch (
    void* ptr
) {
    tBTRCoreHandle  hBTRCore = NULL;
    BOOLEAN         ldispatchThreadQuit = FALSE;
    enBTRCoreRet*   penDispThreadExitStatus = malloc(sizeof(enBTRCoreRet));

    hBTRCore = (stBTRCoreHdl*) ptr;
    BTRCORELOG_DEBUG ("%s \n", "Dispatch Thread Started");


    if (!((stBTRCoreHdl*)hBTRCore) || !((stBTRCoreHdl*)hBTRCore)->connHdl) {
        BTRCORELOG_ERROR ("Dispatch thread failure - BTRCore not initialized\n");
        *penDispThreadExitStatus = enBTRCoreNotInitialized;
        return (void*)penDispThreadExitStatus;
    }
    
    while (1) {
        g_mutex_lock (&((stBTRCoreHdl*)hBTRCore)->dispatchMutex);
        ldispatchThreadQuit = ((stBTRCoreHdl*)hBTRCore)->dispatchThreadQuit;
        g_mutex_unlock (&((stBTRCoreHdl*)hBTRCore)->dispatchMutex);

        if (ldispatchThreadQuit == TRUE)
            break;

#if 1
        usleep(25000); // 25ms
#else
        sched_yield(); // Would like to use some form of yield rather than sleep sometime in the future
#endif

        if (BtrCore_BTSendReceiveMessages(((stBTRCoreHdl*)hBTRCore)->connHdl) != 0)
            break;
    }

    *penDispThreadExitStatus = enBTRCoreSuccess;
    return (void*)penDispThreadExitStatus;
}


void
test_func (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg - enBTRCoreInitFailure\n");
        return;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->fpcBBTRCoreStatus) {
        if (pstlhBTRCore->fpcBBTRCoreStatus(&pstlhBTRCore->stDevStatusCbInfo, pstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
            //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
        }
    }
    else {
        BTRCORELOG_ERROR ("no callback installed\n");
    }

    return;
}


/*  Interfaces  */
enBTRCoreRet
BTRCore_Init (
    tBTRCoreHandle* phBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL; 
    unBTOpIfceProp  lunBtOpAdapProp;

#ifdef RDK_LOGGER_ENABLED
    const char* pDebugConfig = NULL;
    const char* BTRCORE_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
    const char* BTRCORE_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

    /* Init the logger */
    if (access(BTRCORE_DEBUG_OVERRIDE_PATH, F_OK) != -1 ) {
        pDebugConfig = BTRCORE_DEBUG_OVERRIDE_PATH;
    }
    else {
        pDebugConfig = BTRCORE_DEBUG_ACTUAL_PATH;
    }

    if (rdk_logger_init(pDebugConfig) == 0) {
       b_rdk_logger_enabled = 1; 
    }
#endif

    BTRCORELOG_INFO ("BTRCore_Init\n");

    if (!phBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }


    pstlhBTRCore = (stBTRCoreHdl*)malloc(sizeof(stBTRCoreHdl));
    if (!pstlhBTRCore) {
        BTRCORELOG_ERROR ("Insufficient memory - enBTRCoreInitFailure\n");
        return enBTRCoreInitFailure;
    }
    memset(pstlhBTRCore, 0, sizeof(stBTRCoreHdl));


    pstlhBTRCore->connHdl = BtrCore_BTInitGetConnection();
    if (!pstlhBTRCore->connHdl) {
        BTRCORELOG_ERROR ("Can't get on system bus - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    //init array of scanned , known & found devices
    btrCore_InitDataSt(pstlhBTRCore);

    pstlhBTRCore->agentPath = BtrCore_BTGetAgentPath(pstlhBTRCore->connHdl);
    if (!pstlhBTRCore->agentPath) {
        BTRCORELOG_ERROR ("Can't get agent path - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    pstlhBTRCore->dispatchThreadQuit = FALSE;
    g_mutex_init(&pstlhBTRCore->dispatchMutex);
    if((pstlhBTRCore->dispatchThread = g_thread_new("BTRCoreTaskThread", DoDispatch, (void*)pstlhBTRCore)) == NULL) {
        BTRCORELOG_ERROR ("Failed to create Dispatch Thread - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHdl, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        BTRCORELOG_ERROR ("Failed to get BT Adapter - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropAddress;
    if (BtrCore_BTGetProp(pstlhBTRCore->connHdl,
                          pstlhBTRCore->curAdapterPath,
                          enBTAdapter,
                          lunBtOpAdapProp,
                          pstlhBTRCore->curAdapterAddr)) {
        BTRCORELOG_ERROR ("Failed to get BT Adapter Address - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    BTRCORELOG_DEBUG ("Adapter path %s - Adapter Address %s \n", pstlhBTRCore->curAdapterPath, pstlhBTRCore->curAdapterAddr);


    /* Initialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (BTRCore_AVMedia_Init(&pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Init AV Media Subsystem - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    /* Initialize BTRCore SubSystems - LE Gatt profile. */
    if (BTRCore_LE_Init(&pstlhBTRCore->leHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Init LE Subsystem - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }


    if(BtrCore_BTRegisterDevStatusUpdateCb(pstlhBTRCore->connHdl, &btrCore_BTDeviceStatusUpdateCb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Device Status CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnIntimationCb(pstlhBTRCore->connHdl, &btrCore_BTDeviceConnectionIntimationCb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Connection Intimation CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnAuthCb(pstlhBTRCore->connHdl, &btrCore_BTDeviceAuthenticationCb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Connection Authentication CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BTRCore_AVMedia_RegisterMediaStatusUpdateCb(pstlhBTRCore->avMediaHdl, &btrCore_BTMediaStatusUpdateCb, pstlhBTRCore) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to Register Media Status CB - enBTRCoreInitFailure\n");
       BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    *phBTRCore  = (tBTRCoreHandle)pstlhBTRCore;

    //Initialize array of known devices so we can use it for stuff
    btrCore_PopulateListOfPairedDevices(*phBTRCore, pstlhBTRCore->curAdapterPath);
    

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DeInit (
    tBTRCoreHandle  hBTRCore
) {
    void*           penDispThreadExitStatus = NULL;
    enBTRCoreRet    enDispThreadExitStatus = enBTRCoreFailure;
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    int             i;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    BTRCORELOG_INFO ("hBTRCore   =   0x%8p\n", hBTRCore);

    /* Free any memory allotted for use in BTRCore */
    
    /* DeInitialize BTRCore SubSystems - AVMedia/Telemetry..etc. */

    if (BTRCore_AVMedia_DeInit(pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to DeInit AV Media Subsystem\n");
        enDispThreadExitStatus = enBTRCoreFailure;
    }

    g_mutex_lock(&pstlhBTRCore->dispatchMutex);
    pstlhBTRCore->dispatchThreadQuit = TRUE;
    g_mutex_unlock(&pstlhBTRCore->dispatchMutex);

    penDispThreadExitStatus = g_thread_join(pstlhBTRCore->dispatchThread);
    g_mutex_clear(&pstlhBTRCore->dispatchMutex);
    
    BTRCORELOG_INFO ("BTRCore_DeInit - Exiting BTRCore - %d\n", *((enBTRCoreRet*)penDispThreadExitStatus));
    enDispThreadExitStatus = *((enBTRCoreRet*)penDispThreadExitStatus);
    free(penDispThreadExitStatus);

    if (pstlhBTRCore->curAdapterPath) {
        if (BtrCore_BTReleaseAdapterPath(pstlhBTRCore->connHdl, NULL)) {
            BTRCORELOG_ERROR ("Failure BtrCore_BTReleaseAdapterPath\n");
        }
        pstlhBTRCore->curAdapterPath = NULL;
    }

    /* Adapters */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_ADAPTERS; i++) {
        if (pstlhBTRCore->adapterPath[i]) {
            free(pstlhBTRCore->adapterPath[i]);
            pstlhBTRCore->adapterPath[i] = NULL;
        }

        if (pstlhBTRCore->adapterAddr[i]) {
            free(pstlhBTRCore->adapterAddr[i]);
            pstlhBTRCore->adapterAddr[i] = NULL;
        }
    }

    if (pstlhBTRCore->curAdapterAddr) {
        free(pstlhBTRCore->curAdapterAddr);
        pstlhBTRCore->curAdapterAddr = NULL;
    }

    if (pstlhBTRCore->agentPath) {
        if (BtrCore_BTReleaseAgentPath(pstlhBTRCore->connHdl)) {
            BTRCORELOG_ERROR ("Failure BtrCore_BTReleaseAgentPath\n");
        }
        pstlhBTRCore->agentPath = NULL;
    }

    if (pstlhBTRCore->connHdl) {
        if (BtrCore_BTDeInitReleaseConnection(pstlhBTRCore->connHdl)) {
            BTRCORELOG_ERROR ("Failure BtrCore_BTDeInitReleaseConnection\n");
        }
        pstlhBTRCore->connHdl = NULL;
    }

    if (hBTRCore) {
        free(hBTRCore);
        hBTRCore = NULL;
    }

    return  enDispThreadExitStatus;
}


enBTRCoreRet
BTRCore_RegisterAgent (
    tBTRCoreHandle  hBTRCore,
    int             iBTRCapMode
) {
    char            capabilities[32] = {'\0'};
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    
    if (iBTRCapMode == 1) {
        strcpy(capabilities,"DisplayYesNo");
    }
    else {
        strcpy(capabilities,"NoInputNoOutput"); //default is no input no output
    }


    if (BtrCore_BTRegisterAgent(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath, capabilities) < 0) {
        BTRCORELOG_ERROR ("Agent registration ERROR occurred\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_TRACE ("Starting Agent in mode %s\n", capabilities);
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_UnregisterAgent (
    tBTRCoreHandle  hBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTUnregisterAgent(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath) < 0) {
        BTRCORELOG_ERROR ("Agent unregistration  ERROR occurred\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_TRACE ("Stopping Agent\n");
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetListOfAdapters (
    tBTRCoreHandle          hBTRCore,
    stBTRCoreListAdapters*  pstListAdapters
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    enBTRCoreRet    rc = enBTRCoreFailure;
    unBTOpIfceProp  lunBtOpAdapProp;
    int i;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pstListAdapters) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetAdapterList(pstlhBTRCore->connHdl, &pstlhBTRCore->numOfAdapters, pstlhBTRCore->adapterPath)) {
        pstListAdapters->number_of_adapters = pstlhBTRCore->numOfAdapters;
        for (i = 0; i < pstListAdapters->number_of_adapters; i++) {
            memset(pstListAdapters->adapter_path[i], '\0', sizeof(pstListAdapters->adapter_path[i]));
            strncpy(pstListAdapters->adapter_path[i], pstlhBTRCore->adapterPath[i], BD_NAME_LEN - 1);

            memset(pstListAdapters->adapterAddr[i], '\0', sizeof(pstListAdapters->adapterAddr[i]));
            lunBtOpAdapProp.enBtAdapterProp = enBTAdPropAddress;
            if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pstlhBTRCore->adapterPath[i], enBTAdapter, lunBtOpAdapProp, pstlhBTRCore->adapterAddr[i])) {
                strncpy(pstListAdapters->adapterAddr[i], pstlhBTRCore->adapterAddr[i], BD_NAME_LEN - 1);
            }

            rc = enBTRCoreSuccess;
        }
    }

    return rc;
}


enBTRCoreRet
BTRCore_GetAdapters (
    tBTRCoreHandle          hBTRCore,
    stBTRCoreGetAdapters*   pstGetAdapters
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pstGetAdapters) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetAdapterList(pstlhBTRCore->connHdl, &pstlhBTRCore->numOfAdapters, pstlhBTRCore->adapterPath)) {
        pstGetAdapters->number_of_adapters = pstlhBTRCore->numOfAdapters;
        return enBTRCoreSuccess;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHdl, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        BTRCORELOG_ERROR ("Failed to get BT Adapter");
        return enBTRCoreInvalidAdapter;
    }

    if (apstBTRCoreAdapter) {
        apstBTRCoreAdapter->adapter_number   = 0; //hard code to default adapter for now
        apstBTRCoreAdapter->pcAdapterPath    = pstlhBTRCore->curAdapterPath;
        apstBTRCoreAdapter->pcAdapterDevName = NULL;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapter (
    tBTRCoreHandle  hBTRCore,
    int             adapter_number
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    int pathlen;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pathlen = strlen(pstlhBTRCore->curAdapterPath);
    switch (adapter_number) {
    case 0:
        pstlhBTRCore->curAdapterPath[pathlen-1]='0';
        break;
    case 1:
        pstlhBTRCore->curAdapterPath[pathlen-1]='1';
        break;
    case 2:
        pstlhBTRCore->curAdapterPath[pathlen-1]='2';
        break;
    case 3:
        pstlhBTRCore->curAdapterPath[pathlen-1]='3';
        break;
    case 4:
        pstlhBTRCore->curAdapterPath[pathlen-1]='4';
        break;
    case 5:
        pstlhBTRCore->curAdapterPath[pathlen-1]='5';
        break;
    default:
        BTRCORELOG_INFO ("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
        pstlhBTRCore->curAdapterPath[pathlen-1]='0';
        break;
    }

    BTRCORELOG_INFO ("Now current adatper is %s\n", pstlhBTRCore->curAdapterPath);

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_EnableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;
    int                 powered = 1;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    BTRCORELOG_ERROR ("BTRCore_EnableAdapter\n");
    apstBTRCoreAdapter->enable = TRUE; //does this even mean anything?
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropPowered;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &powered)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;
    int                 powered = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    BTRCORELOG_ERROR ("BTRCore_DisableAdapter\n");
    apstBTRCoreAdapter->enable = FALSE;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropPowered;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &powered)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapterAddr (
    tBTRCoreHandle      hBTRCore,
    unsigned char       aui8adapterIdx,
    char*               apui8adapterAddr
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    int                 i = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!apui8adapterAddr) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    for (i = 0; i < pstlhBTRCore->numOfAdapters; i++) {
        if (aui8adapterIdx == i) {
            strncpy(apui8adapterAddr, pstlhBTRCore->adapterAddr[i], BD_NAME_LEN - 1);
            break;
        }
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverable (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;
    int                 discoverable;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    discoverable = apstBTRCoreAdapter->discoverable;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropDiscoverable;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &discoverable)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapterDiscoverable (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char   discoverable
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    unBTOpIfceProp  lunBtOpAdapProp;
    int             isDiscoverable = (int) discoverable;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropDiscoverable;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &isDiscoverable)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverableTimeout (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;
    unsigned int        timeout;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    timeout = apstBTRCoreAdapter->DiscoverableTimeout;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropDiscoverableTimeOut;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &timeout)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapterDiscoverableTimeout (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned short  timeout
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    unBTOpIfceProp  lunBtOpAdapProp;
    unsigned int    givenTimeout = (unsigned int)timeout;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropDiscoverableTimeOut;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &givenTimeout)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapterDiscoverableStatus (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char*  pDiscoverable
) {
    stBTRCoreHdl*   pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    unBTOpIfceProp  lunBtOpAdapProp;
    int             discoverable = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDiscoverable)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropDiscoverable;
    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, lunBtOpAdapProp, &discoverable)) {
        BTRCORELOG_INFO ("Get value for org.bluez.Adapter.Discoverable = %d\n", discoverable);
        *pDiscoverable = (unsigned char) discoverable;
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet 
BTRCore_SetAdapterDeviceName (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter,
    char*               apcAdapterDeviceName
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!apcAdapterDeviceName) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if(apstBTRCoreAdapter->pcAdapterDevName) {
        free(apstBTRCoreAdapter->pcAdapterDevName);
        apstBTRCoreAdapter->pcAdapterDevName = NULL;
    }

    apstBTRCoreAdapter->pcAdapterDevName = strdup(apcAdapterDeviceName); //TODO: Free this memory
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropName;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &(apstBTRCoreAdapter->pcAdapterDevName))) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropName - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapterName (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath,
    const char*         pAdapterName
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    unBTOpIfceProp      lunBtOpAdapProp;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) ||(!pAdapterName)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropName;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &pAdapterName)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropName - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapterName (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    char*           pAdapterName
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    unBTOpIfceProp  lunBtOpAdapProp;

    char name[BD_NAME_LEN + 1] = "";
    memset (name, '\0', sizeof (name));

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterName)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropName;
    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, lunBtOpAdapProp, name)) {
        BTRCORELOG_INFO ("Get value for org.bluez.Adapter.Name = %s\n", name);
        strcpy(pAdapterName, name);
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet
BTRCore_SetAdapterPower (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char   powerStatus
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    unBTOpIfceProp  lunBtOpAdapProp;
    int             power = powerStatus;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropPowered;

    if (BtrCore_BTSetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, lunBtOpAdapProp, &power)) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapterPower (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char*  pAdapterPower
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    unBTOpIfceProp  lunBtOpAdapProp;
    int             powerStatus = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterPower)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    lunBtOpAdapProp.enBtAdapterProp = enBTAdPropPowered;
    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, lunBtOpAdapProp, &powerStatus)) {
        BTRCORELOG_INFO ("Get value for org.bluez.Adapter.powered = %d\n", powerStatus);
        *pAdapterPower = (unsigned char) powerStatus;
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet
BTRCore_GetVersionInfo (
    tBTRCoreHandle  hBTRCore,
    char*           apcBtVersion
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    char            lBtIfceName[BTRCORE_STRINGS_MAX_LEN/4];
    char            lBtVersion[BTRCORE_STRINGS_MAX_LEN/4];

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!apcBtVersion) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    memset(lBtIfceName, '\0', BTRCORE_STRINGS_MAX_LEN/4);
    memset(lBtVersion,  '\0', BTRCORE_STRINGS_MAX_LEN/4);

    if (!BtrCore_BTGetIfceNameVersion(pstlhBTRCore->connHdl, lBtIfceName, lBtVersion)) {
        strncpy(apcBtVersion, lBtIfceName, strlen(lBtIfceName));
        strncat(apcBtVersion, "-", 1);
        strncat(apcBtVersion, lBtVersion, strlen(lBtVersion));
        BTRCORELOG_INFO ("Ifce: %s Version: %s Out:%s\n", lBtIfceName, lBtVersion, apcBtVersion);

        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet
BTRCore_StartDiscovery (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath,
    enBTRCoreDeviceType aenBTRCoreDevType,
    unsigned int        aui32DiscDuration
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    btrCore_ClearScannedDevicesList(pstlhBTRCore);

    if (aenBTRCoreDevType == enBTRCoreLE)  {
        if (BtrCore_BTStartLEDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
            return enBTRCoreDiscoveryFailure;
        }

        if (aui32DiscDuration) {
            sleep(aui32DiscDuration); //TODO: Better to setup a timer which calls BTStopDiscovery
            if (BtrCore_BTStopLEDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
                return enBTRCoreDiscoveryFailure;
            }
        }
    }
    else {
        if (BtrCore_BTStartDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
            return enBTRCoreDiscoveryFailure;
        }

        if (aui32DiscDuration) {
            sleep(aui32DiscDuration); //TODO: Better to setup a timer which calls BTStopDiscovery
            if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
                return enBTRCoreDiscoveryFailure;
            }
        }
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_StopDiscovery (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (aenBTRCoreDevType == enBTRCoreLE)  {
        if (BtrCore_BTStopLEDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
            return enBTRCoreDiscoveryFailure;
        }
    }
    else {
        if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
            return enBTRCoreDiscoveryFailure;
        }
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetListOfScannedDevices (
    tBTRCoreHandle                  hBTRCore,
    stBTRCoreScannedDevicesCount*   pListOfScannedDevices
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    int i;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pListOfScannedDevices) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    BTRCORELOG_INFO ("adapter path is %s\n", pstlhBTRCore->curAdapterPath);
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (pstlhBTRCore->stScannedDevicesArr[i].bFound) {
            BTRCORELOG_INFO ("Device : %d\n", i);
            BTRCORELOG_INFO ("Name   : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceName);
            BTRCORELOG_INFO ("Mac Ad : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceAddress);
            BTRCORELOG_INFO ("Rssi   : %d dbmV\n", pstlhBTRCore->stScannedDevicesArr[i].i32RSSI);
            btrCore_ShowSignalStrength(pstlhBTRCore->stScannedDevicesArr[i].i32RSSI);
        }
    }   

    memset (pListOfScannedDevices, 0, sizeof(stBTRCoreScannedDevicesCount));
    memcpy (pListOfScannedDevices->devices, pstlhBTRCore->stScannedDevicesArr, sizeof (pstlhBTRCore->stScannedDevicesArr));
    pListOfScannedDevices->numberOfDevices = pstlhBTRCore->numOfScannedDevices;
    BTRCORELOG_INFO ("Copied scanned details of %d devices\n", pstlhBTRCore->numOfScannedDevices);

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_PairDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*           pstlhBTRCore        = NULL;
    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstScannedDev       = NULL;
    int                     i32LoopIdx          = 0;
    int                     i32ScannedDevIdx    = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstScannedDev   = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];

        if (pstScannedDev)
            pDeviceAddress  = pstScannedDev->pcDeviceAddress;
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
                i32ScannedDevIdx = i32LoopIdx;
                break;
            }
        }

        pstScannedDev   = &pstlhBTRCore->stScannedDevicesArr[i32ScannedDevIdx];

        if (pstScannedDev)
            pDeviceAddress  = pstScannedDev->pcDeviceAddress;
    }

    if (!pstScannedDev || !pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_DEBUG ("We will pair     %s\n", pstScannedDev->pcDeviceName);
    BTRCORELOG_DEBUG ("We will address  %s\n", pDeviceAddress);

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    enBTAdpOpCreatePairedDev) < 0) {
        BTRCORELOG_ERROR ("Failed to pair a device\n");
        return enBTRCorePairingFailed;
    }

    //Calling this api will update the KnownDevList appropriately
    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);

    BTRCORELOG_INFO ("Pairing Success\n");
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_UnPairDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;

    /* We can enhance the BTRCore with passcode support later point in time */
    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated\n");
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    enBTAdpOpRemovePairedDev) != 0) {
        BTRCORELOG_ERROR ("Failed to unpair a device\n");
        return enBTRCorePairingFailed;
    }

    //Calling this api will update the KnownDevList appropriately
    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);

    BTRCORELOG_INFO ("UnPairing Success\n");
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetListOfPairedDevices (
    tBTRCoreHandle                  hBTRCore,
    stBTRCorePairedDevicesCount*    pListOfDevices
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pListOfDevices) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath) == enBTRCoreSuccess) {
        pListOfDevices->numberOfDevices = pstlhBTRCore->numOfPairedDevices;
        memcpy (pListOfDevices->devices, pstlhBTRCore->stKnownDevicesArr, sizeof (pstlhBTRCore->stKnownDevicesArr));
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet
BTRCore_FindDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*           pstlhBTRCore = NULL;
    stBTRCoreBTDevice* pstScannedDevice = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];

    BTRCORELOG_DEBUG (" We will try to find %s\n"
                     " address %s\n",
                     pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].pcDeviceName,
                     pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].pcDeviceAddress);

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pstScannedDevice->pcDeviceAddress,
                                    enBTAdpOpFindPairedDev) < 0) {
       // BTRCORELOG_ERROR ("device not found\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


/*BTRCore_FindService, other inputs will include string and boolean pointer for returning*/
enBTRCoreRet
BTRCore_FindService (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId,
    const char*     UUID,
    char*           XMLdata,
    int*            found
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_INFO ("Checking for service %s on %s\n", UUID, pDeviceAddress);
    *found = BtrCore_BTFindServiceSupported (pstlhBTRCore->connHdl, pDeviceAddress, UUID, XMLdata);
    if (*found < 0) {
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetSupportedServices (
    tBTRCoreHandle                  hBTRCore,
    tBTRCoreDevId                   aBTRCoreDevId,
    stBTRCoreSupportedServiceList*  pProfileList
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
    stBTDeviceSupportedServiceList profileList;
    int i = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pProfileList) || (!aBTRCoreDevId)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }


    /* Initialize the array */
    memset (pProfileList, 0 , sizeof(stBTRCoreSupportedServiceList));
    memset (&profileList, 0 , sizeof(stBTDeviceSupportedServiceList));

    if (BtrCore_BTDiscoverDeviceServices(pstlhBTRCore->connHdl, pDeviceAddress, &profileList) != 0) {
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Successfully received the supported services... \n");

    pProfileList->numberOfService = profileList.numberOfService;
    for (i = 0; i < profileList.numberOfService; i++) {
        pProfileList->profile[i].uuid_value = profileList.profile[i].uuid_value;
        strncpy (pProfileList->profile[i].profile_name,  profileList.profile[i].profile_name, 30);
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_IsDeviceConnectable (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    const char*         pDeviceAddress = NULL;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->pcDeviceAddress;
    }
    else {
        pDeviceAddress  = btrCore_GetKnownDeviceMac(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }


    if (BtrCore_BTIsDeviceConnectable(pstlhBTRCore->connHdl, pDeviceAddress) != 0) {
        BTRCORELOG_ERROR ("Device NOT CONNECTABLE\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Device CONNECTABLE\n");
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ConnectDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    const char*         pDeviceAddress = NULL;
    const char*         pDeviceName = NULL;
    enBTDeviceType      lenBTDeviceType = enBTDevUnknown;
    int                 i32LoopIdx = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    /* Check the device type as LE, if not, then go ahead and return failure. */
    if ((aenBTRCoreDevType != enBTRCoreLE) &&  (!pstlhBTRCore->numOfPairedDevices)) {
        BTRCORELOG_ERROR ("There is no device paired for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->pcDevicePath;
        pDeviceName     = pstKnownDevice->pcDeviceName;
    }
    else {
        pDeviceAddress  = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
        pDeviceName     = btrCore_GetKnownDeviceName(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        if(aenBTRCoreDevType != enBTRCoreLE) {
            BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
            return enBTRCoreDeviceNotFound;
        }

        if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
            stBTRCoreBTDevice*   pstScannedDevice = NULL;
            pstScannedDevice= &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
            pDeviceAddress  = pstScannedDevice->pcDevicePath;
            pDeviceName     = pstScannedDevice->pcDeviceName;
        }
        else {
            pDeviceAddress  = btrCore_GetScannedDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
            pDeviceName     = btrCore_GetScannedDeviceName(pstlhBTRCore, aBTRCoreDevId);
        }

        if (!pDeviceAddress || !strlen(pDeviceAddress)) {
            BTRCORELOG_ERROR ("Failed to find device in Scanned or Paired devices list\n");
            return enBTRCoreDeviceNotFound;
        }
    }


    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreLE:
        lenBTDeviceType = enBTDevLE;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHdl, pDeviceAddress, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("Connect to device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Connected to device %s Successfully. \n", pDeviceName);

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected     = TRUE;
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState   = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;

        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState  != enBTRCoreDevStConnected) {
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   = enBTRCoreDevStConnecting; 
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected    = TRUE;
                pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState  = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;

                if (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState  != enBTRCoreDevStConnected) {
                    pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState   = enBTRCoreDevStConnecting; 
                }
            }
        }
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisconnectDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
    const char*     pDeviceName = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    int             i32LoopIdx = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if ((aenBTRCoreDevType != enBTRCoreLE) && (!pstlhBTRCore->numOfPairedDevices)) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
        pDeviceName    = pstKnownDevice->pcDeviceName;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
        pDeviceName    = btrCore_GetKnownDeviceName(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        if(aenBTRCoreDevType != enBTRCoreLE) {
            BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
            return enBTRCoreDeviceNotFound;
        }

        if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
            stBTRCoreBTDevice*   pstScannedDevice = NULL;
            pstScannedDevice= &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
            pDeviceAddress  = pstScannedDevice->pcDevicePath;
            pDeviceName     = pstScannedDevice->pcDeviceName;
        }
        else {
            pDeviceAddress  = btrCore_GetScannedDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
            pDeviceName     = btrCore_GetScannedDeviceName(pstlhBTRCore, aBTRCoreDevId);
        }

        if (!pDeviceAddress || !strlen(pDeviceAddress)) {
            BTRCORELOG_ERROR ("Failed to find device in Scanned or Paired devices list\n");
            return enBTRCoreDeviceNotFound;
        }
    }

    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreLE:
        lenBTDeviceType = enBTDevLE;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHdl, pDeviceAddress, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("DisConnect from device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("DisConnected from device %s Successfully.\n", pDeviceName);
    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected     = FALSE;

        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   != enBTRCoreDevStDisconnected &&
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   != enBTRCoreDevStLost) {
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState    = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState    = enBTRCoreDevStDisconnecting; 
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected    = FALSE;

                if (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState != enBTRCoreDevStDisconnected &&
                    pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState != enBTRCoreDevStLost) {
                    pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState  = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                    pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState  = enBTRCoreDevStDisconnecting; 
                }
            }
        }
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetDeviceConnected (
    tBTRCoreHandle      hBTRCore, 
    tBTRCoreDevId       aBTRCoreDevId, 
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    enBTRCoreRet    lenBTRCoreRet   = enBTRCoreFailure;
    int             i32LoopIdx      = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress  = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreLE:
        lenBTDeviceType = enBTDevLE;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    (void)lenBTDeviceType;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState == enBTRCoreDevStConnected) {
            BTRCORELOG_DEBUG ("enBTRCoreDevStConnected\n");
            lenBTRCoreRet = enBTRCoreSuccess;
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                if (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStConnected) {
                    BTRCORELOG_DEBUG ("enBTRCoreDevStConnected\n");
                    lenBTRCoreRet = enBTRCoreSuccess;
                }
            }
        }
    }

    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_GetDeviceDisconnected (
    tBTRCoreHandle      hBTRCore, 
    tBTRCoreDevId       aBTRCoreDevId, 
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    enBTRCoreRet    lenBTRCoreRet   = enBTRCoreFailure;
    int             i32LoopIdx      = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress  = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    (void)lenBTDeviceType;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        if ((pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState == enBTRCoreDevStDisconnected) ||
            (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState == enBTRCoreDevStLost)) {
            BTRCORELOG_DEBUG ("enBTRCoreDevStDisconnected\n");
            lenBTRCoreRet = enBTRCoreSuccess;
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                if ((pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) ||
                    (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStLost)) {
                    BTRCORELOG_DEBUG ("enBTRCoreDevStDisconnected\n");
                    lenBTRCoreRet = enBTRCoreSuccess;
                }
            }
        }
    }

    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_GetDeviceMediaInfo (
    tBTRCoreHandle          hBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType,
    stBTRCoreDevMediaInfo*  apstBTRCoreDevMediaInfo
) {
    stBTRCoreHdl*           pstlhBTRCore = NULL;
    const char*             pDeviceAddress = NULL;
    enBTDeviceType          lenBTDeviceType = enBTDevUnknown;

    stBTRCoreAVMediaInfo        lstBtrCoreMediaInfo;
    stBTRCoreAVMediaSbcInfo     lstBtrCoreMediaSbcInfo;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((aBTRCoreDevId < 0) || !apstBTRCoreDevMediaInfo || !apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_INFO (" We will get Media Info for %s\n", pDeviceAddress);

    // TODO: Implement a Device State Machine and Check whether the device is Connected before making the call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    lstBtrCoreMediaInfo.eBtrCoreAVMType         = eBTRCoreAVMTypeUnknown;
    lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo  = &lstBtrCoreMediaSbcInfo;


    if (BTRCore_AVMedia_GetCurMediaInfo (pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pDeviceAddress, &lstBtrCoreMediaInfo)) {
        BTRCORELOG_ERROR ("AVMedia_GetCurMediaInfo ERROR occurred\n");
        return enBTRCoreFailure;
    }

    switch (lstBtrCoreMediaInfo.eBtrCoreAVMType) {
    case eBTRCoreAVMTypePCM:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypePCM;
        break;
    case eBTRCoreAVMTypeSBC: {
        stBTRCoreDevMediaSbcInfo* lapstBtrCoreDevMCodecInfo = (stBTRCoreDevMediaSbcInfo*)(apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo);

        apstBTRCoreDevMediaInfo->eBtrCoreDevMType        = eBTRCoreDevMediaTypeSBC;

        switch (lstBtrCoreMediaSbcInfo.eAVMAChan) {
        case eBTRCoreAVMAChanMono:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChanMono;
            break;
        case eBTRCoreAVMAChanDualChannel:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChanDualChannel;
            break;
        case eBTRCoreAVMAChanStereo:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChanStereo;
            break;
        case eBTRCoreAVMAChanJointStereo:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChanJointStereo;
            break;
        case eBTRCoreAVMAChan5_1:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChan5_1;
            break;
        case eBTRCoreAVMAChan7_1:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChan7_1;
            break;
        case eBTRCoreAVMAChanUnknown:
        default:
            lapstBtrCoreDevMCodecInfo->eDevMAChan = eBTRCoreDevMediaAChanUnknown;
            break;
        }

        lapstBtrCoreDevMCodecInfo->ui32DevMSFreq         = lstBtrCoreMediaSbcInfo.ui32AVMSFreq;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcAllocMethod = lstBtrCoreMediaSbcInfo.ui8AVMSbcAllocMethod;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcSubbands    = lstBtrCoreMediaSbcInfo.ui8AVMSbcSubbands;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcBlockLength = lstBtrCoreMediaSbcInfo.ui8AVMSbcBlockLength;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcMinBitpool  = lstBtrCoreMediaSbcInfo.ui8AVMSbcMinBitpool;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcMaxBitpool  = lstBtrCoreMediaSbcInfo.ui8AVMSbcMaxBitpool;
        lapstBtrCoreDevMCodecInfo->ui16DevMSbcFrameLen   = lstBtrCoreMediaSbcInfo.ui16AVMSbcFrameLen;
        lapstBtrCoreDevMCodecInfo->ui16DevMSbcBitrate    = lstBtrCoreMediaSbcInfo.ui16AVMSbcBitrate;

        break;
    }
    case eBTRCoreAVMTypeMPEG:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeMPEG;
        break;
    case eBTRCoreAVMTypeAAC:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeAAC;
        break;
    case eBTRCoreAVMTypeUnknown:
    default:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeUnknown;
        break;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_AcquireDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType,
    int*                aiDataPath,
    int*                aidataReadMTU,
    int*                aidataWriteMTU
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    int             liDataPath      = 0;
    int             lidataReadMTU   = 0;
    int             lidataWriteMTU  = 0;
    int             i32LoopIdx      = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!aiDataPath || !aidataReadMTU || !aidataWriteMTU || (aBTRCoreDevId < 0)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_INFO (" We will Acquire Data Path for %s\n", pDeviceAddress);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if (BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pDeviceAddress, &liDataPath, &lidataReadMTU, &lidataWriteMTU) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    *aiDataPath     = liDataPath;
    *aidataReadMTU  = lidataReadMTU;
    *aidataWriteMTU = lidataWriteMTU;


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState   = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;

        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState  != enBTRCoreDevStPlaying) {
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   = enBTRCoreDevStPlaying; 
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState  = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;

                if (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState  != enBTRCoreDevStPlaying) {
                    pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState   = enBTRCoreDevStPlaying; 
                }
            }
        }
    }


    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ReleaseDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_INFO (" We will Release Data Path for %s\n", pDeviceAddress);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if(BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pDeviceAddress) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_MediaControl (
    tBTRCoreHandle      hBTRCore, 
    tBTRCoreDevId       aBTRCoreDevId, 
    enBTRCoreDeviceType aenBTRCoreDevType,
    enBTRCoreMediaCtrl  aenBTRCoreMediaCtrl
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    BOOLEAN         lbBTDeviceConnected = FALSE; 
    int             loop = 0;
    enBTRCoreAVMediaCtrl aenBTRCoreAVMediaCtrl = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreBTDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->pcDevicePath;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }
    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call

    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDevicePath;
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].tDeviceId) {
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].bDeviceConnected;
                pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[loop].pcDevicePath;
            }
        }
    }

    if (lbBTDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }

    switch (aenBTRCoreMediaCtrl) {
    case enBTRCoreMediaCtrlPlay:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlPlay;
        break;
    case enBTRCoreMediaCtrlPause:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlPause;
        break;
    case enBTRCoreMediaCtrlStop:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlStop;
        break;
    case enBTRCoreMediaCtrlNext:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlNext;
        break;
    case enBTRCoreMediaCtrlPrevious:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlPrevious;
        break;
    case enBTRCoreMediaCtrlFastForward:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlFastForward;
        break;
    case enBTRCoreMediaCtrlRewind:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlRewind;
        break;
    case enBTRCoreMediaCtrlVolumeUp:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlVolumeUp;
        break;
    case enBTRCoreMediaCtrlVolumeDown:
        aenBTRCoreAVMediaCtrl = enBTRCoreAVMediaCtrlVolumeDown;
        break;
    default:
        break;
    }


    if (BTRCore_AVMedia_MediaControl(pstlhBTRCore->avMediaHdl,
                                     pstlhBTRCore->connHdl,
                                     pDeviceAddress,
                                     aenBTRCoreAVMediaCtrl) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Media Play Control Failed!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_GetMediaTrackInfo (
    tBTRCoreHandle            hBTRCore,
    tBTRCoreDevId             aBTRCoreDevId,
    enBTRCoreDeviceType       aenBTRCoreDevType,
    stBTRCoreMediaTrackInfo*  apstBTMediaTrackInfo
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    BOOLEAN         lbBTDeviceConnected = FALSE;
    int             loop = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }
    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }
    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDevicePath;
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected;

    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].tDeviceId) {
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].bDeviceConnected;
                pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[loop].pcDevicePath;
            }
        }
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (lbBTDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }

    if (BTRCore_AVMedia_GetTrackInfo(pstlhBTRCore->avMediaHdl,
                                     pstlhBTRCore->connHdl,
                                     pDeviceAddress,
                                     (stBTRCoreAVMediaTrackInfo*)apstBTMediaTrackInfo) != enBTRCoreSuccess)  {
        BTRCORELOG_ERROR ("AVMedia get media track information Failed!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_GetMediaPositionInfo (
    tBTRCoreHandle              hBTRCore,
    tBTRCoreDevId               aBTRCoreDevId,
    enBTRCoreDeviceType         aenBTRCoreDevType,
    stBTRCoreMediaPositionInfo* apstBTMediaPositionInfo
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    BOOLEAN         lbBTDeviceConnected = FALSE;
    int             loop = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }
    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }
    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDevicePath;
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].tDeviceId) {
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].bDeviceConnected;
                pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[loop].pcDevicePath;
            }
        }
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (lbBTDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }


   if (BTRCore_AVMedia_GetPositionInfo(pstlhBTRCore->avMediaHdl,
                                       pstlhBTRCore->connHdl,
                                       pDeviceAddress,
                                       (stBTRCoreAVMediaPositionInfo*)apstBTMediaPositionInfo) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia get Media Position Info Failed!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}



enBTRCoreRet
BTRCore_GetMediaProperty (
    tBTRCoreHandle            hBTRCore,
    tBTRCoreDevId             aBTRCoreDevId,
    enBTRCoreDeviceType       aenBTRCoreDevType,
    const char*               mediaPropertyKey,
    void*                     mediaPropertyValue
) {
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    BOOLEAN         lbBTDeviceConnected = FALSE;
    int             loop = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }
    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }
    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDevicePath;
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].tDeviceId) {
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].bDeviceConnected;
                pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[loop].pcDevicePath;
            }
        }
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (lbBTDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }

    if (BTRCore_AVMedia_GetMediaProperty(pstlhBTRCore->avMediaHdl,
                                         pstlhBTRCore->connHdl,
                                         pDeviceAddress,
                                         mediaPropertyKey,
                                         mediaPropertyValue) != enBTRCoreSuccess)  {
        BTRCORELOG_ERROR ("AVMedia get property Failed!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_ReportMediaPosition (
    tBTRCoreHandle       hBTRCore,
    tBTRCoreDevId        aBTRCoreDevId,
    enBTRCoreDeviceType  aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore        = NULL;
    const char*     pDeviceAddress      = NULL;
    const char*     pDevicePath         = NULL;
    BOOLEAN         lbBTDeviceConnected = FALSE;
    int             loop = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }
    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pDevicePath         = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDevicePath;
        pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].pcDeviceAddress;
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bDeviceConnected;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].tDeviceId) {
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].bDeviceConnected;
                pDeviceAddress      = pstlhBTRCore->stKnownDevicesArr[loop].pcDeviceAddress;
                pDevicePath         = pstlhBTRCore->stKnownDevicesArr[loop].pcDevicePath;
            }
        }
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (lbBTDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }

    if (BTRCore_AVMedia_StartMediaPositionPolling(pstlhBTRCore->avMediaHdl,
                                                  pstlhBTRCore->connHdl,
                                                  pDevicePath,
                                                  pDeviceAddress) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to set AVMedia report media position info!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


// Outgoing callbacks Registration Interfaces
enBTRCoreRet
BTRCore_RegisterDiscoveryCb (
    tBTRCoreHandle                  hBTRCore, 
    fPtr_BTRCore_DeviceDiscoveryCb  afpcBBTRCoreDeviceDiscovery,
    void*                           apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreDeviceDiscovery) {
        pstlhBTRCore->fpcBBTRCoreDeviceDiscovery = afpcBBTRCoreDeviceDiscovery;
        pstlhBTRCore->pvcBDevDiscUserData        = apUserData;
        BTRCORELOG_INFO ("Device Discovery Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("Device Discovery Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterStatusCb (
    tBTRCoreHandle          hBTRCore,
    fPtr_BTRCore_StatusCb   afpcBBTRCoreStatus,
    void*                   apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreStatus) {
        pstlhBTRCore->fpcBBTRCoreStatus = afpcBBTRCoreStatus;
        pstlhBTRCore->pvcBStatusUserData= apUserData; 
        BTRCORELOG_INFO ("BT Status Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Status Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterMediaStatusCb (
    tBTRCoreHandle              hBTRCore,
    fPtr_BTRCore_MediaStatusCb  afpcBBTRCoreMediaStatus,
    void*                       apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
       BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
       return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreMediaStatus) {
        pstlhBTRCore->fpcBBTRCoreMediaStatus = afpcBBTRCoreMediaStatus;
        pstlhBTRCore->pvcBMediaStatusUserData= apUserData;
        BTRCORELOG_INFO ("BT Media Status Callback Registered Successfully\n");
    }
    else {
       BTRCORELOG_INFO ("BT Media Status Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}

       
enBTRCoreRet
BTRCore_RegisterConnectionIntimationCb (
    tBTRCoreHandle           hBTRCore,
    fPtr_BTRCore_ConnIntimCb afpcBBTRCoreConnIntim,
    void*                    apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreConnIntim) {
        pstlhBTRCore->fpcBBTRCoreConnIntim = afpcBBTRCoreConnIntim;
        pstlhBTRCore->pvcBConnIntimUserData= apUserData;
        BTRCORELOG_INFO ("BT Conn In Intimation Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Conn In Intimation Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterConnectionAuthenticationCb (
    tBTRCoreHandle          hBTRCore,
    fPtr_BTRCore_ConnAuthCb afpcBBTRCoreConnAuth,
    void*                   apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreConnAuth) {
        pstlhBTRCore->fpcBBTRCoreConnAuth = afpcBBTRCoreConnAuth;
        pstlhBTRCore->pvcBConnAuthUserData= apUserData;
        BTRCORELOG_INFO ("BT Conn Auth Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Conn Auth Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


/*  Incoming Callbacks */
static int
btrCore_BTDeviceStatusUpdateCb (
    enBTDeviceType  aeBtDeviceType,
    enBTDeviceState aeBtDeviceState,
    stBTDeviceInfo* apstBTDeviceInfo,
    void*           apUserData
) {
    BTRCORELOG_INFO ("enBTDeviceType = %d enBTDeviceState = %d apstBTDeviceInfo = %p\n", aeBtDeviceType, aeBtDeviceState, apstBTDeviceInfo);

    enBTRCoreDeviceType lenBTRCoreDevType = enBTRCoreUnknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevType =  enBTRCoreSpeakers;
        break;
    case enBTDevAudioSource:
        if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_Tablet) {
           lenBTRCoreDevType = enBTRCorePCAudioIn;
        }
        break;
    case enBTDevHFPHeadset:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevHFPHeadsetGateway:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevUnknown:
    default:
        lenBTRCoreDevType = enBTRCoreUnknown;
        break;
    }

    switch (aeBtDeviceState) {
    case enBTDevStCreated: {
        break;
    }
    case enBTDevStScanInProgress: {
        break;
    }
    case enBTDevStFound: {
        if (apstBTDeviceInfo) {
            int j = 0;
            tBTRCoreDevId   lBTRCoreDevId = 0;
            stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

            BTRCORELOG_DEBUG ("bPaired          = %d\n", apstBTDeviceInfo->bPaired);
            BTRCORELOG_DEBUG ("bConnected       = %d\n", apstBTDeviceInfo->bConnected);
            BTRCORELOG_TRACE ("bTrusted         = %d\n", apstBTDeviceInfo->bTrusted);
            BTRCORELOG_TRACE ("bBlocked         = %d\n", apstBTDeviceInfo->bBlocked);
            BTRCORELOG_TRACE ("ui16Vendor       = %d\n", apstBTDeviceInfo->ui16Vendor);
            BTRCORELOG_TRACE ("ui16VendorSource = %d\n", apstBTDeviceInfo->ui16VendorSource);
            BTRCORELOG_TRACE ("ui16Product      = %d\n", apstBTDeviceInfo->ui16Product);
            BTRCORELOG_TRACE ("ui16Version      = %d\n", apstBTDeviceInfo->ui16Version);
            BTRCORELOG_DEBUG ("ui32Class        = %d\n", apstBTDeviceInfo->ui32Class);
            BTRCORELOG_DEBUG ("i32RSSI          = %d\n", apstBTDeviceInfo->i32RSSI);
            BTRCORELOG_DEBUG ("pcName           = %s\n", apstBTDeviceInfo->pcName);
            BTRCORELOG_DEBUG ("pcAddress        = %s\n", apstBTDeviceInfo->pcAddress);
            BTRCORELOG_TRACE ("pcAlias          = %s\n", apstBTDeviceInfo->pcAlias);
            BTRCORELOG_TRACE ("pcIcon           = %s\n", apstBTDeviceInfo->pcIcon);
            BTRCORELOG_TRACE ("pcDevicePath     = %s\n", apstBTDeviceInfo->pcDevicePath);

            for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
                if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                    break;
                else
                    BTRCORELOG_INFO ("aUUIDs = %s\n", apstBTDeviceInfo->aUUIDs[j]);
            }

            // TODO: Think of a way to move this to taskThread
            lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
            if (btrCore_GetScannedDeviceAddress(lpstlhBTRCore, lBTRCoreDevId) != NULL) {
                BTRCORELOG_INFO ("Already we have a entry in the list; Skip Parsing now \n");
            }
            else {
                lpstlhBTRCore->stFoundDevice.bFound         = FALSE;
                lpstlhBTRCore->stFoundDevice.i32RSSI        = apstBTDeviceInfo->i32RSSI;
                lpstlhBTRCore->stFoundDevice.ui32VendorId   = apstBTDeviceInfo->ui16Vendor;
                lpstlhBTRCore->stFoundDevice.tDeviceId      = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
                lpstlhBTRCore->stFoundDevice.enDeviceType   = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
                strncpy(lpstlhBTRCore->stFoundDevice.pcDeviceName, apstBTDeviceInfo->pcName, BD_NAME_LEN);
                strncpy(lpstlhBTRCore->stFoundDevice.pcDeviceAddress, apstBTDeviceInfo->pcAddress, BD_NAME_LEN);
                strncpy(lpstlhBTRCore->stFoundDevice.pcDevicePath, apstBTDeviceInfo->pcDevicePath, BD_NAME_LEN);

                BTRCORELOG_DEBUG ("Unique DevID = %lld\n", lpstlhBTRCore->stFoundDevice.tDeviceId);

                /* Populate the profile supported */
                for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
                    if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                        break;
                    else
                        lpstlhBTRCore->stFoundDevice.stDeviceProfile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                     lpstlhBTRCore->stFoundDevice.stDeviceProfile.profile[j].profile_name);
                }
                lpstlhBTRCore->stFoundDevice.stDeviceProfile.numberOfService = j;

                                            
                if (lpstlhBTRCore->stFoundDevice.enDeviceType == enBTRCore_DC_Unknown) {
                    for (j = 0; j < lpstlhBTRCore->stFoundDevice.stDeviceProfile.numberOfService; j++) {
                        if (lpstlhBTRCore->stFoundDevice.stDeviceProfile.profile[j].uuid_value ==  strtol(BTR_CORE_A2SNK, NULL, 16)) {
                            lpstlhBTRCore->stFoundDevice.enDeviceType = enBTRCore_DC_Loudspeaker;
                        }
                    }
                }


                btrCore_SetScannedDeviceInfo(lpstlhBTRCore);
                if (lpstlhBTRCore->fpcBBTRCoreDeviceDiscovery) {
                    stBTRCoreBTDevice  stFoundDevice;
                    memcpy (&stFoundDevice, &lpstlhBTRCore->stFoundDevice, sizeof(stFoundDevice));
                    if (lpstlhBTRCore->fpcBBTRCoreDeviceDiscovery(stFoundDevice, lpstlhBTRCore->pvcBDevDiscUserData) != enBTRCoreSuccess) {
                        //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
                    }
                }
            }
        }

        break;
    }
    case enBTDevStLost: {
        stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

        if (lpstlhBTRCore && apstBTDeviceInfo) {
            int     i32LoopIdx      = 0;

            tBTRCoreDevId   lBTRCoreDevId     = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

            if (lpstlhBTRCore->numOfPairedDevices) {
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                    if (lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId == lBTRCoreDevId) {
                        BTRCORELOG_INFO ("Device %llu power state Off or OOR", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId);
                        BTRCORELOG_TRACE ("i32LoopIdx = %d\n", i32LoopIdx);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState);

                        if (((lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState == enBTRCoreDevStConnected) ||
                             (lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState == enBTRCoreDevStPlaying)) &&
                            (lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStDisconnecting)) {
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState =  lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStDisconnected;
                        }
                        else {
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState =  lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStLost;
                        }

                        lpstlhBTRCore->stDevStatusCbInfo.deviceId         = lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId;
                        lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass     = lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].enDeviceType;
                        lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState;
                        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                        lpstlhBTRCore->stDevStatusCbInfo.eDeviceType      = lenBTRCoreDevType;
                        lpstlhBTRCore->stDevStatusCbInfo.isPaired         = 1;
                        strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName, lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].pcDeviceName, BD_NAME_LEN - 1);

                        if (lpstlhBTRCore->fpcBBTRCoreStatus) {
                            if (lpstlhBTRCore->fpcBBTRCoreStatus(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
                                //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
                            }
                        }
                        break;
                    }
                }
            }
        }
        break;
    }
    case enBTDevStPairingRequest: {
        break;
    }
    case enBTDevStPairingInProgress: {
        break;
    }
    case enBTDevStPaired: {
        break;
    }
    case enBTDevStUnPaired: {
        break;
    }
    case enBTDevStConnectInProgress: {
        break;
    }
    case enBTDevStConnected: {
        break;
    }
    case enBTDevStDisconnected: {
        break;
    }
    case enBTDevStPropChanged: {
        stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

        if (lpstlhBTRCore && apstBTDeviceInfo) {
            int     i32LoopIdx      = 0;
            int     i32KnownDevIdx  = -1;
            int     i32ScannedDevIdx= -1;

            tBTRCoreDevId        lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
            enBTRCoreDeviceState leBTDevState  = btrCore_BTParseDeviceConnectionState(apstBTDeviceInfo->pcDeviceCurrState);


            if (lpstlhBTRCore->numOfPairedDevices) {
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                    if (lBTRCoreDevId == lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                        i32KnownDevIdx = i32LoopIdx;
                        break;
                    }
                }
            }


            if (lpstlhBTRCore->numOfScannedDevices) {
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
                    if (lBTRCoreDevId == lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
                        i32ScannedDevIdx = i32LoopIdx;
                        break;
                    }
                }
            }


            // Current device for which Property has changed must be either in Found devices or Paired devices
            // TODO: if-else's for SM or HSM are bad. Find a better way
            if (((i32ScannedDevIdx != -1) || (i32KnownDevIdx != -1)) && (leBTDevState != enBTRCoreDevStInitialized)) {
                BOOLEAN bTriggerDevStatusChangeCb = FALSE;

                if (i32KnownDevIdx != -1) {

                    BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);
                    BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);

                    if ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != leBTDevState) &&
                        (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                        if ((enBTRCoreMobileAudioIn != lenBTRCoreDevType) && (enBTRCorePCAudioIn != lenBTRCoreDevType)) {

                            if ( !(((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected)    && (leBTDevState == enBTRCoreDevStDisconnected)) ||
                                   ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) && (leBTDevState == enBTRCoreDevStConnected) && 
                                    ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStPaired) || 
                                     (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStConnecting))))) {
                                bTriggerDevStatusChangeCb = TRUE;
                            }


                            //workaround for notifying the power Up event of a <paired && !connected> devices, as we are not able to track the
                            //power Down event of such devices as per the current analysis
                            if ((leBTDevState == enBTRCoreDevStDisconnected) &&
                                ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected) ||
                                 (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStDisconnecting))) {
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = enBTRCoreDevStPaired;
                            }
                            else if ( !((leBTDevState == enBTRCoreDevStConnected) &&
                                        ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnecting) ||
                                         (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected)  ||
                                         (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStInitialized)))) {
                               lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                            }
                         

                            if ((leBTDevState == enBTRCoreDevStDisconnected) &&
                                (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStPlaying)) {
                                leBTDevState = enBTRCoreDevStLost;
                            }


                            if ( !((leBTDevState == enBTRCoreDevStDisconnected) &&
                                   (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStLost) &&
                                   (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStPlaying))) {

                                if ( !((leBTDevState == enBTRCoreDevStConnected) &&
                                     (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStLost) &&
                                     (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStPlaying))) {
                                    lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                    lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;
                                }
                                else {
                                    lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStConnecting;
                                }
                            }
                            else {
                                leBTDevState = enBTRCoreDevStConnected;
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStConnecting;
                            }

                        }
                        else {
                            bTriggerDevStatusChangeCb = TRUE;

                            if (enBTRCoreDevStDisconnected == leBTDevState) {
                                lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
                                BTRCore_AVMedia_ExitMediaPositionPolling (lpstlhBTRCore->avMediaHdl);
                            }

                            if (enBTRCoreDevStInitialized != lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState) {
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState =
                                                               lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                            }
                            else {
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = enBTRCoreDevStConnecting;
                            }

                            lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;

                            //TODO: There should be no need to do this. Find out why the enDeviceType = 0;
                            lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType = 
                                                            btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
                        }

                        lpstlhBTRCore->stDevStatusCbInfo.isPaired = 1;

                        BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);
                        BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType       = %x\n", lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType);
                    }
                }
                else if (i32ScannedDevIdx != -1) {

                    BTRCORELOG_TRACE ("i32ScannedDevIdx = %d\n", i32ScannedDevIdx);
                    BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState);

                    if ((lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != leBTDevState) &&
                        (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                        lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                        lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState = leBTDevState;
                        lpstlhBTRCore->stDevStatusCbInfo.isPaired = apstBTDeviceInfo->bPaired;

                        if (apstBTDeviceInfo->bPaired       &&
                            apstBTDeviceInfo->bConnected    &&
                            (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState == enBTRCoreDevStFound) &&
                            (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState == enBTRCoreDevStConnected)) {
                            btrCore_AddDeviceToKnownDevicesArr(lpstlhBTRCore, apstBTDeviceInfo);
                        }

                        bTriggerDevStatusChangeCb = TRUE;
                    }
                }

                if (bTriggerDevStatusChangeCb == TRUE) {
                    lpstlhBTRCore->stDevStatusCbInfo.deviceId         = lBTRCoreDevId;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceType      = lenBTRCoreDevType;
                    lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = leBTDevState;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass     = lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType;
                    strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName, lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceName, BD_NAME_LEN - 1);

                    if (lpstlhBTRCore->fpcBBTRCoreStatus) {
                        if (lpstlhBTRCore->fpcBBTRCoreStatus(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
                            /* Invoke the callback */
                            //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
                        }
                    }
                }
            }
        }

        break;
    }
    case enBTDevStUnknown: {
        break;
    }
    default: {
        break;
    }
    }

    return 0;
}


static int
btrCore_BTDeviceConnectionIntimationCb (
    enBTDeviceType  aeBtDeviceType,
    stBTDeviceInfo* apstBTDeviceInfo,
    unsigned int    aui32devPassKey,
    void*           apUserData
) {
    int                 i32DevConnIntimRet = 0;
    int                 j = 0;
    stBTRCoreHdl*       lpstlhBTRCore = (stBTRCoreHdl*)apUserData;
    enBTRCoreDeviceType lenBTRCoreDevType = enBTRCoreUnknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevType =  enBTRCoreSpeakers;
        break;
    case enBTDevAudioSource:
        if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_Tablet) {
           lenBTRCoreDevType = enBTRCorePCAudioIn;
        }
        break;
    case enBTDevHFPHeadset:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevHFPHeadsetGateway:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevUnknown:
    default:
        lenBTRCoreDevType = enBTRCoreUnknown;
        break;
    }


    if (lpstlhBTRCore) {
        BTRCORELOG_DEBUG("btrCore_BTDeviceConnectionIntimationCb\n");
        lpstlhBTRCore->stConnCbInfo.ui32devPassKey = aui32devPassKey;
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        lpstlhBTRCore->stConnCbInfo.stFoundDevice.bFound         = TRUE;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.i32RSSI        = apstBTDeviceInfo->i32RSSI;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.ui32VendorId   = apstBTDeviceInfo->ui16Vendor;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.tDeviceId      = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
        BTRCORELOG_DEBUG ("Unique DevID = %lld\n", lpstlhBTRCore->stConnCbInfo.stFoundDevice.tDeviceId);
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.enDeviceType   = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        strcpy(lpstlhBTRCore->stConnCbInfo.stFoundDevice.pcDeviceName, apstBTDeviceInfo->pcName);
        strcpy(lpstlhBTRCore->stConnCbInfo.stFoundDevice.pcDeviceAddress, apstBTDeviceInfo->pcAddress);

        /* Populate the profile supported */
        for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
            if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                break;
            else
                lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                          lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.profile[j].profile_name);
        }
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.numberOfService = j;

                                    
        if (lpstlhBTRCore->stConnCbInfo.stFoundDevice.enDeviceType == enBTRCore_DC_Unknown) {
            for (j = 0; j < lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.numberOfService; j++) {
                if (lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.profile[j].uuid_value ==  strtol(BTR_CORE_A2SRC, NULL, 16)) {
                    lpstlhBTRCore->stConnCbInfo.stFoundDevice.enDeviceType = enBTRCore_DC_SmartPhone;
                }
            }
        }

        if ((lenBTRCoreDevType == enBTRCoreMobileAudioIn) || (lenBTRCoreDevType == enBTRCorePCAudioIn)) {
            if (lpstlhBTRCore->fpcBBTRCoreConnIntim) {
                if (lpstlhBTRCore->fpcBBTRCoreConnIntim(&lpstlhBTRCore->stConnCbInfo, &i32DevConnIntimRet, lpstlhBTRCore->pvcBConnIntimUserData) != enBTRCoreSuccess) {
                    //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
                }
            }
        }
        else if ((lenBTRCoreDevType == enBTRCoreSpeakers) || (lenBTRCoreDevType == enBTRCoreHeadSet)) {
            if (lpstlhBTRCore->numOfPairedDevices) {
                unsigned int i32LoopIdx = 0;

                //TODO: Even before we loop, check if we are already connected and playing Audio-Out 
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                    if (lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId == lpstlhBTRCore->stConnCbInfo.stFoundDevice.tDeviceId) {
                        BTRCORELOG_DEBUG("ACCEPT INCOMING INTIMATION stKnownDevice : %s", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].pcDeviceName);
                        i32DevConnIntimRet = 1;
                    }
                }
            }
        }
    }

    return i32DevConnIntimRet;
}


static int
btrCore_BTDeviceAuthenticationCb (
    enBTDeviceType  aeBtDeviceType,
    stBTDeviceInfo* apstBTDeviceInfo,
    void*           apUserData
) {
    int i32DevAuthRet = 0;
    int j = 0;
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;
    enBTRCoreDeviceType lenBTRCoreDevType = enBTRCoreUnknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevType =  enBTRCoreSpeakers;
        break;
    case enBTDevAudioSource:
        if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class) == enBTRCore_DC_Tablet) {
           lenBTRCoreDevType = enBTRCorePCAudioIn;
        }
        break;
    case enBTDevHFPHeadset:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevHFPHeadsetGateway:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevUnknown:
    default:
        lenBTRCoreDevType = enBTRCoreUnknown;
        break;
    }


    if (lpstlhBTRCore) {
        BTRCORELOG_DEBUG("btrCore_BTDeviceAuthenticationCb\n");
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        lpstlhBTRCore->stConnCbInfo.stKnownDevice.bFound         = TRUE;
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.ui32VendorId   = apstBTDeviceInfo->ui16Vendor;
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId      = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
        BTRCORELOG_DEBUG ("Unique DevID = %lld\n", lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId);
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.enDeviceType   = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.pcDeviceName,    apstBTDeviceInfo->pcName);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.pcDeviceAddress, apstBTDeviceInfo->pcAddress);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.pcDevicePath,    apstBTDeviceInfo->pcAddress); // Do we need to expose BT-Ifce path?

        for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
            if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                break;
            else
                lpstlhBTRCore->stConnCbInfo.stKnownDevice.stDeviceProfile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                          lpstlhBTRCore->stConnCbInfo.stFoundDevice.stDeviceProfile.profile[j].profile_name);
        }
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.stDeviceProfile.numberOfService = j;

        if (lpstlhBTRCore->stConnCbInfo.stKnownDevice.enDeviceType == enBTRCore_DC_Unknown) {
            for (j = 0; j < lpstlhBTRCore->stConnCbInfo.stKnownDevice.stDeviceProfile.numberOfService; j++) {
                if (lpstlhBTRCore->stConnCbInfo.stKnownDevice.stDeviceProfile.profile[j].uuid_value == strtol(BTR_CORE_A2SRC, NULL, 16)) {
                    lpstlhBTRCore->stConnCbInfo.stKnownDevice.enDeviceType = enBTRCore_DC_SmartPhone;
                }
            }
        }

        if (lpstlhBTRCore->fpcBBTRCoreConnAuth) {
            if (lpstlhBTRCore->fpcBBTRCoreConnAuth(&lpstlhBTRCore->stConnCbInfo, &i32DevAuthRet, lpstlhBTRCore->pvcBConnAuthUserData) != enBTRCoreSuccess) {
                //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
            }
        }


        if (lpstlhBTRCore->numOfPairedDevices && i32DevAuthRet) {

            if ((lenBTRCoreDevType == enBTRCoreMobileAudioIn) ||
                (lenBTRCoreDevType == enBTRCorePCAudioIn)) {
                unsigned int i32LoopIdx = 0;

                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                    if (lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId == lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId) {
                        BTRCORELOG_DEBUG("stKnownDevice.device_connected set : %d", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected);
                        lpstlhBTRCore->stConnCbInfo.stKnownDevice.bDeviceConnected = TRUE;
                        lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected = TRUE;
                    }
                }
            }
            else if ((lenBTRCoreDevType == enBTRCoreSpeakers) || 
                     (lenBTRCoreDevType == enBTRCoreHeadSet)) {
                unsigned int i32LoopIdx = 0;

                //TODO: Even before we loop, check if we are already connected and playing Audio-Out 
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                    if (lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId == lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId) {
                        BTRCORELOG_DEBUG("ACCEPTED INCOMING CONNECT stKnownDevice : %s", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].pcDeviceName);
                        lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;

                        if (lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState != enBTRCoreDevStPlaying)
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStConnected;
                    }
                }
            }
            
        }

    }

    return i32DevAuthRet;
}


static enBTRCoreRet
btrCore_BTMediaStatusUpdateCb (
    void*        apMediaStreamStatus,
    const char*  apBtdevAddr,
    void*        apUserData
) {
    if (!apMediaStreamStatus || !apBtdevAddr || !apUserData) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!");
       return enBTRCoreInvalidArg;
    }

    stBTRCoreHdl*   lpstlhBTRCore  = (stBTRCoreHdl*)apUserData;
    tBTRCoreDevId   lBTRCoreDevId  = btrCore_GenerateUniqueDeviceID(apBtdevAddr);
    int             i32LoopIdx     = 0;
    int             i32KnownDevIdx = -1;

    if (lpstlhBTRCore->numOfPairedDevices) {
       for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
           if (lBTRCoreDevId == lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
              i32KnownDevIdx = i32LoopIdx;
              break;
           }
       }
    }

    if (i32KnownDevIdx == -1) {
       BTRCORELOG_ERROR ("Failed to find device in paired devices list!!!\n");
       return enBTRCoreDeviceNotFound;
    }

    if (lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected == FALSE) {
       BTRCORELOG_ERROR ("Device is not connected!!!\n");
       return enBTRCoreFailure;
    }

    lpstlhBTRCore->stMediaStatusCbInfo.deviceId      = lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId;
    lpstlhBTRCore->stMediaStatusCbInfo.eDeviceClass  = lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType;
    strncpy(lpstlhBTRCore->stMediaStatusCbInfo.deviceName, lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceName, BD_NAME_LEN-1);
    memcpy(&lpstlhBTRCore->stMediaStatusCbInfo.m_mediaStatusUpdate, apMediaStreamStatus, sizeof(stBTRCoreMediaStatusCBInfo));

    if (lpstlhBTRCore->fpcBBTRCoreMediaStatus) {
        if (lpstlhBTRCore->fpcBBTRCoreMediaStatus(&lpstlhBTRCore->stMediaStatusCbInfo, lpstlhBTRCore->pvcBMediaStatusUserData) != enBTRCoreSuccess) {
            /* Invoke the callback */
            //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
        }
    }

    return enBTRCoreSuccess;
}

/* End of File */
