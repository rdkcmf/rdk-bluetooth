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
#include "btrCore_bt_ifce.h"
#include "btrCore_service.h"

#include "btrCore_priv.h"

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

//TODO: Move to a private header
typedef struct _stBTRCoreDevStateInfo {
    enBTRCoreDeviceState    eDevicePrevState;
    enBTRCoreDeviceState    eDeviceCurrState;
} stBTRCoreDevStateInfo;


typedef struct _stBTRCoreHdl {

    tBTRCoreAVMediaHdl          avMediaHdl;

    void*                       connHdl;
    char*                       agentPath;

    unsigned int                numOfAdapters;
    char*                       adapterPath[BTRCORE_MAX_NUM_BT_ADAPTERS];
    char*                       adapterAddr[BTRCORE_MAX_NUM_BT_ADAPTERS];

    char*                       curAdapterPath;
    char*                       curAdapterAddr;

    unsigned int                numOfScannedDevices;
    stBTRCoreScannedDevice      stScannedDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];
    stBTRCoreDevStateInfo       stScannedDevStInfoArr[BTRCORE_MAX_NUM_BT_DEVICES];

    unsigned int                numOfPairedDevices;
    stBTRCoreKnownDevice        stKnownDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];
    stBTRCoreDevStateInfo       stKnownDevStInfoArr[BTRCORE_MAX_NUM_BT_DEVICES];

    stBTRCoreScannedDevice      stFoundDevice;

    stBTRCoreDevStatusCBInfo    stDevStatusCbInfo;

    stBTRCoreConnCBInfo         stConnCbInfo;

    BTRCore_DeviceDiscoveryCb   fptrBTRCoreDeviceDiscoveryCB;
    BTRCore_StatusCb            fptrBTRCoreStatusCB;
    BTRCore_ConnIntimCb         fptrBTRCoreConnIntimCB; 
    BTRCore_ConnAuthCb          fptrBTRCoreConnAuthCB; 

    void*                       pvCBUserData;

    GThread*                    dispatchThread;
    GMutex                      dispatchMutex;
    BOOLEAN                     dispatchThreadQuit;
} stBTRCoreHdl;


