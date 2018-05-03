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

/* System Headers */
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for getpid
#include <sched.h>      //for StopDiscovery test
#include <string.h>     //for strcnp
#include <errno.h>      //for error numbers

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrCore_logger.h"

/* Local Headers */
#include "btrCore.h"
#include "btrCore_service.h"

#include "btrCore_avMedia.h"
#include "btrCore_le.h"

#include "btrCore_bt_ifce.h"


#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

/* Local types */
//TODO: Move to a private header
typedef enum _enBTRCoreTaskOp {
    enBTRCoreTaskOpStart,
    enBTRCoreTaskOpStop,
    enBTRCoreTaskOpIdle,
    enBTRCoreTaskOpProcess,
    enBTRCoreTaskOpExit,
    enBTRCoreTaskOpUnknown
} enBTRCoreTaskOp;

typedef enum _enBTRCoreTaskProcessType {
   enBTRCoreTaskPTcBDeviceDisc,
   enBTRCoreTaskPTcBStatus,
   enBTRCoreTaskPTcBMediaStatus,
   enBTRCoreTaskPTcBConnIntim,
   enBTRCoreTaskPTcBConnAuth,
   enBTRCoreTaskPTUnknown
} enBTRCoreTaskProcessType;


typedef struct _stBTRCoreTaskGAqData {
    enBTRCoreTaskOp             enBTRCoreTskOp;
    enBTRCoreTaskProcessType    enBTRCoreTskPT;
    void*                       pvBTRCoreTskInData;
} stBTRCoreTaskGAqData;


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


    stBTRCoreDevStatusCBInfo        stDevStatusCbInfo;
    stBTRCoreMediaStatusCBInfo      stMediaStatusCbInfo;
    stBTRCoreConnCBInfo             stConnCbInfo;

    fPtr_BTRCore_DeviceDiscCb       fpcBBTRCoreDeviceDisc;
    fPtr_BTRCore_StatusCb           fpcBBTRCoreStatus;
    fPtr_BTRCore_MediaStatusCb      fpcBBTRCoreMediaStatus;
    fPtr_BTRCore_ConnIntimCb        fpcBBTRCoreConnIntim; 
    fPtr_BTRCore_ConnAuthCb         fpcBBTRCoreConnAuth; 

    void*                           pvcBDevDiscUserData;
    void*                           pvcBStatusUserData;
    void*                           pvcBMediaStatusUserData;
    void*                           pvcBConnIntimUserData;
    void*                           pvcBConnAuthUserData;


    GThread*                        pThreadRunTask;
    GAsyncQueue*                    pGAQueueRunTask;

    GThread*                        pThreadOutTask;
    GAsyncQueue*                    pGAQueueOutTask;

} stBTRCoreHdl;


/* Static Function Prototypes */
static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevId btrCore_GenerateUniqueDeviceID (const char* apcDeviceAddress);
static enBTRCoreDeviceClass btrCore_MapClassIDtoDeviceType(unsigned int classID);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static int btrCore_AddDeviceToScannedDevicesArr (stBTRCoreHdl* apsthBTRCore, stBTDeviceInfo* apstBTDeviceInfo, stBTRCoreBTDevice* apstFoundDevice); 
static int btrCore_AddDeviceToKnownDevicesArr (stBTRCoreHdl* apsthBTRCore, stBTDeviceInfo* apstBTDeviceInfo);
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static void btrCore_MapKnownDeviceListFromPairedDeviceInfo (stBTRCoreBTDevice* knownDevicesArr, stBTPairedDeviceInfo* pairedDeviceInfo);
static const char* btrCore_GetKnownDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceMac (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_ShowSignalStrength (short strength);
static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut);
static enBTRCoreDeviceState btrCore_BTParseDeviceConnectionState (const char* pcStateValue);

static enBTRCoreRet btrCore_RunTaskAddOp (GAsyncQueue* apRunTaskGAq, enBTRCoreTaskOp aenRunTaskOp, enBTRCoreTaskProcessType aenRunTaskPT, void* apvRunTaskInData);
static enBTRCoreRet btrCore_OutTaskAddOp (GAsyncQueue* apOutTaskGAq, enBTRCoreTaskOp aenOutTaskOp, enBTRCoreTaskProcessType aenOutTaskPT, void* apvOutTaskInData);

/* Local Op Threads Prototypes */
static gpointer btrCore_RunTask (gpointer apsthBTRCore);
static gpointer btrCore_OutTask (gpointer apsthBTRCore);

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

    
    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stKnownDevicesArr[i].tDeviceId = 0;
        apsthBTRCore->stKnownDevicesArr[i].bDeviceConnected = FALSE;
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

    apsthBTRCore->fpcBBTRCoreDeviceDisc     = NULL;
    apsthBTRCore->fpcBBTRCoreStatus         = NULL;
    apsthBTRCore->fpcBBTRCoreMediaStatus    = NULL;
    apsthBTRCore->fpcBBTRCoreConnIntim      = NULL;
    apsthBTRCore->fpcBBTRCoreConnAuth       = NULL;

    apsthBTRCore->pvcBDevDiscUserData       = NULL;
    apsthBTRCore->pvcBStatusUserData        = NULL;
    apsthBTRCore->pvcBMediaStatusUserData   = NULL;
    apsthBTRCore->pvcBConnIntimUserData     = NULL;
    apsthBTRCore->pvcBConnAuthUserData      = NULL;

    apsthBTRCore->pThreadRunTask            = NULL;
    apsthBTRCore->pGAQueueRunTask           = NULL;
                     
    apsthBTRCore->pThreadOutTask            = NULL;
    apsthBTRCore->pGAQueueOutTask           = NULL;

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

    if (classID == enBTRCore_DC_Tile) {
        BTRCORELOG_INFO ("enBTRCore_DC_Tile\n");
        rc = enBTRCore_DC_Tile;
    } else
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
        apsthBTRCore->stScannedDevicesArr[i].tDeviceId          = 0;
        apsthBTRCore->stScannedDevicesArr[i].i32RSSI            = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].bFound             = FALSE;
        apsthBTRCore->stScannedDevicesArr[i].bDeviceConnected   = FALSE;
        apsthBTRCore->stScannedDevicesArr[i].enDeviceType       = enBTRCore_DC_Unknown;

        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceName,     '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].pcDeviceName));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress,  '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDevicePath,     '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].pcDevicePath));

        apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    apsthBTRCore->numOfScannedDevices = 0;
}


static const char*
btrCore_GetScannedDeviceAddress (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int i = 0;

    if (apsthBTRCore->numOfScannedDevices) {
        for (i = 0; i < apsthBTRCore->numOfScannedDevices; i++) {
            if (aBTRCoreDevId == apsthBTRCore->stScannedDevicesArr[i].tDeviceId)
                return apsthBTRCore->stScannedDevicesArr[i].pcDevicePath;
        }
    }

    return NULL;
}


