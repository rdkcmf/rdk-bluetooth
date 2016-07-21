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
#include "btrCore_dbus_bt.h"


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

    stBTRCoreDevStateCB         stDevStateCbInfo;


    BTRCore_DeviceDiscoveryCb   fptrBTRCoreDeviceDiscoveryCB;
    BTRCore_StatusCb            fptrBTRCoreStatusCB;

    GThread*                    dispatchThread;
    GMutex                      dispatchMutex;
    BOOLEAN                     dispatchThreadQuit;
} stBTRCoreHdl;


static void btrCore_InitDataSt (stBTRCoreHdl* apsthBTRCore);
static tBTRCoreDevHandle btrCore_GenerateUniqueHandle (const char* apcDeviceAddress);
static void btrCore_ClearScannedDevicesList (stBTRCoreHdl* apsthBTRCore);
static const char* btrCore_GetScannedDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevHandle handle);
static void btrCore_SetScannedDeviceInfo (stBTRCoreHdl* apsthBTRCore); 
static enBTRCoreRet btrCore_PopulateListOfPairedDevices(stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath);
static const char* btrCore_GetKnownDeviceAddress (stBTRCoreHdl* apsthBTRCore, tBTRCoreDevHandle handle);
static const char* btrCore_GetKnownDeviceName(stBTRCoreHdl* apsthBTRCore, tBTRCoreDevHandle   aBTRCoreDevHandle);
static void btrCore_ShowSignalStrength (short strength);

/* Callbacks */
static int btrCore_BTDeviceStatusUpdate_cb(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo,  void* apUserData);


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
        apsthBTRCore->stScannedDevicesArr[i].device_handle = 0;
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
        apsthBTRCore->stKnownDevicesArr[i].device_handle = 0;
        memset (apsthBTRCore->stKnownDevicesArr[i].bd_path, '\0', sizeof(BD_NAME));
        memset (apsthBTRCore->stKnownDevicesArr[i].device_name, '\0', sizeof(BD_NAME));
    }

    /* Callback Info */
    memset(apsthBTRCore->stDevStateCbInfo.cDeviceType, '\0', sizeof(apsthBTRCore->stDevStateCbInfo.cDeviceType));
    memset(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, '\0', sizeof(apsthBTRCore->stDevStateCbInfo.cDevicePrevState));
    memset(apsthBTRCore->stDevStateCbInfo.cDeviceCurrState, '\0', sizeof(apsthBTRCore->stDevStateCbInfo.cDeviceCurrState));

    strncpy(apsthBTRCore->stDevStateCbInfo.cDeviceType, "Bluez", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(apsthBTRCore->stDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);

    apsthBTRCore->fptrBTRCoreDeviceDiscoveryCB = NULL;
    apsthBTRCore->fptrBTRCoreStatusCB = NULL;

    /* Always safer to initialze Global variables, init if any left or added */
}


static tBTRCoreDevHandle
btrCore_GenerateUniqueHandle (
    const char* apcDeviceAddress
) {
    tBTRCoreDevHandle   lBTRCoreDevHandle = 0;
    char                lcDevHdlArr[13] = {'\0'};

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

        lBTRCoreDevHandle = (tBTRCoreDevHandle) strtoll(lcDevHdlArr, NULL, 16);
    }

    return lBTRCoreDevHandle;
}


static void
btrCore_ClearScannedDevicesList (
    stBTRCoreHdl* apsthBTRCore
) {
    int i;

    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        apsthBTRCore->stScannedDevicesArr[i].device_handle = 0;
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
            apsthBTRCore->stScannedDevicesArr[i].device_handle = btrCore_GenerateUniqueHandle(apsthBTRCore->stFoundDevice.device_address);
            apsthBTRCore->numOfScannedDevices++;
            break;
        }
    }
}


static const char*
btrCore_GetScannedDeviceAddress (
    stBTRCoreHdl*       apsthBTRCore,
    tBTRCoreDevHandle   aBTRCoreDevHandle
) {
    int loop = 0;

    if ((0 == aBTRCoreDevHandle) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfScannedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfScannedDevices; loop++) {
            if (aBTRCoreDevHandle == apsthBTRCore->stScannedDevicesArr[loop].device_handle)
             return apsthBTRCore->stScannedDevicesArr[loop].device_address;
        }
    }

    return NULL;
}

