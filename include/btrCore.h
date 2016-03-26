/* Bluetooth Header file - June 04, 2015*/
/* Make header file for the various functions... include structures*/
/* commit 6aca47c658cf75cad0192a824915dabf82d3200f*/
#ifndef __BTR_CORE_H__
#define __BTR_CORE_H__

#include "btrCoreTypes.h"

#define BTRCore_LOG(...) printf(__VA_ARGS__)


#define BTRCORE_MAX_NUM_BT_ADAPTERS 4   // TODO:Better to make this configurable at runtime
#define BTRCORE_MAX_NUM_BT_DEVICES  32  // TODO:Better to make this configurable at runtime
#define BTRCORE_STRINGS_MAX_LEN     32


typedef enum _enBTRCoreDeviceType {
    enBTRCoreSpeakers,
    enBTRCoreHeadSet,
    enBTRCoreMobileAudioIn,
    enBTRCorePCAudioIn,
    enBTRCoreUnknown
} enBTRCoreDeviceType;

/*platform specific data lengths */
typedef unsigned char   U8;
typedef unsigned short  U16;
typedef unsigned int    U32;
   
/* bd addr length and type */
#ifndef BD_ADDR_LEN
#define BD_ADDR_LEN     6
typedef U8 BD_ADDR[BD_ADDR_LEN];
#endif


#define BD_NAME_LEN     248
typedef char BD_NAME[BD_NAME_LEN + 1];     /* Device name */
typedef char *BD_NAME_PTR;                 /* Pointer to Device name */

#define UUID_LEN 63
typedef char UUID[UUID_LEN+1];
      
/*BT getAdapters*/
typedef struct _stBTRCoreGetAdapters {
    U8  number_of_adapters;
} stBTRCoreGetAdapters;

/*BT AdvertiseService*/
typedef struct _stBTRCoreAdvertiseService {
    BD_NAME service_name;
    UUID    uuid;
    U8      class;
    BD_NAME provider;
    BD_NAME description;
} stBTRCoreAdvertiseService;

/*Abort Discovery structure, may be needed for some BT stacks*/
typedef struct _stBTRCoreAbortDiscovery {
  int dummy;
} stBTRCoreAbortDiscovery;

typedef struct _stBTRCoreFilterMode {
   BD_ADDR  bd_address;
   BD_NAME  service_name;
   UUID     uuid;
} stBTRCoreFilterMode;

typedef struct _stBTRCoreDevStateCB {
   char cDeviceType[BTRCORE_STRINGS_MAX_LEN];
   char cDevicePrevState[BTRCORE_STRINGS_MAX_LEN];
   char cDeviceCurrState[BTRCORE_STRINGS_MAX_LEN];
} stBTRCoreDevStateCB;

void (*p_Status_callback) ();

/*BT getAdapter*/
typedef struct _stBTRCoreGetAdapter {
    U8      adapter_number;
    char*   pcAdapterPath;
    char*   pcAdapterDevName;
    BOOLEAN enable;
    BOOLEAN discoverable;
    BOOLEAN connectable;
    BOOLEAN first_available; /*search for first available BT adapater*/
    U32 DiscoverableTimeout;
    int (*p_callback) ();//delete this later TODO
} stBTRCoreGetAdapter;

/*BT find service*/
typedef struct _stBTRCoreFindService {
    U8          adapter_number;
    stBTRCoreFilterMode filter_mode;
    int (*p_callback) ();
} stBTRCoreFindService;

/*startdiscovery*/
typedef struct _stBTRCoreStartDiscovery {
    U8  adapter_number;
    U32 duration;
    U8  max_devices;
    BOOLEAN lookup_names;
    U32 flags;
    int (*p_callback) ();
} stBTRCoreStartDiscovery;

typedef struct _stBTRCoreScannedDevices {
   BD_NAME bd_address;
   BD_NAME device_name;
   int RSSI;
   BOOLEAN found;
} stBTRCoreScannedDevices;

typedef struct _stBTRCoreKnownDevice {
   BD_NAME bd_path;
   BD_NAME device_name;
   BOOLEAN found;
} stBTRCoreKnownDevice;


