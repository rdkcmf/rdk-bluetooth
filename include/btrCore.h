/* Bluetooth Header file - June 04, 2015*/
/* Make header file for the various functions... include structures*/
/* commit 6aca47c658cf75cad0192a824915dabf82d3200f*/
#ifndef __BTR_CORE_H__
#define __BTR_CORE_H__

#include "btrCoreTypes.h"

#define BTRCore_LOG(...) printf(__VA_ARGS__)


#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif


#define BTRCORE_MAX_NUM_BT_DEVICES  32  // TODO:Better to make this configurable at runtime


typedef enum _BOOLEAN {
    FALSE,
    TRUE
} BOOLEAN;

typedef enum _enBTRCoreDeviceType {
    enBTRCoreAudioSink,
    enBTRCoreHeadSet
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

#define COD_LEN     3
typedef U8 CLASS_OF_DEVICE[COD_LEN];

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
    U16     socket;
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

typedef struct _stBTRCoreDevStatusCB {
   char device_type[64];
   char device_state[64];
} stBTRCoreDevStatusCB;

void (*p_Status_callback) ();

/*Radio params*/
typedef struct _stBTRCoreRadioParams {
    U8  first_disabled_channel;
    U8  last_disabled_channel;
    U32 page_scan_interval;
    U32 page_scan_window;
    U32 inquiry_scan_interval;
    U32 inquiry_scan_window;
    U8  tx_power;
} stBTRCoreRadioParams;

/*BT getAdapter*/
typedef struct _stBTRCoreGetAdapter {
    U8      adapter_number;
    char*   adapter_path;
    BOOLEAN enable;
    BOOLEAN discoverable;
    BOOLEAN connectable;
    BOOLEAN first_available; /*search for first available BT adapater*/
    BD_ADDR bd_address;
    U16     sock;
    CLASS_OF_DEVICE class_of_device;
    BD_NAME device_name;
    stBTRCoreRadioParams RadioParams;
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
    U16 sock;
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
enBTRCoreRet BTRCore_Init(void);

/* Deinitialze and free BTRCore */
enBTRCoreRet BTRCore_DeInit(void);

/*BTRCore_GetAdapters  call to determine the number of BT radio interfaces... typically one, but could be more*/
enBTRCoreRet BTRCore_GetAdapters(stBTRCoreGetAdapters* pstGetAdapters);

/*BTRCore_GetAdapter get info about a specific adatper*/
enBTRCoreRet BTRCore_GetAdapter(stBTRCoreGetAdapter* pstGetAdapter);

/*BTRCore_SetAdapter Set Current Bluetotth Adapter to use*/
enBTRCoreRet BTRCore_SetAdapter(int adapter_number);

/* BTRCore_EnableAdapter enable specific adapter*/
enBTRCoreRet BTRCore_EnableAdapter(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_DisableAdapter disable specific adapter*/
enBTRCoreRet BTRCore_DisableAdapter(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetDiscoverable set adapter as discoverable or not discoverable*/
enBTRCoreRet BTRCore_SetDiscoverable(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetDiscoverableTimeout set how long the adapter is discoverable*/
enBTRCoreRet BTRCore_SetDiscoverableTimeout(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_SetDeviceName set friendly name of BT adapter*/
enBTRCoreRet BTRCore_SetDeviceName(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_ResetAdapter reset specific adapter*/
enBTRCoreRet BTRCore_ResetAdapter(stBTRCoreGetAdapter* pstGetAdapter);

/*BTRCore_ConfigureAdapter... set a particular attribute for the adapter*/
enBTRCoreRet BTRCore_ConfigureAdapter(stBTRCoreGetAdapter* pstGetAdapter);

/* BTRCore_StartDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDiscovery(stBTRCoreStartDiscovery* pstStartDiscovery);

/* BTRCore_AbortDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_AbortDiscovery(stBTRCoreAbortDiscovery* pstAbortDiscovery);

/*BTRCore_DiscoverServices - finds a service amongst discovered devices*/
enBTRCoreRet BTRCore_DiscoverServices(stBTRCoreFindService* pstFindService);

/*BTRCore_AdvertiseService - Advertise Service on local SDP server*/
enBTRCoreRet BTRCore_AdvertiseService(stBTRCoreAdvertiseService* pstAdvertiseService);

/*BTRCore_PairDevice*/
enBTRCoreRet BTRCore_PairDevice(stBTRCoreScannedDevices* pstScannedDevice);

/*BTRCore_FindDevice*/
enBTRCoreRet BTRCore_FindDevice(stBTRCoreScannedDevices* pstScannedDevice);

/*BTRCore_ConnectDevice*/
enBTRCoreRet BTRCore_ConnectDevice(stBTRCoreKnownDevice* pstKnownDevice, enBTRCoreDeviceType enDeviceType);

/*BTRCore_ConnectDevice*/
enBTRCoreRet BTRCore_DisconnectDevice(stBTRCoreKnownDevice* pstKnownDevice, enBTRCoreDeviceType enDeviceType);

/*BTRCore_ForgetDevice*/
enBTRCoreRet BTRCore_ForgetDevice(stBTRCoreKnownDevice* pstKnownDevice);

/*BTRCore_ListKnownDevices*/
enBTRCoreRet BTRCore_ListKnownDevices(stBTRCoreGetAdapter* pstGetAdapter); /*- list previously Paired Devices*/

/*BTRCore_FindService - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_FindService (stBTRCoreKnownDevice* pstKnownDevice, const char* UUID, char* XMLdata, int* found);

/*BTRCore_RegisterStatusCallback - callback for unsolicited status changes*/
enBTRCoreRet BTRCore_RegisterStatusCallback(void * cb);


#endif // __BTR_CORE_H__