static enBTRCoreRet btrCore_PopulateListOfPairedDevices (stBTRCoreHdl* apsthBTRCore, const char* pAdapterPath)
{
    int i;
    stBTPairedDeviceInfo pairedDeviceInfo;

    memset (&pairedDeviceInfo, 0, sizeof(pairedDeviceInfo));
    if (0 == BtrCore_BTGetPairedDeviceInfo (apsthBTRCore->connHandle, pAdapterPath, &pairedDeviceInfo))
    {
        apsthBTRCore->numOfPairedDevices = pairedDeviceInfo.numberOfDevices;
        for (i = 0; i < pairedDeviceInfo.numberOfDevices; i++)
        {
            strcpy(apsthBTRCore->stKnownDevicesArr[i].bd_path,        pairedDeviceInfo.devicePath[i]);
            strcpy(apsthBTRCore->stKnownDevicesArr[i].device_name,    pairedDeviceInfo.deviceInfo[i].pcName);
            strcpy(apsthBTRCore->stKnownDevicesArr[i].device_address, pairedDeviceInfo.deviceInfo[i].pcAddress);
            apsthBTRCore->stKnownDevicesArr[i].vendor_id      =    pairedDeviceInfo.deviceInfo[i].ui16Vendor;
            apsthBTRCore->stKnownDevicesArr[i].device_handle  =    btrCore_GenerateUniqueHandle(pairedDeviceInfo.deviceInfo[i].pcAddress);
        }
        return enBTRCoreSuccess;
    }
    else
    {
        printf ("Failed to populate List Of Paired Devices\n");
        return enBTRCoreFailure;
    }
    return enBTRCoreSuccess;
}


static const char*
btrCore_GetKnownDeviceAddress (
    stBTRCoreHdl*       apsthBTRCore,
    tBTRCoreDevHandle   aBTRCoreDevHandle
) {
    int loop = 0;

    if ((0 == aBTRCoreDevHandle) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevHandle == apsthBTRCore->stKnownDevicesArr[loop].device_handle)
             return apsthBTRCore->stKnownDevicesArr[loop].bd_path;
        }
    }

    return NULL;
}