/* Static Function Prototypes */
static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevId btrCore_GenerateUniqueDeviceID (const char* apcDeviceAddress);
static enBTRCoreDeviceClass btrCore_MapClassIDtoDeviceType(unsigned int classID);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_SetScannedDeviceInfo (stBTRCoreHdl* apsthBTRCore); 
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static void btrCore_MapKnownDeviceListFromPairedDeviceInfo (stBTRCoreKnownDevice* knownDevicesArr, stBTPairedDeviceInfo* pairedDeviceInfo);
static const char* btrCore_GetKnownDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceName (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceMac (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_ShowSignalStrength (short strength);
static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut);
static enBTRCoreDeviceState btrCore_BTParseDeviceConnectionState (const char* pcStateValue);

/* Callbacks */
static int btrCore_BTDeviceStatusUpdate_cb(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo,  void* apUserData);
static int btrCore_BTDeviceConnectionIntimation_cb(stBTDeviceInfo* apstBTDeviceInfo, unsigned int aui32devPassKey, void* apUserData);
static int btrCore_BTDeviceAuthentication_cb(stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);


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
        apsthBTRCore->stScannedDevicesArr[i].deviceId = 0;
        memset (apsthBTRCore->stScannedDevicesArr[i].device_address, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stScannedDevicesArr[i].device_name, '\0', sizeof(BD_NAME));
        apsthBTRCore->stScannedDevicesArr[i].RSSI = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].found = FALSE;

        apsthBTRCore->stScannedDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stScannedDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    apsthBTRCore->numOfScannedDevices = 0;
    apsthBTRCore->numOfPairedDevices = 0;

    /* Found Device */
    memset (apsthBTRCore->stFoundDevice.device_address, '\0', sizeof(BD_NAME));
    memset (apsthBTRCore->stFoundDevice.device_name, '\0', sizeof(BD_NAME));
    apsthBTRCore->stFoundDevice.RSSI = INT_MIN;
    apsthBTRCore->stFoundDevice.found = FALSE;

    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stKnownDevicesArr[i].deviceId = 0;
        apsthBTRCore->stKnownDevicesArr[i].device_connected = 0;
        memset (apsthBTRCore->stKnownDevicesArr[i].bd_path, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stKnownDevicesArr[i].device_name, '\0', sizeof(BD_NAME));
        apsthBTRCore->stKnownDevicesArr[i].RSSI = INT_MIN;
        apsthBTRCore->stKnownDevicesArr[i].found = FALSE;

        apsthBTRCore->stKnownDevStInfoArr[i].eDevicePrevState = enBTRCoreDevStInitialized;
        apsthBTRCore->stKnownDevStInfoArr[i].eDeviceCurrState = enBTRCoreDevStInitialized;
    }

    /* Callback Info */
    apsthBTRCore->stDevStatusCbInfo.eDevicePrevState = enBTRCoreDevStInitialized;
    apsthBTRCore->stDevStatusCbInfo.eDeviceCurrState = enBTRCoreDevStInitialized;


    memset(&apsthBTRCore->stConnCbInfo, 0, sizeof(stBTRCoreConnCBInfo));

    apsthBTRCore->fptrBTRCoreDeviceDiscoveryCB = NULL;
    apsthBTRCore->fptrBTRCoreStatusCB = NULL;
    apsthBTRCore->fptrBTRCoreConnIntimCB = NULL;
    apsthBTRCore->fptrBTRCoreConnAuthCB = NULL;

    apsthBTRCore->pvCBUserData = NULL;
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
    BTRCORELOG_DEBUG("btrCore_MapClassIDtoDeviceType - classID=%x\n", classID);

    if ((classID & 0x200) || (classID & 0x400)) {
        unsigned int ui32DevClassID = (classID & 0xFFF);
        BTRCORELOG_DEBUG("btrCore_MapClassIDtoDeviceType - ui32DevClassID=%x\n", ui32DevClassID);

        if (ui32DevClassID == enBTRCore_DC_SmartPhone) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_SmartPhone\n");
            rc = enBTRCore_DC_SmartPhone;
        }
        else if (ui32DevClassID == enBTRCore_DC_WearableHeadset) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_WearableHeadset\n");
            rc = enBTRCore_DC_WearableHeadset;
        }
        else if (ui32DevClassID == enBTRCore_DC_Handsfree) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Handsfree\n");
            rc = enBTRCore_DC_Handsfree;
        }
        else if (ui32DevClassID == enBTRCore_DC_Reserved) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Reserved\n");
            rc = enBTRCore_DC_Reserved;
        }
        else if (ui32DevClassID == enBTRCore_DC_Microphone) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Microphone\n");
            rc = enBTRCore_DC_Microphone;
        }
        else if (ui32DevClassID == enBTRCore_DC_Loudspeaker) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Loudspeaker\n");
            rc = enBTRCore_DC_Loudspeaker;
        }
        else if (ui32DevClassID == enBTRCore_DC_Headphones) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Headphones\n");
            rc = enBTRCore_DC_Headphones;
        }
        else if (ui32DevClassID == enBTRCore_DC_PortableAudio) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_PortableAudio\n");
            rc = enBTRCore_DC_PortableAudio;
        }
        else if (ui32DevClassID == enBTRCore_DC_CarAudio) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_CarAudio\n");
            rc = enBTRCore_DC_CarAudio;
        }
        else if (ui32DevClassID == enBTRCore_DC_STB) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_STB\n");
            rc = enBTRCore_DC_STB;
        }
        else if (ui32DevClassID == enBTRCore_DC_HIFIAudioDevice) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_HIFIAudioDevice\n");
            rc = enBTRCore_DC_HIFIAudioDevice;
        }
        else if (ui32DevClassID == enBTRCore_DC_VCR) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_VCR\n");
            rc = enBTRCore_DC_VCR;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoCamera) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_VideoCamera\n");
            rc = enBTRCore_DC_VideoCamera;
        }
        else if (ui32DevClassID == enBTRCore_DC_Camcoder) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_Camcoder\n");
            rc = enBTRCore_DC_Camcoder;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoMonitor) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_VideoMonitor\n");
            rc = enBTRCore_DC_VideoMonitor;
        }
        else if (ui32DevClassID == enBTRCore_DC_TV) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_TV\n");
            rc = enBTRCore_DC_TV;
        }
        else if (ui32DevClassID == enBTRCore_DC_VideoConference) {
            BTRCORELOG_INFO ("Its a enBTRCore_DC_VideoConference\n");
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
        apsthBTRCore->stScannedDevicesArr[i].deviceId = 0;
        memset (apsthBTRCore->stScannedDevicesArr[i].device_name, '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].device_name));
        memset (apsthBTRCore->stScannedDevicesArr[i].device_address,  '\0', sizeof(apsthBTRCore->stScannedDevicesArr[i].device_address));
        apsthBTRCore->stScannedDevicesArr[i].RSSI = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].found = FALSE;

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
        if (!apsthBTRCore->stScannedDevicesArr[i].found) {
            //BTRCORELOG_ERROR ("adding %s at location %d\n",apsthBTRCore->stFoundDevice.device_address,i);
            apsthBTRCore->stScannedDevicesArr[i].found = TRUE; //mark the record as found
            strcpy(apsthBTRCore->stScannedDevicesArr[i].device_address, apsthBTRCore->stFoundDevice.device_address);
            strcpy(apsthBTRCore->stScannedDevicesArr[i].device_name, apsthBTRCore->stFoundDevice.device_name);
            apsthBTRCore->stScannedDevicesArr[i].RSSI = apsthBTRCore->stFoundDevice.RSSI;
            apsthBTRCore->stScannedDevicesArr[i].vendor_id = apsthBTRCore->stFoundDevice.vendor_id;
            apsthBTRCore->stScannedDevicesArr[i].device_type = apsthBTRCore->stFoundDevice.device_type;
            apsthBTRCore->stScannedDevicesArr[i].deviceId = apsthBTRCore->stFoundDevice.deviceId;

            /* Copy the profile supports */
            memcpy (&apsthBTRCore->stScannedDevicesArr[i].device_profile, &apsthBTRCore->stFoundDevice.device_profile, sizeof(stBTRCoreSupportedServiceList));

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

    if ((0 == aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfScannedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfScannedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stScannedDevicesArr[loop].deviceId)
                return apsthBTRCore->stScannedDevicesArr[loop].device_address;
        }
    }

    return NULL;
}

