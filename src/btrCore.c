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
#include <stdlib.h>     //for strtoll
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
    enBTRCoreTaskPTcBAdapterStatus,
    enBTRCoreTaskPTcBDeviceDisc,
    enBTRCoreTaskPTcBDeviceRemoved,
    enBTRCoreTaskPTcBDeviceLost,
    enBTRCoreTaskPTcBDeviceStatus,
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


typedef struct _stBTRCoreOTskInData {
    tBTRCoreDevId       bTRCoreDevId;
    enBTRCoreDeviceType enBTRCoreDevType;
    stBTDeviceInfo*     pstBTDevInfo;
} stBTRCoreOTskInData;


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


    stBTRCoreDiscoveryCBInfo        stDiscoveryCbInfo;
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
    enBTRCoreDeviceType             aenDeviceDiscoveryType;

} stBTRCoreHdl;


/* Static Function Prototypes */
static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevId btrCore_GenerateUniqueDeviceID (const char* apcDeviceMac);
static enBTRCoreDeviceClass btrCore_MapClassIDtoAVDevClass(unsigned int aui32ClassId);
static enBTRCoreDeviceClass btrCore_MapServiceClasstoDevType(unsigned int aui32ClassId);
static enBTRCoreDeviceClass btrCore_MapClassIDtoDevClass(unsigned int aui32ClassId);
static enBTRCoreDeviceType btrCore_MapClassIDToDevType(unsigned int aui32ClassId, enBTDeviceType aeBtDeviceType);
static enBTRCoreDeviceType btrCore_MapDevClassToDevType(enBTRCoreDeviceClass aenBTRCoreDevCl);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static int btrCore_AddDeviceToScannedDevicesArr (stBTRCoreHdl* apsthBTRCore, stBTDeviceInfo* apstBTDeviceInfo, stBTRCoreBTDevice* apstFoundDevice); 
static int btrCore_AddDeviceToKnownDevicesArr (stBTRCoreHdl* apsthBTRCore, stBTDeviceInfo* apstBTDeviceInfo);
static enBTRCoreRet btrCore_RemoveDeviceFromScannedDevicesArr (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId, stBTRCoreBTDevice* astRemovedDevice);
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static void btrCore_MapKnownDeviceListFromPairedDeviceInfo (stBTRCoreBTDevice* knownDevicesArr, stBTPairedDeviceInfo* pairedDeviceInfo);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceMac (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static enBTRCoreRet btrCore_GetDeviceInfo (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType,
                                             enBTDeviceType* apenBTDeviceType, stBTRCoreBTDevice** appstBTRCoreBTDevice,
                                               stBTRCoreDevStateInfo** appstBTRCoreDevStateInfo, const char** appcBTRCoreBTDevicePath, const char** appcBTRCoreBTDeviceName);
static enBTRCoreRet btrCore_GetDeviceInfoKnown (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType,
                                                  enBTDeviceType* apenBTDeviceType, stBTRCoreBTDevice** appstBTRCoreBTDevice,
                                                    stBTRCoreDevStateInfo** appstBTRCoreDevStateInfo, const char** appcBTRCoreBTDevicePath);
static void btrCore_ShowSignalStrength (short strength);
static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut);
static enBTRCoreDeviceState btrCore_BTParseDeviceState (const char* pcStateValue);

static enBTRCoreRet btrCore_RunTaskAddOp (GAsyncQueue* apRunTaskGAq, enBTRCoreTaskOp aenRunTaskOp, enBTRCoreTaskProcessType aenRunTaskPT, void* apvRunTaskInData);
static enBTRCoreRet btrCore_OutTaskAddOp (GAsyncQueue* apOutTaskGAq, enBTRCoreTaskOp aenOutTaskOp, enBTRCoreTaskProcessType aenOutTaskPT, void* apvOutTaskInData);

/* Local Op Threads Prototypes */
static gpointer btrCore_RunTask (gpointer apsthBTRCore);
static gpointer btrCore_OutTask (gpointer apsthBTRCore);

/* Incoming Callbacks Prototypes */
static int btrCore_BTAdapterStatusUpdateCb (enBTAdapterProp aeBtAdapterProp, stBTAdapterInfo* apstBTAdapterInfo,  void* apUserData);
static int btrCore_BTDeviceStatusUpdateCb (enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo,  void* apUserData);
static int btrCore_BTDeviceConnectionIntimationCb (enBTDeviceType  aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, unsigned int aui32devPassKey, unsigned char ucIsReqConfirmation, void* apUserData);
static int btrCore_BTDeviceAuthenticationCb (enBTDeviceType  aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);

static enBTRCoreRet btrCore_BTMediaStatusUpdateCb (stBTRCoreAVMediaStatusUpdate* apMediaStreamStatus, const char*  apBtdevAddr, void* apUserData);
static enBTRCoreRet btrCore_BTLeStatusUpdateCb (stBTRCoreLeGattInfo* apstBtrLeInfo, const char*  apcBtdevAddr, void* apvUserData);


/* Static Function Definition */
static void
btrCore_InitDataSt (
    stBTRCoreHdl*   apsthBTRCore
) {
    int i;

    /* Current Adapter */
    apsthBTRCore->curAdapterAddr = (char*)g_malloc0(sizeof(char) * BD_NAME_LEN);
    memset(apsthBTRCore->curAdapterAddr, '\0', sizeof(char) * BD_NAME_LEN);

    /* Adapters */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_ADAPTERS; i++) {
        apsthBTRCore->adapterPath[i] = (char*)g_malloc0(sizeof(char) * BD_NAME_LEN);
        memset(apsthBTRCore->adapterPath[i], '\0', sizeof(char) * BD_NAME_LEN);

        apsthBTRCore->adapterAddr[i] = (char*)g_malloc0(sizeof(char) * BD_NAME_LEN);
        memset(apsthBTRCore->adapterAddr[i], '\0', sizeof(char) * BD_NAME_LEN);
    }

    /* Scanned Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stScannedDevicesArr[i].tDeviceId          = 0;
        apsthBTRCore->stScannedDevicesArr[i].enDeviceType       = enBTRCore_DC_Unknown;
        apsthBTRCore->stScannedDevicesArr[i].bFound             = FALSE;
        apsthBTRCore->stScannedDevicesArr[i].bDeviceConnected   = FALSE;
        apsthBTRCore->stScannedDevicesArr[i].i32RSSI            = INT_MIN;

        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceName,      '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDeviceAddress,   '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stScannedDevicesArr[i].pcDevicePath,      '\0', sizeof(BD_NAME));

        apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    apsthBTRCore->numOfScannedDevices = 0;
    apsthBTRCore->numOfPairedDevices  = 0;

    
    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stKnownDevicesArr[i].tDeviceId            = 0;
        apsthBTRCore->stKnownDevicesArr[i].enDeviceType         = enBTRCore_DC_Unknown;
        apsthBTRCore->stKnownDevicesArr[i].bFound               = FALSE;
        apsthBTRCore->stKnownDevicesArr[i].bDeviceConnected     = FALSE;
        apsthBTRCore->stKnownDevicesArr[i].i32RSSI              = INT_MIN;

        memset (apsthBTRCore->stKnownDevicesArr[i].pcDeviceName,    '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stKnownDevicesArr[i].pcDeviceAddress, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stKnownDevicesArr[i].pcDevicePath,    '\0', sizeof(BD_NAME));

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
    const char* apcDeviceMac
) {
    tBTRCoreDevId   lBTRCoreDevId = 0;
    char            lcDevHdlArr[13] = {'\0'};

    // MAC Address Format 
    // AA:BB:CC:DD:EE:FF\0
    if (apcDeviceMac && (strlen(apcDeviceMac) >= 17)) {
        lcDevHdlArr[0]  = apcDeviceMac[0];
        lcDevHdlArr[1]  = apcDeviceMac[1];
        lcDevHdlArr[2]  = apcDeviceMac[3];
        lcDevHdlArr[3]  = apcDeviceMac[4];
        lcDevHdlArr[4]  = apcDeviceMac[6];
        lcDevHdlArr[5]  = apcDeviceMac[7];
        lcDevHdlArr[6]  = apcDeviceMac[9];
        lcDevHdlArr[7]  = apcDeviceMac[10];
        lcDevHdlArr[8]  = apcDeviceMac[12];
        lcDevHdlArr[9]  = apcDeviceMac[13];
        lcDevHdlArr[10] = apcDeviceMac[15];
        lcDevHdlArr[11] = apcDeviceMac[16];

        lBTRCoreDevId = (tBTRCoreDevId) strtoll(lcDevHdlArr, NULL, 16);
    }

    return lBTRCoreDevId;
}

static enBTRCoreDeviceClass
btrCore_MapClassIDtoAVDevClass (
    unsigned int aui32ClassId
) {
    enBTRCoreDeviceClass rc = enBTRCore_DC_Unknown;

    if (((aui32ClassId & 0x400u) == 0x400u) || ((aui32ClassId & 0x200u) == 0x200u) || ((aui32ClassId & 0x100u) == 0x100u)) {
        unsigned int ui32DevClassID = aui32ClassId & 0xFFFu;
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

static enBTRCoreDeviceClass
btrCore_MapServiceClasstoDevType (
    unsigned int aui32ClassId
) {
    enBTRCoreDeviceClass rc = enBTRCore_DC_Unknown;

    /* Refer https://www.bluetooth.com/specifications/assigned-numbers/baseband
     * The bit 18 set to represent AUDIO OUT service Devices.
     * The bit 19 can be set to represent AUDIO IN Service devices
     * The bit 21 set to represent AUDIO Services (Mic, Speaker, headset).
     * The bit 22 set to represent Telephone Services (headset).
     */

    if (0x40000u & aui32ClassId) {
        BTRCORELOG_TRACE ("Its a Rendering Class of Service.\n");
        if ((rc = btrCore_MapClassIDtoAVDevClass(aui32ClassId)) == enBTRCore_DC_Unknown) {
            BTRCORELOG_TRACE ("Its a Rendering Class of Service. But no Audio Device Class defined\n");
        }
    }
    else if (0x80000u & aui32ClassId) {
        BTRCORELOG_TRACE ("Its a Capturing Service.\n");
        if ((rc = btrCore_MapClassIDtoAVDevClass(aui32ClassId)) == enBTRCore_DC_Unknown) {
            BTRCORELOG_TRACE ("Its a Capturing Service. But no Audio Device Class defined\n");
        }
    }
    else if (0x200000u & aui32ClassId) {
        BTRCORELOG_TRACE ("Its a Audio Class of Service.\n");
        if ((rc = btrCore_MapClassIDtoAVDevClass(aui32ClassId)) == enBTRCore_DC_Unknown) {
            BTRCORELOG_TRACE ("Its a Audio Class of Service. But no Audio Device Class defined; Lets assume its Loud Speaker\n");
            rc = enBTRCore_DC_Loudspeaker;
        }
    }
    else if (0x400000u & aui32ClassId) {
        BTRCORELOG_TRACE ("Its a Telephony Class of Service. So, enBTDevAudioSink\n");
        if ((rc = btrCore_MapClassIDtoAVDevClass(aui32ClassId)) == enBTRCore_DC_Unknown) {
            BTRCORELOG_TRACE ("Its a Telephony Class of Service. But no Audio Device Class defined;\n");
        }
    }

    return rc;
}