static const char* btrCore_GetKnownDeviceName(
   stBTRCoreHdl*       apsthBTRCore,
   tBTRCoreDevHandle   aBTRCoreDevHandle
  )
{
    int loop = 0;

   if ((0 == aBTRCoreDevHandle) || (!apsthBTRCore))
        return NULL;

    if (apsthBTRCore->numOfPairedDevices) {
        for (loop = 0; loop < apsthBTRCore->numOfPairedDevices; loop++) {
            if (aBTRCoreDevHandle == apsthBTRCore->stKnownDevicesArr[loop].device_handle)
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
        pstlhBTRCore->fptrBTRCoreStatusCB(&pstlhBTRCore->stDevStateCbInfo);
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

   /*The p_ConnAuth_callback is initialized to NULL.  An app can register this callback to allow a user to accept or reject a
      connection request from a remote device.  If the app does not register this callback, the behavior is similar to the
      NoInputNoOuput setting, where connections are automatically accepted.
    */
    p_ConnAuth_callback = NULL;

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
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_StartDiscovery (
    tBTRCoreHandle           hBTRCore,
    stBTRCoreStartDiscovery* pstStartDiscovery
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_GetAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
    int pathlen;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_ListKnownDevices (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int pathlen; //temporary variable shoud be refactored away
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    pathlen = strlen(pstlhBTRCore->curAdapterPath);

    switch (apstBTRCoreAdapter->adapter_number) {
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

    printf("adapter path is %s\n", pstlhBTRCore->curAdapterPath);
    return btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pstlhBTRCore->curAdapterPath);
}


enBTRCoreRet
BTRCore_GetAdapters (
    tBTRCoreHandle          hBTRCore,
    stBTRCoreGetAdapters*   pstGetAdapters
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!BtrCore_BTGetAdapterList(pstlhBTRCore->connHandle, &pstlhBTRCore->numOfAdapters, pstlhBTRCore->adapterPath)) {
        pstGetAdapters->number_of_adapters = pstlhBTRCore->numOfAdapters;
        return enBTRCoreSuccess;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ForgetDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*           pstlhBTRCore = NULL;
    stBTRCoreKnownDevice*   pstKnownDevice = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId > (pstlhBTRCore->numOfPairedDevices -1)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreFailure - Device ID Not found\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreFailure;
    }

    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    printf(" We will remove %s\n", pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

    BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                pstlhBTRCore->curAdapterPath,
                                pstlhBTRCore->agentPath,
                                pstKnownDevice->bd_path,
                                enBTDevOpRemovePairedDev);

    return enBTRCoreSuccess;
}


/*BTRCore_FindServiceByIndex, other inputs will include string and boolean pointer for returning*/
enBTRCoreRet
BTRCore_FindServiceByIndex (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId,
    const char*     UUID,
    char*           XMLdata,
    int*            found
) {
    stBTRCoreHdl*           pstlhBTRCore = NULL;
    stBTRCoreKnownDevice*   pstKnownDevice = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    //BTRCore_LOG(("BTRCore_FindServiceByIndex\n"));
    //printf("looking for %s\n", UUID);

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    printf("Checking for service %s on %s\n", UUID, pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

    *found = BtrCore_BTFindServiceSupported (pstlhBTRCore->connHandle, pstKnownDevice->bd_path, UUID, XMLdata);
    if (*found < 0) {
        return enBTRCoreFailure;
     }
     else {
        return enBTRCoreSuccess;
     }
}


enBTRCoreRet
BTRCore_ShowFoundDevices (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int i;
    int pathlen; //temporary variable shoud be refactored away
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pathlen = strlen(pstlhBTRCore->curAdapterPath);

    switch (apstBTRCoreAdapter->adapter_number) {
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

    printf("adapter path is %s\n", pstlhBTRCore->curAdapterPath);


    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (pstlhBTRCore->stScannedDevicesArr[i].found) {
            printf("Device %d. %s\n - %s  %d dbmV ", i, pstlhBTRCore->stScannedDevicesArr[i].device_name,
                                                        pstlhBTRCore->stScannedDevicesArr[i].device_address,
                                                        pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            btrCore_ShowSignalStrength(pstlhBTRCore->stScannedDevicesArr[i].RSSI);
            printf("\n\n");
        }
    }   

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_PairDeviceByIndex (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreScannedDevices* pstScannedDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstScannedDevice = &pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId];

    printf(" We will pair %s\n", pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_name);
    printf(" address %s\n", pstlhBTRCore->stScannedDevicesArr[aBTRCoreDevId].device_address);

    if (BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                    pstlhBTRCore->curAdapterPath,
                                    pstlhBTRCore->agentPath,
                                    pstScannedDevice->device_address,
                                    enBTDevOpCreatePairedDev) < 0) {
        BTRCore_LOG("pairing ERROR occurred\n");
        return enBTRCorePairingFailed;
    }

   //Update array of known devices now that we paired something
    btrCore_PopulateListOfPairedDevices(hBTRCore, pstlhBTRCore->curAdapterPath);

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_RegisterAgent (
    tBTRCoreHandle  hBTRCore,
    int iBTRCapMode
) {
   // const char *capabilities = "NoInputNoOutput";   //I dont want to deal with pins and passcodes at this time
    char capabilities[32];
    stBTRCoreHdl*   pstlhBTRCore = NULL;
    
     if (iBTRCapMode == 1)
      {
        strcpy(capabilities,"DisplayYesNo");
       }
       else
       {//default is no input no output
         strcpy(capabilities,"NoInputNoOutput");
       }

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


   // printf("Starting agent in mode %s\n",capabilities);
    if (BTRCore_BTRegisterAgent(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath, capabilities) < 0) {
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if ( BTRCore_BTUnregisterAgent(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath) < 0) {
        BTRCore_LOG("agent unregistration  ERROR occurred\n");
        return enBTRCorePairingFailed;//TODO add an enum error code for this situation
    }

    return enBTRCoreSuccess;
}
 
enBTRCoreRet
BTRCore_FindDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreScannedDevices* pstScannedDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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


enBTRCoreRet
BTRCore_ConnectDeviceByIndex (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTDeviceType          lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice*   pstKnownDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (aBTRCoreDevId > (pstlhBTRCore->numOfPairedDevices -1)) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreFailure - Device ID Not found\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreFailure;
    }

    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    printf(" We will connect %s\n", pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

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

    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, lenBTDeviceType)) {
        BTRCore_LOG("connection ERROR occurred\n");
        return enBTRCoreFailure;
    }
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_DisconnectDeviceByIndex (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTDeviceType          lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice*   pstKnownDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    printf(" We will disconnect %s\n", pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

    // TODO: Implement a Device State Machine and Check whether the device is in a Disconnectable State
    // before making the Disconnect call
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

    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, lenBTDeviceType)) {
        BTRCore_LOG("disconnection ERROR occurred\n");
        return enBTRCoreFailure;
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

    enBTDeviceType lenBTDeviceType = enBTDevUnknown;
    int liDataPath = 0;
    int lidataReadMTU = 0;
    int lidataWriteMTU = 0;

    stBTRCoreKnownDevice* pstKnownDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    if (!aiDataPath || !aidataReadMTU || !aidataWriteMTU) {
        fprintf(stderr, "BTRCore_AcquireDeviceDataPath - Invalid Arguments \n");
        return enBTRCoreInvalidArg;
    }

    printf(" We will Acquire Data Path for %s\n", pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

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
    if(enBTRCoreSuccess != BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, &liDataPath, &lidataReadMTU, &lidataWriteMTU)) {
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
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice* pstKnownDevice = NULL;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    pstKnownDevice = &pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId];

    printf(" We will Release Data Path for %s\n", pstlhBTRCore->stKnownDevicesArr[aBTRCoreDevId].bd_path);

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

    if(enBTRCoreSuccess != BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->connHandle, pstKnownDevice->bd_path)) {
        BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_RegisterConnectionAuthenticationCallback (
    tBTRCoreHandle  hBTRCore,
    void*           cb
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

  p_ConnAuth_callback = cb;
  return enBTRCoreSuccess;
}

enBTRCoreRet
BTRCore_EnableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int powered;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_SetDiscoverableTimeout (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    U32 timeout;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_SetDiscoverable (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int discoverable;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
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
BTRCore_SetAdapterDeviceName (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter,
	char*				apcAdapterDeviceName
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

	if (!apcAdapterDeviceName) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
	}

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
BTRCore_GetListOfAdapters (
    tBTRCoreHandle          hBTRCore,
    stBTRCoreListAdapters*  pstListAdapters
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreFailure;
    int i;

    if ((!hBTRCore) || (!pstListAdapters)) {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        return enBTRCoreNotInitialized;
    }

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
BTRCore_SetAdapterName (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    const char*     pAdapterName
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;

    if ((!hBTRCore) || (!pAdapterPath) ||(!pAdapterName)) {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropName, &pAdapterName)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropName - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Name Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_SetAdapterPower (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char   powerStatus
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int power = powerStatus;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &power)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Powered Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_SetAdapterDiscoverableTimeout (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned short  timeout
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    U32 givenTimeout = (U32) timeout;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &givenTimeout)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.DiscoverableTimeout Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_SetAdapterDiscoverable (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char   discoverable
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int isDiscoverable = (int) discoverable;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &isDiscoverable)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Discoverable Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_GetAdapterName (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    char*           pAdapterName
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    char name[BD_NAME_LEN + 1] = "";

    memset (name, '\0', sizeof (name));

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterName)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "org.bluez.Adapter", "Name", name)) {
            printf("%s:%d - Get value for org.bluez.Adapter.Name = %s\n", __FUNCTION__, __LINE__, name);
            strcpy(pAdapterName, name);
            rc = enBTRCoreSuccess;
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.Name failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}


enBTRCoreRet
BTRCore_GetAdapterPower (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char*  pAdapterPower
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int powerStatus = 0;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterPower)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "org.bluez.Adapter", "Powered", &powerStatus)) {
            printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, powerStatus);
            *pAdapterPower = (unsigned char) powerStatus;
            rc = enBTRCoreSuccess;
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.powered failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}


enBTRCoreRet
BTRCore_GetAdapterDiscoverableStatus (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath,
    unsigned char*  pDiscoverable
) {
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int discoverable = 0;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDiscoverable)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (!BtrCore_BTGetProp(pstlhBTRCore->connHandle, pAdapterPath, "org.bluez.Adapter", "Discoverable", &discoverable)) {
            printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, discoverable);
            *pDiscoverable = (unsigned char) discoverable;
            rc = enBTRCoreSuccess;
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.powered failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}


enBTRCoreRet
BTRCore_StartDeviceDiscovery (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath
) {
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        btrCore_ClearScannedDevicesList(pstlhBTRCore);
        if (0 == BtrCore_BTStartDiscovery(pstlhBTRCore->connHandle, pAdapterPath, pstlhBTRCore->agentPath))
            rc = enBTRCoreSuccess;
        else
            printf("%s:%d - Failed to Start\n", __FUNCTION__, __LINE__);
    }

    return rc;
}


enBTRCoreRet
BTRCore_StopDeviceDiscovery (
    tBTRCoreHandle  hBTRCore,
    const char*     pAdapterPath
) {
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (0 ==  BtrCore_BTStopDiscovery(pstlhBTRCore->connHandle, pAdapterPath, pstlhBTRCore->agentPath))
            rc = enBTRCoreSuccess;
        else
            printf("%s:%d - Failed to Stop\n", __FUNCTION__, __LINE__);
    }

    return rc;
}


enBTRCoreRet
BTRCore_PairDevice (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath,
    tBTRCoreDevHandle   handle
) {
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (0 == handle)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        const char *pDeviceAddress = btrCore_GetScannedDeviceAddress(pstlhBTRCore, handle);
        if (pDeviceAddress) {
            if (BtrCore_BTPerformDeviceOp ( pstlhBTRCore->connHandle,
                                            pAdapterPath,
                                            pstlhBTRCore->agentPath,
                                            pDeviceAddress,
                                            enBTDevOpCreatePairedDev) < 0) {
                printf("%s:%d - Failed to pair a device\n", __FUNCTION__, __LINE__);
                rc = enBTRCorePairingFailed;
            }
            else {
                rc = enBTRCoreSuccess;
                printf("%s:%d - Pairing Success\n", __FUNCTION__, __LINE__);

                /* Keep the list upto date */
                btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath);
            }
        }
        else {
            printf("%s:%d - Failed to find a %llu in the scanned list\n", __FUNCTION__, __LINE__, handle);
            rc = enBTRCorePairingFailed;
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_GetListOfPairedDevices (
    tBTRCoreHandle                  hBTRCore,
    const char*                     pAdapterPath,
    stBTRCorePairedDevicesCount*    pListOfDevices
) {
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*   pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pListOfDevices)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else if (enBTRCoreSuccess ==  btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath)) {
        pListOfDevices->numberOfDevices = pstlhBTRCore->numOfPairedDevices;
        memcpy (pListOfDevices->devices, pstlhBTRCore->stKnownDevicesArr, sizeof (pstlhBTRCore->stKnownDevicesArr));
        rc = enBTRCoreSuccess;
        printf("%s:%d - Copied all the known devices\n", __FUNCTION__, __LINE__);
    }

    return rc;
}


enBTRCoreRet
BTRCore_GetListOfScannedDevices (
    tBTRCoreHandle                  hBTRCore,
    const char*                     pAdapterPath, 
    stBTRCoreScannedDevicesCount*   pListOfScannedDevices
) {
    stBTRCoreHdl*   pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet    rc = enBTRCoreInvalidArg;

    if ((!hBTRCore) || (!pAdapterPath) || (!pListOfScannedDevices)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        memset (pListOfScannedDevices, 0, sizeof(stBTRCoreScannedDevicesCount));
        memcpy (pListOfScannedDevices->devices, pstlhBTRCore->stScannedDevicesArr, sizeof (pstlhBTRCore->stScannedDevicesArr));
        pListOfScannedDevices->numberOfDevices = pstlhBTRCore->numOfScannedDevices;
        printf("%s:%d - Copied scanned details of %d devices\n", __FUNCTION__, __LINE__, pstlhBTRCore->numOfScannedDevices);
        rc = enBTRCoreSuccess;
    }

    return rc;
}


enBTRCoreRet
BTRCore_UnPairDevice (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath,
    tBTRCoreDevHandle   handle
) {
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    /* We can enhance the BTRCore with passcode support later point in time */

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (0 == handle)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (0 == pstlhBTRCore->numOfPairedDevices) {
            printf("%s:%d - Possibly the list is not populated\n", __FUNCTION__, __LINE__);
            /* Keep the list upto date */
            btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath);
        }

        if (pstlhBTRCore->numOfPairedDevices) {
            const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
            if (pDeviceAddress) {
                if (0 != BtrCore_BTPerformDeviceOp (pstlhBTRCore->connHandle,
                                                    pAdapterPath,
                                                    pstlhBTRCore->agentPath,
                                                    pDeviceAddress,
                                                    enBTDevOpRemovePairedDev)) {
                    printf("%s:%d - Failed to unpair a device\n", __FUNCTION__, __LINE__);
                    rc = enBTRCorePairingFailed;
                }
                else {
                    rc = enBTRCoreSuccess;
                    printf("%s:%d - UnPairing Success\n", __FUNCTION__, __LINE__);

                    /* Keep the list upto date */
                    btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath);
                }
            }
            else {
                printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
                rc = enBTRCorePairingFailed;
            }
        }
        else {
            printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreFailure;
        }

    }
    return rc;
}