/* Generic call to init any needed stack.. may be called during powerup*/
enBTRCoreRet BTRCore_Init(tBTRCoreHandle* hBTRCore);


/* Deinitialze and free BTRCore */
enBTRCoreRet BTRCore_DeInit(tBTRCoreHandle hBTRCore);

/*BTRCore_GetAdapters  call to determine the number of BT radio interfaces... typically one, but could be more*/
enBTRCoreRet BTRCore_GetAdapters(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapters* pstGetAdapters);

/*BTRCore_GetAdapter get info about a specific adatper*/
enBTRCoreRet BTRCore_GetAdapter(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/*BTRCore_SetAdapter Set Current Bluetotth Adapter to use*/
enBTRCoreRet BTRCore_SetAdapter(tBTRCoreHandle hBTRCore, int adapter_number);

/* BTRCore_EnableAdapter enable specific adapter*/
enBTRCoreRet BTRCore_EnableAdapter(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_DisableAdapter disable specific adapter*/
enBTRCoreRet BTRCore_DisableAdapter(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetDiscoverable set adapter as discoverable or not discoverable*/
enBTRCoreRet BTRCore_SetDiscoverable(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetDiscoverableTimeout set how long the adapter is discoverable*/
enBTRCoreRet BTRCore_SetDiscoverableTimeout(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetAdapterDeviceName set friendly name of BT adapter*/
enBTRCoreRet BTRCore_SetAdapterDeviceName(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter, char* apcAdapterDeviceName);

/* BTRCore_ResetAdapter reset specific adapter*/
enBTRCoreRet BTRCore_ResetAdapter(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/*BTRCore_ConfigureAdapter... set a particular attribute for the adapter*/
enBTRCoreRet BTRCore_ConfigureAdapter(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_StartDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDiscovery(tBTRCoreHandle hBTRCore, stBTRCoreStartDiscovery* pstStartDiscovery);

/* BTRCore_AbortDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_AbortDiscovery(tBTRCoreHandle hBTRCore, stBTRCoreAbortDiscovery* pstAbortDiscovery);

/*BTRCore_DiscoverServices - finds a service amongst discovered devices*/
enBTRCoreRet BTRCore_DiscoverServices(tBTRCoreHandle hBTRCore, stBTRCoreFindService* pstFindService);

/*BTRCore_AdvertiseService - Advertise Service on local SDP server*/
enBTRCoreRet BTRCore_AdvertiseService(tBTRCoreHandle hBTRCore, stBTRCoreAdvertiseService* pstAdvertiseService);

/*BTRCore_ShowFoundDevices - Utility function to display Devices found on a Bluetooth Adapter */
enBTRCoreRet BTRCore_ShowFoundDevices(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter);

/*BTRCore_PairDevice*/
enBTRCoreRet BTRCore_PairDevice(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/*BTRCore_FindDevice*/
enBTRCoreRet BTRCore_FindDevice(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/*BTRCore_ConnectDevice*/
enBTRCoreRet BTRCore_ConnectDevice(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/*BTRCore_DisconnectDevice*/
enBTRCoreRet BTRCore_DisconnectDevice(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/*BTRCore_AcquireDeviceDataPath*/
enBTRCoreRet BTRCore_AcquireDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, int* aiDataPath,
                                            int* aidataReadMTU, int* aidataWriteMTU); //TODO: Change to a unique device Identifier

/*BTRCore_ReleaseDeviceDataPath*/
enBTRCoreRet BTRCore_ReleaseDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/*BTRCore_ForgetDevice*/
enBTRCoreRet BTRCore_ForgetDevice(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/*BTRCore_ListKnownDevices*/
enBTRCoreRet BTRCore_ListKnownDevices(tBTRCoreHandle hBTRCore, stBTRCoreGetAdapter* pstGetAdapter); /*- list previously Paired Devices*/

/*BTRCore_FindService - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_FindService (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* UUID, char* XMLdata, int* found); //TODO: Change to a unique device Identifier

/*BTRCore_RegisterStatusCallback - callback for unsolicited status changes*/
enBTRCoreRet BTRCore_RegisterStatusCallback(tBTRCoreHandle hBTRCore, void * cb);


#endif // __BTR_CORE_H__