static enBTRCoreDeviceClass
btrCore_MapClassIDtoDevClass (
    unsigned int aui32ClassId
) {
    enBTRCoreDeviceClass rc = enBTRCore_DC_Unknown;
    BTRCORELOG_DEBUG ("classID = 0x%x\n", aui32ClassId);

    if (aui32ClassId == enBTRCore_DC_Tile) {
        BTRCORELOG_INFO ("enBTRCore_DC_Tile\n");
        rc = enBTRCore_DC_Tile;
    }

    if (rc == enBTRCore_DC_Unknown)
        rc = btrCore_MapServiceClasstoDevType(aui32ClassId);

    /* If the Class of Service is not audio, lets parse the COD */
    if (rc == enBTRCore_DC_Unknown) {
        if ((aui32ClassId & 0x500u) == 0x500u) {
            unsigned int ui32DevClassID = aui32ClassId & 0xFFFu;
            BTRCORELOG_DEBUG ("ui32DevClassID = 0x%x\n", ui32DevClassID);

            if (ui32DevClassID == enBTRCore_DC_HID_Keyboard) {
                BTRCORELOG_INFO ("Its a enBTRCore_DC_HID_Keyboard\n");
                rc = enBTRCore_DC_HID_Keyboard;
            }
            else if (ui32DevClassID == enBTRCore_DC_HID_Mouse) {
                BTRCORELOG_INFO ("Its a enBTRCore_DC_HID_Mouse\n");
                rc = enBTRCore_DC_HID_Mouse;
            }
            else if (ui32DevClassID == enBTRCore_DC_HID_MouseKeyBoard) {
                BTRCORELOG_INFO ("Its a enBTRCore_DC_HID_MouseKeyBoard\n");
                rc = enBTRCore_DC_HID_MouseKeyBoard;
            }
            else if (ui32DevClassID == enBTRCore_DC_HID_Joystick) {
                BTRCORELOG_INFO ("Its a enBTRCore_DC_HID_Joystick\n");
                rc = enBTRCore_DC_HID_Joystick;
            }
        }
        else
        {
            rc = btrCore_MapClassIDtoAVDevClass(aui32ClassId);
        }
    }

    return rc;
}

static enBTRCoreDeviceType
btrCore_MapClassIDToDevType (
    unsigned int    aui32ClassId,
    enBTDeviceType  aeBtDeviceType
) {
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;
    enBTRCoreDeviceClass lenBTRCoreDevCl    = enBTRCore_DC_Unknown;

    switch (aeBtDeviceType) {
    case enBTDevAudioSink:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDevClass(aui32ClassId);
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
        else {
           lenBTRCoreDevType = enBTRCoreSpeakers;
        }
        break;
    case enBTDevAudioSource:
        lenBTRCoreDevCl = btrCore_MapClassIDtoDevClass(aui32ClassId);
        if (lenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
           lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
        }
        else if (lenBTRCoreDevCl == enBTRCore_DC_Tablet) {
           lenBTRCoreDevType = enBTRCorePCAudioIn;
        }
        else {
           lenBTRCoreDevType = enBTRCoreMobileAudioIn;
        }
        break;
    case enBTDevHFPHeadset:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevHFPAudioGateway:
        lenBTRCoreDevType =  enBTRCoreHeadSet;
        break;
    case enBTDevLE:
        lenBTRCoreDevType = enBTRCoreLE;
        break;
    case enBTDevHID:
        lenBTRCoreDevType = enBTRCoreHID;
        break;
    case enBTDevUnknown:
    default:
        lenBTRCoreDevType = enBTRCoreUnknown;
        break;
    }

    return lenBTRCoreDevType;
}


static enBTRCoreDeviceType
btrCore_MapDevClassToDevType (
    enBTRCoreDeviceClass    aenBTRCoreDevCl
) {
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;

    if (aenBTRCoreDevCl == enBTRCore_DC_WearableHeadset) {
       lenBTRCoreDevType =  enBTRCoreHeadSet;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_Headphones) {
       lenBTRCoreDevType = enBTRCoreHeadSet;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_Loudspeaker) {
       lenBTRCoreDevType = enBTRCoreSpeakers;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_HIFIAudioDevice) {
       lenBTRCoreDevType = enBTRCoreSpeakers;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_PortableAudio) {
       lenBTRCoreDevType = enBTRCoreSpeakers;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_CarAudio) {
       lenBTRCoreDevType = enBTRCoreSpeakers;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_SmartPhone) {
       lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_Tablet) {
       lenBTRCoreDevType = enBTRCorePCAudioIn;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_HID_Keyboard) {
       lenBTRCoreDevType = enBTRCoreHID;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_HID_Mouse) {
       lenBTRCoreDevType = enBTRCoreHID;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_HID_MouseKeyBoard) {
       lenBTRCoreDevType = enBTRCoreHID;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_HID_Joystick) {
       lenBTRCoreDevType = enBTRCoreHID;
    }
    else if (aenBTRCoreDevCl == enBTRCore_DC_Tile) {
        lenBTRCoreDevType = enBTRCoreLE;
        //TODO: May be use should have AudioDeviceClass & LE DeviceClass 
        //      will help us to identify the device Type as LE
    }
    else {
        lenBTRCoreDevType = enBTRCoreUnknown; 
    }

    return lenBTRCoreDevType;
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


static int
btrCore_AddDeviceToScannedDevicesArr (
    stBTRCoreHdl*       apsthBTRCore,
    stBTDeviceInfo*     apstBTDeviceInfo,
    stBTRCoreBTDevice*  apstFoundDevice
) {
    int                 i;
    stBTRCoreBTDevice   lstFoundDevice;

    memset(&lstFoundDevice, 0, sizeof(stBTRCoreBTDevice));


    lstFoundDevice.bFound               = FALSE;
    lstFoundDevice.i32RSSI              = apstBTDeviceInfo->i32RSSI;
    lstFoundDevice.ui32VendorId         = apstBTDeviceInfo->ui16Vendor;
    lstFoundDevice.tDeviceId            = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
    lstFoundDevice.enDeviceType         = btrCore_MapClassIDtoDevClass(apstBTDeviceInfo->ui32Class);
    lstFoundDevice.ui32DevClassBtSpec   = apstBTDeviceInfo->ui32Class;

    strncpy(lstFoundDevice.pcDeviceName,    apstBTDeviceInfo->pcName,       BD_NAME_LEN);
    strncpy(lstFoundDevice.pcDeviceAddress, apstBTDeviceInfo->pcAddress,    BD_NAME_LEN);
    strncpy(lstFoundDevice.pcDevicePath,    apstBTDeviceInfo->pcDevicePath, BD_NAME_LEN);

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
            else if ((lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_GATT_TILE_1, NULL, 16)) ||
                     (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_GATT_TILE_2, NULL, 16)) ||
                     (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_GATT_TILE_3, NULL, 16))) {
                lstFoundDevice.enDeviceType = enBTRCore_DC_Tile;
            }
            else if (lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_HID_1, NULL, 16) ||
                     lstFoundDevice.stDeviceProfile.profile[i].uuid_value == strtol(BTR_CORE_HID_2, NULL, 16)) {
                lstFoundDevice.enDeviceType = enBTRCore_DC_HID_Keyboard;
            }
        }
    }

    if (apsthBTRCore->aenDeviceDiscoveryType == enBTRCoreHID) {
        if ((lstFoundDevice.enDeviceType != enBTRCore_DC_HID_Keyboard)      &&
            (lstFoundDevice.enDeviceType != enBTRCore_DC_HID_Mouse)         &&
            (lstFoundDevice.enDeviceType != enBTRCore_DC_HID_Joystick)      &&
            (lstFoundDevice.enDeviceType != enBTRCore_DC_HID_MouseKeyBoard) ){
            BTRCORELOG_ERROR("Skipped the device %s DevID = %lld as the it is not interested device\n", lstFoundDevice.pcDeviceAddress, lstFoundDevice.tDeviceId);
            return -1;
        }
    }

    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if ((lstFoundDevice.tDeviceId == apsthBTRCore->stScannedDevicesArr[i].tDeviceId) || (apsthBTRCore->stScannedDevicesArr[i].bFound == FALSE)) {
            BTRCORELOG_DEBUG ("Unique DevID = %lld\n",      lstFoundDevice.tDeviceId);
            BTRCORELOG_DEBUG ("Adding %s at location %d\n", lstFoundDevice.pcDeviceAddress, i);

            lstFoundDevice.bFound   = TRUE;     //mark the record as found

            memcpy (&apsthBTRCore->stScannedDevicesArr[i], &lstFoundDevice, sizeof(stBTRCoreBTDevice));

            apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState;
            apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStFound;

            apsthBTRCore->numOfScannedDevices++;

            break;
        }
    }


    if ((i < BTRCORE_MAX_NUM_BT_DEVICES) || (lstFoundDevice.enDeviceType == enBTRCore_DC_Tile)) {

        if (lstFoundDevice.enDeviceType == enBTRCore_DC_Tile) {
            lstFoundDevice.bFound   = TRUE;     //mark the record as found
        }

        memcpy(apstFoundDevice, &lstFoundDevice, sizeof(stBTRCoreBTDevice));
        return i;
    }

    BTRCORELOG_DEBUG ("Skipped %s DevID = %lld\n", lstFoundDevice.pcDeviceAddress, lstFoundDevice.tDeviceId);
    return -1;
}

static enBTRCoreRet
btrCore_RemoveDeviceFromScannedDevicesArr (
    stBTRCoreHdl*       apstlhBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    stBTRCoreBTDevice*  astRemovedDevice
) {
    enBTRCoreRet    retResult   = enBTRCoreSuccess;
    int             i32LoopIdx  = -1;

    for (i32LoopIdx = 0; i32LoopIdx < apstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
        if (apstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId == aBTRCoreDevId) {
            break;
        }
    }

    if (i32LoopIdx != apstlhBTRCore->numOfScannedDevices) {
        BTRCORELOG_TRACE ("i32ScannedDevIdx = %d\n", i32LoopIdx);
        BTRCORELOG_TRACE ("pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].eDeviceCurrState = %d\n", apstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx].eDeviceCurrState);
        BTRCORELOG_TRACE ("pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].eDevicePrevState = %d\n", apstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx].eDevicePrevState);

        memcpy (astRemovedDevice, &apstlhBTRCore->stScannedDevicesArr[i32LoopIdx], sizeof(stBTRCoreBTDevice));
        astRemovedDevice->bFound = FALSE;

        // Clean flipping logic. This will suffice
        if (i32LoopIdx != apstlhBTRCore->numOfScannedDevices - 1) {
            memcpy (&apstlhBTRCore->stScannedDevicesArr[i32LoopIdx], &apstlhBTRCore->stScannedDevicesArr[apstlhBTRCore->numOfScannedDevices - 1], sizeof(stBTRCoreBTDevice));
            memcpy (&apstlhBTRCore->stScannedDevStInfoArr[i32LoopIdx], &apstlhBTRCore->stScannedDevStInfoArr[apstlhBTRCore->numOfScannedDevices - 1], sizeof(stBTRCoreDevStateInfo));
        }

        memset(&apstlhBTRCore->stScannedDevicesArr[apstlhBTRCore->numOfScannedDevices - 1], 0, sizeof(stBTRCoreBTDevice));
        memset(&apstlhBTRCore->stScannedDevStInfoArr[apstlhBTRCore->numOfScannedDevices - 1], 0, sizeof(stBTRCoreDevStateInfo));

        apstlhBTRCore->numOfScannedDevices--;
    }
    else {
        BTRCORELOG_ERROR ("Device %lld not found in Scanned List!\n", aBTRCoreDevId);
        retResult = enBTRCoreDeviceNotFound;
    }

    return retResult;
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

    memcpy (&apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx], &apsthBTRCore->stScannedDevicesArr[i32ScannedDevIdx], sizeof(stBTRCoreBTDevice));
    apsthBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected   = apstBTDeviceInfo->bConnected;

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
        knownDevicesArr[i_idx].enDeviceType         = btrCore_MapClassIDtoDevClass(pairedDeviceInfo->deviceInfo[i_idx].ui32Class);
        knownDevicesArr[i_idx].ui32DevClassBtSpec   = pairedDeviceInfo->deviceInfo[i_idx].ui32Class;

        strncpy(knownDevicesArr[i_idx].pcDeviceName,    pairedDeviceInfo->deviceInfo[i_idx].pcName,     BD_NAME_LEN);
        strncpy(knownDevicesArr[i_idx].pcDeviceAddress, pairedDeviceInfo->deviceInfo[i_idx].pcAddress,  BD_NAME_LEN);
        strncpy(knownDevicesArr[i_idx].pcDevicePath,    pairedDeviceInfo->devicePath[i_idx],            BD_NAME_LEN);
   
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
                else if ((knownDevicesArr[i_idx].stDeviceProfile.profile[j_idx].uuid_value == strtol(BTR_CORE_HID_1, NULL, 16)) ||
                         (knownDevicesArr[i_idx].stDeviceProfile.profile[j_idx].uuid_value == strtol(BTR_CORE_HID_2, NULL, 16))) {
                    knownDevicesArr[i_idx].enDeviceType = enBTRCore_DC_HID_Keyboard;
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


static const char*
btrCore_GetKnownDeviceMac (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int i32LoopIdx = -1;

    if (apsthBTRCore->numOfPairedDevices) {
        for (i32LoopIdx = 0; i32LoopIdx < apsthBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId)
                return apsthBTRCore->stKnownDevicesArr[i32LoopIdx].pcDeviceAddress;
        }
    }

    return NULL;
}


