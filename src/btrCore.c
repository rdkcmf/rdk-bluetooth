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


typedef struct _stBTRCoreHdl {
    void*                       connHandle;
    char*                       agentPath;

    unsigned int                numOfAdapters;
    char*                       adapterPath[BTRCORE_MAX_NUM_BT_ADAPTERS];

    char*                       curAdapterPath;

    unsigned int                numOfScannedDevices;
    stBTRCoreScannedDevices     stScannedDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];

    unsigned int                numOfPairedDevices;
    stBTRCoreKnownDevice        stKnownDevicesArr[BTRCORE_MAX_NUM_BT_DEVICES];

    stBTRCoreScannedDevices     stFoundDevice;

    stBTRCoreDevStateCBInfo     stDevStateCbInfo;

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


static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevId btrCore_GenerateUniqueDeviceID (const char* apcDeviceAddress);
static enBTRCoreDeviceClass btrCore_MapClassIDtoDeviceType(unsigned int classID);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_SetScannedDeviceInfo (stBTRCoreHdl* apsthBTRCore); 
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static const char* btrCore_GetKnownDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static const char* btrCore_GetKnownDeviceName (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevId aBTRCoreDevId);
static void btrCore_ShowSignalStrength (short strength);
static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut);

/* Callbacks */
static int btrCore_BTDeviceStatusUpdate_cb(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo,  void* apUserData);
static int btrCore_BTDeviceConnectionIntimation_cb(const char* apBtDeviceName, unsigned int aui32devPassKey, void* apUserData);
static int btrCore_BTDeviceAuthetication_cb(const char* apBtDeviceName, void* apUserData);