static int
btrCore_AddDeviceToScannedDevicesArr (
    stBTRCoreHdl*       apsthBTRCore,
    stBTDeviceInfo*     apstBTDeviceInfo,
    stBTRCoreBTDevice*  apstFoundDevice
) {
    int                 i;
    stBTRCoreBTDevice   lstFoundDevice;

    memset(&lstFoundDevice, 0, sizeof(stBTRCoreBTDevice));


    lstFoundDevice.bFound       = FALSE;
    lstFoundDevice.i32RSSI      = apstBTDeviceInfo->i32RSSI;
    lstFoundDevice.ui32VendorId = apstBTDeviceInfo->ui16Vendor;
    lstFoundDevice.tDeviceId    = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
    lstFoundDevice.enDeviceType = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
    strncpy(lstFoundDevice.pcDeviceName,      apstBTDeviceInfo->pcName,       BD_NAME_LEN);
    strncpy(lstFoundDevice.pcDeviceAddress,   apstBTDeviceInfo->pcAddress,    BD_NAME_LEN);
    strncpy(lstFoundDevice.pcDevicePath,      apstBTDeviceInfo->pcDevicePath, BD_NAME_LEN);

    /* Populate the profile supported */
    for (i = 0; i < BT_MAX_DEVICE_PROFILE; i++) {
        if (apstBTDeviceInfo->aUUIDs[i][0] == '\0')
            break;
        else
            lstFoundDevice.stDeviceProfile.profile[i].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[i],
                                                                                            lstFoundDevice.stDeviceProfile.profile[i].profile_name);
    }
    lstFoundDevice.stDeviceProfile.numberOfService = i;

    if (lstFoundDevice.enDeviceType == enBTRCore_DC_Unknown) {
        for (i = 0; i < lstFoundDevice.stDeviceProfile.numberOfService; i++) {
            if (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_A2SNK, NULL, 16)) {
                lstFoundDevice.enDeviceType = enBTRCore_DC_Loudspeaker;
            }
            else if (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_A2SRC, NULL, 16)) {
                lstFoundDevice.enDeviceType = enBTRCore_DC_SmartPhone;
            }
            else if (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_GATT_TILE_1, NULL, 16) ||
                     lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_GATT_TILE_2, NULL, 16) ){
                lstFoundDevice.enDeviceType = enBTRCore_DC_Tile;
            }
        }
    }


    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if ((lstFoundDevice.tDeviceId == apsthBTRCore->stScannedDevicesArr[i].tDeviceId) || (apsthBTRCore->stScannedDevicesArr[i].bFound == FALSE)) {
            BTRCORELOG_DEBUG ("Unique DevID = %lld\n", lstFoundDevice.tDeviceId);
            BTRCORELOG_DEBUG ("Adding %s at location %d\n", lstFoundDevice.pcDeviceAddress, i);
            apsthBTRCore->stScannedDevicesArr[i].bFound         = TRUE;     //mark the record as found
            apsthBTRCore->stScannedDevicesArr[i].i32RSSI        = lstFoundDevice.i32RSSI;
            apsthBTRCore->stScannedDevicesArr[i].ui32VendorId   = lstFoundDevice.ui32VendorId;
            apsthBTRCore->stScannedDevicesArr[i].enDeviceType   = lstFoundDevice.enDeviceType;
            apsthBTRCore->stScannedDevicesArr[i].tDeviceId      = lstFoundDevice.tDeviceId;

            strncpy(apsthBTRCore->stScannedDevicesArr[i].pcDeviceName,   lstFoundDevice.pcDeviceName,    BD_NAME_LEN);
            strncpy(apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress,lstFoundDevice.pcDeviceAddress, BD_NAME_LEN);
            strncpy(apsthBTRCore->stScannedDevicesArr[i].pcDevicePath,   lstFoundDevice.pcDevicePath,    BD_NAME_LEN);
            
            /* Copy the profile supports */
            memcpy (&apsthBTRCore->stScannedDevicesArr[i].stDeviceProfile, &lstFoundDevice.stDeviceProfile, sizeof(stBTRCoreSupportedServiceList));

            apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState;
            apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStFound;

            apsthBTRCore->numOfScannedDevices++;

            break;
        }
    }

    memcpy(apstFoundDevice, &lstFoundDevice, sizeof(stBTRCoreBTDevice));

    return i;
}


static int
btrCore_AddDeviceToKnownDevicesArr (
    stBTRCoreHdl*   apsthBTRCore,
    stBTDeviceInfo* apstBTDeviceInfo
) {
    tBTRCoreDevId   ltDeviceId;
    int             i32LoopIdx      = 0;
    int             i32KnownDevIdx  = -1;
    int             i32ScannedDevIdx= -1;


    ltDeviceId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

    for (i32LoopIdx = 0; i32LoopIdx < apsthBTRCore->numOfPairedDevices; i32LoopIdx++) {
        if (ltDeviceId == apsthBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
            i32KnownDevIdx = i32LoopIdx;
            break;
        }
    }

    if (i32KnownDevIdx != -1) {
        BTRCORELOG_DEBUG ("Already Present in stKnownDevicesArr - DevID = %lld\n", ltDeviceId);
        return i32KnownDevIdx;
    }

    if (apsthBTRCore->numOfPairedDevices >= BTRCORE_MAX_NUM_BT_DEVICES) {
        BTRCORELOG_ERROR ("No Space in stKnownDevicesArr - DevID = %lld\n", ltDeviceId);
        return i32KnownDevIdx;
    }

    for (i32LoopIdx = 0; i32LoopIdx < apsthBTRCore->numOfScannedDevices; i32LoopIdx++) {
        if (ltDeviceId == apsthBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
            i32ScannedDevIdx = i32LoopIdx;
            break;
        }
    }

    if (i32ScannedDevIdx == -1) {
        BTRCORELOG_DEBUG ("Not Present in stScannedDevicesArr - DevID = %lld\n", ltDeviceId);
        return i32ScannedDevIdx;
    }


    i32KnownDevIdx = apsthBTRCore->numOfPairedDevices++;
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

    BTRCORELOG_TRACE ("Added in stKnownDevicesArr - DevID = %lld  i32KnownDevIdx = %d  NumOfPairedDevices = %d\n", ltDeviceId, i32KnownDevIdx, apsthBTRCore->numOfPairedDevices);

    return i32KnownDevIdx;
}

static void
btrCore_MapKnownDeviceListFromPairedDeviceInfo (
    stBTRCoreBTDevice*      knownDevicesArr,
    stBTPairedDeviceInfo*   pairedDeviceInfo
) {
    unsigned char i_idx = 0;
    unsigned char j_idx = 0;
  
    for (i_idx = 0; i_idx < pairedDeviceInfo->numberOfDevices; i_idx++) {
        knownDevicesArr[i_idx].ui32VendorId         = pairedDeviceInfo->deviceInfo[i_idx].ui16Vendor;
        knownDevicesArr[i_idx].bDeviceConnected     = pairedDeviceInfo->deviceInfo[i_idx].bConnected;
        knownDevicesArr[i_idx].tDeviceId            = btrCore_GenerateUniqueDeviceID(pairedDeviceInfo->deviceInfo[i_idx].pcAddress);
        knownDevicesArr[i_idx].enDeviceType         = btrCore_MapClassIDtoDeviceType(pairedDeviceInfo->deviceInfo[i_idx].ui32Class);
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

    /* Initially stBTRCoreKnownDevice List is populated from pairedDeviceInfo(bluez i/f) directly *********/  
    if (!apsthBTRCore->numOfPairedDevices) { 
        apsthBTRCore->numOfPairedDevices = pairedDeviceInfo.numberOfDevices;
        btrCore_MapKnownDeviceListFromPairedDeviceInfo (knownDevicesArr, &pairedDeviceInfo); 
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
                    apsthBTRCore->stKnownDevicesArr[apsthBTRCore->numOfPairedDevices].bFound             = TRUE; // Paired Now
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

        else if (!strcasecmp(aUUID, BTR_CORE_GATT_TILE_1))
            strcpy(pServiceNameOut, BTR_CORE_GATT_TILE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_GATT_TILE_2))
            strcpy(pServiceNameOut, BTR_CORE_GATT_TILE_TEXT);

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


static enBTRCoreRet
btrCore_RunTaskAddOp (
    GAsyncQueue*                apRunTaskGAq,
    enBTRCoreTaskOp             aenRunTaskOp,
    enBTRCoreTaskProcessType    aenRunTaskPT,
    void*                       apvRunTaskInData
) {
    stBTRCoreTaskGAqData*       lpstRunTaskGAqData = NULL;

    if ((apRunTaskGAq == NULL) || (aenRunTaskOp == enBTRCoreTaskOpUnknown)) {
        return enBTRCoreInvalidArg;
    }


    if (!(lpstRunTaskGAqData = g_malloc0(sizeof(stBTRCoreTaskGAqData)))) {
        return enBTRCoreFailure;
    }


    lpstRunTaskGAqData->enBTRCoreTskOp = aenRunTaskOp;
    switch (lpstRunTaskGAqData->enBTRCoreTskOp) {
    case enBTRCoreTaskOpStart:
        break;
    case enBTRCoreTaskOpStop:
        break;
    case enBTRCoreTaskOpIdle:
        break;
    case enBTRCoreTaskOpProcess:
        break;
    case enBTRCoreTaskOpExit:
        break;
    case enBTRCoreTaskOpUnknown:
    default:
        break;
    }


    lpstRunTaskGAqData->enBTRCoreTskPT = aenRunTaskPT;
    if (lpstRunTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTUnknown) {
    }
    else {
    }


    BTRCORELOG_INFO ("g_async_queue_push %d %d %p\n", lpstRunTaskGAqData->enBTRCoreTskOp, lpstRunTaskGAqData->enBTRCoreTskPT, lpstRunTaskGAqData->pvBTRCoreTskInData);
    g_async_queue_push(apRunTaskGAq, lpstRunTaskGAqData);


    return enBTRCoreSuccess;
}

static enBTRCoreRet
btrCore_OutTaskAddOp (
    GAsyncQueue*                apOutTaskGAq,
    enBTRCoreTaskOp             aenOutTaskOp,
    enBTRCoreTaskProcessType    aenOutTaskPT,
    void*                       apvOutTaskInData
) {
    stBTRCoreTaskGAqData*       lpstOutTaskGAqData = NULL;

    if ((apOutTaskGAq == NULL) || (aenOutTaskOp == enBTRCoreTaskOpUnknown)) {
        return enBTRCoreInvalidArg;
    }


    if (!(lpstOutTaskGAqData = g_malloc0(sizeof(stBTRCoreTaskGAqData)))) {
        return enBTRCoreFailure;
    }


    lpstOutTaskGAqData->enBTRCoreTskOp = aenOutTaskOp;
    switch (lpstOutTaskGAqData->enBTRCoreTskOp) {
    case enBTRCoreTaskOpStart:
        break;
    case enBTRCoreTaskOpStop:
        break;
    case enBTRCoreTaskOpIdle:
        break;
    case enBTRCoreTaskOpProcess:
        break;
    case enBTRCoreTaskOpExit:
        break;
    case enBTRCoreTaskOpUnknown:
    default:
        break;
    }


    lpstOutTaskGAqData->enBTRCoreTskPT = aenOutTaskPT;
    if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBDeviceDisc) {
        if ((lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTDeviceInfo)))) {
            memcpy(lpstOutTaskGAqData->pvBTRCoreTskInData, (stBTDeviceInfo*)apvOutTaskInData, sizeof(stBTDeviceInfo));
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBStatus) {
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBMediaStatus) {
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBConnIntim) {
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBConnAuth) {
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTUnknown) {
    }
    else {
    }


    BTRCORELOG_INFO ("g_async_queue_push %d %d %p\n", lpstOutTaskGAqData->enBTRCoreTskOp, lpstOutTaskGAqData->enBTRCoreTskPT, lpstOutTaskGAqData->pvBTRCoreTskInData);
    g_async_queue_push(apOutTaskGAq, lpstOutTaskGAqData);


    return enBTRCoreSuccess;
}