static enBTRCoreRet
btrCore_GetDeviceInfo (
    stBTRCoreHdl*           apsthBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType,
    enBTDeviceType*         apenBTDeviceType,
    stBTRCoreBTDevice**     appstBTRCoreBTDevice,
    stBTRCoreDevStateInfo** appstBTRCoreDevStateInfo,
    const char**            appcBTRCoreBTDevicePath,
    const char**            appcBTRCoreBTDeviceName
) {
    stBTRCoreBTDevice*      lpstBTRCoreBTDeviceArr  = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStInfoArr = NULL;
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;


    if (!apsthBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        btrCore_PopulateListOfPairedDevices(apsthBTRCore, apsthBTRCore->curAdapterPath);    /* Keep the list upto date */
    }


    if (aenBTRCoreDevType != enBTRCoreLE) {
        ui32NumOfDevices        = apsthBTRCore->numOfPairedDevices;
        lpstBTRCoreBTDeviceArr  = apsthBTRCore->stKnownDevicesArr;
        lpstBTRCoreDevStInfoArr = apsthBTRCore->stKnownDevStInfoArr;
    }
    else {
        ui32NumOfDevices        = apsthBTRCore->numOfScannedDevices;
        lpstBTRCoreBTDeviceArr  = apsthBTRCore->stScannedDevicesArr;
        lpstBTRCoreDevStInfoArr = apsthBTRCore->stScannedDevStInfoArr;
    }


    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried/scanned for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        *appstBTRCoreBTDevice       = &lpstBTRCoreBTDeviceArr[aBTRCoreDevId];
        *appstBTRCoreDevStateInfo   = &lpstBTRCoreDevStInfoArr[aBTRCoreDevId];
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == lpstBTRCoreBTDeviceArr[ui32LoopIdx].tDeviceId) {
                *appstBTRCoreBTDevice       = &lpstBTRCoreBTDeviceArr[ui32LoopIdx];
                *appstBTRCoreDevStateInfo   = &lpstBTRCoreDevStInfoArr[ui32LoopIdx];
                break;
            }
        }
    }

    if (*appstBTRCoreBTDevice) {
        *appcBTRCoreBTDevicePath    = (*appstBTRCoreBTDevice)->pcDevicePath;
        *appcBTRCoreBTDeviceName    = (*appstBTRCoreBTDevice)->pcDeviceName;
    }

    if (!(*appcBTRCoreBTDevicePath) || !strlen(*appcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired/scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }


    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        *apenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        *apenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreLE:
        *apenBTDeviceType = enBTDevLE;
        break;
    case enBTRCoreUnknown:
    default:
        *apenBTDeviceType = enBTDevUnknown;
        break;
    }

    return enBTRCoreSuccess;
}


static enBTRCoreRet
btrCore_GetDeviceInfoKnown (
    stBTRCoreHdl*           apsthBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType,
    enBTDeviceType*         apenBTDeviceType,
    stBTRCoreBTDevice**     appstBTRCoreBTDevice,
    stBTRCoreDevStateInfo** appstBTRCoreDevStateInfo,
    const char**            appcBTRCoreBTDevicePath
) {
    unsigned int            ui32NumOfDevices        = 0;
    unsigned int            ui32LoopIdx             = 0;

    if (!apsthBTRCore->numOfPairedDevices) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        btrCore_PopulateListOfPairedDevices(apsthBTRCore, apsthBTRCore->curAdapterPath); /* Keep the list upto date */
    }


    ui32NumOfDevices = apsthBTRCore->numOfPairedDevices;
    if (!ui32NumOfDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        *appstBTRCoreBTDevice       = &apsthBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        *appstBTRCoreDevStateInfo   = &apsthBTRCore->stKnownDevStInfoArr[aBTRCoreDevId];
    }
    else {
        for (ui32LoopIdx = 0; ui32LoopIdx < ui32NumOfDevices; ui32LoopIdx++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[ui32LoopIdx].tDeviceId) {
                *appstBTRCoreBTDevice       = &apsthBTRCore->stKnownDevicesArr[ui32LoopIdx];
                *appstBTRCoreDevStateInfo   = &apsthBTRCore->stKnownDevStInfoArr[ui32LoopIdx];
            }
        }
    }

    if (*appstBTRCoreBTDevice) {
        *appcBTRCoreBTDevicePath = (*appstBTRCoreBTDevice)->pcDevicePath;
    }

    if (!(*appcBTRCoreBTDevicePath) || !strlen(*appcBTRCoreBTDevicePath)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }


    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        *apenBTDeviceType  = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        *apenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        *apenBTDeviceType = enBTDevUnknown;
        break;
    }

    return enBTRCoreSuccess;
}

static void 
btrCore_ShowSignalStrength (
    short strength
) {
    short pos_str;

    pos_str = 100 + strength;//strength is usually negative with number meaning more strength

    BTRCORELOG_TRACE (" Signal Strength: %d dbmv  \n", strength);

    if (pos_str > 70) {
        BTRCORELOG_TRACE ("++++\n");
    }

    if ((pos_str > 50) && (pos_str <= 70)) {
        BTRCORELOG_TRACE ("+++\n");
    }

    if ((pos_str > 37) && (pos_str <= 50)) {
        BTRCORELOG_TRACE ("++\n");
    }

    if (pos_str <= 37) {
        BTRCORELOG_TRACE ("+\n");
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

        else if (!strcasecmp(aUUID, BTR_CORE_GATT_TILE_3))
            strcpy(pServiceNameOut, BTR_CORE_GATT_TILE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_GEN_ACCESS))
            strcpy(pServiceNameOut, BTR_CORE_GEN_ACCESS_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_GEN_ATTRIBUTE))
            strcpy(pServiceNameOut, BTR_CORE_GEN_ATTRIBUTE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_DEVICE_INFO))
            strcpy(pServiceNameOut, BTR_CORE_DEVICE_INFO_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_BATTERY_SERVICE))
            strcpy(pServiceNameOut, BTR_CORE_BATTERY_SERVICE_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HID_1))
            strcpy(pServiceNameOut, BTR_CORE_HID_TEXT);

        else if (!strcasecmp(aUUID, BTR_CORE_HID_2))
            strcpy(pServiceNameOut, BTR_CORE_HID_TEXT);

        else
            strcpy (pServiceNameOut, "Not Identified");
    }
    else
        strcpy (pServiceNameOut, "Not Identified");

    return uuid_value;
}