static void
btrCore_MapKnownDeviceListFromPairedDeviceInfo (
    stBTRCoreKnownDevice* knownDevicesArr,
    stBTPairedDeviceInfo* pairedDeviceInfo
) {
    U8 i_idx=0, j_idx=0;
  
    for ( ;i_idx < pairedDeviceInfo->numberOfDevices; i_idx++) {
       strncpy(knownDevicesArr[i_idx].bd_path,        pairedDeviceInfo->devicePath[i_idx],           BT_MAX_STR_LEN-1);
       strncpy(knownDevicesArr[i_idx].device_name,    pairedDeviceInfo->deviceInfo[i_idx].pcName,    BT_MAX_STR_LEN-1);
       strncpy(knownDevicesArr[i_idx].device_address, pairedDeviceInfo->deviceInfo[i_idx].pcAddress, BT_MAX_STR_LEN-1);
       knownDevicesArr[i_idx].vendor_id             = pairedDeviceInfo->deviceInfo[i_idx].ui16Vendor;
       knownDevicesArr[i_idx].device_type           = btrCore_MapClassIDtoDeviceType(pairedDeviceInfo->deviceInfo[i_idx].ui32Class);
       knownDevicesArr[i_idx].deviceId              = btrCore_GenerateUniqueDeviceID(pairedDeviceInfo->deviceInfo[i_idx].pcAddress);
   
       for (j_idx=0; j_idx < BT_MAX_DEVICE_PROFILE; j_idx++) {
          if (pairedDeviceInfo->deviceInfo[i_idx].aUUIDs[j_idx][0] == '\0')
             break;
         else
             knownDevicesArr[i_idx].device_profile.profile[j_idx].uuid_value = btrCore_BTParseUUIDValue (
                                                                                   pairedDeviceInfo->deviceInfo[i_idx].aUUIDs[j_idx],
                                                                  knownDevicesArr[i_idx].device_profile.profile[j_idx].profile_name);
       }
       knownDevicesArr[i_idx].device_profile.numberOfService   =  j_idx;

       if (knownDevicesArr[i_idx].device_type == enBTRCore_DC_Unknown) {
          for (j_idx = 0; j_idx < knownDevicesArr[i_idx].device_profile.numberOfService; j_idx++) {
             if (knownDevicesArr[i_idx].device_profile.profile[j_idx].uuid_value == strtol(BTR_CORE_A2SNK, NULL, 16)) {
                 knownDevicesArr[i_idx].device_type = enBTRCore_DC_Loudspeaker;
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
    U8 i_idx=0, j_idx=0;
    enBTRCoreRet           retResult = enBTRCoreSuccess;
    stBTPairedDeviceInfo   pairedDeviceInfo;
    stBTRCoreKnownDevice   knownDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];

    memset (&pairedDeviceInfo, 0,  sizeof(pairedDeviceInfo));
    memset (knownDevicesArr,   0,  sizeof(knownDevicesArr ));

    if (0 == BtrCore_BTGetPairedDeviceInfo (apsthBTRCore->connHdl, pAdapterPath, &pairedDeviceInfo)) {
       btrCore_MapKnownDeviceListFromPairedDeviceInfo (knownDevicesArr, &pairedDeviceInfo); 
       /* Initially stBTRCoreKnownDevice List is populated from pairedDeviceInfo(bluez i/f) directly *********/  
       if (0 == apsthBTRCore->numOfPairedDevices) { 
            apsthBTRCore->numOfPairedDevices = pairedDeviceInfo.numberOfDevices;
            memcpy (apsthBTRCore->stKnownDevicesArr, knownDevicesArr, (sizeof(stBTRCoreKnownDevice)*apsthBTRCore->numOfPairedDevices));

            for (i_idx = 0; i_idx < pairedDeviceInfo.numberOfDevices; i_idx++) {
              apsthBTRCore->stKnownDevStInfoArr[i_idx].eDevicePrevState = enBTRCoreDevStPaired;
              apsthBTRCore->stKnownDevStInfoArr[i_idx].eDeviceCurrState = enBTRCoreDevStPaired;
            }
       } 
       else {/**************************************************************************************************
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
         U8 knownDev_index_array[BTRCORE_MAX_NUM_BT_DEVICES], pairedDev_index_array[BTRCORE_MAX_NUM_BT_DEVICES];
         U8 count=0, k_idx=0, numOfDevices=0;
         memset (knownDev_index_array,  0, sizeof(knownDev_index_array ));
         memset (pairedDev_index_array, 0, sizeof(pairedDev_index_array));
         memcpy (knownDevicesArr, apsthBTRCore->stKnownDevicesArr,  sizeof(apsthBTRCore->stKnownDevicesArr)); 
         memset (apsthBTRCore->stKnownDevicesArr,               0,  sizeof(apsthBTRCore->stKnownDevicesArr));
       /*Loops through to mark the new added and removed device entries in the list              */  
         for (i_idx=0, j_idx=0;  i_idx < pairedDeviceInfo.numberOfDevices && j_idx < apsthBTRCore->numOfPairedDevices;  j_idx++) {
           if (btrCore_GenerateUniqueDeviceID(pairedDeviceInfo.deviceInfo[i_idx].pcAddress)   ==   knownDevicesArr[j_idx].deviceId) {
              knownDev_index_array[j_idx]=1;  pairedDev_index_array[i_idx]=1; i_idx++;
           }
           else {
              for (k_idx=i_idx+1; k_idx < pairedDeviceInfo.numberOfDevices; k_idx++) {
                 if (btrCore_GenerateUniqueDeviceID(pairedDeviceInfo.deviceInfo[k_idx].pcAddress)  == knownDevicesArr[j_idx].deviceId) {
                    knownDev_index_array[j_idx]=1; pairedDev_index_array[k_idx]=1; break;
                 }
              }
           }
         }          
         numOfDevices = apsthBTRCore->numOfPairedDevices;
       /*Loops through to check for the removal of Device entries from the list during Unpairing */ 
         for (i_idx=0; i_idx < numOfDevices; i_idx++) {
           if (knownDev_index_array[i_idx]) {
               memcpy (&apsthBTRCore->stKnownDevicesArr[i_idx - count], &knownDevicesArr[i_idx], sizeof(stBTRCoreKnownDevice));
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
         for (i_idx=0; i_idx < pairedDeviceInfo.numberOfDevices; i_idx++) {
           if (!pairedDev_index_array[i_idx]) {
             memcpy(&apsthBTRCore->stKnownDevicesArr[apsthBTRCore->numOfPairedDevices], &knownDevicesArr[i_idx], sizeof(stBTRCoreKnownDevice));
             apsthBTRCore->stKnownDevStInfoArr[apsthBTRCore->numOfPairedDevices].eDevicePrevState = enBTRCoreDevStPaired;
             apsthBTRCore->stKnownDevStInfoArr[apsthBTRCore->numOfPairedDevices].eDeviceCurrState = enBTRCoreDevStPaired;
             apsthBTRCore->numOfPairedDevices++;
           }
         }
       }         
    }
    else {
        BTRCORELOG_ERROR ("Failed to populate List Of Paired Devices\n");
        retResult  = enBTRCoreFailure;
    }
    return retResult;
}


static const char*
btrCore_GetKnownDeviceAddress (
    stBTRCoreHdl*   apsthBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    int loop = 0;

    if ((0 == aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].deviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].bd_path;
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

   if ((0 == aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].deviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].device_name;
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

    if ((0 == aBTRCoreDevId) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == apsthBTRCore->stKnownDevicesArr[loop].deviceId)
             return apsthBTRCore->stKnownDevicesArr[loop].device_address;
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
        if (strcasecmp (aUUID, BTR_CORE_SP) == 0)
            strcpy (pServiceNameOut, BTR_CORE_SP_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_HEADSET) == 0)
            strcpy (pServiceNameOut, BTR_CORE_HEADSET_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_A2SRC) == 0)
            strcpy (pServiceNameOut, BTR_CORE_A2SRC_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_A2SNK) == 0)
            strcpy (pServiceNameOut, BTR_CORE_A2SNK_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_AVRTG) == 0)
            strcpy (pServiceNameOut, BTR_CORE_AVRTG_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_AAD) == 0)
            strcpy (pServiceNameOut, BTR_CORE_AAD_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_AVRCT) == 0)
            strcpy (pServiceNameOut, BTR_CORE_AVRCT_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_AVREMOTE) == 0)
            strcpy (pServiceNameOut, BTR_CORE_AVREMOTE_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_HS_AG) == 0)
            strcpy (pServiceNameOut, BTR_CORE_HS_AG_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_HANDSFREE) == 0)
            strcpy (pServiceNameOut, BTR_CORE_HANDSFREE_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_HAG) == 0)
            strcpy (pServiceNameOut, BTR_CORE_HAG_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_HEADSET2) == 0)
            strcpy (pServiceNameOut, BTR_CORE_HEADSET2_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_GEN_AUDIO) == 0)
            strcpy (pServiceNameOut, BTR_CORE_GEN_AUDIO_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_PNP) == 0)
            strcpy (pServiceNameOut, BTR_CORE_PNP_TEXT);

        else if (strcasecmp (aUUID, BTR_CORE_GEN_ATRIB) == 0)
            strcpy (pServiceNameOut, BTR_CORE_GEN_ATRIB_TEXT);

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

        if (strcasecmp ("disconnected", pcStateValue) == 0) {
            rc = enBTRCoreDevStDisconnected;
        }
        else if (strcasecmp ("connecting", pcStateValue) == 0) {
            rc = enBTRCoreDevStConnecting;
        }
        else if (strcasecmp ("connected", pcStateValue) == 0) {
            rc = enBTRCoreDevStConnected;
        }
        else if (strcasecmp ("playing", pcStateValue) == 0) {
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

    if (pstlhBTRCore->fptrBTRCoreStatusCB != NULL) {
        pstlhBTRCore->fptrBTRCoreStatusCB(&pstlhBTRCore->stDevStatusCbInfo, NULL);
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

#ifdef RDK_LOGGER_ENABLED
    const char* pDebugConfig = NULL;
    const char* BTRCORE_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
    const char* BTRCORE_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

    /* Init the logger */
    if( access(BTRCORE_DEBUG_OVERRIDE_PATH, F_OK) != -1 ) {
        pDebugConfig = BTRCORE_DEBUG_OVERRIDE_PATH;
    }
    else {
        pDebugConfig = BTRCORE_DEBUG_ACTUAL_PATH;
    }
    if( 0==rdk_logger_init(pDebugConfig) ) {
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

    if (BtrCore_BTGetProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdapter, "Address", pstlhBTRCore->curAdapterAddr)) {
        BTRCORELOG_ERROR ("Failed to get BT Adapter Address - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    BTRCORELOG_DEBUG ("Adapter path %s - Adapter Address %s \n", pstlhBTRCore->curAdapterPath, pstlhBTRCore->curAdapterAddr);

    /* Initialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (enBTRCoreSuccess != BTRCore_AVMedia_Init(&pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath)) {
        BTRCORELOG_ERROR ("Failed to Init AV Media Subsystem - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    if(BtrCore_BTRegisterDevStatusUpdatecB(pstlhBTRCore->connHdl, &btrCore_BTDeviceStatusUpdate_cb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Device Status CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnIntimationcB(pstlhBTRCore->connHdl, &btrCore_BTDeviceConnectionIntimation_cb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Connection Intimation CB - enBTRCoreInitFailure\n");
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnAuthcB(pstlhBTRCore->connHdl, &btrCore_BTDeviceAuthentication_cb, pstlhBTRCore)) {
        BTRCORELOG_ERROR ("Failed to Register Connection Authentication CB - enBTRCoreInitFailure\n");
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

    if (enBTRCoreSuccess != BTRCore_AVMedia_DeInit(pstlhBTRCore->avMediaHdl, pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath)) {
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
            if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pstlhBTRCore->adapterPath[i], enBTAdapter, "Address", pstlhBTRCore->adapterAddr[i])) {
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
    }
    BTRCORELOG_INFO ("Now current adatper is %s\n", pstlhBTRCore->curAdapterPath);

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_EnableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    int powered;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    powered = 1;
    BTRCORELOG_ERROR ("BTRCore_EnableAdapter\n");

    apstBTRCoreAdapter->enable = TRUE;//does this even mean anything?

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
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
    int powered;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    powered = 0;
    BTRCORELOG_ERROR ("BTRCore_DisableAdapter\n");

    apstBTRCoreAdapter->enable = FALSE;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
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
    stBTRCoreHdl*   pstlhBTRCore = NULL;
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
    int discoverable;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    discoverable = apstBTRCoreAdapter->discoverable;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &discoverable)) {
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
    int isDiscoverable = (int) discoverable;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &isDiscoverable)) {
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
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    U32 timeout;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    timeout = apstBTRCoreAdapter->DiscoverableTimeout;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &timeout)) {
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
    U32 givenTimeout = (U32) timeout;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &givenTimeout)) {
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
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    int discoverable = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDiscoverable)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, "Discoverable", &discoverable)) {
        BTRCORELOG_INFO ("Get value for org.bluez.Adapter.powered = %d\n", discoverable);
        *pDiscoverable = (unsigned char) discoverable;
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet 
BTRCore_SetAdapterDeviceName (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter,
    char*                apcAdapterDeviceName
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

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

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropName, &(apstBTRCoreAdapter->pcAdapterDevName))) {
        BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropName - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapterName (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    const char*     pAdapterName
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) ||(!pAdapterName)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropName, &pAdapterName)) {
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

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, "Name", name)) {
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
    int power = powerStatus;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &power)) {
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
    int powerStatus = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterPower)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHdl, pAdapterPath, enBTAdapter, "Powered", &powerStatus)) {
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
    tBTRCoreHandle           hBTRCore,
    stBTRCoreStartDiscovery* pstStartDiscovery
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    btrCore_ClearScannedDevicesList(pstlhBTRCore);

    if (BtrCore_BTStartDiscovery(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    sleep(pstStartDiscovery->duration); //TODO: Better to setup a timer which calls BTStopDiscovery

    if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHdl, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_StartDeviceDiscovery (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath
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
    if (0 == BtrCore_BTStartDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreSuccess;
    }

    return enBTRCoreDiscoveryFailure;
}


enBTRCoreRet
BTRCore_StopDeviceDiscovery (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath
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

    if (0 ==  BtrCore_BTStopDiscovery(pstlhBTRCore->connHdl, pAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreSuccess;
    }

    return enBTRCoreDiscoveryFailure;
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
        if (pstlhBTRCore->stScannedDevicesArr[i].found) {
            BTRCORELOG_INFO ("Device %d. %s\n - %s  %d dbmV ",
                                                        i,
                                                        pstlhBTRCore->stScannedDevicesArr[i].device_name,
                                                        pstlhBTRCore->stScannedDevicesArr[i].device_address,
                                                        pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            btrCore_ShowSignalStrength(pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            BTRCORELOG_INFO ("\n\n");
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
    stBTRCoreHdl*   pstlhBTRCore    = NULL;
    const char*     pDeviceAddress  = NULL;
    int             i32LoopIdx      = 0;
    int             i32KnownDevIdx  = -1;

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
        stBTRCoreScannedDevice* pstScannedDevice = NULL;

        BTRCORELOG_DEBUG ("We will pair %s\n"
                         "address %s\n",
                         pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_name,
                         pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_address);

        pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstScannedDevice->device_address;
    }
    else {
        pDeviceAddress = btrCore_GetScannedDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in Scanned devices list\n");
        return enBTRCoreDeviceNotFound;
    }

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    enBTAdpOpCreatePairedDev) < 0) {
        BTRCORELOG_ERROR ("Failed to pair a device\n");
        return enBTRCorePairingFailed;
    }


    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    if (pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_TRACE ("Scanned Device address = %s\n", pDeviceAddress);
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            BTRCORELOG_TRACE ("Known device address = %s\n", pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_address);
            if (!strcmp(pDeviceAddress, pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_address)) {
                i32KnownDevIdx = i32LoopIdx;
                break;
            }
        }

        
        if (i32KnownDevIdx != -1) {
            pstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].device_connected    = FALSE;
            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState  = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
            pstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState  = enBTRCoreDevStPaired; 
        }
    }


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
    int             i32LoopIdx      = 0;

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

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated\n");
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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


    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected = FALSE;
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState = enBTRCoreDevStUnpaired; 
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
                pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_connected  = FALSE;
                pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStUnpaired; 
            }
        }
    }

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

    if (enBTRCoreSuccess ==  btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath)) {
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
    stBTRCoreScannedDevice* pstScannedDevice = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];

    BTRCORELOG_DEBUG (" We will try to find %s\n"
                     " address %s\n",
                     pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_name,
                     pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_address);

    if (BtrCore_BTPerformAdapterOp( pstlhBTRCore->connHdl,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pstScannedDevice->device_address,
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
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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
    else if ((!pProfileList) || (0 == aBTRCoreDevId)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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

    if (BtrCore_BTDiscoverDeviceServices (pstlhBTRCore->connHdl, pDeviceAddress, &profileList) != 0) {
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

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->device_address;
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

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->bd_path;
        pDeviceName     = pstKnownDevice->device_name;
    }
    else {
        pDeviceAddress  = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
        pDeviceName     = btrCore_GetKnownDeviceName(pstlhBTRCore, aBTRCoreDevId);
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

    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHdl, pDeviceAddress, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("Connect to device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("Connected to device %s Successfully. Lets start Play the audio\n", pDeviceName);

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected     = TRUE;
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState   = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;

        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState  != enBTRCoreDevStConnected) {
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   = enBTRCoreDevStConnecting; 
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
                pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_connected    = TRUE;
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

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        BTRCORELOG_ERROR ("Failed to find device in paired devices list\n");
        return enBTRCoreDeviceNotFound;
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
        case enBTRCoreUnknown:
        default:
            lenBTDeviceType = enBTDevUnknown;
            break;
    }

    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHdl, pDeviceAddress, lenBTDeviceType) != 0) {
        BTRCORELOG_ERROR ("DisConnect from device failed\n");
        return enBTRCoreFailure;
    }

    BTRCORELOG_INFO ("DisConnected from device Successfully.\n");
    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected     = FALSE;
        pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState   = pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState;

        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState   != enBTRCoreDevStDisconnected) {
            pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState    = enBTRCoreDevStDisconnecting; 
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
                pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_connected    = FALSE;
                pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState  = pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;

                if (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState != enBTRCoreDevStDisconnected) {
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
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->bd_path;
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
        if (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState == enBTRCoreDevStConnected) {
            BTRCORELOG_DEBUG ("enBTRCoreDevStConnected\n");
            lenBTRCoreRet = enBTRCoreSuccess;
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
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
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice  = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress  = pstKnownDevice->bd_path;
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
            ((pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDevicePrevState == enBTRCoreDevStDisconnected) &&
             (pstlhBTRCore->stKnownDevStInfoArr[aBTRCoreDevId].eDeviceCurrState == enBTRCoreDevStLost))) {
            BTRCORELOG_DEBUG ("enBTRCoreDevStDisconnected\n");
            lenBTRCoreRet = enBTRCoreSuccess;
        }
    }
    else {
        for (i32LoopIdx = 0; i32LoopIdx < pstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
                if ((pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) ||
                    ((pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState == enBTRCoreDevStDisconnected) &&
                     (pstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState == enBTRCoreDevStLost))) {
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
    else if ((aBTRCoreDevId < 0) || (apstBTRCoreDevMediaInfo == NULL) || (apstBTRCoreDevMediaInfo->pstBtrCoreDevMCodecInfo == NULL)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
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
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
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
        stBTRCoreKnownDevice* pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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
BTRCore_MediaPlayControl (
    tBTRCoreHandle      hBTRCore, 
    tBTRCoreDevId       aBTRCoreDevId, 
    enBTRCoreDeviceType aenBTRCoreDevType,
    enBTRCoreMediaCtrl  aenBTRCoreDMCtrl
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    BOOLEAN         lbBTDeviceConnected = FALSE; 
    int             loop = 0;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        BTRCORELOG_DEBUG ("Possibly the list is not populated; like booted and connecting\n");
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        BTRCORELOG_ERROR ("There is no device paried for this adapter\n");
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
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

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].deviceId)
                lbBTDeviceConnected = pstlhBTRCore->stKnownDevicesArr[loop].device_connected;
        }
    }

    if (lbBTDeviceConnected == FALSE)
        return enBTRCoreFailure;

    if (BtrCore_BTDevMediaPlayControl(pstlhBTRCore->connHdl, pDeviceAddress, lenBTDeviceType, aenBTRCoreDMCtrl) != 0) {
        BTRCORELOG_ERROR ("Connect to device failed\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterDiscoveryCallback (
    tBTRCoreHandle              hBTRCore, 
    BTRCore_DeviceDiscoveryCb   afptrBTRCoreDeviceDiscoveryCB,
    void*                       apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB) {
        pstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB = afptrBTRCoreDeviceDiscoveryCB;
        BTRCORELOG_INFO ("Device Discovery Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("Device Discovery Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterStatusCallback (
    tBTRCoreHandle     hBTRCore,
    BTRCore_StatusCb   afptrBTRCoreStatusCB,
    void*              apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreStatusCB) {
        pstlhBTRCore->fptrBTRCoreStatusCB = afptrBTRCoreStatusCB;
        pstlhBTRCore->pvCBUserData = apUserData; 
        BTRCORELOG_INFO ("BT Status Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Status Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterConnectionIntimationCallback (
    tBTRCoreHandle      hBTRCore,
    BTRCore_ConnIntimCb afptrBTRCoreConnIntimCB,
    void*               apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreConnIntimCB) {
        pstlhBTRCore->fptrBTRCoreConnIntimCB = afptrBTRCoreConnIntimCB;
        BTRCORELOG_INFO ("BT Conn In Intimation Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Conn In Intimation Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterConnectionAuthenticationCallback (
    tBTRCoreHandle      hBTRCore,
    BTRCore_ConnAuthCb  afptrBTRCoreConnAuthCB,
    void*               apUserData
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        BTRCORELOG_ERROR ("enBTRCoreNotInitialized\n");
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreConnAuthCB) {
        pstlhBTRCore->fptrBTRCoreConnAuthCB = afptrBTRCoreConnAuthCB;
        BTRCORELOG_INFO ("BT Conn Auth Callback Registered Successfully\n");
    }
    else {
        BTRCORELOG_INFO ("BT Conn Auth Callback Already Registered - Not Registering current CB\n");
    }

    return enBTRCoreSuccess;
}


/*  Incoming Callbacks */
static int
btrCore_BTDeviceStatusUpdate_cb (
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
            lenBTRCoreDevType =  enBTRCoreMobileAudioIn;
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

                BTRCORELOG_DEBUG ("\n"
                                 "bPaired = %d\n"
                                 "bConnected = %d\n"
                                 "bTrusted = %d\n"
                                 "bBlocked = %d\n"
                                 "ui16Vendor = %d\n"
                                 "ui16VendorSource = %d\n"
                                 "ui16Product = %d\n"
                                 "ui16Version = %d\n"
                                 "ui32Class = %d\n"
                                 "i32RSSI = %d\n"
                                 "pcName = %s\n"
                                 "pcAddress = %s\n"
                                 "pcAlias = %s\n"
                                 "pcIcon = %s\n",
                                 apstBTDeviceInfo->bPaired,
                                 apstBTDeviceInfo->bConnected,
                                 apstBTDeviceInfo->bTrusted,
                                 apstBTDeviceInfo->bBlocked,
                                 apstBTDeviceInfo->ui16Vendor,
                                 apstBTDeviceInfo->ui16VendorSource,
                                 apstBTDeviceInfo->ui16Product,
                                 apstBTDeviceInfo->ui16Version,
                                 apstBTDeviceInfo->ui32Class,
                                 apstBTDeviceInfo->i32RSSI,
                                 apstBTDeviceInfo->pcName,
                                 apstBTDeviceInfo->pcAddress,
                                 apstBTDeviceInfo->pcAlias,
                                 apstBTDeviceInfo->pcIcon);

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
                    lpstlhBTRCore->stFoundDevice.found  = FALSE;
                    lpstlhBTRCore->stFoundDevice.RSSI   = apstBTDeviceInfo->i32RSSI;
                    lpstlhBTRCore->stFoundDevice.vendor_id = apstBTDeviceInfo->ui16Vendor;
                    lpstlhBTRCore->stFoundDevice.device_type = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
                    lpstlhBTRCore->stFoundDevice.deviceId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
                    strcpy(lpstlhBTRCore->stFoundDevice.device_name, apstBTDeviceInfo->pcName);
                    strcpy(lpstlhBTRCore->stFoundDevice.device_address, apstBTDeviceInfo->pcAddress);

                    /* Populate the profile supported */
                    for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
                        if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                            break;
                        else
                            lpstlhBTRCore->stFoundDevice.device_profile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                         lpstlhBTRCore->stFoundDevice.device_profile.profile[j].profile_name);
                    }
                    lpstlhBTRCore->stFoundDevice.device_profile.numberOfService = j;

                                                
                    if (lpstlhBTRCore->stFoundDevice.device_type == enBTRCore_DC_Unknown) {
                        for (j = 0; j < lpstlhBTRCore->stFoundDevice.device_profile.numberOfService; j++) {
                            if (lpstlhBTRCore->stFoundDevice.device_profile.profile[j].uuid_value ==  strtol(BTR_CORE_A2SNK, NULL, 16)) {
                                lpstlhBTRCore->stFoundDevice.device_type = enBTRCore_DC_Loudspeaker;
                            }
                        }
                    }


                    btrCore_SetScannedDeviceInfo(lpstlhBTRCore);
                    if (lpstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB)
                    {
                        stBTRCoreScannedDevice  stFoundDevice;
                        memcpy (&stFoundDevice, &lpstlhBTRCore->stFoundDevice, sizeof(stFoundDevice));
                        lpstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB(stFoundDevice);
                    }
                }
            }

            break;
        }
        case enBTDevStLost: {
            stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

            if ((lpstlhBTRCore != NULL) && (lpstlhBTRCore->fptrBTRCoreStatusCB != NULL) && apstBTDeviceInfo) {
                int     i32LoopIdx      = 0;

                tBTRCoreDevId   lBTRCoreDevId     = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);

                if (lpstlhBTRCore->numOfPairedDevices) {
                   for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                      if (lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId == lBTRCoreDevId) {
                          BTRCORELOG_INFO ("Device %llu power state Off or OOR", lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId);

                          lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState = 
                                                              lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                          lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState = enBTRCoreDevStLost;


                          lpstlhBTRCore->stDevStatusCbInfo.deviceId         = lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId;
                          lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass     = lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_type;
                          lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDevicePrevState;
                          lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = lpstlhBTRCore->stKnownDevStInfoArr[i32LoopIdx].eDeviceCurrState;
                          lpstlhBTRCore->stDevStatusCbInfo.eDeviceType      = lenBTRCoreDevType;
                          lpstlhBTRCore->stDevStatusCbInfo.isPaired         = 1;
                          strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName, 
                                  lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].device_name, (BD_NAME_LEN-1));

                          lpstlhBTRCore->fptrBTRCoreStatusCB(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvCBUserData);
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

            if ((lpstlhBTRCore != NULL) && (lpstlhBTRCore->fptrBTRCoreStatusCB != NULL) && apstBTDeviceInfo) {
                int     i32LoopIdx      = 0;
                int     i32KnownDevIdx  = -1;
                int     i32ScannedDevIdx= -1;

                tBTRCoreDevId        lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
                enBTRCoreDeviceState leBTDevState  = btrCore_BTParseDeviceConnectionState(apstBTDeviceInfo->pcDeviceCurrState);


                if (lpstlhBTRCore->numOfPairedDevices) {
                    for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfPairedDevices; i32LoopIdx++) {
                        if (lBTRCoreDevId == lpstlhBTRCore->stKnownDevicesArr[i32LoopIdx].deviceId) {
                            i32KnownDevIdx = i32LoopIdx;
                            break;
                        }
                    }
                }


                if (lpstlhBTRCore->numOfScannedDevices) {
                    for (i32LoopIdx = 0; i32LoopIdx < lpstlhBTRCore->numOfScannedDevices; i32LoopIdx++) {
                        if (lBTRCoreDevId == lpstlhBTRCore->stScannedDevicesArr[i32LoopIdx].deviceId) {
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

                        if ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != leBTDevState) &&
                            (lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState != enBTRCoreDevStInitialized)) {
                            if (!(((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStConnected) && (leBTDevState == enBTRCoreDevStDisconnected)) ||
                                  ((lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState == enBTRCoreDevStDisconnected) && (leBTDevState == enBTRCoreDevStConnected)))) {
                                bTriggerDevStatusChangeCb = TRUE;
                            }

#if 0
                            //workaround for notifying the power Up event of a <paired && !connected> devices, as we are not able to track the
                            //power Down event of such devices as per the current analysis
                            if ((enBTRCoreDevStDisconnected == leBTDevState)
                                && ((enBTRCoreDevStConnected     == lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState)
                                   || (enBTRCoreDevStDisconnecting == lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState)
                               )   ) {
                               lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = enBTRCoreDevStPaired;
                               lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = enBTRCoreDevStPaired;
                            }
                            else {
                               if (! (enBTRCoreDevStConnected==leBTDevState
                                     && enBTRCoreDevStDisconnecting==lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState)
                                  ) {
                                  lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState =
                                                                    lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                               }
                               lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;
                            }
#else
                            lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState;
                            lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDeviceCurrState = leBTDevState;
#endif

                            lpstlhBTRCore->stDevStatusCbInfo.isPaired = 1;
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
                            lpstlhBTRCore->stDevStatusCbInfo.isPaired = 0;
                            bTriggerDevStatusChangeCb = TRUE;
                        }
                    }


                    if (bTriggerDevStatusChangeCb == TRUE) {
                       lpstlhBTRCore->stDevStatusCbInfo.deviceId         = lBTRCoreDevId;
                       lpstlhBTRCore->stDevStatusCbInfo.eDeviceType      = lenBTRCoreDevType;
                       lpstlhBTRCore->stDevStatusCbInfo.eDevicePrevState = lpstlhBTRCore->stKnownDevStInfoArr[i32KnownDevIdx].eDevicePrevState;
                       lpstlhBTRCore->stDevStatusCbInfo.eDeviceCurrState = leBTDevState;
                       lpstlhBTRCore->stDevStatusCbInfo.eDeviceClass     = lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].device_type;
                       strncpy(lpstlhBTRCore->stDevStatusCbInfo.deviceName, lpstlhBTRCore->stKnownDevicesArr[i32KnownDevIdx].device_name,
                                                                                                                        (BD_NAME_LEN-1));
                        /* Invoke the callback */
                        lpstlhBTRCore->fptrBTRCoreStatusCB(&lpstlhBTRCore->stDevStatusCbInfo, lpstlhBTRCore->pvCBUserData);
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
btrCore_BTDeviceConnectionIntimation_cb (
    stBTDeviceInfo* apstBTDeviceInfo,
    unsigned int    aui32devPassKey,
    void*           apUserData
) {
    int i32DevConnIntimRet = 0;
    int j = 0;
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

    if (lpstlhBTRCore && (lpstlhBTRCore->fptrBTRCoreConnIntimCB)) {
        BTRCORELOG_DEBUG("btrCore_BTDeviceConnectionIntimation_cb\n");
        lpstlhBTRCore->stConnCbInfo.ui32devPassKey = aui32devPassKey;
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        lpstlhBTRCore->stConnCbInfo.stFoundDevice.found         = TRUE;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.RSSI          = apstBTDeviceInfo->i32RSSI;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.vendor_id     = apstBTDeviceInfo->ui16Vendor;
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_type   = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.deviceId      = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
        strcpy(lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_name, apstBTDeviceInfo->pcName);
        strcpy(lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_address, apstBTDeviceInfo->pcAddress);

        /* Populate the profile supported */
        for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
            if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                break;
            else
                lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                          lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.profile[j].profile_name);
        }
        lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.numberOfService = j;

                                    
        if (lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_type == enBTRCore_DC_Unknown) {
            for (j = 0; j < lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.numberOfService; j++) {
                if (lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.profile[j].uuid_value ==  strtol(BTR_CORE_A2SRC, NULL, 16)) {
                    lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_type = enBTRCore_DC_SmartPhone;
                }
            }
        }

        i32DevConnIntimRet = lpstlhBTRCore->fptrBTRCoreConnIntimCB(&lpstlhBTRCore->stConnCbInfo);
    }

    return i32DevConnIntimRet;
}