/* Static Function Definition */
static void
btrCore_InitDataSt (
    stBTRCoreHdl*   apsthBTRCore
) {
    int i;

    /* Adapters */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_ADAPTERS; i++) {
        apsthBTRCore->adapterPath[i] = (char*)malloc(sizeof(char) * BD_NAME_LEN);
        memset(apsthBTRCore->adapterPath[i], '\0', sizeof(char) * BD_NAME_LEN);
    }

    /* Scanned Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stScannedDevicesArr[i].deviceId = 0;
        memset (apsthBTRCore->stScannedDevicesArr[i].device_address, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stScannedDevicesArr[i].device_name, '\0', sizeof(BD_NAME));
        apsthBTRCore->stScannedDevicesArr[i].RSSI = INT_MIN;
        apsthBTRCore->stScannedDevicesArr[i].found = FALSE;
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
    }

    /* Callback Info */
    memset(apsthBTRCore->stDevStateCbInfo.cDeviceType,      '\0', BTRCORE_STRINGS_MAX_LEN);
    memset(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, '\0', BTRCORE_STRINGS_MAX_LEN);
    memset(apsthBTRCore->stDevStateCbInfo.cDeviceCurrState, '\0', BTRCORE_STRINGS_MAX_LEN);

    strncpy(apsthBTRCore->stDevStateCbInfo.cDeviceType, "Bluez", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);

    apsthBTRCore->stConnCbInfo.ui32devPassKey = 0;
    memset(apsthBTRCore->stConnCbInfo.cConnAuthDeviceName, '\0', BTRCORE_STRINGS_MAX_LEN);

    apsthBTRCore->fptrBTRCoreDeviceDiscoveryCB = NULL;
    apsthBTRCore->fptrBTRCoreStatusCB = NULL;
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
    enBTRCoreDeviceClass rc = enBTRCoreAV_Unknown;
    if (classID & 0x400) {
        unsigned int lastByte = (classID & 0xFF);

        if (lastByte == enBTRCoreAV_WearableHeadset) {
            printf ("Its a enBTRCoreAV_WearableHeadset \n");
            rc = enBTRCoreAV_WearableHeadset;
        }
        else if (lastByte == enBTRCoreAV_Handsfree) {
            printf ("Its a enBTRCoreAV_Handsfree \n");
            rc = enBTRCoreAV_Handsfree;
        }
        else if (lastByte == enBTRCoreAV_Reserved) {
            printf ("Its a enBTRCoreAV_Reserved \n");
            rc = enBTRCoreAV_Reserved;
        }
        else if (lastByte == enBTRCoreAV_Microphone) {
            printf ("Its a enBTRCoreAV_Microphone \n");
            rc = enBTRCoreAV_Microphone;
        }
        else if (lastByte == enBTRCoreAV_Loudspeaker) {
            printf ("Its a enBTRCoreAV_Loudspeaker \n");
            rc = enBTRCoreAV_Loudspeaker;
        }
        else if (lastByte == enBTRCoreAV_Headphones) {
            printf ("Its a enBTRCoreAV_Headphones \n");
            rc = enBTRCoreAV_Headphones;
        }
        else if (lastByte == enBTRCoreAV_PortableAudio) {
            printf ("Its a enBTRCoreAV_PortableAudio \n");
            rc = enBTRCoreAV_PortableAudio;
        }
        else if (lastByte == enBTRCoreAV_CarAudio) {
            printf ("Its a enBTRCoreAV_CarAudio \n");
            rc = enBTRCoreAV_CarAudio;
        }
        else if (lastByte == enBTRCoreAV_STB) {
            printf ("Its a enBTRCoreAV_STB \n");
            rc = enBTRCoreAV_STB;
        }
        else if (lastByte == enBTRCoreAV_HIFIAudioDevice) {
            printf ("Its a enBTRCoreAV_HIFIAudioDevice \n");
            rc = enBTRCoreAV_HIFIAudioDevice;
        }
        else if (lastByte == enBTRCoreAV_VCR) {
            printf ("Its a enBTRCoreAV_VCR \n");
            rc = enBTRCoreAV_VCR;
        }
        else if (lastByte == enBTRCoreAV_VideoCamera) {
            printf ("Its a enBTRCoreAV_VideoCamera \n");
            rc = enBTRCoreAV_VideoCamera;
        }
        else if (lastByte == enBTRCoreAV_Camcoder) {
            printf ("Its a enBTRCoreAV_Camcoder \n");
            rc = enBTRCoreAV_Camcoder;
        }
        else if (lastByte == enBTRCoreAV_VideoMonitor) {
            printf ("Its a enBTRCoreAV_VideoMonitor \n");
            rc = enBTRCoreAV_VideoMonitor;
        }
        else if (lastByte == enBTRCoreAV_TV) {
            printf ("Its a enBTRCoreAV_TV \n");
            rc = enBTRCoreAV_TV;
        }
        else if (lastByte == enBTRCoreAV_VideoConference) {
            printf ("Its a enBTRCoreAV_VideoConference \n");
            rc = enBTRCoreAV_TV;
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
            //printf("adding %s at location %d\n",apsthBTRCore->stFoundDevice.device_address,i);
            apsthBTRCore->stScannedDevicesArr[i].found = TRUE; //mark the record as found
            strcpy(apsthBTRCore->stScannedDevicesArr[i].device_address, apsthBTRCore->stFoundDevice.device_address);
            strcpy(apsthBTRCore->stScannedDevicesArr[i].device_name, apsthBTRCore->stFoundDevice.device_name);
            apsthBTRCore->stScannedDevicesArr[i].RSSI = apsthBTRCore->stFoundDevice.RSSI;
            apsthBTRCore->stScannedDevicesArr[i].vendor_id = apsthBTRCore->stFoundDevice.vendor_id;
            apsthBTRCore->stScannedDevicesArr[i].device_type = apsthBTRCore->stFoundDevice.device_type;
            apsthBTRCore->stScannedDevicesArr[i].deviceId = apsthBTRCore->stFoundDevice.deviceId;

            /* Copy the profile supports */
            memcpy (&apsthBTRCore->stScannedDevicesArr[i].device_profile, &apsthBTRCore->stFoundDevice.device_profile, sizeof(stBTRCoreSupportedServiceList));

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

static enBTRCoreRet
btrCore_PopulateListOfPairedDevices (
    stBTRCoreHdl*   apsthBTRCore,
    const char*     pAdapterPath
) {
    int i, j;
    stBTPairedDeviceInfo pairedDeviceInfo;

    memset (&pairedDeviceInfo, 0, sizeof(pairedDeviceInfo));
    if (0 == BtrCore_BTGetPairedDeviceInfo (apsthBTRCore->connHandle, pAdapterPath, &pairedDeviceInfo)) {
        apsthBTRCore->numOfPairedDevices = pairedDeviceInfo.numberOfDevices;

        for (i = 0; i < pairedDeviceInfo.numberOfDevices; i++) {
            strcpy(apsthBTRCore->stKnownDevicesArr[i].bd_path,        pairedDeviceInfo.devicePath[i]);
            strcpy(apsthBTRCore->stKnownDevicesArr[i].device_name,    pairedDeviceInfo.deviceInfo[i].pcName);
            strcpy(apsthBTRCore->stKnownDevicesArr[i].device_address, pairedDeviceInfo.deviceInfo[i].pcAddress);
            apsthBTRCore->stKnownDevicesArr[i].vendor_id      =    pairedDeviceInfo.deviceInfo[i].ui16Vendor;
            apsthBTRCore->stKnownDevicesArr[i].device_type    =    btrCore_MapClassIDtoDeviceType(pairedDeviceInfo.deviceInfo[i].ui32Class);
            apsthBTRCore->stKnownDevicesArr[i].deviceId  =    btrCore_GenerateUniqueDeviceID(pairedDeviceInfo.deviceInfo[i].pcAddress);

            for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
                if (pairedDeviceInfo.deviceInfo[i].aUUIDs[j][0] == '\0')
                    break;
                else
                    apsthBTRCore->stKnownDevicesArr[i].device_profile.profile[j].uuid_value = btrCore_BTParseUUIDValue(pairedDeviceInfo.deviceInfo[i].aUUIDs[j],
                                                                                                                       apsthBTRCore->stKnownDevicesArr[i].device_profile.profile[j].profile_name);
            }
            apsthBTRCore->stKnownDevicesArr[i].device_profile.numberOfService = j;
        }

        return enBTRCoreSuccess;
    }
    else {
        printf ("Failed to populate List Of Paired Devices\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
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

static void 
btrCore_ShowSignalStrength (
    short strength
) {
    short pos_str;

    pos_str = 100 + strength;//strength is usually negative with number meaning more strength

    printf(" Signal Strength: %d dbmv  ",strength);

    if (pos_str > 70) {
        printf("++++\n");
    }

    if ((pos_str > 50) && (pos_str <= 70)) {
        printf("+++\n");
    }

    if ((pos_str > 37) && (pos_str <= 50)) {
        printf("++\n");
    }

    if (pos_str <= 37) {
        printf("+\n");
    } 
}


void*
DoDispatch (
    void* ptr
) {
    tBTRCoreHandle  hBTRCore = NULL;
    BOOLEAN         ldispatchThreadQuit = FALSE;
    enBTRCoreRet*   penDispThreadExitStatus = malloc(sizeof(enBTRCoreRet));

    hBTRCore = (stBTRCoreHdl*) ptr;
    printf("%s \n", "Dispatch Thread Started");


    if (!((stBTRCoreHdl*)hBTRCore) || !((stBTRCoreHdl*)hBTRCore)->connHandle) {
        fprintf(stderr, "Dispatch thread failure - BTRCore not initialized\n");
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

        if (BtrCore_BTSendReceiveMessages(((stBTRCoreHdl*)hBTRCore)->connHandle) != 0)
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->fptrBTRCoreStatusCB != NULL) {
        pstlhBTRCore->fptrBTRCoreStatusCB(&pstlhBTRCore->stDevStateCbInfo, NULL);
    }
    else {
        printf("no callback installed\n");
    }

    return;
}


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_Init (
    tBTRCoreHandle* phBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL; 

    BTRCore_LOG(("BTRCore_Init\n"));

    if (!phBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }


    pstlhBTRCore = (stBTRCoreHdl*)malloc(sizeof(stBTRCoreHdl));
    if (!pstlhBTRCore) {
        fprintf(stderr, "%s:%d:%s - Insufficient memory - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInitFailure;
    }
    memset(pstlhBTRCore, 0, sizeof(stBTRCoreHdl));


    pstlhBTRCore->connHandle = BtrCore_BTInitGetConnection();
    if (!pstlhBTRCore->connHandle) {
        fprintf(stderr, "%s:%d:%s - Can't get on system bus - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    //init array of scanned , known & found devices
    btrCore_InitDataSt(pstlhBTRCore);

    pstlhBTRCore->agentPath = BtrCore_BTGetAgentPath(pstlhBTRCore->connHandle);
    if (!pstlhBTRCore->agentPath) {
        fprintf(stderr, "%s:%d:%s - Can't get agent path - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    pstlhBTRCore->dispatchThreadQuit = FALSE;
    g_mutex_init(&pstlhBTRCore->dispatchMutex);
    if((pstlhBTRCore->dispatchThread = g_thread_new("BTRCoreTaskThread", DoDispatch, (void*)pstlhBTRCore)) == NULL) {
        fprintf(stderr, "%s:%d:%s - Failed to create Dispatch Thread - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHandle, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        fprintf(stderr, "%s:%d:%s - Failed to get BT Adapter - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    printf("BTRCore_Init - adapter path %s\n", pstlhBTRCore->curAdapterPath);

    /* Initialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (enBTRCoreSuccess != BTRCore_AVMedia_Init(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath)) {
        fprintf(stderr, "%s:%d:%s - Failed to Init AV Media Subsystem - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    if(BtrCore_BTRegisterDevStatusUpdatecB(pstlhBTRCore->connHandle, &btrCore_BTDeviceStatusUpdate_cb, pstlhBTRCore)) {
        fprintf(stderr, "%s:%d:%s - Failed to Register Device Status CB - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnIntimationcB(pstlhBTRCore->connHandle, &btrCore_BTDeviceConnectionIntimation_cb, pstlhBTRCore)) {
        fprintf(stderr, "%s:%d:%s - Failed to Register Connection Intimation CB - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
    }

    if(BtrCore_BTRegisterConnAuthcB(pstlhBTRCore->connHandle, &btrCore_BTDeviceAuthetication_cb, pstlhBTRCore)) {
        fprintf(stderr, "%s:%d:%s - Failed to Register Connection Authentication CB - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    fprintf(stderr, "hBTRCore   =   0x%8p\n", hBTRCore);

    /* Free any memory allotted for use in BTRCore */
    
    /* DeInitialize BTRCore SubSystems - AVMedia/Telemetry..etc. */

    if (enBTRCoreSuccess != BTRCore_AVMedia_DeInit(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath)) {
        fprintf(stderr, "Failed to DeInit AV Media Subsystem\n");
        enDispThreadExitStatus = enBTRCoreFailure;
    }

    g_mutex_lock(&pstlhBTRCore->dispatchMutex);
    pstlhBTRCore->dispatchThreadQuit = TRUE;
    g_mutex_unlock(&pstlhBTRCore->dispatchMutex);

    penDispThreadExitStatus = g_thread_join(pstlhBTRCore->dispatchThread);
    g_mutex_clear(&pstlhBTRCore->dispatchMutex);
    
    fprintf(stderr, "BTRCore_DeInit - Exiting BTRCore - %d\n", *((enBTRCoreRet*)penDispThreadExitStatus));
    enDispThreadExitStatus = *((enBTRCoreRet*)penDispThreadExitStatus);
    free(penDispThreadExitStatus);

    if (pstlhBTRCore->curAdapterPath) {
        if (BtrCore_BTReleaseAdapterPath(pstlhBTRCore->connHandle, NULL)) {
            fprintf(stderr, "%s:%d:%s - Failure BtrCore_BTReleaseAdapterPath\n", __FILE__, __LINE__, __FUNCTION__);
        }
        pstlhBTRCore->curAdapterPath = NULL;
    }

    /* Adapters */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_ADAPTERS; i++) {
        if (pstlhBTRCore->adapterPath[i]) {
            free(pstlhBTRCore->adapterPath[i]);
            pstlhBTRCore->adapterPath[i] = NULL;
        }
    }

    if (pstlhBTRCore->agentPath) {
        if (BtrCore_BTReleaseAgentPath(pstlhBTRCore->connHandle)) {
            fprintf(stderr, "%s:%d:%s - Failure BtrCore_BTReleaseAgentPath\n", __FILE__, __LINE__, __FUNCTION__);
        }
        pstlhBTRCore->agentPath = NULL;
    }

    if (pstlhBTRCore->connHandle) {
        if (BtrCore_BTDeInitReleaseConnection(pstlhBTRCore->connHandle)) {
            fprintf(stderr, "%s:%d:%s - Failure BtrCore_BTDeInitReleaseConnection\n", __FILE__, __LINE__, __FUNCTION__);
        }
        pstlhBTRCore->connHandle = NULL;
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
   // const char *capabilities = "NoInputNoOutput";   //I dont want to deal with pins and passcodes at this time
    char capabilities[32] = {'\0'};
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    
    if (iBTRCapMode == 1) {
        strcpy(capabilities,"DisplayYesNo");
    }
    else {
        //default is no input no output
        strcpy(capabilities,"NoInputNoOutput");
    }

    // printf("Starting agent in mode %s\n",capabilities);
    if (BtrCore_BTRegisterAgent(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath, capabilities) < 0) {
        BTRCore_LOG("agent registration ERROR occurred\n");
        return enBTRCorePairingFailed;
    }

     return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_UnregisterAgent (
    tBTRCoreHandle  hBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTUnregisterAgent(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath) < 0) {
        BTRCore_LOG("agent unregistration  ERROR occurred\n");
        return enBTRCorePairingFailed;//TODO add an enum error code for this situation
    }

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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pstListAdapters) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetAdapterList(pstlhBTRCore->connHandle, &pstlhBTRCore->numOfAdapters, pstlhBTRCore->adapterPath)) {
        pstListAdapters->number_of_adapters = pstlhBTRCore->numOfAdapters;
        for (i = 0; i < pstListAdapters->number_of_adapters; i++) {
            memset(pstListAdapters->adapter_path[i], '\0', sizeof(pstListAdapters->adapter_path[i]));
            strncpy(pstListAdapters->adapter_path[i], pstlhBTRCore->adapterPath[i], BD_NAME_LEN);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pstGetAdapters) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetAdapterList(pstlhBTRCore->connHandle, &pstlhBTRCore->numOfAdapters, pstlhBTRCore->adapterPath)) {
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHandle, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        fprintf(stderr, "Failed to get BT Adapter");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
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
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
    }
    printf("Now current adatper is %s\n", pstlhBTRCore->curAdapterPath);

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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    powered = 1;
    BTRCore_LOG(("BTRCore_EnableAdapter\n"));

    apstBTRCoreAdapter->enable = TRUE;//does this even mean anything?

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    powered = 0;
    BTRCore_LOG(("BTRCore_DisableAdapter\n"));

    apstBTRCoreAdapter->enable = FALSE;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    discoverable = apstBTRCoreAdapter->discoverable;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &discoverable)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &isDiscoverable)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    timeout = apstBTRCoreAdapter->DiscoverableTimeout;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &timeout)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &givenTimeout)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDiscoverable)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "Discoverable", &discoverable)) {
        printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, discoverable);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!apcAdapterDeviceName) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if(apstBTRCoreAdapter->pcAdapterDevName) {
        free(apstBTRCoreAdapter->pcAdapterDevName);
        apstBTRCoreAdapter->pcAdapterDevName = NULL;
    }

    apstBTRCoreAdapter->pcAdapterDevName = strdup(apcAdapterDeviceName); //TODO: Free this memory

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropName, &(apstBTRCoreAdapter->pcAdapterDevName))) {
        BTRCore_LOG("Set Adapter Property enBTAdPropName - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) ||(!pAdapterName)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropName, &pAdapterName)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropName - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterName)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "Name", name)) {
        printf("%s:%d - Get value for org.bluez.Adapter.Name = %s\n", __FUNCTION__, __LINE__, name);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &power)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterPower)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "Powered", &powerStatus)) {
        printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, powerStatus);
        *pAdapterPower = (unsigned char) powerStatus;
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    btrCore_ClearScannedDevicesList(pstlhBTRCore);

    if (BtrCore_BTStartDiscovery(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    sleep(pstStartDiscovery->duration); //TODO: Better to setup a timer which calls BTStopDiscovery

    if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    btrCore_ClearScannedDevicesList(pstlhBTRCore);
    if (0 == BtrCore_BTStartDiscovery(pstlhBTRCore->connHandle, pAdapterPath, pstlhBTRCore->agentPath)) {
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (0 ==  BtrCore_BTStopDiscovery(pstlhBTRCore->connHandle, pAdapterPath, pstlhBTRCore->agentPath)) {
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pListOfScannedDevices) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    fprintf(stderr, "%s:%d:%s - adapter path is %s\n", __FILE__, __LINE__, __FUNCTION__, pstlhBTRCore->curAdapterPath);
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (pstlhBTRCore->stScannedDevicesArr[i].found) {
            printf("Device %d. %s\n - %s  %d dbmV ", i, pstlhBTRCore->stScannedDevicesArr[i].device_name,
                                                        pstlhBTRCore->stScannedDevicesArr[i].device_address,
                                                        pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            btrCore_ShowSignalStrength(pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            printf("\n\n");
        }
    }   

    memset (pListOfScannedDevices, 0, sizeof(stBTRCoreScannedDevicesCount));
    memcpy (pListOfScannedDevices->devices, pstlhBTRCore->stScannedDevicesArr, sizeof (pstlhBTRCore->stScannedDevicesArr));
    pListOfScannedDevices->numberOfDevices = pstlhBTRCore->numOfScannedDevices;
    fprintf(stderr, "%s:%d:%s - Copied scanned details of %d devices\n", __FILE__, __LINE__, __FUNCTION__, pstlhBTRCore->numOfScannedDevices);

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_PairDevice (
    tBTRCoreHandle hBTRCore,
    tBTRCoreDevId  aBTRCoreDevId
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreScannedDevices* pstScannedDevice = NULL;

        fprintf(stderr, "%s:%d:%s - We will pair %s\n", __FILE__, __LINE__, __FUNCTION__, pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_name);
        fprintf(stderr, "%s:%d:%s - address %s\n", __FILE__, __LINE__, __FUNCTION__, pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_address);

        pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstScannedDevice->device_address;
    }
    else {
        pDeviceAddress = btrCore_GetScannedDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
    }

    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        fprintf(stderr, "%s:%d:%s - Failed to find device in Scanned devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }

    if (BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    enBTDevOpCreatePairedDev) < 0) {
        printf("%s:%d - Failed to pair a device\n", __FUNCTION__, __LINE__);
        return enBTRCorePairingFailed;
    }

    printf("%s:%d - Pairing Success\n", __FUNCTION__, __LINE__);

#if 0
    /* Keep the list upto date */
    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
#endif

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_UnPairDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;

    /* We can enhance the BTRCore with passcode support later point in time */
    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        fprintf(stderr, "%s:%d:%s - Possibly the list is not populated\n", __FILE__, __LINE__, __FUNCTION__);
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        fprintf(stderr, "%s:%d:%s - There is no device paried for this adapter\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in paired devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }

    if (BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pDeviceAddress,
                                    enBTDevOpRemovePairedDev) != 0) {
        fprintf(stderr, "%s:%d:%s - Failed to unpair a device\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCorePairingFailed;
    }

    fprintf(stderr, "%s:%d:%s - UnPairing Success\n", __FILE__, __LINE__, __FUNCTION__);

#if 0
    /* Keep the list upto date */
    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
#endif

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetListOfPairedDevices (
    tBTRCoreHandle                  hBTRCore,
    stBTRCorePairedDevicesCount*    pListOfDevices
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!pListOfDevices) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (enBTRCoreSuccess ==  btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath)) {
        pListOfDevices->numberOfDevices = pstlhBTRCore->numOfPairedDevices;
        memcpy (pListOfDevices->devices, pstlhBTRCore->stKnownDevicesArr, sizeof (pstlhBTRCore->stKnownDevicesArr));
        printf("%s:%d - Copied all the known devices\n", __FUNCTION__, __LINE__);
        return enBTRCoreSuccess;
    }

    return enBTRCoreFailure;
}


enBTRCoreRet
BTRCore_FindDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    stBTRCoreScannedDevices* pstScannedDevice = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];

    printf(" We will try to find %s\n", pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_name);
    printf(" address %s\n", pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_address);

    if (BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pstScannedDevice->device_address,
                                    enBTDevOpFindPairedDev) < 0) {
       // BTRCore_LOG("device not found\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in Scanned devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }

    printf("Checking for service %s on %s\n", UUID, pDeviceAddress);
    *found = BtrCore_BTFindServiceSupported (pstlhBTRCore->connHandle, pDeviceAddress, UUID, XMLdata);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if ((!pProfileList) || (0 == aBTRCoreDevId)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in Scanned devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }


    /* Initialize the array */
    memset (pProfileList, 0 , sizeof(stBTRCoreSupportedServiceList));
    memset (&profileList, 0 , sizeof(stBTDeviceSupportedServiceList));

    if (BtrCore_BTDiscoverDeviceServices (pstlhBTRCore->connHandle, pDeviceAddress, &profileList) != 0) {
        return enBTRCoreFailure;
    }

    printf ("%s:%d - Successfully received the supported services... \n", __FUNCTION__, __LINE__);

    pProfileList->numberOfService = profileList.numberOfService;
    for (i = 0; i < profileList.numberOfService; i++) {
        pProfileList->profile[i].uuid_value = profileList.profile[i].uuid_value;
        strncpy (pProfileList->profile[i].profile_name,  profileList.profile[i].profile_name, 30);
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ConnectDevice (
    tBTRCoreHandle      hBTRCore, 
    tBTRCoreDevId       aBTRCoreDevId, 
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
    const char*     pDeviceName = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    int loop = 0;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        fprintf(stderr, "%s:%d:%s - Possibly the list is not populated; like booted and connecting\n", __FILE__, __LINE__, __FUNCTION__);
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        fprintf(stderr, "%s:%d:%s - There is no device paried for this adapter\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreFailure;
    }

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        stBTRCoreKnownDevice*   pstKnownDevice = NULL;
        pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];
        pDeviceAddress = pstKnownDevice->bd_path;
        pDeviceName = pstKnownDevice->device_name;
    }
    else {
        pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, aBTRCoreDevId);
        pDeviceName =  btrCore_GetKnownDeviceName(pstlhBTRCore, aBTRCoreDevId);
    }


    if (!pDeviceAddress || !strlen(pDeviceAddress)) {
        fprintf(stderr, "%s:%d:%s - Failed to find device in paired devices list\n", __FILE__, __LINE__, __FUNCTION__);
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

    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHandle, pDeviceAddress, lenBTDeviceType) != 0) {
        fprintf(stderr, "%s:%d:%s - Connect to device failed\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreFailure;
    }

    fprintf(stderr, "%s:%d:%s - Connected to device %s Successfully. Lets start Play the audio\n", __FILE__, __LINE__, __FUNCTION__, pDeviceName);

    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected = TRUE;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].deviceId)
                pstlhBTRCore->stKnownDevicesArr[loop].device_connected = TRUE;
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

    int loop = 0;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        fprintf(stderr, "%s:%d:%s - There is no device paried for this adapter\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in paired devices list\n", __FILE__, __LINE__, __FUNCTION__);
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

    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHandle, pDeviceAddress, lenBTDeviceType) != 0) {
        fprintf(stderr, "%s:%d:%s - DisConnect from device failed\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreFailure;
    }

    fprintf(stderr, "%s:%d:%s - DisConnected from device Successfully.\n", __FILE__, __LINE__, __FUNCTION__);
    if (aBTRCoreDevId < BTRCORE_MAX_NUM_BT_DEVICES) {
        pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].device_connected = FALSE;
    }
    else {
        for (loop = 0; loop < pstlhBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevId == pstlhBTRCore->stKnownDevicesArr[loop].deviceId)
                pstlhBTRCore->stKnownDevicesArr[loop].device_connected = FALSE;
        }
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
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    const char*     pDeviceAddress = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    int             liDataPath = 0;
    int             lidataReadMTU = 0;
    int             lidataWriteMTU = 0;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (!aiDataPath || !aidataReadMTU || !aidataWriteMTU || (aBTRCoreDevId < 0)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (pstlhBTRCore->numOfPairedDevices == 0) {
        printf("%s:%d - Possibly the list is not populated; like booted and connecting\n", __FUNCTION__, __LINE__);
        /* Keep the list upto date */
        btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
    }

    if (!pstlhBTRCore->numOfPairedDevices) {
        fprintf(stderr, "%s:%d:%s - There is no device paried for this adapter\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in paired devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }

    printf(" We will Acquire Data Path for %s\n", pDeviceAddress);

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

    if (BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->connHandle, pDeviceAddress, &liDataPath, &lidataReadMTU, &lidataWriteMTU) != enBTRCoreSuccess) {
        BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    *aiDataPath     = liDataPath;
    *aidataReadMTU  = lidataReadMTU;
    *aidataWriteMTU = lidataWriteMTU;

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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }
    else if (aBTRCoreDevId < 0) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->numOfPairedDevices) {
        fprintf(stderr, "%s:%d:%s - There is no device paried for this adapter\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - Failed to find device in paired devices list\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreDeviceNotFound;
    }

    printf(" We will Release Data Path for %s\n", pDeviceAddress);

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

    if(BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->connHandle, pDeviceAddress) != enBTRCoreSuccess) {
        BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB) {
        pstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB = afptrBTRCoreDeviceDiscoveryCB;
        printf("%s:%d - Device Discovery Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }
    else {
        printf("%s:%d - Device Discovery Callback Already Registered - Not Registering current CB\n", __FUNCTION__, __LINE__);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreStatusCB) {
        pstlhBTRCore->fptrBTRCoreStatusCB = afptrBTRCoreStatusCB;
        pstlhBTRCore->pvCBUserData = apUserData; 
        printf("%s:%d - BT Status Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }
    else {
        printf("%s:%d - BT Status Callback Already Registered - Not Registering current CB\n", __FUNCTION__, __LINE__);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreConnIntimCB) {
        pstlhBTRCore->fptrBTRCoreConnIntimCB = afptrBTRCoreConnIntimCB;
        printf("%s:%d - BT Conn In Intimation Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }
    else {
        printf("%s:%d - BT Conn In Intimation Callback Already Registered - Not Registering current CB\n", __FUNCTION__, __LINE__);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreNotInitialized\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreConnAuthCB) {
        pstlhBTRCore->fptrBTRCoreConnAuthCB = afptrBTRCoreConnAuthCB;
        printf("%s:%d - BT Conn Auth Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }
    else {
        printf("%s:%d - BT Conn Auth Callback Already Registered - Not Registering current CB\n", __FUNCTION__, __LINE__);
    }

    return enBTRCoreSuccess;
}


static int
btrCore_BTDeviceStatusUpdate_cb (
    enBTDeviceType  aeBtDeviceType,
    enBTDeviceState aeBtDeviceState,
    stBTDeviceInfo* apstBTDeviceInfo,
    void*           apUserData
) {
    printf("%s:%d - enBTDeviceType = %d enBTDeviceState = %d apstBTDeviceInfo = %p\n", __FUNCTION__, __LINE__, aeBtDeviceType, aeBtDeviceState, apstBTDeviceInfo);

    switch (aeBtDeviceState) {
        case enBTDevStCreated: {
        }
        break;
        case enBTDevStScanInProgress: {
        }
        break;
        case enBTDevStFound: {
            if (apstBTDeviceInfo) {
                int j = 0;
                tBTRCoreDevId   lBTRCoreDevId = 0;
                stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

                printf("%s:%d - bPaired = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->bPaired);
                printf("%s:%d - bConnected = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->bConnected);
                printf("%s:%d - bTrusted = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->bTrusted);
                printf("%s:%d - bBlocked = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->bBlocked);
                printf("%s:%d - ui16Vendor = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->ui16Vendor);
                printf("%s:%d - ui16VendorSource = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->ui16VendorSource);
                printf("%s:%d - ui16Product = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->ui16Product);
                printf("%s:%d - ui16Version = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->ui16Version);
                printf("%s:%d - ui32Class = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->ui32Class);
                printf("%s:%d - i32RSSI = %d\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->i32RSSI);
                printf("%s:%d - pcName = %s\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->pcName);
                printf("%s:%d - pcAddress = %s\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->pcAddress);
                printf("%s:%d - pcAlias = %s\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->pcAlias);
                printf("%s:%d - pcIcon = %s\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->pcIcon);

                for (j = 0; j < BT_MAX_DEVICE_PROFILE; j++) {
                    if (apstBTDeviceInfo->aUUIDs[j][0] == '\0')
                        break;
                    else
                        printf("%s:%d - aUUIDs = %s\n", __FUNCTION__, __LINE__, apstBTDeviceInfo->aUUIDs[j]);
                }

                // TODO: Think of a way to move this to taskThread
                lBTRCoreDevId = btrCore_GenerateUniqueDeviceID(apstBTDeviceInfo->pcAddress);
                if (btrCore_GetScannedDeviceAddress(apUserData, lBTRCoreDevId) != NULL) {
                    printf ("Already we have a entry in the list; Skip Parsing now \n");
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

                    btrCore_SetScannedDeviceInfo(lpstlhBTRCore);
                    if (lpstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB)
                    {
                        stBTRCoreScannedDevices stFoundDevice;
                        memcpy (&stFoundDevice, &lpstlhBTRCore->stFoundDevice, sizeof(stFoundDevice));
                        lpstlhBTRCore->fptrBTRCoreDeviceDiscoveryCB(stFoundDevice);
                    }
                }
            }
        }
        break;
        case enBTDevStLost: {
        }
        break;
        case enBTDevStPairingRequest: {
        }
        break;
        case enBTDevStPairingInProgress: {
        }
        break;
        case enBTDevStPaired: {
        }
        break;
        case enBTDevStUnPaired: {
        }
        break;
        case enBTDevStConnectInProgress: {
        }
        break;
        case enBTDevStConnected: {
        }
        break;
        case enBTDevStDisconnected: {
        }
        break;
        case enBTDevStPropChanged: {
            stBTRCoreHdl*       lpstlhBTRCore = (stBTRCoreHdl*)apUserData;
            if ((lpstlhBTRCore != NULL) && (lpstlhBTRCore->fptrBTRCoreStatusCB != NULL)) {
                strcpy(lpstlhBTRCore->stDevStateCbInfo.cDevicePrevState,apstBTDeviceInfo->pcDevicePrevState);
                strcpy(lpstlhBTRCore->stDevStateCbInfo.cDeviceCurrState,apstBTDeviceInfo->pcDeviceCurrState);
                lpstlhBTRCore->fptrBTRCoreStatusCB(&lpstlhBTRCore->stDevStateCbInfo, lpstlhBTRCore->pvCBUserData);
            }
        }
        break;
        case enBTDevStUnknown: {
        }
        break;
        default: {
        }
        break;
    }

    return 0;
}


static int
btrCore_BTDeviceConnectionIntimation_cb (
    const char*     apBtDeviceName,
    unsigned int    aui32devPassKey,
    void*           apUserData
) {
    int i32DevConnIntimRet = 0;
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

    if (lpstlhBTRCore && (lpstlhBTRCore->fptrBTRCoreConnAuthCB)) {
        lpstlhBTRCore->stConnCbInfo.ui32devPassKey = aui32devPassKey;
        if (apBtDeviceName)
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apBtDeviceName, (strlen(apBtDeviceName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apBtDeviceName) : BTRCORE_STRINGS_MAX_LEN - 1);

        i32DevConnIntimRet = lpstlhBTRCore->fptrBTRCoreConnAuthCB(&lpstlhBTRCore->stConnCbInfo);
    }

    return i32DevConnIntimRet;
}


static int
btrCore_BTDeviceAuthetication_cb (
    const char*     apBtDeviceName,
    void*           apUserData
) {
    int i32DevAuthRet = 0;
    stBTRCoreHdl*   lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

    if (lpstlhBTRCore && (lpstlhBTRCore->fptrBTRCoreConnAuthCB)) {
        if (apBtDeviceName) {
            strncpy(lpstlhBTRCore->stConnCbInfo.cConnAuthDeviceName, apBtDeviceName, (strlen(apBtDeviceName) < (BTRCORE_STRINGS_MAX_LEN - 1)) ? strlen(apBtDeviceName) : BTRCORE_STRINGS_MAX_LEN - 1);
            i32DevAuthRet = lpstlhBTRCore->fptrBTRCoreConnAuthCB(&lpstlhBTRCore->stConnCbInfo);
        }
    }

    return i32DevAuthRet;
} 

static unsigned int btrCore_BTParseUUIDValue (const char *pUUIDString, char* pServiceNameOut)
{
    char aUUID[8];
    unsigned int uuid_value = 0;


    if (pUUIDString)
    {
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



/* End of File */