static enBTRCoreDeviceState
btrCore_BTParseDeviceState (
    const char* pcStateValue
) {
    enBTRCoreDeviceState rc = enBTRCoreDevStInitialized;

    if ((pcStateValue) && (pcStateValue[0] != '\0')) {
        BTRCORELOG_DEBUG ("Current State of this connection is @@%s@@\n", pcStateValue);

        if (!strcasecmp("unpaired", pcStateValue)) {
            rc = enBTRCoreDevStUnpaired;
        }
        else if (!strcasecmp("paired", pcStateValue)) {
            rc = enBTRCoreDevStPaired;
        }
        else if (!strcasecmp("disconnected", pcStateValue)) {
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
    if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBAdapterStatus) {
        if ((lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTAdapterInfo)))) {
            memcpy(lpstOutTaskGAqData->pvBTRCoreTskInData, (stBTAdapterInfo*)apvOutTaskInData, sizeof(stBTAdapterInfo));
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBDeviceDisc) {
        if ((apvOutTaskInData) &&
            (lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTRCoreOTskInData))) &&
            (((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo = g_malloc0(sizeof(stBTDeviceInfo)))) {
            memcpy(((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo, (stBTDeviceInfo*)((stBTRCoreOTskInData*)apvOutTaskInData)->pstBTDevInfo, sizeof(stBTDeviceInfo));
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->bTRCoreDevId    = ((stBTRCoreOTskInData*)apvOutTaskInData)->bTRCoreDevId;
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->enBTRCoreDevType= ((stBTRCoreOTskInData*)apvOutTaskInData)->enBTRCoreDevType;
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBDeviceRemoved) {
        if ((apvOutTaskInData) &&
            (lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTRCoreOTskInData))) &&
            (((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo = g_malloc0(sizeof(stBTDeviceInfo)))) {
            memcpy(((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo, (stBTDeviceInfo*)((stBTRCoreOTskInData*)apvOutTaskInData)->pstBTDevInfo, sizeof(stBTDeviceInfo));
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->bTRCoreDevId    = ((stBTRCoreOTskInData*)apvOutTaskInData)->bTRCoreDevId;
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->enBTRCoreDevType= ((stBTRCoreOTskInData*)apvOutTaskInData)->enBTRCoreDevType;
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBDeviceLost) {
        if ((apvOutTaskInData) &&
            (lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTRCoreOTskInData))) &&
            (((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo = g_malloc0(sizeof(stBTDeviceInfo)))) {
            memcpy(((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo, (stBTDeviceInfo*)((stBTRCoreOTskInData*)apvOutTaskInData)->pstBTDevInfo, sizeof(stBTDeviceInfo));
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->bTRCoreDevId    = ((stBTRCoreOTskInData*)apvOutTaskInData)->bTRCoreDevId;
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->enBTRCoreDevType= ((stBTRCoreOTskInData*)apvOutTaskInData)->enBTRCoreDevType;
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBDeviceStatus) {
        if ((apvOutTaskInData) &&
            (lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTRCoreOTskInData))) &&
            (((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo = g_malloc0(sizeof(stBTDeviceInfo)))) {
            memcpy(((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->pstBTDevInfo, (stBTDeviceInfo*)((stBTRCoreOTskInData*)apvOutTaskInData)->pstBTDevInfo, sizeof(stBTDeviceInfo));
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->bTRCoreDevId    = ((stBTRCoreOTskInData*)apvOutTaskInData)->bTRCoreDevId;
            ((stBTRCoreOTskInData*)lpstOutTaskGAqData->pvBTRCoreTskInData)->enBTRCoreDevType= ((stBTRCoreOTskInData*)apvOutTaskInData)->enBTRCoreDevType;
        }
    }
    else if (lpstOutTaskGAqData->enBTRCoreTskPT == enBTRCoreTaskPTcBMediaStatus) {
        if ((lpstOutTaskGAqData->pvBTRCoreTskInData = g_malloc0(sizeof(stBTRCoreMediaStatusCBInfo)))) {
            memcpy(lpstOutTaskGAqData->pvBTRCoreTskInData, (stBTRCoreMediaStatusCBInfo*)apvOutTaskInData, sizeof(stBTRCoreMediaStatusCBInfo));
        }
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
    
    if (!(penExitStatusRunTask = g_malloc0(sizeof(enBTRCoreRet)))) {
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
    guint16                     lui16msTimeout      = 50;
    gboolean                    lbOutTaskExit       = FALSE;
    enBTRCoreTaskOp             lenOutTskOpPrv      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskOp             lenOutTskOpCur      = enBTRCoreTaskOpUnknown;
    enBTRCoreTaskProcessType    lenOutTskPTCur      = enBTRCoreTaskPTUnknown;
    stBTRCoreOTskInData*        lpstOutTskInData    = NULL;
    stBTRCoreTaskGAqData*       lpstOutTaskGAqData  = NULL;



    pstlhBTRCore = (stBTRCoreHdl*)apsthBTRCore;

    if (!(penExitStatusOutTask = g_malloc0(sizeof(enBTRCoreRet)))) {
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
                lenOutTskOpCur  = lpstOutTaskGAqData->enBTRCoreTskOp;
                lenOutTskPTCur  = lpstOutTaskGAqData->enBTRCoreTskPT;
                lpstOutTskInData= lpstOutTaskGAqData->pvBTRCoreTskInData;
                g_free(lpstOutTaskGAqData);
                lpstOutTaskGAqData = NULL;
                BTRCORELOG_INFO ("g_async_queue_timeout_pop %d %d %p\n", lenOutTskOpCur, lenOutTskPTCur, lpstOutTskInData);
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
        /* Should handle all State updates - Handle with care */
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

                    if (lpstOutTskInData && lpstOutTskInData->pstBTDevInfo) {
                        stBTDeviceInfo*     lpstBTDeviceInfo = (stBTDeviceInfo*)lpstOutTskInData->pstBTDevInfo;
                        stBTRCoreBTDevice   lstFoundDevice;
                        int                 i32ScannedDevIdx = -1;

                        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(pstlhBTRCore, lpstBTDeviceInfo, &lstFoundDevice)) != -1) {
                            BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d\n", i32ScannedDevIdx);

                            pstlhBTRCore->stDiscoveryCbInfo.type = enBTRCoreOpTypeDevice;
                            memcpy(&pstlhBTRCore->stDiscoveryCbInfo.device, &lstFoundDevice, sizeof(stBTRCoreBTDevice));

                            if (pstlhBTRCore->fpcBBTRCoreDeviceDisc) {
                                if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreDeviceDisc(&pstlhBTRCore->stDiscoveryCbInfo,
                                                                                          pstlhBTRCore->pvcBDevDiscUserData)) != enBTRCoreSuccess) {
                                    BTRCORELOG_ERROR ("Failure fpcBBTRCoreDeviceDisc Ret = %d\n", lenBTRCoreRet);
                                }
                            }
                        }

                        g_free(lpstOutTskInData->pstBTDevInfo);
                        lpstOutTskInData->pstBTDevInfo = NULL;
                    }

                    if (lpstOutTskInData) {
                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBDeviceRemoved) {

                    if (lpstOutTskInData && lpstOutTskInData->pstBTDevInfo) {
                        stBTDeviceInfo*     lpstBTDeviceInfo = (stBTDeviceInfo*)lpstOutTskInData->pstBTDevInfo;
                        stBTRCoreBTDevice   lstRemovedDevice;
                        tBTRCoreDevId       lBTRCoreDevId = lpstOutTskInData->bTRCoreDevId;

                        (void)lpstBTDeviceInfo;
                        memset (&lstRemovedDevice, 0, sizeof(stBTRCoreBTDevice));

                        lenBTRCoreRet = btrCore_RemoveDeviceFromScannedDevicesArr (pstlhBTRCore, lBTRCoreDevId, &lstRemovedDevice);

                        if (lenBTRCoreRet == enBTRCoreSuccess && lstRemovedDevice.tDeviceId) {

                            pstlhBTRCore->stDiscoveryCbInfo.type = enBTRCoreOpTypeDevice;
                            memcpy(&pstlhBTRCore->stDiscoveryCbInfo.device, &lstRemovedDevice, sizeof(stBTRCoreBTDevice));

                            if (pstlhBTRCore->fpcBBTRCoreDeviceDisc) {
                                if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreDeviceDisc(&pstlhBTRCore->stDiscoveryCbInfo,
                                        pstlhBTRCore->pvcBDevDiscUserData)) != enBTRCoreSuccess) {
                                    BTRCORELOG_ERROR ("Failure fpcBBTRCoreDeviceDisc Ret = %d\n", lenBTRCoreRet);
                                }
                            }
                        }
                        else {
                            BTRCORELOG_ERROR ("Failed to remove dev %lld from Scanned List | Ret = %d\n", lBTRCoreDevId, lenBTRCoreRet);
                        }


                        if (!pstlhBTRCore->numOfScannedDevices) {
                            BTRCORELOG_INFO ("\nClearing Scanned Device List...\n");
                            btrCore_ClearScannedDevicesList(pstlhBTRCore);

                            pstlhBTRCore->stDevStatusCbInfo.deviceId         = 0; // Need to have any special IDs for this purpose like 0xFFFFFFFF
                            pstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = enBTRCoreDevStFound;
                            pstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStLost;
                            pstlhBTRCore->stDevStatusCbInfo.isPaired         = 0;           

                            if (pstlhBTRCore->fpcBBTRCoreStatus) {
                                // We are already in OutTask - But use btrCore_OutTaskAddOp - Dont trigger Status callbacks from DeviceRemoved Process Type
                                //if (pstlhBTRCore->fpcBBTRCoreStatus(&pstlhBTRCore->stDevStatusCbInfo, pstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
                                //}      
                            }
                        }

                        g_free(lpstOutTskInData->pstBTDevInfo);
                        lpstOutTskInData->pstBTDevInfo = NULL;
                    }

                    if (lpstOutTskInData) {
                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBDeviceLost) {

                    if (lpstOutTskInData && lpstOutTskInData->pstBTDevInfo) {
                        stBTDeviceInfo*     lpstBTDeviceInfo = (stBTDeviceInfo*)lpstOutTskInData->pstBTDevInfo;
                        tBTRCoreDevId       lBTRCoreDevId = lpstOutTskInData->bTRCoreDevId;
                        int                 i32LoopIdx = -1;
                        int                 i32KnownDevIdx  = -1;
                        gboolean            postEvent = FALSE;

                        (void)lpstBTDeviceInfo;

                        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                            if (lBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                                i32KnownDevIdx = i32LoopIdx;
                                break;
                            }
                        }


                        if (pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId == lBTRCoreDevId) {
                            BTRCORELOG_INFO ("Device %llu power state Off or OOR\n", pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId);
                            BTRCORELOG_TRACE ("i32LoopIdx = %d\n", i32KnownDevIdx);
                            BTRCORELOG_TRACE ("pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                            BTRCORELOG_TRACE ("pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);

                            if (((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStConnected) ||
                                 (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStPlaying)) &&
                                 (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnecting)) {

                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStDisconnected;
                                pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
                                postEvent = TRUE;
                            }
                            else if (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStPlaying   ||
                                     pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected ) {

                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState =  pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStLost;
                                pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
                                postEvent = TRUE;
                            }

                            // move this out of if block. populating stDevStatusCbInfo should be done common for both paired and scanned devices 
                            if (postEvent) {
                                pstlhBTRCore->stDevStatusCbInfo.deviceId           = pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceClass       = pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType;
                                pstlhBTRCore->stDevStatusCbInfo.ui32DevClassBtSpec = pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].ui32DevClassBtSpec;
                                pstlhBTRCore->stDevStatusCbInfo.eDevicePrevState   = pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState   = pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceType        = btrCore_MapDevClassToDevType(pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType);
                                pstlhBTRCore->stDevStatusCbInfo.isPaired           = 1;
                                strncpy(pstlhBTRCore->stDevStatusCbInfo.deviceName, pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceName, BD_NAME_LEN - 1);

                                if (pstlhBTRCore->fpcBBTRCoreStatus) {
                                    if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreStatus(&pstlhBTRCore->stDevStatusCbInfo, pstlhBTRCore->pvcBStatusUserData)) != enBTRCoreSuccess) {
                                        BTRCORELOG_ERROR ("Failure fpcBBTRCoreStatus Ret = %d\n", lenBTRCoreRet);
                                    }
                                }
                            }
                        }

                        g_free(lpstOutTskInData->pstBTDevInfo);
                        lpstOutTskInData->pstBTDevInfo = NULL;
                    }

                    if (lpstOutTskInData) {
                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBDeviceStatus) {

                    if (lpstOutTskInData && lpstOutTskInData->pstBTDevInfo) {
                        stBTDeviceInfo* lpstBTDeviceInfo = (stBTDeviceInfo*)lpstOutTskInData->pstBTDevInfo;

                        int             i32LoopIdx       = -1;
                        int             i32KnownDevIdx   = -1;
                        int             i32ScannedDevIdx = -1;

                        tBTRCoreDevId        lBTRCoreDevId = lpstOutTskInData->bTRCoreDevId;
                        enBTRCoreDeviceType  lenBTRCoreDevType = lpstOutTskInData->enBTRCoreDevType;
                        enBTRCoreDeviceState leBTDevState  = btrCore_BTParseDeviceState(lpstBTDeviceInfo->pcDeviceCurrState);


                        if (pstlhBTRCore->numOfPairedDevices) {
                            for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                                if (lBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                                    i32KnownDevIdx = i32LoopIdx;
                                    break;
                                }
                            }
                        }

                        if (pstlhBTRCore->numOfScannedDevices) {
                            for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
                                if (lBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
                                    i32ScannedDevIdx = i32LoopIdx;
                                    break;
                                }
                            }
                        }


                        // Current device for which Property has changed must be either in Found devices or Paired devices
                        // TODO: if-else's for SM or HSM are bad. Find a better way
                        if (((i32ScannedDevIdx != -1) || (i32KnownDevIdx != -1)) && (leBTDevState != enBTRCoreDevStInitialized)) {
                            BOOLEAN                 bTriggerDevStatusChangeCb   = FALSE;
                            stBTRCoreBTDevice*      lpstBTRCoreBTDevice         = NULL;
                            stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo     = NULL;

                            if (i32KnownDevIdx != -1) {

                                BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);
                                BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                                BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                                BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);

                                if ((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != leBTDevState) &&
                                    (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                                    if ((enBTRCoreMobileAudioIn != lenBTRCoreDevType) && (enBTRCorePCAudioIn != lenBTRCoreDevType)) {

                                        if ( !(((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected) && (leBTDevState == enBTRCoreDevStDisconnected)) ||
                                               ((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) && (leBTDevState == enBTRCoreDevStConnected) && 
                                                ((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStPaired) || 
                                                 (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStConnecting))))) {
                                            bTriggerDevStatusChangeCb = TRUE;
                                        }
                                        // To make the state changes in a better logical way once the BTRCore dev structures are unified further

                                        //workaround for notifying the power Up event of a <paired && !connected> devices, as we are not able to track the
                                        //power Down event of such devices as per the current analysis
                                        if ((leBTDevState == enBTRCoreDevStDisconnected) &&
                                            ((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected) ||
                                             (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStDisconnecting))) {
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = enBTRCoreDevStPaired;
                                        }
                                        else if ( !((leBTDevState == enBTRCoreDevStConnected) &&
                                                    ((pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnecting) ||
                                                     (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected)  ||
                                                     (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStInitialized)))) {
                                           pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                        }
                                     

                                        if ((leBTDevState == enBTRCoreDevStDisconnected) &&
                                            (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStPlaying)) {
                                            leBTDevState = enBTRCoreDevStLost;
                                            pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
                                        }


                                        if ( !((leBTDevState == enBTRCoreDevStDisconnected) &&
                                               (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStLost) &&
                                               (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState == enBTRCoreDevStPlaying))) {

                                            if ( !((leBTDevState == enBTRCoreDevStConnected) &&
                                                 (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStLost) &&
                                                 (pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState != enBTRCoreDevStPlaying))) {
                                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;
                                            }
                                            else {
                                                pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStConnecting;
                                            }
                                        }
                                        else {
                                            leBTDevState = enBTRCoreDevStConnected;
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStConnecting;
                                        }

                                    }
                                    else {
                                        bTriggerDevStatusChangeCb = TRUE;

                                        if (enBTRCoreDevStDisconnected == leBTDevState) {
                                            pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected = FALSE;
                                        }

                                        if (enBTRCoreDevStInitialized != pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState) {
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState =
                                                                           pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                                        }
                                        else {
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = enBTRCoreDevStConnecting;
                                        }

                                        pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;

                                        //TODO: There should be no need to do this. Find out why the enDeviceType = 0;
                                        pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType = btrCore_MapClassIDtoDevClass(lpstBTDeviceInfo->ui32Class);
                                    }

                                    pstlhBTRCore->stDevStatusCbInfo.isPaired = 1;
                                    lpstBTRCoreBTDevice     = &pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx];
                                    lpstBTRCoreDevStateInfo = &pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx];

                                    BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);
                                    BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState);
                                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = %d\n", pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState);
                                    BTRCORELOG_TRACE ("lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType       = %x\n", pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType);
                                }
                            }
                            else if (i32ScannedDevIdx != -1) {

                                BTRCORELOG_TRACE ("i32ScannedDevIdx = %d\n", i32ScannedDevIdx);
                                BTRCORELOG_TRACE ("leBTDevState = %d\n", leBTDevState);
                                BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState = %d\n", pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState);
                                BTRCORELOG_TRACE ("lpstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = %d\n", pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState);

                                if ((pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != leBTDevState) &&
                                    (pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {

                                    pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                                    pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState = leBTDevState;
                                    pstlhBTRCore->stDevStatusCbInfo.isPaired = lpstBTDeviceInfo->bPaired;

                                    lpstBTRCoreBTDevice     = &pstlhBTRCore->stScannedDevicesArr[i32ScannedDevIdx];
                                    lpstBTRCoreDevStateInfo = &pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx];

                                    if (lpstBTDeviceInfo->bPaired       &&
                                        lpstBTDeviceInfo->bConnected    &&
                                        (pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState == enBTRCoreDevStFound) &&
                                        (pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState == enBTRCoreDevStConnected)) {

                                        if ((i32KnownDevIdx = btrCore_AddDeviceToKnownDevicesArr(pstlhBTRCore, lpstBTDeviceInfo)) != -1) {
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState;
                                            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDeviceCurrState;
                                            BTRCORELOG_DEBUG ("btrCore_AddDeviceToKnownDevicesArr - Success Index = %d\n", i32KnownDevIdx);
                                        }

                                        //TODO: Really really dont like this - Live with it for now
                                        if (pstlhBTRCore->numOfPairedDevices) {
                                            for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                                                if (lBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                                                    //i32KnownDevIdx = i32LoopIdx;
                                                    lpstBTRCoreBTDevice     = &pstlhBTRCore->stKnownDevicesArr[i32LoopIdx];
                                                    lpstBTRCoreDevStateInfo = &pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx];
                                                }
                                            }
                                        }
                                    }

                                    if (lenBTRCoreDevType   == enBTRCoreLE &&
                                        (leBTDevState       == enBTRCoreDevStConnected   ||
                                         leBTDevState       == enBTRCoreDevStDisconnected)) {
                                        lpstBTRCoreBTDevice     = &pstlhBTRCore->stScannedDevicesArr[i32ScannedDevIdx];
                                        lpstBTRCoreDevStateInfo = &pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx];

                                        if (leBTDevState == enBTRCoreDevStDisconnected) {
                                            lpstBTRCoreBTDevice->bDeviceConnected = FALSE;
                                        }

                                        if (pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState == enBTRCoreDevStInitialized) {
                                            pstlhBTRCore->stScannedDevStInfoArr[i32ScannedDevIdx].eDevicePrevState = enBTRCoreDevStConnecting;
                                        }
                                    }

                                    bTriggerDevStatusChangeCb = TRUE;
                                }
                            }

                            if (bTriggerDevStatusChangeCb == TRUE) {
                                pstlhBTRCore->stDevStatusCbInfo.deviceId           = lBTRCoreDevId;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceType        = lenBTRCoreDevType;
                                pstlhBTRCore->stDevStatusCbInfo.eDevicePrevState   = lpstBTRCoreDevStateInfo->eDevicePrevState;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState   = leBTDevState;
                                pstlhBTRCore->stDevStatusCbInfo.eDeviceClass       = lpstBTRCoreBTDevice->enDeviceType;
                                pstlhBTRCore->stDevStatusCbInfo.ui32DevClassBtSpec = lpstBTRCoreBTDevice->ui32DevClassBtSpec;
                                strncpy(pstlhBTRCore->stDevStatusCbInfo.deviceName,lpstBTRCoreBTDevice->pcDeviceName, BD_NAME_LEN - 1);

                                if (pstlhBTRCore->fpcBBTRCoreStatus) {
                                    if (pstlhBTRCore->fpcBBTRCoreStatus(&pstlhBTRCore->stDevStatusCbInfo, pstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
                                        /* Invoke the callback */
                                    }
                                }
                            }
                        }

                        g_free(lpstOutTskInData->pstBTDevInfo);
                        lpstOutTskInData->pstBTDevInfo = NULL;
                    }

                    if (lpstOutTskInData) {
                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBAdapterStatus) {
                    if (lpstOutTskInData) {
                        stBTRCoreAdapter* lpstBTRCoreAdapter = (stBTRCoreAdapter*)lpstOutTskInData;

                        BTRCORELOG_TRACE ("stDiscoveryCbInfo.adapter.bDiscovering = %d, lpstBTRCoreAdapter->bDiscovering = %d\n",
                                pstlhBTRCore->stDiscoveryCbInfo.adapter.bDiscovering, lpstBTRCoreAdapter->bDiscovering);

                        // invoke the callback to mgr only if the adapter's properties (such as its discovering state) changed
                        if (pstlhBTRCore->stDiscoveryCbInfo.adapter.bDiscovering != lpstBTRCoreAdapter->bDiscovering)
                        {
                            pstlhBTRCore->stDiscoveryCbInfo.type = enBTRCoreOpTypeAdapter;
                            memcpy(&pstlhBTRCore->stDiscoveryCbInfo.adapter, lpstBTRCoreAdapter, sizeof(stBTRCoreAdapter));
                            if (pstlhBTRCore->fpcBBTRCoreDeviceDisc) {
                                if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreDeviceDisc(&pstlhBTRCore->stDiscoveryCbInfo,
                                        pstlhBTRCore->pvcBDevDiscUserData)) != enBTRCoreSuccess) {
                                    BTRCORELOG_ERROR ("Failure fpcBBTRCoreDeviceDisc Ret = %d\n", lenBTRCoreRet);
                                }
                            }
                        }

                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
                }
                else if (lenOutTskPTCur == enBTRCoreTaskPTcBMediaStatus) {
                    if (lpstOutTskInData) {
                        stBTRCoreMediaStatusCBInfo* lpstMediaStatusUpdateCbInfo = (stBTRCoreMediaStatusCBInfo*)lpstOutTskInData;
                        tBTRCoreDevId               lBTRCoreDevId   = lpstMediaStatusUpdateCbInfo->deviceId;
                        int                         i32LoopIdx      = -1;
                        int                         i32KnownDevIdx  = -1;

                        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                            if (lBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].tDeviceId) {
                                i32KnownDevIdx = i32LoopIdx;
                                break;
                            }
                        }

                        BTRCORELOG_TRACE ("i32KnownDevIdx = %d\n", i32KnownDevIdx);

                        if ((i32KnownDevIdx != -1) && (pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].bDeviceConnected == TRUE)) {
                            memcpy(&pstlhBTRCore->stMediaStatusCbInfo, lpstMediaStatusUpdateCbInfo, sizeof(stBTRCoreMediaStatusCBInfo));
                            pstlhBTRCore->stMediaStatusCbInfo.deviceId      = pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].tDeviceId;
                            pstlhBTRCore->stMediaStatusCbInfo.eDeviceClass  = pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].enDeviceType;
                            strncpy(pstlhBTRCore->stMediaStatusCbInfo.deviceName, pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].pcDeviceName, BD_NAME_LEN-1);

                            if (pstlhBTRCore->fpcBBTRCoreMediaStatus) {
                                if ((lenBTRCoreRet = pstlhBTRCore->fpcBBTRCoreMediaStatus(&pstlhBTRCore->stMediaStatusCbInfo, pstlhBTRCore->pvcBMediaStatusUserData)) != enBTRCoreSuccess) {
                                    BTRCORELOG_ERROR ("Failure fpcBBTRCoreMediaStatus Ret = %d\n", lenBTRCoreRet);
                                }
                            }
                        }

                        g_free(lpstOutTskInData);
                        lpstOutTskInData = NULL;
                    }
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


    pstlhBTRCore = (stBTRCoreHdl*)g_malloc0(sizeof(stBTRCoreHdl));
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


    if(BtrCore_BTRegisterAdapterStatusUpdateCb(pstlhBTRCore->connHdl, &btrCore_BTAdapterStatusUpdateCb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Adapter Status CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
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

    if(BTRCore_LE_RegisterStatusUpdateCb(pstlhBTRCore->leHdl, &btrCore_BTLeStatusUpdateCb, pstlhBTRCore) != enBTRCoreSuccess) {
       BTRCORELOG_ERROR ("Failed to Register LE Status CB - enBTRCoreInitFailure\n");
       BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    *phBTRCore  = (tBTRCoreHandle)pstlhBTRCore;

    //Initialize array of known devices so we can use it for stuff
    btrCore_PopulateListOfPairedDevices(*phBTRCore, pstlhBTRCore->curAdapterPath);
    
    /* Discovery Type */
    pstlhBTRCore->aenDeviceDiscoveryType = enBTRCoreUnknown;

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
    if (BTRCore_LE_DeInit(pstlhBTRCore->leHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to DeInit LE Subsystem\n");
        lenBTRCoreRet = enBTRCoreFailure;
    }

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
        g_free(penExitStatusRunTask);
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
        g_free(penExitStatusOutTask);
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
            g_free(pstlhBTRCore->adapterPath[i]);
            pstlhBTRCore->adapterPath[i] = NULL;
        }

        if (pstlhBTRCore->adapterAddr[i]) {
            g_free(pstlhBTRCore->adapterAddr[i]);
            pstlhBTRCore->adapterAddr[i] = NULL;
        }
    }

    if (pstlhBTRCore->curAdapterAddr) {
        g_free(pstlhBTRCore->curAdapterAddr);
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
        g_free(hBTRCore);
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
        g_free(apstBTRCoreAdapter->pcAdapterDevName);
        apstBTRCoreAdapter->pcAdapterDevName = NULL;
    }

    apstBTRCoreAdapter->pcAdapterDevName = g_strndup(apcAdapterDeviceName, BTRCORE_MAX_STR_LEN - 1); //TODO: Free this memory
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
        BTRCORELOG_INFO ("Ifce: %s Version: %s", lBtIfceName, lBtVersion);
        BTRCORELOG_INFO ("Out:  %s\n", apcBtVersion);
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

    /* Discovery Type */
    pstlhBTRCore->aenDeviceDiscoveryType = aenBTRCoreDevType;

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
    else if ((aenBTRCoreDevType == enBTRCoreSpeakers) || (aenBTRCoreDevType == enBTRCoreHeadSet) ||
             (aenBTRCoreDevType == enBTRCoreMobileAudioIn) || (aenBTRCoreDevType == enBTRCorePCAudioIn)) {
        if (BtrCore_BTStartClassicDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
            return enBTRCoreDiscoveryFailure;
        }

        if (aui32DiscDuration) {
            sleep(aui32DiscDuration); //TODO: Better to setup a timer which calls BTStopDiscovery
            if (BtrCore_BTStopClassicDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
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
    else if ((aenBTRCoreDevType == enBTRCoreSpeakers) || (aenBTRCoreDevType == enBTRCoreHeadSet) ||
             (aenBTRCoreDevType == enBTRCoreMobileAudioIn) || (aenBTRCoreDevType == enBTRCorePCAudioIn)) {
        if (BtrCore_BTStopClassicDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
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

    BTRCORELOG_TRACE ("adapter path is %s\n", pstlhBTRCore->curAdapterPath);
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (pstlhBTRCore->stScannedDevicesArr[i].bFound) {
            BTRCORELOG_TRACE ("Device : %d\n", i);
            BTRCORELOG_TRACE ("Name   : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceName);
            BTRCORELOG_TRACE ("Mac Ad : %s\n", pstlhBTRCore->stScannedDevicesArr[i].pcDeviceAddress);
            BTRCORELOG_TRACE ("Rssi   : %d dbmV\n", pstlhBTRCore->stScannedDevicesArr[i].i32RSSI);
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
    enBTAdapterOp           pairingOp           = enBTAdpOpCreatePairedDev;

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
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stScannedDevicesArr[i32LoopIdx].tDeviceId) {
                pstScannedDev   = &pstlhBTRCore->stScannedDevicesArr[i32LoopIdx];
                break;
            }
        }
    }


    if (pstScannedDev)
        pDeviceAddress  = pstScannedDev->pcDeviceAddress;


    if (!pstScannedDev || !pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    BTRCORELOG_DEBUG ("We will pair     %s\n", pstScannedDev->pcDeviceName);
    BTRCORELOG_DEBUG ("We will address  %s\n", pDeviceAddress);

    if ((pstScannedDev->enDeviceType == enBTRCore_DC_HID_Keyboard)      ||
        (pstScannedDev->enDeviceType == enBTRCore_DC_HID_Mouse)         ||
        (pstScannedDev->enDeviceType == enBTRCore_DC_HID_MouseKeyBoard) ||
        (pstScannedDev->enDeviceType == enBTRCore_DC_HID_Joystick))     {

        BTRCORELOG_DEBUG ("We will do a Async Pairing for the HID Devices\n");
        pairingOp = enBTAdpOpCreatePairedDevASync;
    }

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    pairingOp) < 0) {
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
    stBTRCoreHdl*           pstlhBTRCore    = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    enBTRCoreDeviceType     aenBTRCoreDevType = enBTRCoreUnknown;
    stBTRCoreBTDevice       pstScannedDevice;

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

    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
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

    memset (&pstScannedDevice, 0 ,sizeof(stBTRCoreBTDevice));
    //Clear corresponding  device entry from Scanned List if any
    if (btrCore_GetScannedDeviceAddress(pstlhBTRCore, aBTRCoreDevId)) {
        lenBTRCoreRet = btrCore_RemoveDeviceFromScannedDevicesArr (pstlhBTRCore, aBTRCoreDevId, &pstScannedDevice);

        if (!(enBTRCoreSuccess == lenBTRCoreRet && pstScannedDevice.tDeviceId)) {
            BTRCORELOG_ERROR ("Remove device %lld from Scanned List Failed!\n", aBTRCoreDevId);
        }
    }

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
    stBTRCoreHdl*           pstlhBTRCore    = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    enBTRCoreDeviceType     aenBTRCoreDevType = enBTRCoreUnknown;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
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
    stBTRCoreHdl*           pstlhBTRCore            = NULL;

    const char*             lpcBTRCoreBTDevicePath  = NULL;
    stBTRCoreBTDevice*      lpstBTRCoreBTDevice     = NULL;
    stBTRCoreDevStateInfo*  lpstBTRCoreDevStateInfo = NULL;
    enBTDeviceType          lenBTDeviceType         = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet           = enBTRCoreFailure;

    enBTRCoreDeviceType     aenBTRCoreDevType   = enBTRCoreUnknown;
    unsigned int            ui32LoopIdx         = 0;

    stBTDeviceSupportedServiceList profileList;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pProfileList) || (!aBTRCoreDevId)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &lpstBTRCoreBTDevice, &lpstBTRCoreDevStateInfo, &lpcBTRCoreBTDevicePath)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }


    /* Initialize the array */
    memset (pProfileList, 0 , sizeof(stBTRCoreSupportedServiceList));
    memset (&profileList, 0 , sizeof(stBTDeviceSupportedServiceList));


    if (BtrCore_BTDiscoverDeviceServices(pstlhBTRCore->connHdl, lpcBTRCoreBTDevicePath, &profileList) != 0) {
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Successfully received the supported services... \n");

    pProfileList->numberOfService = profileList.numberOfService;
    for (ui32LoopIdx = 0; ui32LoopIdx < profileList.numberOfService; ui32LoopIdx++) {
        pProfileList->profile[ui32LoopIdx].uuid_value = profileList.profile[ui32LoopIdx].uuid_value;
        strncpy (pProfileList->profile[ui32LoopIdx].profile_name,  profileList.profile[ui32LoopIdx].profile_name, 30);
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_IsDeviceConnectable (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId
) {
    stBTRCoreHdl*       pstlhBTRCore = NULL;
    const char*         pDeviceMac = NULL;
    stBTRCoreBTDevice*  pstKnownDevice = NULL;


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
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceMac      = pstKnownDevice->pcDeviceAddress;
    }
    else {
        pDeviceMac      = btrCore_GetKnownDeviceMac(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceMac || !strlen(pDeviceMac)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
    }


    if (BtrCore_BTIsDeviceConnectable(pstlhBTRCore->connHdl, pDeviceMac) != 0) {
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
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if ((lenBTRCoreRet = btrCore_GetDeviceInfo(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                               &lenBTDeviceType, &lpstBTRCoreBTDevice, &lpstBTRCoreDevStateInfo,
                                               &lpcBTRCoreBTDevicePath, &lpcBTRCoreBTDeviceName)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information - %llu\n", aBTRCoreDevId);
        return lenBTRCoreRet;
    }


    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHdl, lpcBTRCoreBTDevicePath, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("Connect to device failed - %llu\n", aBTRCoreDevId);
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Connected to device %s Successfully. = %llu\n", lpcBTRCoreBTDeviceName, aBTRCoreDevId);
    /* Should think on moving a connected LE device from scanned list to paired list */


    lpstBTRCoreBTDevice->bDeviceConnected      = TRUE;
    lpstBTRCoreDevStateInfo->eDevicePrevState  = lpstBTRCoreDevStateInfo->eDeviceCurrState;

     if (lpstBTRCoreDevStateInfo->eDeviceCurrState  != enBTRCoreDevStConnected) {
         lpstBTRCoreDevStateInfo->eDeviceCurrState   = enBTRCoreDevStConnecting;

        lenBTRCoreRet = enBTRCoreSuccess;
     }


    BTRCORELOG_DEBUG ("Ret - %d - %llu\n", lenBTRCoreRet, aBTRCoreDevId);
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
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if ((lenBTRCoreRet = btrCore_GetDeviceInfo(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                               &lenBTDeviceType, &lpstBTRCoreBTDevice, &lpstBTRCoreDevStateInfo,
                                               &lpcBTRCoreBTDevicePath, &lpcBTRCoreBTDeviceName)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information - %llu\n", aBTRCoreDevId);
        return lenBTRCoreRet;
    }


    // TODO: Implement a Device State Machine and Check whether the device is in a Disconnectable State
    // before making the connect call
    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHdl, lpcBTRCoreBTDevicePath, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("DisConnect from device failed - %llu\n", aBTRCoreDevId);
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

    BTRCORELOG_DEBUG ("Ret - %d - %llu\n", lenBTRCoreRet, aBTRCoreDevId);
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
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if ((lenBTRCoreRet = btrCore_GetDeviceInfo(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                               &lenBTDeviceType, &lpstBTRCoreBTDevice, &lpstBTRCoreDevStateInfo,
                                               &lpcBTRCoreBTDevicePath, &lpcBTRCoreBTDeviceName)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }


    (void)lenBTDeviceType;

    if (lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStConnected) {
        BTRCORELOG_DEBUG ("enBTRCoreDevStConnected = %s\n", lpcBTRCoreBTDeviceName);
        lenBTRCoreRet = enBTRCoreSuccess;
    }
    else {
        lenBTRCoreRet = enBTRCoreFailure;
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
    const char*             lpcBTRCoreBTDevicePath  = NULL;
    const char*             lpcBTRCoreBTDeviceName  = NULL;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if ((lenBTRCoreRet = btrCore_GetDeviceInfo(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                               &lenBTDeviceType, &lpstBTRCoreBTDevice, &lpstBTRCoreDevStateInfo,
                                               &lpcBTRCoreBTDevicePath, &lpcBTRCoreBTDeviceName)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }


    (void)lenBTDeviceType;

    if ((lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStDisconnected) ||
        (lpstBTRCoreDevStateInfo->eDeviceCurrState == enBTRCoreDevStLost)) {
        BTRCORELOG_DEBUG ("enBTRCoreDevStDisconnected = %s\n", lpcBTRCoreBTDeviceName);
        lenBTRCoreRet = enBTRCoreSuccess;
    }
    else {
        lenBTRCoreRet = enBTRCoreFailure;
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
    *apenBTRCoreDevTy = btrCore_MapDevClassToDevType(pstBTDevice->enDeviceType);

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetDeviceMediaInfo (
    tBTRCoreHandle          hBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType,
    stBTRCoreDevMediaInfo*  apstBTRCoreDevMediaInfo
) {
    stBTRCoreHdl*           pstlhBTRCore    = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    stBTRCoreAVMediaInfo    lstBtrCoreMediaInfo;
    unsigned int            ui32AVMCodecInfoSize = 0;

    
    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((aBTRCoreDevId < 0) || !apstBTRCoreDevMediaInfo || !apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    memset(&lstBtrCoreMediaInfo, 0, sizeof(stBTRCoreAVMediaInfo));
    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }


    BTRCORELOG_INFO (" We will get Media Info for %s - DevTy %d\n", pDeviceAddress, lenBTDeviceType);

    switch (lenBTDeviceType) {
    case enBTDevAudioSink:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowOut;
        break;
    case enBTDevAudioSource:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowIn;
        break;
    case enBTDevHFPHeadset:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowInOut;
        break;
    case enBTDevHFPAudioGateway:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowInOut;
        break;
    case enBTDevLE:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowUnknown;
        break;
    case enBTDevUnknown:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowUnknown;
        break;
    default:
        lstBtrCoreMediaInfo.eBtrCoreAVMFlow = eBTRCoreAVMediaFlowUnknown;
        break;
    }


    ui32AVMCodecInfoSize = sizeof(stBTRCoreAVMediaMpegInfo) > sizeof(stBTRCoreAVMediaSbcInfo) ? sizeof(stBTRCoreAVMediaMpegInfo) : sizeof(stBTRCoreAVMediaSbcInfo);
    ui32AVMCodecInfoSize = ui32AVMCodecInfoSize > sizeof(stBTRMgrAVMediaPcmInfo) ? ui32AVMCodecInfoSize : sizeof(stBTRMgrAVMediaPcmInfo);

    lstBtrCoreMediaInfo.eBtrCoreAVMType = eBTRCoreAVMTypeUnknown;
    if (!(lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo = g_malloc0(ui32AVMCodecInfoSize))) {
        BTRCORELOG_ERROR ("AVMedia_GetCurMediaInfo - Unable to alloc Memory\n");
        return lenBTRCoreRet;
    }


    // TODO: Implement a Device State Machine and Check whether the device is Connected before making the call
    if ((lenBTRCoreRet = BTRCore_AVMedia_GetCurMediaInfo (pstlhBTRCore->avMediaHdl, pDeviceAddress, &lstBtrCoreMediaInfo)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia_GetCurMediaInfo ERROR occurred\n");
        g_free(lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo);
        return lenBTRCoreRet;
    }

    switch (lstBtrCoreMediaInfo.eBtrCoreAVMType) {
    case eBTRCoreAVMTypePCM:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypePCM;
        break;
    case eBTRCoreAVMTypeSBC: {
        stBTRCoreDevMediaSbcInfo*   lapstBtrCoreDevMCodecInfo = (stBTRCoreDevMediaSbcInfo*)(apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo);
        stBTRCoreAVMediaSbcInfo*    lpstBtrCoreAVMSbcInfo = (stBTRCoreAVMediaSbcInfo*)(lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo);

        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeSBC;

        switch (lpstBtrCoreAVMSbcInfo->eAVMAChan) {
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

        lapstBtrCoreDevMCodecInfo->ui32DevMSFreq         = lpstBtrCoreAVMSbcInfo->ui32AVMSFreq;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcAllocMethod = lpstBtrCoreAVMSbcInfo->ui8AVMSbcAllocMethod;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcSubbands    = lpstBtrCoreAVMSbcInfo->ui8AVMSbcSubbands;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcBlockLength = lpstBtrCoreAVMSbcInfo->ui8AVMSbcBlockLength;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcMinBitpool  = lpstBtrCoreAVMSbcInfo->ui8AVMSbcMinBitpool;
        lapstBtrCoreDevMCodecInfo->ui8DevMSbcMaxBitpool  = lpstBtrCoreAVMSbcInfo->ui8AVMSbcMaxBitpool;
        lapstBtrCoreDevMCodecInfo->ui16DevMSbcFrameLen   = lpstBtrCoreAVMSbcInfo->ui16AVMSbcFrameLen;
        lapstBtrCoreDevMCodecInfo->ui16DevMSbcBitrate    = lpstBtrCoreAVMSbcInfo->ui16AVMSbcBitrate;

        break;
    }
    case eBTRCoreAVMTypeMPEG:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeMPEG;
        break;
    case eBTRCoreAVMTypeAAC: {
        stBTRCoreDevMediaMpegInfo* lapstBtrCoreDevMCodecInfo = (stBTRCoreDevMediaMpegInfo*)(apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo);
        stBTRCoreAVMediaMpegInfo*  lpstBtrCoreAVMAacInfo = (stBTRCoreAVMediaMpegInfo*)(lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo);

        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeAAC;

        switch (lpstBtrCoreAVMAacInfo->eAVMAChan) {
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

        lapstBtrCoreDevMCodecInfo->ui32DevMSFreq        = lpstBtrCoreAVMAacInfo->ui32AVMSFreq;
        lapstBtrCoreDevMCodecInfo->ui8DevMMpegCrc       = lpstBtrCoreAVMAacInfo->ui8AVMMpegCrc;
        lapstBtrCoreDevMCodecInfo->ui8DevMMpegLayer     = lpstBtrCoreAVMAacInfo->ui8AVMMpegLayer;
        lapstBtrCoreDevMCodecInfo->ui8DevMMpegMpf       = lpstBtrCoreAVMAacInfo->ui8AVMMpegMpf;
        lapstBtrCoreDevMCodecInfo->ui8DevMMpegRfa       = lpstBtrCoreAVMAacInfo->ui8AVMMpegRfa;
        lapstBtrCoreDevMCodecInfo->ui16DevMMpegFrameLen = lpstBtrCoreAVMAacInfo->ui16AVMMpegFrameLen;
        lapstBtrCoreDevMCodecInfo->ui16DevMMpegBitrate  = lpstBtrCoreAVMAacInfo->ui16AVMMpegBitrate;

        break;
    }
    case eBTRCoreAVMTypeUnknown:
    default:
        apstBTRCoreDevMediaInfo->eBtrCoreDevMType = eBTRCoreDevMediaTypeUnknown;
        break;
    }

    g_free(lstBtrCoreMediaInfo.pstBtrCoreAVMCodecInfo);
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
    stBTRCoreHdl*           pstlhBTRCore        = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    int                     liDataPath      = 0;
    int                     lidataReadMTU   = 0;
    int                     lidataWriteMTU  = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!aiDataPath || !aidataReadMTU || !aidataWriteMTU || (aBTRCoreDevId < 0)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }


    BTRCORELOG_INFO (" We will Acquire Data Path for %s\n", pDeviceAddress);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call
    if (BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->avMediaHdl, pDeviceAddress, &liDataPath, &lidataReadMTU, &lidataWriteMTU) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    *aiDataPath     = liDataPath;
    *aidataReadMTU  = lidataReadMTU;
    *aidataWriteMTU = lidataWriteMTU;


    lpstKnownDevStInfo->eDevicePrevState = lpstKnownDevStInfo->eDeviceCurrState;
    if (lpstKnownDevStInfo->eDeviceCurrState != enBTRCoreDevStPlaying) {
        lpstKnownDevStInfo->eDeviceCurrState = enBTRCoreDevStPlaying; 
    }


    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ReleaseDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*           pstlhBTRCore        = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    BTRCORELOG_INFO (" We will Release Data Path for %s\n", pDeviceAddress);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call
    if(BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->avMediaHdl, pDeviceAddress) != enBTRCoreSuccess) {
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
    stBTRCoreHdl*           pstlhBTRCore        = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    BOOLEAN                 lbBTDeviceConnected = FALSE; 
    enBTRCoreAVMediaCtrl    aenBTRCoreAVMediaCtrl = 0;


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }

    if ((lbBTDeviceConnected = pstKnownDevice->bDeviceConnected) == FALSE) {
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
    stBTRCoreHdl*           pstlhBTRCore    = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    BOOLEAN                 lbBTDeviceConnected = FALSE; 


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }

    if ((lbBTDeviceConnected = pstKnownDevice->bDeviceConnected) == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }


    if (BTRCore_AVMedia_GetTrackInfo(pstlhBTRCore->avMediaHdl,
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
    stBTRCoreHdl*           pstlhBTRCore        = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    BOOLEAN                 lbBTDeviceConnected = FALSE; 


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }

    if ((lbBTDeviceConnected = pstKnownDevice->bDeviceConnected) == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }


   if (BTRCore_AVMedia_GetPositionInfo(pstlhBTRCore->avMediaHdl,
                                       pDeviceAddress,
                                       (stBTRCoreAVMediaPositionInfo*)apstBTMediaPositionInfo) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("AVMedia get Media Position Info Failed!!!\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}



enBTRCoreRet
BTRCore_GetMediaProperty (
    tBTRCoreHandle          hBTRCore,
    tBTRCoreDevId           aBTRCoreDevId,
    enBTRCoreDeviceType     aenBTRCoreDevType,
    const char*             mediaPropertyKey,
    void*                   mediaPropertyValue
) {
    stBTRCoreHdl*           pstlhBTRCore        = NULL;

    const char*             pDeviceAddress      = NULL;
    stBTRCoreBTDevice*      pstKnownDevice      = NULL;
    stBTRCoreDevStateInfo*  lpstKnownDevStInfo  = NULL;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;

    BOOLEAN                 lbBTDeviceConnected = FALSE; 


    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ((lenBTRCoreRet = btrCore_GetDeviceInfoKnown(pstlhBTRCore, aBTRCoreDevId, aenBTRCoreDevType,
                                                    &lenBTDeviceType, &pstKnownDevice, &lpstKnownDevStInfo, &pDeviceAddress)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to Get Device Information\n");
        return lenBTRCoreRet;
    }

    if ((lbBTDeviceConnected = pstKnownDevice->bDeviceConnected) == FALSE) {
       BTRCORELOG_ERROR ("Device is not Connected!!!\n");
       return enBTRCoreFailure;
    }


    if (BTRCore_AVMedia_GetMediaProperty(pstlhBTRCore->avMediaHdl,
                                         pDeviceAddress,
                                         mediaPropertyKey,
                                         mediaPropertyValue) != enBTRCoreSuccess)  {
        BTRCORELOG_ERROR ("AVMedia get property Failed!!!\n");
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

    if (lenBTRCoreLEGattProp == enBTRCoreLEGPropUnknown || BTRCore_LE_GetGattProperty(pstlhBTRCore->leHdl,
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
    char*             apLeOpArg,
    char*             rpLeOpRes
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

    if (lenBTRCoreLEGattOp == enBTRCoreLEGOpUnknown || BtrCore_LE_PerformGattOp(pstlhBTRCore->leHdl,
                                                                                ltBTRCoreDevId,
                                                                                apBtUuid,
                                                                                lenBTRCoreLEGattOp,
                                                                                apLeOpArg,
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
btrCore_BTAdapterStatusUpdateCb (
    enBTAdapterProp  aeBtAdapterProp,
    stBTAdapterInfo* apstBTAdapterInfo,
    void*            apUserData
) {
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;
    enBTRCoreRet    lenBTRCoreRet  = enBTRCoreSuccess;
    stBTRCoreAdapter lstAdapterInfo;
    int pathlen;

    if (!apstBTAdapterInfo || !apUserData) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!");
       return -1;
    }

    if (!apstBTAdapterInfo->pcPath || !(pathlen = strlen (apstBTAdapterInfo->pcPath)) ||
            strcmp(apstBTAdapterInfo->pcPath, lpstlhBTRCore->curAdapterPath))
    {
        BTRCORELOG_INFO ("Dropping event for non-current adapter path %s", apstBTAdapterInfo->pcPath ? apstBTAdapterInfo->pcPath : "<null>");
        return -1;
    }

    lstAdapterInfo.adapter_number = atoi(apstBTAdapterInfo->pcPath+pathlen-1);

    BTRCORELOG_DEBUG ("adapter number = %d, path = %s, discovering = %d\n",
            lstAdapterInfo.adapter_number, apstBTAdapterInfo->pcPath, apstBTAdapterInfo->bDiscovering);

    switch (aeBtAdapterProp) {
    case enBTAdPropDiscoveryStatus: {
        lstAdapterInfo.discoverable = apstBTAdapterInfo->bDiscoverable;
        lstAdapterInfo.bDiscovering = apstBTAdapterInfo->bDiscovering;

        if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask,
                enBTRCoreTaskOpProcess,
                enBTRCoreTaskPTcBAdapterStatus,
                &lstAdapterInfo)) != enBTRCoreSuccess) {
            BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBAdapterStatus %d\n", lenBTRCoreRet);
        }
        break;
    }
    case enBTAdPropUnknown: {
        break;
    }
    default: {
        break;
    }
    }
    return 0;
}


static int
btrCore_BTDeviceStatusUpdateCb (
    enBTDeviceType  aeBtDeviceType,
    enBTDeviceState aeBtDeviceState,
    stBTDeviceInfo* apstBTDeviceInfo,
    void*           apUserData
) {
    enBTRCoreRet         lenBTRCoreRet      = enBTRCoreFailure;
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;


    BTRCORELOG_INFO ("enBTDeviceType = %d enBTDeviceState = %d apstBTDeviceInfo = %p\n", aeBtDeviceType, aeBtDeviceState, apstBTDeviceInfo);

    lenBTRCoreDevType = btrCore_MapClassIDToDevType(apstBTDeviceInfo->ui32Class, aeBtDeviceType);

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
                stBTRCoreOTskInData lstOTskInData; 
                
                lstOTskInData.bTRCoreDevId      = lBTRCoreDevId;
                lstOTskInData.enBTRCoreDevType  = lenBTRCoreDevType;
                lstOTskInData.pstBTDevInfo      = apstBTDeviceInfo;

                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceDisc,  &lstOTskInData)) != enBTRCoreSuccess) {
                    BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceDisc %d\n", lenBTRCoreRet);
                }
            }
        }

        break;
    }
    case enBTDevStLost: {
        stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

        if (lpstlhBTRCore && apstBTDeviceInfo) {
            tBTRCoreDevId   lBTRCoreDevId     = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

            if (btrCore_GetKnownDeviceMac(lpstlhBTRCore, lBTRCoreDevId)) { 
                stBTRCoreOTskInData lstOTskInData; 
                
                lstOTskInData.bTRCoreDevId      = lBTRCoreDevId;
                lstOTskInData.enBTRCoreDevType  = lenBTRCoreDevType;
                lstOTskInData.pstBTDevInfo      = apstBTDeviceInfo;

                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceLost, &lstOTskInData)) != enBTRCoreSuccess) {
                    BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceLost%d\n", lenBTRCoreRet);
                }
            }
            else if (btrCore_GetScannedDeviceAddress(lpstlhBTRCore, lBTRCoreDevId)) {
                stBTRCoreOTskInData lstOTskInData; 
                
                lstOTskInData.bTRCoreDevId      = lBTRCoreDevId;
                lstOTskInData.enBTRCoreDevType  = lenBTRCoreDevType;
                lstOTskInData.pstBTDevInfo      = apstBTDeviceInfo;

                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceRemoved, &lstOTskInData)) != enBTRCoreSuccess) {
                    BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceRemoved %d\n", lenBTRCoreRet);
                }
            }
            else {
                BTRCORELOG_INFO ("We dont have a entry in the list; Skip Parsing now \n");
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
            tBTRCoreDevId   lBTRCoreDevId     = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

            if (btrCore_GetKnownDeviceMac(lpstlhBTRCore, lBTRCoreDevId)) {
                stBTRCoreOTskInData lstOTskInData; 
                
                lstOTskInData.bTRCoreDevId      = lBTRCoreDevId;
                lstOTskInData.enBTRCoreDevType  = lenBTRCoreDevType;
                lstOTskInData.pstBTDevInfo      = apstBTDeviceInfo;

                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceStatus, &lstOTskInData)) != enBTRCoreSuccess) {
                    BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceStatus %d\n", lenBTRCoreRet);
                }
            }
            else if (btrCore_GetScannedDeviceAddress(lpstlhBTRCore, lBTRCoreDevId)) {
                stBTRCoreOTskInData lstOTskInData; 
                
                lstOTskInData.bTRCoreDevId      = lBTRCoreDevId;
                lstOTskInData.enBTRCoreDevType  = lenBTRCoreDevType;
                lstOTskInData.pstBTDevInfo      = apstBTDeviceInfo;

                if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBDeviceStatus, &lstOTskInData)) != enBTRCoreSuccess) {
                    BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBDeviceStatus %d\n", lenBTRCoreRet);
                }
            }
        }

        break;
    }
    case enBTDevStRSSIUpdate: {
        BTRCORELOG_INFO ("Received RSSI Update...\n");
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
    unsigned char   ucIsReqConfirmation,
    void*           apUserData
) {
    int                  i32DevConnIntimRet = 0;
    stBTRCoreHdl*        lpstlhBTRCore      = (stBTRCoreHdl*)apUserData;
    enBTRCoreDeviceType  lenBTRCoreDevType  = enBTRCoreUnknown;

    lenBTRCoreDevType = btrCore_MapClassIDToDevType(apstBTDeviceInfo->ui32Class, aeBtDeviceType);


    if (lpstlhBTRCore) {
        stBTRCoreBTDevice   lstFoundDevice;
        int                 i32ScannedDevIdx = -1;

        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(lpstlhBTRCore, apstBTDeviceInfo, &lstFoundDevice)) != -1) {
           BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d\n", i32ScannedDevIdx);
        }

        BTRCORELOG_DEBUG("btrCore_BTDeviceConnectionIntimationCb\n");
        lpstlhBTRCore->stConnCbInfo.ui32devPassKey = aui32devPassKey;
        lpstlhBTRCore->stConnCbInfo.ucIsReqConfirmation = ucIsReqConfirmation;

        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        memcpy (&lpstlhBTRCore->stConnCbInfo.stFoundDevice, &lstFoundDevice, sizeof(stBTRCoreBTDevice));
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.bFound = TRUE;

        if ((lenBTRCoreDevType == enBTRCoreMobileAudioIn) || (lenBTRCoreDevType == enBTRCorePCAudioIn) || (lenBTRCoreDevType == enBTRCoreHID)) {
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

    lenBTRCoreDevType = btrCore_MapClassIDToDevType(apstBTDeviceInfo->ui32Class, aeBtDeviceType);


    if (lpstlhBTRCore) {
        stBTRCoreBTDevice   lstFoundDevice;
        int                 i32ScannedDevIdx = -1;
        int                 i32KnownDevIdx   = -1;

        if ((i32ScannedDevIdx = btrCore_AddDeviceToScannedDevicesArr(lpstlhBTRCore, apstBTDeviceInfo, &lstFoundDevice)) != -1) {
           BTRCORELOG_DEBUG ("btrCore_AddDeviceToScannedDevicesArr - Success Index = %d\n", i32ScannedDevIdx);
        }

        BTRCORELOG_DEBUG("btrCore_BTDeviceAuthenticationCb\n");
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        if ((i32KnownDevIdx = btrCore_AddDeviceToKnownDevicesArr(lpstlhBTRCore, apstBTDeviceInfo)) != -1) {
            memcpy (&lpstlhBTRCore->stConnCbInfo.stKnownDevice, &lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx], sizeof(stBTRCoreBTDevice));
            BTRCORELOG_DEBUG ("btrCore_AddDeviceToKnownDevicesArr - Success Index = %d Unique DevID = %lld\n", i32KnownDevIdx, lpstlhBTRCore->stConnCbInfo.stKnownDevice.tDeviceId);
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

                        BTRCORELOG_DEBUG("stKnownDevice.device_connected set : %d\n", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].bDeviceConnected);
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
                        BTRCORELOG_DEBUG("ACCEPTED INCOMING CONNECT stKnownDevice : %s\n", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].pcDeviceName);
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
    stBTRCoreAVMediaStatusUpdate*   apMediaStreamStatus,
    const char*                     apBtdevAddr,
    void*                           apUserData
) {
    stBTRCoreHdl*                   lpstlhBTRCore  = (stBTRCoreHdl*)apUserData;
    enBTRCoreRet                    lenBTRCoreRet  = enBTRCoreSuccess;
    tBTRCoreDevId                   lBTRCoreDevId  = 0;
    stBTRCoreMediaStatusCBInfo      lstMediaStatusUpdateCbInfo;


    if (!apMediaStreamStatus || !apBtdevAddr || !apUserData) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!");
       return enBTRCoreInvalidArg;
    }


    lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apBtdevAddr);
    if (!btrCore_GetKnownDeviceMac(lpstlhBTRCore, lBTRCoreDevId)) {
        BTRCORELOG_INFO ("We dont have a entry in the list; Skip Parsing now \n");
        return enBTRCoreDeviceNotFound;
    }


    switch (apMediaStreamStatus->eAVMediaState) {

    case eBTRCoreAVMediaTrkStStarted:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaTrkStStarted;
        memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaPositionInfo, &apMediaStreamStatus->m_mediaPositionInfo, sizeof(stBTRCoreMediaPositionInfo));
        break;
    case eBTRCoreAVMediaTrkStPlaying:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaTrkStPlaying;
        memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaPositionInfo, &apMediaStreamStatus->m_mediaPositionInfo, sizeof(stBTRCoreMediaPositionInfo));
        break;
    case eBTRCoreAVMediaTrkStPaused:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaTrkStPaused;
        memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaPositionInfo, &apMediaStreamStatus->m_mediaPositionInfo, sizeof(stBTRCoreMediaPositionInfo));
        break;
    case eBTRCoreAVMediaTrkStStopped:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaTrkStStopped;
        memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaPositionInfo, &apMediaStreamStatus->m_mediaPositionInfo, sizeof(stBTRCoreMediaPositionInfo));
        break;
    case eBTRCoreAVMediaTrkStChanged:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaTrkStChanged;
        memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaTrackInfo, &apMediaStreamStatus->m_mediaTrackInfo, sizeof(stBTRCoreMediaTrackInfo));
        break;
    case eBTRCoreAVMediaPlaybackEnded:
        lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.eBTMediaStUpdate = eBTRCoreMediaPlaybackEnded;
        //memcpy (&lstMediaStatusUpdateCbInfo.m_mediaStatusUpdate.m_mediaPositionInfo, &apMediaStreamStatus->m_mediaPositionInfo, sizeof(stBTRCoreMediaPositionInfo));
        break;
    default:
        break;
    }

    lstMediaStatusUpdateCbInfo.deviceId = lBTRCoreDevId;

    if ((lenBTRCoreRet = btrCore_OutTaskAddOp(lpstlhBTRCore->pGAQueueOutTask, enBTRCoreTaskOpProcess, enBTRCoreTaskPTcBMediaStatus, &lstMediaStatusUpdateCbInfo)) != enBTRCoreSuccess) {
        BTRCORELOG_WARN("Failure btrCore_OutTaskAddOp enBTRCoreTaskOpProcess enBTRCoreTaskPTcBMediaStatus %d\n", lenBTRCoreRet);
        lenBTRCoreRet = enBTRCoreFailure;
    }


    return lenBTRCoreRet;
}