enBTRCoreRet
BTRCore_RegisterDiscoveryCallback (
    tBTRCoreHandle              hBTRCore, 
    BTRCore_DeviceDiscoveryCb   afptrBTRCoreDeviceDiscoveryCB
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
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
    BTRCore_StatusCb   afptrBTRCoreStatusCB
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!pstlhBTRCore->fptrBTRCoreStatusCB) {
        pstlhBTRCore->fptrBTRCoreStatusCB = afptrBTRCoreStatusCB;
        printf("%s:%d - BT Status Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }
    else {
        printf("%s:%d - BT Status Callback Already Registered - Not Registering current CB\n", __FUNCTION__, __LINE__);
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet BTRCore_GetSupportedServices (tBTRCoreHandle hBTRCore, tBTRCoreDevHandle handle, stBTRCoreSupportedServiceList *pProfileList)
{
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pProfileList) || (0 == handle)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        stBTDeviceSupportedServiceList profileList;
        /* Initialize the array */
        memset (pProfileList, 0 , sizeof(stBTRCoreSupportedServiceList));
        memset (&profileList, 0 , sizeof(stBTDeviceSupportedServiceList));

        const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
        if (pDeviceAddress)
        {
            if (0 == BtrCore_BTDiscoverDeviceServices (pstlhBTRCore->connHandle, pDeviceAddress, &profileList)) {
                int i = 0;
                printf ("%s:%d - Successfully received the supported services... \n", __FUNCTION__, __LINE__);

                pProfileList->numberOfService = profileList.numberOfService;
                for (i = 0; i < profileList.numberOfService; i++)
                {
                    pProfileList->profile[i].uuid_value = profileList.profile[i].uuid_value;
                    strncpy (pProfileList->profile[i].profile_name,  profileList.profile[i].profile_name, 30);
                }
                rc = enBTRCoreSuccess;
            }
            else
                printf("%s:%d - Failed to Get the Services\n", __FUNCTION__, __LINE__);
        }
        else
            printf("%s:%d - This device is not paired to Get the Services\n", __FUNCTION__, __LINE__);
    }

    return rc;
}

enBTRCoreRet
BTRCore_ConnectDevice (
    tBTRCoreHandle      hBTRCore, 
    const char*         pAdapterPath,
    tBTRCoreDevHandle   handle, 
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (0 == handle)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (0 == pstlhBTRCore->numOfPairedDevices) {
            printf("%s:%d - Possibly the list is not populated; like booted and connecting\n", __FUNCTION__, __LINE__);
            /* Keep the list upto date */
            btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath);
        }

        if (pstlhBTRCore->numOfPairedDevices) {
            const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
            const char *pDeviceName =  btrCore_GetKnownDeviceName(pstlhBTRCore, handle);
            if (pDeviceAddress) {
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
                if (0 == BtrCore_BTConnectDevice(pstlhBTRCore->connHandle, pDeviceAddress, lenBTDeviceType)) {
                    rc = enBTRCoreSuccess;
                    printf("%s:%d - Connected to device %s Successfully. Lets start Play the audio\n", __FUNCTION__, __LINE__,pDeviceName);
                }
                else
                    printf("%s:%d - Connect to device failed\n", __FUNCTION__, __LINE__);
            }
            else {
                printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
            }
        }
        else {
            printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_DisconnectDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevHandle   handle, 
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (0 == handle) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (pstlhBTRCore->numOfPairedDevices) {
            const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
            if (pDeviceAddress) {
                /* TODO */
                /* Stop the audio playback before disconnect */

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

                if (0 == BtrCore_BTDisconnectDevice(pstlhBTRCore->connHandle, pDeviceAddress, lenBTDeviceType)) {
                    rc = enBTRCoreSuccess;
                    printf("%s:%d - DisConnected from device Successfully.\n", __FUNCTION__, __LINE__);
                }
                else
                    printf("%s:%d - DisConnect from device failed\n", __FUNCTION__, __LINE__);
            }
            else {
                printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
            }
        }
        else {
            printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}


enBTRCoreRet
BTRCore_GetDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    const char*         pAdapterPath, 
    tBTRCoreDevHandle   handle,
    int*                pDeviceFD,
    int*                pDeviceReadMTU,
    int*                pDeviceWriteMTU
) {
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    int liDataPath = 0;
    int lidataReadMTU = 0;
    int lidataWriteMTU = 0;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (0 == handle) || (!pDeviceFD) || (!pDeviceReadMTU) || (!pDeviceWriteMTU)) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (0 == pstlhBTRCore->numOfPairedDevices) {
            printf("%s:%d - Possibly the list is not populated; like booted and connecting\n", __FUNCTION__, __LINE__);
            /* Keep the list upto date */
            btrCore_PopulateListOfPairedDevices(pstlhBTRCore, pAdapterPath);
        }

        if (pstlhBTRCore->numOfPairedDevices) {
            const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
            if (pDeviceAddress) {
                if(enBTRCoreSuccess != BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->connHandle, pDeviceAddress, &liDataPath, &lidataReadMTU, &lidataWriteMTU)) {
                    BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
                    rc = enBTRCoreFailure;
                }
                else {
                    *pDeviceFD = liDataPath;
                    *pDeviceReadMTU = lidataReadMTU;
                    *pDeviceWriteMTU = lidataWriteMTU;
                    rc = enBTRCoreSuccess;
                }
            }
            else {
                printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
            }
        }
        else {
            printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
        }
    }

    return rc;
}