static int
btrCore_BTDeviceAuthentication_cb (
    stBTDeviceInfo* apstBTDeviceInfo,
    void*           apUserData
) {
    int i32DevAuthRet = 0;
    int j = 0;
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

    if (lpstlhBTRCore && (lpstlhBTRCore->fptrBTRCoreConnAuthCB)) {
        BTRCORELOG_DEBUG("btrCore_BTDeviceAuthentication_cb\n");
        if (apstBTDeviceInfo->pcName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apstBTDeviceInfo->pcName, (strlen(apstBTDeviceInfo->pcName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apstBTDeviceInfo->pcName) : BTRCORE_STRINGS_MAX_LEN - 1);


        lpstlhBTRCore->stConnCbInfo.stKnownDevice.found         = TRUE;
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.vendor_id     = apstBTDeviceInfo->ui16Vendor;
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_type   = btrCore_MapClassIDtoDeviceType(apstBTDeviceInfo->ui32Class);
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.deviceId      = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.bd_path,        apstBTDeviceInfo->pcAddress);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_name,    apstBTDeviceInfo->pcName);
        strcpy(lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_address, apstBTDeviceInfo->pcAddress);

        for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
            if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                break;
            else
                lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_profile.profile[j].uuid_value = btrCore_BTParseUUIDValue(apstBTDeviceInfo->aUUIDs[j],
                                                                                                                          lpstlhBTRCore->stConnCbInfo.stFoundDevice.device_profile.profile[j].profile_name);
        }
        lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_profile.numberOfService = j;

        if (lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_type == enBTRCore_DC_Unknown) {
            for (j = 0; j < lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_profile.numberOfService; j++) {
                if (lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_profile.profile[j].uuid_value == strtol(BTR_CORE_A2SRC, NULL, 16)) {
                    lpstlhBTRCore->stConnCbInfo.stKnownDevice.device_type = enBTRCore_DC_SmartPhone;
                }
            }
        }

        i32DevAuthRet = lpstlhBTRCore->fptrBTRCoreConnAuthCB(&lpstlhBTRCore->stConnCbInfo);
    }

    return i32DevAuthRet;
}


/* End of File */