static enBTRCoreRet
btrCore_BTLeStatusUpdateCb (
    stBTRCoreLeGattInfo*    apstBtrLeInfo,
    const char*             apcBtdevAddr,
    void*                   apvUserData
) {
    stBTRCoreHdl*           lpstlhBTRCore       = (stBTRCoreHdl*)apvUserData;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;
    enBTDeviceType          lenBTDeviceType     = enBTDevUnknown;
    stBTRCoreBTDevice*      lpstScannedDevice   = NULL;
    stBTRCoreDevStateInfo*  lpstScannedDevStInfo= NULL;
    const char*             pDevicePath         = NULL;
    const char*             pDeviceName         = NULL;
    tBTRCoreDevId           lBTRCoreDevId       = 0;


    if (!apstBtrLeInfo || !apcBtdevAddr || !apstBtrLeInfo->pui8Value || !apvUserData) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!\n");
        return enBTRCoreInvalidArg;
    }


    //TODO : Look if we can get the infos from the CB itslef
    lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apcBtdevAddr);
    if ((lenBTRCoreRet = btrCore_GetDeviceInfo(lpstlhBTRCore, lBTRCoreDevId, enBTRCoreLE, &lenBTDeviceType,
                                   &lpstScannedDevice, &lpstScannedDevStInfo, &pDevicePath, &pDeviceName)) != enBTRCoreSuccess) {
        BTRCORELOG_ERROR ("Failed to find Device in ScannedList!\n");
        return enBTRCoreDeviceNotFound;
    }


    BTRCORELOG_DEBUG ("LE Dev %s Path %s\n", pDeviceName, pDevicePath);

    lpstlhBTRCore->stDevStatusCbInfo.deviceId           = lBTRCoreDevId;
    lpstlhBTRCore->stDevStatusCbInfo.eDeviceType        = enBTRCoreLE;
    lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass       = lpstScannedDevice->enDeviceType;
    strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName, pDeviceName, BD_NAME_LEN);

    switch (apstBtrLeInfo->enLeOper) {
    case enBTRCoreLEGOpReady:
        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStOpReady;
        strncpy(lpstlhBTRCore->stDevStatusCbInfo.devOpResponse, apstBtrLeInfo->pui8Value, BTRCORE_MAX_STR_LEN - 1);
        break;
    case enBTRCoreLEGOpReadValue:
        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStOpInfo;
        strncpy(lpstlhBTRCore->stDevStatusCbInfo.devOpResponse, apstBtrLeInfo->pui8Value, BTRCORE_MAX_STR_LEN - 1);
        break;
    case enBTRCoreLEGOpWriteValue:
        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStOpInfo;
        strncpy(lpstlhBTRCore->stDevStatusCbInfo.devOpResponse, apstBtrLeInfo->pui8Value, BTRCORE_MAX_STR_LEN - 1);
        break;
    case enBTRCoreLEGOpStartNotify:
        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStOpInfo;
        strncpy(lpstlhBTRCore->stDevStatusCbInfo.devOpResponse, apstBtrLeInfo->pui8Value, BTRCORE_MAX_STR_LEN - 1);
        break;
    case enBTRCoreLEGOpStopNotify:
        lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStOpInfo;
        strncpy(lpstlhBTRCore->stDevStatusCbInfo.devOpResponse, apstBtrLeInfo->pui8Value, BTRCORE_MAX_STR_LEN - 1);
        break;
    case enBTRCoreLEGOpUnknown:
    default:
        break;
    }


    if (lpstlhBTRCore->fpcBBTRCoreStatus) {
        /* Invoke the callback */
        //TODO: Triggering Outgoing callbacks from Incoming callbacks..aaaaaaaahhhh not a good idea - can move this in next commit
        if (lpstlhBTRCore->fpcBBTRCoreStatus(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvcBStatusUserData) != enBTRCoreSuccess) {
            BTRCORELOG_ERROR (" CallBack Error !!!!!!\n");
            lenBTRCoreRet = enBTRCoreFailure;
        }
    }

    return lenBTRCoreRet;
}
/* End of File */
