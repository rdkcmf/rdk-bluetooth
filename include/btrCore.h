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
#define BTRCORE_MAX_DEVICE_PROFILE  32


typedef enum _enBTRCoreDeviceType {
    enBTRCoreSpeakers,
    enBTRCoreHeadSet,
    enBTRCoreMobileAudioIn,
    enBTRCorePCAudioIn,
    enBTRCoreUnknown
} enBTRCoreDeviceType;

typedef enum _enBTRCoreDeviceClass {
    enBTRCoreAV_WearableHeadset  = 0x04,
    enBTRCoreAV_Handsfree        = 0x08,
    enBTRCoreAV_Reserved         = 0x0C,
    enBTRCoreAV_Microphone       = 0x10,
    enBTRCoreAV_Loudspeaker      = 0x14,
    enBTRCoreAV_Headphones       = 0x18,
    enBTRCoreAV_PortableAudio    = 0x1C,
    enBTRCoreAV_CarAudio         = 0x20,
    enBTRCoreAV_STB              = 0x24,
    enBTRCoreAV_HIFIAudioDevice  = 0x28,
    enBTRCoreAV_VCR              = 0x2C,
    enBTRCoreAV_VideoCamera      = 0x30,
    enBTRCoreAV_Camcoder         = 0x34,
    enBTRCoreAV_VideoMonitor     = 0x38,
    enBTRCoreAV_TV               = 0x3C,
    enBTRCoreAV_VideoConference   = 0x40,
    enBTRCoreAV_Unknown          = 0x00,
} enBTRCoreDeviceClass;

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

typedef struct _stBTRCoreListAdapters {
   U8 number_of_adapters;
   BD_NAME adapter_path[BTRCORE_MAX_NUM_BT_ADAPTERS];
} stBTRCoreListAdapters;

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

/*BT Adapter*/
typedef struct _stBTRCoreAdapter {
    U8      adapter_number;
    char*   pcAdapterPath;
    char*   pcAdapterDevName;
    BOOLEAN enable;
    BOOLEAN discoverable;
    BOOLEAN bFirstAvailable; /*search for first available BT adapater*/
    U32 DiscoverableTimeout;
} stBTRCoreAdapter;

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
   tBTRCoreDevHandle device_handle;
   BD_NAME device_name;
   BD_NAME device_address;
   enBTRCoreDeviceType device_type;
   int RSSI;
   unsigned int vendor_id;
   BOOLEAN found;
} stBTRCoreScannedDevices;

typedef struct _stBTRCoreKnownDevice {
   tBTRCoreDevHandle device_handle;
   BD_NAME device_name;
   BD_NAME device_address;
   enBTRCoreDeviceType device_type;
   BD_NAME bd_path;
   unsigned int vendor_id;
   int RSSI;
   BOOLEAN found;
   BOOLEAN device_connected;
} stBTRCoreKnownDevice;