/*  Local Op Threads */
static gpointer
btrCore_RunTask (
    gpointer        apsthBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore        = NULL;
    enBTRCoreRet*   penExitStatusRunTask= NULL;
    enBTRCoreRet    lenBTRCoreRet       = enBTRCoreSuccess;

    gint64                      li64usTimeout       = 0;
    guint16                     lui16msTimeout      = 20;
    gboolean                    lbRunTaskExit       = FALSE;
    enBTRCoreTaskOp             lenRunTskOpPrv      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskOp             lenRunTskOpCur      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskProcessType    lenRunTskPTCur      = enBTRCoreTaskPTUnknown;
    void*                       lpvRunTskInData     = NULL;
    stBTRCoreTaskGAqData*       lpstRunTaskGAqData  = NULL;


    pstlhBTRCore = (stBTRCoreHdl*)apsthBTRCore;
    
    if (!(penExitStatusRunTask = malloc(sizeof(enBTRCoreRet)))) {
        BTRCORELOG_ERROR ("RunTask failure - No Memory\n");
        return NULL;
    }

    if (!pstlhBTRCore || !pstlhBTRCore->connHdl) {
        BTRCORELOG_ERROR ("RunTask failure - BTRCore not initialized\n");
        *penExitStatusRunTask = enBTRCoreNotInitialized;
        return (gpointer)penExitStatusRunTask;
    }


    BTRCORELOG_DEBUG ("%s \n", "RunTask Started");

    do {

        /* Process incoming task-ops */
        {
            li64usTimeout = lui16msTimeout * G_TIME_SPAN_MILLISECOND;
            if ((lpstRunTaskGAqData = g_async_queue_timeout_pop(pstlhBTRCore->pGAQueueRunTask, li64usTimeout))) {
                lenRunTskOpCur = lpstRunTaskGAqData->enBTRCoreTskOp;
                lenRunTskPTCur = lpstRunTaskGAqData->enBTRCoreTskPT;
                lpvRunTskInData= lpstRunTaskGAqData->pvBTRCoreTskInData;
                g_free(lpstRunTaskGAqData);
                lpstRunTaskGAqData = NULL;
                BTRCORELOG_INFO ("g_async_queue_timeout_pop %d %d %p\n", lenRunTskOpCur, lenRunTskPTCur, lpvRunTskInData);
            }
        }


        /* Set up operation - Schedule state changes for next interation */
        if (lenRunTskOpPrv != lenRunTskOpCur) {
            lenRunTskOpPrv = lenRunTskOpCur;

            switch (lenRunTskOpCur) {
            case enBTRCoreTaskOpStart: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpStart\n");
                if ((lenBTRCoreRet = btrCore_RunTaskAddOp(pstlhBTRCore->pGAQueueRunTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTUnknown, NULL)) != enBTRCoreSuccess) {
                    BTRCORELOG_ERROR("Failure btrCore_RunTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTUnknown = %d\n", lenBTRCoreRet);
                }

                break;
            }
            case enBTRCoreTaskOpStop: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpStop\n");
                
                break;
            }
            case enBTRCoreTaskOpIdle: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpIdle\n");
                
                break;
            }
            case enBTRCoreTaskOpProcess: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpProcess\n");

                break;
            }
            case enBTRCoreTaskOpExit: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpExit\n");

                break;
            }
            case enBTRCoreTaskOpUnknown: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpUnknown\n");

                break;
            }
            default:
                BTRCORELOG_INFO ("default\n");
                break;
            }

        }


        /* Process Operations */
        {
            switch (lenRunTskOpCur) {
            case enBTRCoreTaskOpStart: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpStop: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpIdle: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpProcess: {
                if (BtrCore_BTSendReceiveMessages(pstlhBTRCore->connHdl) != 0) {
                    *penExitStatusRunTask = enBTRCoreFailure;
                    lbRunTaskExit = TRUE;
                }

                break;
            }
            case enBTRCoreTaskOpExit: {
                *penExitStatusRunTask = enBTRCoreSuccess;
                lbRunTaskExit = TRUE;

                break;
            }
            case enBTRCoreTaskOpUnknown: {
                g_thread_yield();

                break;
            }
            default:
                g_thread_yield();
                break;
            }
            
        }

    } while (lbRunTaskExit == FALSE);

    BTRCORELOG_DEBUG ("RunTask Exiting\n");


    return (gpointer)penExitStatusRunTask;
}