enBTRCoreRet
BTRCore_FreeDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevHandle   handle
) {
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore) {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (0 == handle) {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else {
        if (pstlhBTRCore->numOfPairedDevices) {
            const char *pDeviceAddress = btrCore_GetKnownDeviceAddress(pstlhBTRCore, handle);
            if (pDeviceAddress) {
                if(enBTRCoreSuccess != BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->connHandle, pDeviceAddress)) {
                    BTRCore_LOG("AVMedia_ReleaseDataPath ERROR occurred\n");
                }
                else {
                    BTRCore_LOG("AVMedia_ReleaseDataPath Success\n");
                    rc = enBTRCoreSuccess;
                }
            }
            else
                BTRCore_LOG("Given device is Not found\n");
        }
        else
            BTRCore_LOG("No device is found\n");
    }

    return rc;
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
                tBTRCoreDevHandle   lBTRCoreDevHandle = 0;
                stBTRCoreHdl*       lpstlhBTRCore = (stBTRCoreHdl*)apUserData;

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

                // TODO: Think of a way to move this to taskThread
                lBTRCoreDevHandle = btrCore_GenerateUniqueHandle(apstBTDeviceInfo->pcAddress);
                if (btrCore_GetScannedDeviceAddress(apUserData, lBTRCoreDevHandle) != NULL) {
                    printf ("Already we have a entry in the list; Skip Parsing now \n");
                }
                else {
                    lpstlhBTRCore->stFoundDevice.found  = FALSE;
                    lpstlhBTRCore->stFoundDevice.RSSI   = apstBTDeviceInfo->i32RSSI;
                    lpstlhBTRCore->stFoundDevice.vendor_id = apstBTDeviceInfo->ui16Vendor;
                    strcpy(lpstlhBTRCore->stFoundDevice.device_name, apstBTDeviceInfo->pcName);
                    strcpy(lpstlhBTRCore->stFoundDevice.device_address, apstBTDeviceInfo->pcAddress);
                    btrCore_SetScannedDeviceInfo(lpstlhBTRCore);
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
         if (lpstlhBTRCore->fptrBTRCoreStatusCB != NULL) {
             strcpy(lpstlhBTRCore->stDevStateCbInfo.cDevicePrevState,apstBTDeviceInfo->pcDevicePrevState );
             strcpy(lpstlhBTRCore->stDevStateCbInfo.cDeviceCurrState,apstBTDeviceInfo->pcDeviceCurrState );
             lpstlhBTRCore->fptrBTRCoreStatusCB(&lpstlhBTRCore->stDevStateCbInfo);
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

/* End of File */