typedef struct _stBTRCoreScannedDevicesCount
{
    int numberOfDevices;
    stBTRCoreScannedDevices devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCoreScannedDevicesCount;

typedef struct _stBTRCorePairedDevicesCount
{
    int numberOfDevices;
    stBTRCoreKnownDevice devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCorePairedDevicesCount;

typedef struct _stBTRCoreSupportedService
{
    unsigned int uuid_value;
    BD_NAME profile_name;
} stBTRCoreSupportedService;

typedef struct _stBTRCoreSupportedServiceList
{
    int numberOfService;
    stBTRCoreSupportedService profile[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreSupportedServiceList;

typedef void (*BTRCore_DeviceDiscoveryCb) (stBTRCoreScannedDevicesCount astBTRCoreScannedDevicesCount);
typedef void (*BTRCore_StatusCb) (stBTRCoreDevStateCB* apstDevStateCbInfo);


/*
 * Interfaces
 */
// TODO: Reduce the number of interfaces exposed to the outside world
// TODO: Combine interfaces which perform the same functionality

/* Generic call to init any needed stack.. may be called during powerup*/
enBTRCoreRet BTRCore_Init (tBTRCoreHandle* hBTRCore);

/* Deinitialze and free BTRCore */
enBTRCoreRet BTRCore_DeInit (tBTRCoreHandle hBTRCore);

//Register Agent to accept connection requests
enBTRCoreRet BTRCore_RegisterAgent (tBTRCoreHandle hBTRCore, int iBTRCapMode);

//unregister agent to support pairing initiated from settop
enBTRCoreRet BTRCore_UnregisterAgent (tBTRCoreHandle hBTRCore);

/* BTRCore_GetListOfAdapters call to determine the number of BT radio interfaces... typically one, but could be more*/
enBTRCoreRet BTRCore_GetListOfAdapters (tBTRCoreHandle hBTRCore, stBTRCoreListAdapters* pstListAdapters);

/* BTRCore_SetAdapterPower call to set BT radio power to ON or OFF ... */
enBTRCoreRet BTRCore_SetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char powerStatus);

/* BTRCore_GetAdapterPower call to gets BT radio power status as ON or OFF ... */
enBTRCoreRet BTRCore_GetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pAdapterPower);

/*BTRCore_GetAdapters  call to determine the number of BT radio interfaces... typically one, but could be more*/
enBTRCoreRet BTRCore_GetAdapters (tBTRCoreHandle hBTRCore, stBTRCoreGetAdapters* pstGetAdapters);

/*BTRCore_GetAdapter get info about a specific adatper*/
enBTRCoreRet BTRCore_GetAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/*BTRCore_SetAdapter Set Current Bluetotth Adapter to use*/
enBTRCoreRet BTRCore_SetAdapter (tBTRCoreHandle hBTRCore, int adapter_number);

/* BTRCore_EnableAdapter enable specific adapter*/
enBTRCoreRet BTRCore_EnableAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_DisableAdapter disable specific adapter*/
enBTRCoreRet BTRCore_DisableAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_SetDiscoverable set adapter as discoverable or not discoverable*/
enBTRCoreRet BTRCore_SetDiscoverable (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_SetAdapterDiscoverable set adapter as discoverable or not discoverable*/
enBTRCoreRet BTRCore_SetAdapterDiscoverable (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char discoverable);

/* BTRCore_SetDiscoverableTimeout set how long the adapter is discoverable*/
enBTRCoreRet BTRCore_SetDiscoverableTimeout (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_SetAdapterDiscoverableTimeout set how long the adapter is discoverable*/
enBTRCoreRet BTRCore_SetAdapterDiscoverableTimeout (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned short timeout);

/* BTRCore_GetAdapterDiscoverableStatus checks whether the discovery is in progres or not */
enBTRCoreRet BTRCore_GetAdapterDiscoverableStatus (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pDiscoverable);

/* BTRCore_SetAdapterDeviceName set friendly name of BT adapter*/
enBTRCoreRet BTRCore_SetAdapterDeviceName (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter, char* apcAdapterDeviceName);

/* BTRCore_SetAdapterName sets friendly name of BT adapter*/
enBTRCoreRet BTRCore_SetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pAdapterName);

/* BTRCore_GetAdapterName gets friendly name of BT adapter*/
enBTRCoreRet BTRCore_GetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, char* pAdapterName);

/* BTRCore_ResetAdapter reset specific adapter*/
enBTRCoreRet BTRCore_ResetAdapter(tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/*BTRCore_ConfigureAdapter... set a particular attribute for the adapter*/
enBTRCoreRet BTRCore_ConfigureAdapter(tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_StartDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDiscovery (tBTRCoreHandle hBTRCore, stBTRCoreStartDiscovery* pstStartDiscovery);

/* BTRCore_StartDeviceDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath);

/* BTRCore_AbortDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_AbortDiscovery(tBTRCoreHandle hBTRCore, stBTRCoreAbortDiscovery* pstAbortDiscovery);

/* BTRCore_StopDeviceDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_StopDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath);

/* BTRCore_GetListOfScannedDevices - gets the discovered devices list */
enBTRCoreRet BTRCore_GetListOfScannedDevices (tBTRCoreHandle hBTRCore, const char* pAdapterPath, stBTRCoreScannedDevicesCount *pListOfScannedDevices);

/*BTRCore_DiscoverServices - finds a service amongst discovered devices*/
enBTRCoreRet BTRCore_DiscoverServices (tBTRCoreHandle hBTRCore, stBTRCoreFindService* pstFindService);

/*BTRCore_AdvertiseService - Advertise Service on local SDP server*/
enBTRCoreRet BTRCore_AdvertiseService (tBTRCoreHandle hBTRCore, stBTRCoreAdvertiseService* pstAdvertiseService);

/*BTRCore_ShowFoundDevices - Utility function to display Devices found on a Bluetooth Adapter */
enBTRCoreRet BTRCore_ShowFoundDevices (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/* BTRCore_GetSupportedServices - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_GetSupportedServices (tBTRCoreHandle hBTRCore, tBTRCoreDevHandle handle, stBTRCoreSupportedServiceList *pProfileList);

/*BTRCore_PairDeviceByIndex */
enBTRCoreRet BTRCore_PairDeviceByIndex (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/* BTRCore_PairDevice*/
enBTRCoreRet BTRCore_PairDevice (tBTRCoreHandle hBTRCore, const char* pAdapterPath, tBTRCoreDevHandle handle);

/*BTRCore_ForgetDevice*/
enBTRCoreRet BTRCore_ForgetDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/* BTRCore_UnPairDevice is similar to BTRCore_ForgetDevice */
enBTRCoreRet BTRCore_UnPairDevice (tBTRCoreHandle hBTRCore, const char* pAdapterPath, tBTRCoreDevHandle handle);

/* BTRCore_GetListOfPairedDevices - gets the paired devices list */
enBTRCoreRet BTRCore_GetListOfPairedDevices (tBTRCoreHandle hBTRCore, const char* pAdapterPath, stBTRCorePairedDevicesCount *pListOfDevices);

/*BTRCore_ListKnownDevices */
enBTRCoreRet BTRCore_ListKnownDevices (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter); /*- list previously Paired Devices*/

/*BTRCore_FindDevice*/
enBTRCoreRet BTRCore_FindDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/*BTRCore_FindServiceByIndex - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_FindServiceByIndex (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* UUID, char* XMLdata, int* found); //TODO: Change to a unique device Identifier

/*BTRCore_ConnectDeviceByIndex*/
enBTRCoreRet BTRCore_ConnectDeviceByIndex(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/* BTRCore_ConnectDevice */
enBTRCoreRet BTRCore_ConnectDevice (tBTRCoreHandle hBTRCore, const char* pAdapterPath, tBTRCoreDevHandle handle, enBTRCoreDeviceType aenBTRCoreDevType);

/*BTRCore_DisconnectDeviceByIndex*/
enBTRCoreRet BTRCore_DisconnectDeviceByIndex (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/* BTRCore_DisconnectDevice */
enBTRCoreRet BTRCore_DisconnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevHandle handle, enBTRCoreDeviceType aenBTRCoreDevType);

/*BTRCore_AcquireDeviceDataPath*/
enBTRCoreRet BTRCore_AcquireDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, int* aiDataPath,
                                            int* aidataReadMTU, int* aidataWriteMTU); //TODO: Change to a unique device Identifier

/* BTRCore_GetDeviceDataPath */
enBTRCoreRet BTRCore_GetDeviceDataPath (tBTRCoreHandle hBTRCore, const char* pAdapterPath, tBTRCoreDevHandle handle, int* pDeviceFD, int* pDeviceReadMTU, int* pDeviceWriteMTU);

/*BTRCore_ReleaseDeviceDataPath*/
enBTRCoreRet BTRCore_ReleaseDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/* BTRCore_FreeDeviceDataPath */
enBTRCoreRet BTRCore_FreeDeviceDataPath (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/* Callback to notify the application every time when a new device is found and added to discovery list */
enBTRCoreRet BTRCore_RegisterDiscoveryCallback (tBTRCoreHandle  hBTRCore, BTRCore_DeviceDiscoveryCb afptrBTRCoreDeviceDiscoveryCB);

/*BTRCore_RegisterStatusCallback - callback for unsolicited status changes*/
enBTRCoreRet BTRCore_RegisterStatusCallback (tBTRCoreHandle hBTRCore, BTRCore_StatusCb afptrBTRCoreStatusCB);

/*BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device*/
enBTRCoreRet BTRCore_RegisterConnectionAuthenticationCallback (tBTRCoreHandle hBTRCore, void * cb);

#endif // __BTR_CORE_H__