static gpointer
btrCore_OutTask (
    gpointer        apsthBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore        = NULL;
    enBTRCoreRet*   penExitStatusOutTask= NULL;
    enBTRCoreRet    lenBTRCoreRet       = enBTRCoreSuccess;

    gint64                      li64usTimeout       = 0;
    guint16                     lui16msTimeout      = 100;
    gboolean                    lbOutTaskExit       = FALSE;
    enBTRCoreTaskOp             lenOutTskOpPrv      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskOp             lenOutTskOpCur      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskProcessType    lenOutTskPTCur      = enBTRCoreTaskPTUnknown;
    void*                       lpvOutTskInData     = NULL;
    stBTRCoreTaskGAqData*       lpstOutTaskGAqData  = NULL;



    pstlhBTRCore = (stBTRCoreHdl*)apsthBTRCore;

    if (!(penExitStatusOutTask = malloc(sizeof(enBTRCoreRet)))) {
        BTRCORELOG_ERROR ("OutTask failure - No Memory\n");
        return NULL;
    }

    if (!pstlhBTRCore || !pstlhBTRCore->connHdl) {
        BTRCORELOG_ERROR ("OutTask failure - BTRCore not initialized\n");
        *penExitStatusOutTask = enBTRCoreNotInitialized;
        return (gpointer)penExitStatusOutTask;
    }


    BTRCORELOG_DEBUG ("OutTask Started\n");

    do {

        /* Process incoming task-ops */
        {
            li64usTimeout = lui16msTimeout * G_TIME_SPAN_MILLISECOND;
            if ((lpstOutTaskGAqData = (stBTRCoreTaskGAqData*)(g_async_queue_timeout_pop(pstlhBTRCore->pGAQueueOutTask, li64usTimeout)))) {
                lenOutTskOpCur = lpstOutTaskGAqData->enBTRCoreTskOp;
                lenOutTskPTCur = lpstOutTaskGAqData->enBTRCoreTskPT;
                lpvOutTskInData= lpstOutTaskGAqData->pvBTRCoreTskInData;
                g_free(lpstOutTaskGAqData);
                lpstOutTaskGAqData = NULL;
                BTRCORELOG_INFO ("g_async_queue_timeout_pop %d %d %p\n", lenOutTskOpCur, lenOutTskPTCur, lpvOutTskInData);
            }
        }


        /* Set up operation - Schedule state changes for next interation */
        if (lenOutTskOpPrv != lenOutTskOpCur) {
            lenOutTskOpPrv = lenOutTskOpCur;

            switch (lenOutTskOpCur) {
            case enBTRCoreTaskOpStart: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpStart\n");
                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(pstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpIdle, enBTRCoreTaskPTUnknown, NULL)) != enBTRCoreSuccess) {
                    BTRCORELOG_ERROR("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTUnknown = %d\n", lenBTRCoreRet);
                }
                
                break;
            }
            case enBTRCoreTaskOpStop: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpStop\n");
                
                break;
            }
            case enBTRCoreTaskOpIdle: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpIdle\n");
                
                break;
            }
            case enBTRCoreTaskOpProcess: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpProcess\n");
                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(pstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpIdle, enBTRCoreTaskPTUnknown, NULL)) != enBTRCoreSuccess) {
                    BTRCORELOG_ERROR("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTUnknown = %d\n", lenBTRCoreRet);
                }

                break;
            }
            case enBTRCoreTaskOpExit: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpExit\n");

                break;
            }
            case enBTRCoreTaskOpUnknown: {
                BTRCORELOG_INFO ("enBTRCoreTaskOpUnknown\n");

                break;
            }
            default:
                BTRCORELOG_INFO ("default\n");
                break;
            }

        }


        /* Process Operations */
        {
            switch (lenOutTskOpCur) {
            case enBTRCoreTaskOpStart: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpStop: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpIdle: {
                g_thread_yield();

                break;
            }
            case enBTRCoreTaskOpProcess: {

                if (lenOutTskPTCur == enBTRCoreTaskPTcBDeviceDisc) {
                    if (lpvOutTskInData) {
                        stBTDeviceInfo*     lpstBTDeviceInfo = (stBTDeviceInfo*)lpvOutTskInData;
                        stBTRCoreBTDevice   lstFoundDevice;
                        int                 i32ScannedDevIdx = -1;

                        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(pstlhBTRCore, lpstBTDeviceInfo, &lstFoundDevice)) != -1) {
                           BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d", i32ScannedDevIdx);
                        }


                        if (pstlhBTRCore->fpcBBTRCoreDeviceDisc) {
                            if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreDeviceDisc(lstFoundDevice, pstlhBTRCore->pvcBDevDiscUserData)) != enBTRCoreSuccess) {
                                BTRCORELOG_ERROR ("Failure fpcBBTRCoreDeviceDisc Ret = %d\n", lenBTRCoreRet);
                            }
                        }

                        g_free(lpvOutTskInData);
                        lpvOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBStatus) {
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBMediaStatus) {
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBConnIntim) {
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBConnAuth) {
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTUnknown) {
                    g_thread_yield();
                }
                else {
                    g_thread_yield();
                }

                break;
            }
            case enBTRCoreTaskOpExit: {
                lbOutTaskExit = TRUE;
                break;
            }
            case enBTRCoreTaskOpUnknown: {
                g_thread_yield();

                break;
            }
            default:
                g_thread_yield();
                break;
            }

        }

    } while (lbOutTaskExit == FALSE);

    BTRCORELOG_DEBUG ("OutTask Exiting\n");


    *penExitStatusOutTask = enBTRCoreSuccess;
    return (gpointer)penExitStatusOutTask;
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


    if (!(pstlhBTRCore->pGAQueueOutTask = g_async_queue_new()) ||
        !(pstlhBTRCore->pThreadOutTask  = g_thread_new("btrCore_OutTask", btrCore_OutTask, (gpointer)pstlhBTRCore))) {
        BTRCORELOG_ERROR ("Failed to create btrCore_OutTask Thread - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    if (!(pstlhBTRCore->pGAQueueRunTask = g_async_queue_new()) ||
        !(pstlhBTRCore->pThreadRunTask  = g_thread_new("btrCore_RunTask", btrCore_RunTask, (gpointer)pstlhBTRCore))) {
        BTRCORELOG_ERROR ("Failed to create btrCore_RunTask Thread - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }
   
    if (btrCore_OutTaskAddOp(pstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpStart, enBTRCoreTaskPTUnknown, NULL) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpStart enBTRCoreTaskPTUnknown\n");
    }

    if (btrCore_RunTaskAddOp(pstlhBTRCore->pGAQueueRunTask, enBTRCoreTaskOpStart, enBTRCoreTaskPTUnknown, NULL) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR("Failure btrCore_RunTaskAddOp enBTRCoreTaskOpStart enBTRCoreTaskPTUnknown\n");
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
/*
    if(BTRCore_LE_RegisterDevStatusUpdateCb(pstlhBTRCore->leHdl, btrCore_LeDevStatusUpdateCb, pstlhBTRCore) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to Register LE Dev Status CB - enBTRCoreInitFailure\n");
       BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }
*/
    *phBTRCore  = (tBTRCoreHandle)pstlhBTRCore;

    //Initialize array of known devices so we can use it for stuff
    btrCore_PopulateListOfPairedDevices(*phBTRCore, pstlhBTRCore->curAdapterPath);
    

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DeInit (
    tBTRCoreHandle  hBTRCore
) {
    gpointer        penExitStatusRunTask = NULL;
    enBTRCoreRet    lenExitStatusRunTask = enBTRCoreSuccess;

    gpointer        penExitStatusOutTask = NULL;
    enBTRCoreRet    lenExitStatusOutTask = enBTRCoreSuccess;

    enBTRCoreRet    lenBTRCoreRet = enBTRCoreSuccess;

    stBTRCoreHdl*   pstlhBTRCore = NULL;
    int             i;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    BTRCORELOG_INFO ("hBTRCore   =   %8p\n", hBTRCore);

    /* Free any memory allotted for use in BTRCore */
    
    /* DeInitialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (BTRCore_AVMedia_DeInit(pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to DeInit AV Media Subsystem\n");
        lenBTRCoreRet = enBTRCoreFailure;
    }


    /* Stop BTRCore Task Threads */
    if (pstlhBTRCore->pThreadRunTask) {

        if (pstlhBTRCore->pGAQueueRunTask) {
            if (btrCore_RunTaskAddOp(pstlhBTRCore->pGAQueueRunTask, enBTRCoreTaskOpExit, enBTRCoreTaskPTUnknown, NULL) != enBTRCoreSuccess) {
                BTRCORELOG_ERROR("Failure btrCore_RunTaskAddOp enBTRCoreTaskOpExit enBTRCoreTaskPTUnknown\n");
            }
        }

        penExitStatusRunTask = g_thread_join(pstlhBTRCore->pThreadRunTask);
        pstlhBTRCore->pThreadRunTask = NULL;
    }

    if (penExitStatusRunTask) {
        BTRCORELOG_INFO ("BTRCore_DeInit - RunTask Exiting BTRCore - %d\n", *((enBTRCoreRet*)penExitStatusRunTask));
        lenExitStatusRunTask = *((enBTRCoreRet*)penExitStatusRunTask);
        free(penExitStatusRunTask);
        penExitStatusRunTask = NULL;
    }

    if (pstlhBTRCore->pGAQueueRunTask) {
        g_async_queue_unref(pstlhBTRCore->pGAQueueRunTask);
        pstlhBTRCore->pGAQueueRunTask = NULL;
    }


    if (pstlhBTRCore->pThreadOutTask) {

        if (pstlhBTRCore->pGAQueueOutTask) {
            if (btrCore_OutTaskAddOp(pstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpExit, enBTRCoreTaskPTUnknown, NULL) != enBTRCoreSuccess) {
                BTRCORELOG_ERROR("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpExit enBTRCoreTaskPTUnknown\n");
            }
        }

        penExitStatusOutTask = g_thread_join(pstlhBTRCore->pThreadOutTask);
        pstlhBTRCore->pThreadOutTask = NULL;
    }

    if (penExitStatusOutTask) {
        BTRCORELOG_INFO ("BTRCore_DeInit - OutTask Exiting BTRCore - %d\n", *((enBTRCoreRet*)penExitStatusOutTask));
        lenExitStatusOutTask = *((enBTRCoreRet*)penExitStatusOutTask);
        free(penExitStatusOutTask);
        penExitStatusOutTask = NULL;
    }

    if (pstlhBTRCore->pGAQueueOutTask) {
        g_async_queue_unref(pstlhBTRCore->pGAQueueOutTask);
        pstlhBTRCore->pGAQueueOutTask = NULL;
    }

    if (pstlhBTRCore->curAdapterPath) {
        if (BtrCore_BTReleaseAdapterPath(pstlhBTRCore->connHdl, NULL)) {
            BTRCORELOG_ERROR ("Failure BtrCore_BTReleaseAdapterPath\n");
            lenBTRCoreRet = enBTRCoreFailure;
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
            lenBTRCoreRet = enBTRCoreFailure;
        }
        pstlhBTRCore->agentPath = NULL;
    }

    if (pstlhBTRCore->connHdl) {
        if (BtrCore_BTDeInitReleaseConnection(pstlhBTRCore->connHdl)) {
            BTRCORELOG_ERROR ("Failure BtrCore_BTDeInitReleaseConnection\n");
            lenBTRCoreRet = enBTRCoreFailure;
        }
        pstlhBTRCore->connHdl = NULL;
    }

    if (hBTRCore) {
        free(hBTRCore);
        hBTRCore = NULL;
    }

    lenBTRCoreRet = ((lenExitStatusRunTask == enBTRCoreSuccess) &&
                     (lenExitStatusOutTask == enBTRCoreSuccess) &&
                     (lenBTRCoreRet == enBTRCoreSuccess)) ? enBTRCoreSuccess : enBTRCoreFailure;
    BTRCORELOG_DEBUG ("Exit Status = %d\n", lenBTRCoreRet);


    return lenBTRCoreRet;
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
    tBTRCoreHandle  hBTRCore,
    unsigned char   aui8adapterIdx,
    char*           apui8adapterAddr
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    enBTRCoreRet    lenBTRCoreRet= enBTRCoreFailure;
    int             i = 0;

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
            lenBTRCoreRet = enBTRCoreSuccess;
            break;
        }
    }

    return lenBTRCoreRet;
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
    memset (pListOfScannedDevices, 0, sizeof(stBTRCoreScannedDevicesCount));

    BTRCORELOG_INFO ("adapter path is %s\n", pstlhBTRCore->curAdapterPath);
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (pstlhBTRCore->stScannedDevicesArr[i].bFound) {
            BTRCORELOG_INFO ("Device : %d\n", i);
            BTRCORELOG_INFO ("Name   : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceName);
            BTRCORELOG_INFO ("Mac Ad : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceAddress);
            BTRCORELOG_INFO ("Rssi   : %d dbmV\n", pstlhBTRCore->stScannedDevicesArr[i].i32RSSI);
            btrCore_ShowSignalStrength(pstlhBTRCore->stScannedDevicesArr[i].i32RSSI);

            memcpy (&pListOfScannedDevices->devices[pListOfScannedDevices->numberOfDevices++], &pstlhBTRCore->stScannedDevicesArr[i], sizeof (stBTRCoreBTDevice));
        }
    }   

    BTRCORELOG_INFO ("Copied scanned details of %d devices\n", pListOfScannedDevices->numberOfDevices);

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
                pstScannedDev   = &pstlhBTRCore->stScannedDevicesArr[i32LoopIdx];
                break;
            }
        }
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
    tBTRCoreHandle          hBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType
) {
    stBTRCoreHdl*           pstlhBTRCore            = NULL;
    enBTRCoreRet            lenBTRCoreRet           = enBTRCoreFailure;
    enBTDeviceType          lenBTDeviceType         = enBTDevUnknown;
    stBTRCoreBTDevice*      lpstBTRCoreBTDevice     = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo = NULL;
    stBTRCoreBTDevice*      lpstBTRCoreBTDeviceArr  = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStInfoArr = NULL;
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;


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
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);    /* Keep the list upto date */
    }


    if (aenBTRCoreDevType != enBTRCoreLE) {
        ui32NumOfDevices        = pstlhBTRCore->numOfPairedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stKnownDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stKnownDevStInfoArr;
    }
    else {
        ui32NumOfDevices        = pstlhBTRCore->numOfScannedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stScannedDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stScannedDevStInfoArr;

    }


    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried/scanned for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[aBTRCoreDevId];
        lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[aBTRCoreDevId];
        lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
        lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == lpstBTRCoreBTDeviceArr[ui32LoopIdx].tDeviceId) {
                lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[ui32LoopIdx];
                lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[ui32LoopIdx];
                lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
                lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
                break;
            }
        }
    }


    if (!lpcBTRCoreBTDevicePath || !strlen(lpcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired/scanned devices list\n");
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


    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHdl, lpcBTRCoreBTDevicePath, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("Connect to device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Connected to device %s Successfully. \n", lpcBTRCoreBTDeviceName);
    /* Should think on moving a connected LE device from scanned list to paired list */


    lpstBTRCoreBTDevice->bDeviceConnected      = TRUE;
    lpstBTRCoreDevStateInfo->eDevicePrevState  = lpstBTRCoreDevStateInfo->eDeviceCurrState;

     if (lpstBTRCoreDevStateInfo->eDeviceCurrState  != enBTRCoreDevStConnected) {
         lpstBTRCoreDevStateInfo->eDeviceCurrState   = enBTRCoreDevStConnecting;

        lenBTRCoreRet = enBTRCoreSuccess;
     }


    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisconnectDevice (
    tBTRCoreHandle          hBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType
) {
    stBTRCoreHdl*           pstlhBTRCore            = NULL;
    enBTRCoreRet            lenBTRCoreRet           = enBTRCoreFailure;
    enBTDeviceType          lenBTDeviceType         = enBTDevUnknown;
    stBTRCoreBTDevice*      lpstBTRCoreBTDevice     = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo = NULL;
    stBTRCoreBTDevice*      lpstBTRCoreBTDeviceArr  = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStInfoArr = NULL;
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;


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
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);    /* Keep the list upto date */
    }


    if (aenBTRCoreDevType != enBTRCoreLE) {
        ui32NumOfDevices        = pstlhBTRCore->numOfPairedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stKnownDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stKnownDevStInfoArr;
    }
    else {
        ui32NumOfDevices        = pstlhBTRCore->numOfScannedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stScannedDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stScannedDevStInfoArr;

    }


    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried/scanned for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[aBTRCoreDevId];
        lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[aBTRCoreDevId];
        lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
        lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == lpstBTRCoreBTDeviceArr[ui32LoopIdx].tDeviceId) {
                lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[ui32LoopIdx];
                lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[ui32LoopIdx];
                lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
                lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
                break;
            }
        }
    }


    if (!lpcBTRCoreBTDevicePath || !strlen(lpcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired/scanned devices list\n");
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


    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHdl, lpcBTRCoreBTDevicePath, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("DisConnect from device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("DisConnected from device %s Successfully.\n", lpcBTRCoreBTDeviceName);

    
    lpstBTRCoreBTDevice->bDeviceConnected = FALSE;

    if (lpstBTRCoreDevStateInfo->eDeviceCurrState   != enBTRCoreDevStDisconnected &&
        lpstBTRCoreDevStateInfo->eDeviceCurrState   != enBTRCoreDevStLost) {
        lpstBTRCoreDevStateInfo->eDevicePrevState    = lpstBTRCoreDevStateInfo->eDeviceCurrState;
        lpstBTRCoreDevStateInfo->eDeviceCurrState    = enBTRCoreDevStDisconnecting; 

        lenBTRCoreRet = enBTRCoreSuccess;
    }

    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetDeviceConnected (
    tBTRCoreHandle          hBTRCore, 
    tBTRCoreDevId           aBTRCoreDevId, 
    enBTRCoreDeviceType     aenBTRCoreDevType
) {
    stBTRCoreHdl*           pstlhBTRCore            = NULL;
    enBTRCoreRet            lenBTRCoreRet           = enBTRCoreFailure;
    enBTDeviceType          lenBTDeviceType         = enBTDevUnknown;
    stBTRCoreBTDevice*      lpstBTRCoreBTDevice     = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo = NULL;
    stBTRCoreBTDevice*      lpstBTRCoreBTDeviceArr  = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStInfoArr = NULL;
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;


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
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);    /* Keep the list upto date */
    }


    if (aenBTRCoreDevType != enBTRCoreLE) {
        ui32NumOfDevices        = pstlhBTRCore->numOfPairedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stKnownDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stKnownDevStInfoArr;
    }
    else {
        ui32NumOfDevices        = pstlhBTRCore->numOfScannedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stScannedDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stScannedDevStInfoArr;

    }


    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried/scanned for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[aBTRCoreDevId];
        lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[aBTRCoreDevId];
        lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
        lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == lpstBTRCoreBTDeviceArr[ui32LoopIdx].tDeviceId) {
                lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[ui32LoopIdx];
                lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[ui32LoopIdx];
                lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
                lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
                break;
            }
        }
    }


    if (!lpcBTRCoreBTDevicePath || !strlen(lpcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired/scanned devices list\n");
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

    if (lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStConnected) {
        BTRCORELOG_DEBUG ("enBTRCoreDevStConnected = %s\n", lpcBTRCoreBTDeviceName);
        lenBTRCoreRet = enBTRCoreSuccess;
    }

    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_GetDeviceDisconnected (
    tBTRCoreHandle          hBTRCore, 
    tBTRCoreDevId           aBTRCoreDevId, 
    enBTRCoreDeviceType     aenBTRCoreDevType
) {
    stBTRCoreHdl*           pstlhBTRCore            = NULL;
    enBTRCoreRet            lenBTRCoreRet           = enBTRCoreFailure;
    enBTDeviceType          lenBTDeviceType         = enBTDevUnknown;
    stBTRCoreBTDevice*      lpstBTRCoreBTDevice     = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo = NULL;
    stBTRCoreBTDevice*      lpstBTRCoreBTDeviceArr  = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStInfoArr = NULL;
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;


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
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);    /* Keep the list upto date */
    }


    if (aenBTRCoreDevType != enBTRCoreLE) {
        ui32NumOfDevices        = pstlhBTRCore->numOfPairedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stKnownDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stKnownDevStInfoArr;
    }
    else {
        ui32NumOfDevices        = pstlhBTRCore->numOfScannedDevices;
        lpstBTRCoreBTDeviceArr  = pstlhBTRCore->stScannedDevicesArr;
        lpstBTRCoreDevStInfoArr = pstlhBTRCore->stScannedDevStInfoArr;

    }


    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried/scanned for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[aBTRCoreDevId];
        lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[aBTRCoreDevId];
        lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
        lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == lpstBTRCoreBTDeviceArr[ui32LoopIdx].tDeviceId) {
                lpstBTRCoreBTDevice     = &lpstBTRCoreBTDeviceArr[ui32LoopIdx];
                lpstBTRCoreDevStateInfo = &lpstBTRCoreDevStInfoArr[ui32LoopIdx];
                lpcBTRCoreBTDevicePath  = lpstBTRCoreBTDevice->pcDevicePath;
                lpcBTRCoreBTDeviceName  = lpstBTRCoreBTDevice->pcDeviceName;
                break;
            }
        }
    }


    if (!lpcBTRCoreBTDevicePath || !strlen(lpcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired/scanned devices list\n");
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

    if ((lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStDisconnected) ||
        (lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStLost)) {
        BTRCORELOG_DEBUG ("enBTRCoreDevStDisconnected = %s\n", lpcBTRCoreBTDeviceName);
        lenBTRCoreRet = enBTRCoreSuccess;
    }

    BTRCORELOG_DEBUG ("Ret - %d\n", lenBTRCoreRet);
    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_GetDeviceTypeClass (
    tBTRCoreHandle          hBTRCore, 
    tBTRCoreDevId           aBTRCoreDevId, 
    enBTRCoreDeviceType*    apenBTRCoreDevTy,
    enBTRCoreDeviceClass*   apenBTRCoreDevCl
) {
    stBTRCoreHdl*           pstlhBTRCore    = NULL;
    stBTRCoreBTDevice*      pstBTDevice     = NULL;
    int                     i32LoopIdx      = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }
    else if (!apenBTRCoreDevTy || !apenBTRCoreDevCl) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }


    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices) {
        if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
            pstBTDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        }
        else {
            for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                    pstBTDevice = &pstlhBTRCore->stKnownDevicesArr[i32LoopIdx];
                    break;
                }
            }
        }
    }


    if (!pstBTDevice && pstlhBTRCore->numOfScannedDevices) {
        if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
            pstBTDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
        }
        else {
            for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
                if (aBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
                    pstBTDevice = &pstlhBTRCore->stScannedDevicesArr[i32LoopIdx];
                    break;
                }
            }
        }
    }

    
    if (!pstBTDevice) {
        *apenBTRCoreDevTy = enBTRCoreUnknown; 
        *apenBTRCoreDevCl = enBTRCore_DC_Unknown;
        return enBTRCoreFailure;
    }


    *apenBTRCoreDevCl = pstBTDevice->enDeviceType;

    if (*apenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
       *apenBTRCoreDevTy =  enBTRCoreMobileAudioIn;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_Tablet) {
       *apenBTRCoreDevTy = enBTRCorePCAudioIn;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_WearableHeadset) {
       *apenBTRCoreDevTy =  enBTRCoreHeadSet;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_Headphones) {
       *apenBTRCoreDevTy = enBTRCoreHeadSet;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_Loudspeaker) {
       *apenBTRCoreDevTy = enBTRCoreSpeakers;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_HIFIAudioDevice) {
       *apenBTRCoreDevTy = enBTRCoreSpeakers;
    }
    else if (*apenBTRCoreDevCl == enBTRCore_DC_Tile) {
       *apenBTRCoreDevTy = enBTRCoreLE;
        //TODO: May be use should have AudioDeviceClass & LE DeviceClass 
        //      will help us to identify the device Type as LE
    } else {
        *apenBTRCoreDevTy = enBTRCoreUnknown; 
    }


    return enBTRCoreSuccess;
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


enBTRCoreRet
BTRCore_GetLEProperty (
    tBTRCoreHandle     hBTRCore,
    tBTRCoreDevId      aBTRCoreDevId,
    const char*        apcBTRCoreLEUuid,
    enBTRCoreLeProp    aenBTRCoreLeProp,
    void*              apvBTRCorePropValue
) {

    if (!hBTRCore || !apcBTRCoreLEUuid || aBTRCoreDevId < 0) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    stBTRCoreHdl*         pstlhBTRCore   = (stBTRCoreHdl*)hBTRCore;
    stBTRCoreBTDevice*    pstScannedDev  = NULL;
    tBTRCoreDevId         ltBTRCoreDevId = 0;
    int                   i32LoopIdx     = 0;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
       pstScannedDev  = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
    }
    else {
       for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
           if (aBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
              pstScannedDev  = &pstlhBTRCore->stScannedDevicesArr[i32LoopIdx];
              break;
           }
       }
    }

    if (pstScannedDev) {
        ltBTRCoreDevId = pstScannedDev->tDeviceId;
    }

    if (!pstScannedDev || !ltBTRCoreDevId) {
       BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
       return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_DEBUG ("Get LE Property for Device : %s\n", pstScannedDev->pcDeviceName);
    BTRCORELOG_DEBUG ("LE DeviceID  %llu\n", ltBTRCoreDevId);

    enBTRCoreLEGattProp  lenBTRCoreLEGattProp = enBTRCoreLEGPropUnknown;

    switch (aenBTRCoreLeProp) {

    case enBTRCoreLePropGUUID:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropUUID;
        break;
    case enBTRCoreLePropGPrimary:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropPrimary;
        break;
    case enBTRCoreLePropGDevice:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropDevice;
        break;
    case enBTRCoreLePropGService:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropService;
        break;
    case enBTRCoreLePropGValue:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropValue;
        break;
    case enBTRCoreLePropGNotifying:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropNotifying;
        break;
    case enBTRCoreLePropGFlags:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropFlags;
        break;
    case enBTRCoreLePropGChar:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropChar;
        break;
    case enBTRCoreLePropUnknown:
    default:
        lenBTRCoreLEGattProp = enBTRCoreLEGPropUnknown;
    }

    if (lenBTRCoreLEGattProp == enBTRCoreLEGPropUnknown || BTRCore_LE_GetGattProperty (pstlhBTRCore->leHdl,
                                                                                      pstlhBTRCore->connHdl,
                                                                                      ltBTRCoreDevId,
                                                                                      apcBTRCoreLEUuid,
                                                                                      lenBTRCoreLEGattProp,
                                                                                      apvBTRCorePropValue) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to get Gatt Property %d!!!\n", lenBTRCoreLEGattProp);
      return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}
 
   
enBTRCoreRet 
BTRCore_PerformLEOp (
    tBTRCoreHandle    hBTRCore,
    tBTRCoreDevId     aBTRCoreDevId,
    const char*       apBtUuid,
    enBTRCoreLeOp     aenBTRCoreLeOp,
    void*             rpLeOpRes
) {

    if (!hBTRCore || !apBtUuid || aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    stBTRCoreHdl*       pstlhBTRCore   = (stBTRCoreHdl*)hBTRCore;
    stBTRCoreBTDevice*  pstScannedDev  = NULL;
    tBTRCoreDevId       ltBTRCoreDevId = 0;
    int                 i32LoopIdx     = 0;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
       pstScannedDev  = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
    }
    else {
       for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
           if (aBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
              pstScannedDev  = &pstlhBTRCore->stScannedDevicesArr[i32LoopIdx];
              break;
           }
       }
    }

    if (pstScannedDev) {
        ltBTRCoreDevId = pstScannedDev->tDeviceId;
    }

    if (!pstScannedDev || !ltBTRCoreDevId) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (!pstScannedDev->bFound || !pstScannedDev->bDeviceConnected) {
        BTRCORELOG_ERROR ("Le Device is not connected, Please connect and perform LE method operation\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_DEBUG ("Perform LE Op for Device : %s\n", pstScannedDev->pcDeviceName);
    BTRCORELOG_DEBUG ("LE DeviceID  %llu\n", ltBTRCoreDevId);

    enBTRCoreLEGattOp  lenBTRCoreLEGattOp = enBTRCoreLEGOpUnknown;

    switch (aenBTRCoreLeOp) {
   
    case enBTRCoreLeOpGReadValue:
         lenBTRCoreLEGattOp = enBTRCoreLEGOpReadValue;
         break;
    case enBTRCoreLeOpGWriteValue:
         lenBTRCoreLEGattOp =  enBTRCoreLEGOpWriteValue;
         break;
    case enBTRCoreLeOpGStartNotify:
         lenBTRCoreLEGattOp =  enBTRCoreLEGOpStartNotify; 
         break;
    case enBTRCoreLeOpGStopNotify:
         lenBTRCoreLEGattOp =  enBTRCoreLEGOpStopNotify;
         break;
    case enBTRCoreLeOpUnknown:
    default : 
         lenBTRCoreLEGattOp = enBTRCoreLEGOpUnknown;
    }

    if (lenBTRCoreLEGattOp == enBTRCoreLEGOpUnknown || BtrCore_LE_PerformGattOp (pstlhBTRCore->leHdl,
                                                                                 pstlhBTRCore->connHdl,
                                                                                 ltBTRCoreDevId,
                                                                                 apBtUuid,
                                                                                 lenBTRCoreLEGattOp,
                                                                                 rpLeOpRes) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to Perform LE Method Op %d!!!\n", aenBTRCoreLeOp);
       return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


// Outgoing callbacks Registration Interfaces
enBTRCoreRet
BTRCore_RegisterDiscoveryCb (
    tBTRCoreHandle              hBTRCore, 
    fPtr_BTRCore_DeviceDiscCb   afpcBBTRCoreDeviceDisc,
    void*                       apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fpcBBTRCoreDeviceDisc) {
        pstlhBTRCore->fpcBBTRCoreDeviceDisc = afpcBBTRCoreDeviceDisc;
        pstlhBTRCore->pvcBDevDiscUserData   = apUserData;
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
    enBTRCoreRet         lenBTRCoreRet      = enBTRCoreFailure;
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;
    enBTRCoreDeviceClass lenBTRCoreDevCl    = enBTRCore_DC_Unknown;


    BTRCORELOG_INFO ("enBTDeviceType = %d enBTDeviceState = %d apstBTDeviceInfo = %p\n", aeBtDeviceType, aeBtDeviceState, apstBTDeviceInfo);

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevType =  enBTRCoreSpeakers;
        break;
    case enBTDevAudioSource:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        if (lenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Tablet) {
           lenBTRCoreDevType = enBTRCorePCAudioIn;
        }
        break;
    case enBTDevHFPHeadset:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevHFPHeadsetGateway:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevLE:
        lenBTRCoreDevType = enBTRCoreLE;
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
        stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

        if (lpstlhBTRCore && apstBTDeviceInfo) {
            int j = 0;
            tBTRCoreDevId   lBTRCoreDevId = 0;

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

            lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
            if (btrCore_GetScannedDeviceAddress(lpstlhBTRCore, lBTRCoreDevId)) {
                BTRCORELOG_INFO ("Already we have a entry in the list; Skip Parsing now \n");
            }
            else {
                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceDisc, apstBTDeviceInfo)) != enBTRCoreSuccess) {
                    BTRCORELOG_ERROR("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceDisc %d\n", lenBTRCoreRet);
                }
            }
        }

        break;
    }
    case enBTDevStLost: {
        stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

        if (lpstlhBTRCore && apstBTDeviceInfo) {
            int     i32LoopIdx      = 0;
            BOOLEAN postEvent       = FALSE;

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
                            lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected = FALSE;
                            postEvent = TRUE;
                        }
                        else 
                        if (lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStPlaying   ||
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStConnected ){

                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState =  lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStLost;
                            lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected = FALSE;
                            postEvent = TRUE;
                        }

                        // move this out of if block. populating stDevStatusCbInfo should be done common for both paired and scanned devices 
                        if (postEvent) {
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
                        }
                        break;
                    }
                }
            }

            if (lpstlhBTRCore->numOfScannedDevices) {
                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
                    if (lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId == lBTRCoreDevId) {
                        BTRCORELOG_TRACE ("i32LoopIdx = %d\n", i32LoopIdx);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx].eDeviceCurrState);
                        BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx].eDevicePrevState);

                        {   // To confirm if this index preserving method is fine or scan 32 fixed element list always just by flipping  bFound to TRUE/FALSE
                            if (i32LoopIdx != lpstlhBTRCore->numOfScannedDevices-1) {
                                memcpy (&lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx], &lpstlhBTRCore->stScannedDevicesArr[lpstlhBTRCore->numOfScannedDevices-1], sizeof(stBTRCoreBTDevice));
                                memcpy (&lpstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx], &lpstlhBTRCore->stScannedDevStInfoArr[lpstlhBTRCore->numOfScannedDevices-1], sizeof(stBTRCoreDevStateInfo));   
                            }
                            lpstlhBTRCore->stScannedDevicesArr[lpstlhBTRCore->numOfScannedDevices-1].tDeviceId = 0;
                            lpstlhBTRCore->stScannedDevicesArr[lpstlhBTRCore->numOfScannedDevices-1].bFound    = FALSE;
                            lpstlhBTRCore->numOfScannedDevices--;
                        }

                        if (!lpstlhBTRCore->numOfScannedDevices) {
                            BTRCORELOG_INFO ("\nClearing Scanned Device List...\n");
                            btrCore_ClearScannedDevicesList(lpstlhBTRCore);

                            lpstlhBTRCore->stDevStatusCbInfo.deviceId         = 0; // Need to have any special IDs for this purpose like 0xFFFFFFFF
                            lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = enBTRCoreDevStFound;
                            lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStLost;
                            lpstlhBTRCore->stDevStatusCbInfo.isPaired         = 0;           

                            if (lpstlhBTRCore->fpcBBTRCoreStatus) {
                                // Do it in OutTask
                                //if (lpstlhBTRCore->fpcBBTRCoreStatus(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
                                    //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea
                                //}      
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
                BOOLEAN bTriggerDevStatusChangeCb               = FALSE;
                stBTRCoreBTDevice  *lpstBTRCoreBTDevice         = NULL;
                stBTRCoreDevStateInfo *lpstBTRCoreDevStateInfo  = NULL;

                if (i32KnownDevIdx != -1) {

                    BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);
                    BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);

                    if ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != leBTDevState) &&
                        (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                        if ((enBTRCoreMobileAudioIn != lenBTRCoreDevType) && (enBTRCorePCAudioIn != lenBTRCoreDevType)) {

                            if ( !(((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected) && (leBTDevState == enBTRCoreDevStDisconnected)) ||
                                   ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) && (leBTDevState == enBTRCoreDevStConnected) && 
                                    ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStPaired) || 
                                     (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStConnecting))))) {
                                bTriggerDevStatusChangeCb = TRUE;
                            }
                            // To make the state changes in a better logical way once the BTRCore dev structures are unified further

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
                                lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
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
                        lpstBTRCoreBTDevice     = &lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx];
                        lpstBTRCoreDevStateInfo = &lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx];

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
                    BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = %d\n", lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState);

                    if ((lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != leBTDevState) &&
                        (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                        lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                        lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState = leBTDevState;
                        lpstlhBTRCore->stDevStatusCbInfo.isPaired = apstBTDeviceInfo->bPaired;

                        if (apstBTDeviceInfo->bPaired       &&
                            apstBTDeviceInfo->bConnected    &&
                            (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState == enBTRCoreDevStFound) &&
                            (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState == enBTRCoreDevStConnected)) {

                            if ((i32KnownDevIdx = btrCore_AddDeviceToKnownDevicesArr(lpstlhBTRCore, apstBTDeviceInfo)) != -1) {
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState;
                                lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                                BTRCORELOG_DEBUG ("btrCore_AddDeviceToKnownDevicesArr - Success Index = %d", i32KnownDevIdx);
                            }

                            //TODO: Really really dont like this - Live with it for now
                            if (lpstlhBTRCore->numOfPairedDevices) {
                                for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                                    if (lBTRCoreDevId == lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                                        //i32KnownDevIdx = i32LoopIdx;
                                        lpstBTRCoreBTDevice     = &lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx];
                                        lpstBTRCoreDevStateInfo = &lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx];
                                    }
                                }
                            }
                        }

                        if (lenBTRCoreDevType == enBTRCoreLE &&
                            (leBTDevState == enBTRCoreDevStConnected   ||
                            leBTDevState == enBTRCoreDevStDisconnected)){
                            lpstBTRCoreBTDevice     = &lpstlhBTRCore->stScannedDevicesArr[i32ScannedDevIdx];
                            lpstBTRCoreDevStateInfo = &lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx];

                            if (leBTDevState == enBTRCoreDevStDisconnected) {
                                lpstBTRCoreBTDevice->bDeviceConnected = FALSE;
                            }

                            if (lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState == enBTRCoreDevStInitialized) {
                                lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = enBTRCoreDevStConnecting;
                            }
                        }

                        bTriggerDevStatusChangeCb = TRUE;
                    }
                }

                if (bTriggerDevStatusChangeCb == TRUE) {
                    lpstlhBTRCore->stDevStatusCbInfo.deviceId         = lBTRCoreDevId;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceType      = lenBTRCoreDevType;
                    lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = lpstBTRCoreDevStateInfo->eDevicePrevState;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = leBTDevState;
                    lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass     = lpstBTRCoreBTDevice->enDeviceType;
                    strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName,lpstBTRCoreBTDevice->pcDeviceName, BD_NAME_LEN - 1);

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
    int                  i32DevConnIntimRet = 0;
    stBTRCoreHdl*        lpstlhBTRCore      = (stBTRCoreHdl*)apUserData;
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;
    enBTRCoreDeviceClass lenBTRCoreDevCl    = enBTRCore_DC_Unknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        if (lenBTRCoreDevCl == enBTRCore_DC_WearableHeadset) {
           lenBTRCoreDevType =  enBTRCoreHeadSet;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Headphones) {
           lenBTRCoreDevType = enBTRCoreHeadSet;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Loudspeaker) {
           lenBTRCoreDevType = enBTRCoreSpeakers;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_HIFIAudioDevice) {
           lenBTRCoreDevType = enBTRCoreSpeakers;
        }
        break;
    case enBTDevAudioSource:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        if (lenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Tablet) {
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
        stBTRCoreBTDevice   lstFoundDevice;
        int                 i32ScannedDevIdx = -1;

        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(lpstlhBTRCore, apstBTDeviceInfo, &lstFoundDevice)) != -1) {
           BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d", i32ScannedDevIdx);
        }

        BTRCORELOG_DEBUG("btrCore_BTDeviceConnectionIntimationCb\n");
        lpstlhBTRCore->stConnCbInfo.ui32devPassKey = aui32devPassKey;
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        memcpy (&lpstlhBTRCore->stConnCbInfo.stFoundDevice, &lstFoundDevice, sizeof(stBTRCoreBTDevice));
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.bFound = TRUE;

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
    int                  i32DevAuthRet      = 0;
    stBTRCoreHdl*        lpstlhBTRCore      = (stBTRCoreHdl*)apUserData;
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;
    enBTRCoreDeviceClass lenBTRCoreDevCl    = enBTRCore_DC_Unknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        if (lenBTRCoreDevCl == enBTRCore_DC_WearableHeadset) {
           lenBTRCoreDevType =  enBTRCoreHeadSet;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Headphones) {
           lenBTRCoreDevType = enBTRCoreHeadSet;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Loudspeaker) {
           lenBTRCoreDevType = enBTRCoreSpeakers;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_HIFIAudioDevice) {
           lenBTRCoreDevType = enBTRCoreSpeakers;
        }
        break;
    case enBTDevAudioSource:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        if (lenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Tablet) {
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
        stBTRCoreBTDevice   lstFoundDevice;
        int                 i32ScannedDevIdx = -1;
        int                 i32KnownDevIdx   = -1;

        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(lpstlhBTRCore, apstBTDeviceInfo, &lstFoundDevice)) != -1) {
           BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d", i32ScannedDevIdx);
        }

        BTRCORELOG_DEBUG("btrCore_BTDeviceAuthenticationCb\n");
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        if ((i32KnownDevIdx = btrCore_AddDeviceToKnownDevicesArr(lpstlhBTRCore, apstBTDeviceInfo)) != -1) {
            memcpy (&lpstlhBTRCore->stConnCbInfo.stKnownDevice, &lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx], sizeof(stBTRCoreBTDevice));
            BTRCORELOG_DEBUG ("btrCore_AddDeviceToKnownDevicesArr - Success Index = %d Unique DevID = %lld", i32KnownDevIdx, lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId);
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

                        if (lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStInitialized) {
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState;
                            lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                        }

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
