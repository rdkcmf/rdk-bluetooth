/* Bluetooth Header file - June 04, 2015*/
/* Make header file for the various functions... include structures*/
/* commit 6aca47c658cf75cad0192a824915dabf82d3200f*/
#ifndef _BTR_H_
#define _BTR_H_

#define BT_LOG(...) printf(__VA_ARGS__)

/* BT Init */
typedef enum 
{
    NO_ERROR, 
    ERROR1
} BT_error;

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

typedef enum 
{
    FALSE,
    TRUE
} BOOLEAN;

typedef enum
{
    eBtrAudioSink,
    eBtrHeadSet
} eBtrDeviceType;

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
typedef char BD_NAME[BD_NAME_LEN + 1];         /* Device name */
typedef char *BD_NAME_PTR;                 /* Pointer to Device name */

#define UUID_LEN 63
typedef char UUID[UUID_LEN+1];
      
/*BT getAdapters*/
typedef struct
{
    U8  number_of_adapters;
} tGetAdapters;

/*BT AdvertiseService*/
typedef struct
{
    U16     socket;
    BD_NAME service_name;
    UUID    uuid;
    U8      class;
    BD_NAME provider;
    BD_NAME description;
} tAdvertiseService;

/*Abort Discovery structure, may be needed for some BT stacks*/
typedef struct
{
  int dummy;
} tAbortDiscovery;

typedef struct
{
   BD_ADDR  bd_address;
   BD_NAME  service_name;
   UUID     uuid;
} tFilterMode;

typedef struct
{
   char device_type[64];
   char device_state[64];
} tStatusCB;
void (*p_Status_callback) ();

/*Radio params*/

typedef struct
{
    U8  first_disabled_channel;
    U8  last_disabled_channel;
    U32 page_scan_interval;
    U32 page_scan_window;
    U32 inquiry_scan_interval;
    U32 inquiry_scan_window;
    U8  tx_power;
} tRadioParams;

/*BT getAdapter*/
typedef struct
{
    U8      adapter_number;
    BOOLEAN enable;
    BOOLEAN discoverable;
    BOOLEAN connectable;
    BOOLEAN first_available; /*search for first available BT adapater*/
    BD_ADDR bd_address;
    U16     sock;
    CLASS_OF_DEVICE class_of_device;
    BD_NAME device_name;
    tRadioParams RadioParams;
    U32 DiscoverableTimeout;
    int (*p_callback) ();//delete this later TODO
} tGetAdapter;

/*BT find service*/
typedef struct
{
    U8          adapter_number;
    tFilterMode filter_mode;
    int (*p_callback) ();
} tFindService;

/*startdiscovery*/
typedef struct
{
    U8  adapter_number;
    U32 duration;
    U8  max_devices;
    BOOLEAN lookup_names;
    U32 flags;
    U16 sock;
    int (*p_callback) ();
} tStartDiscovery;

typedef struct
{
   BD_NAME bd_address;
   BD_NAME device_name;
   int RSSI;
   int found;
} tScannedDevices;

typedef struct
{
   BD_NAME bd_path;
   BD_NAME device_name;
   int found;
} tKnownDevices;


/* Generic call to init any needed stack.. may be called during powerup*/
BT_error BT_Init(void);

/*BT_GetAdapters  call to determine the number of BT radio interfaces... typically one, but could be more*/
BT_error BT_GetAdapters(tGetAdapters *p_get_adapters);

/*BT_GetAdapter get info about a specific adatper*/
BT_error BT_GetAdapter(tGetAdapter *p_get_adapter);

/* BT_EnableAdapter enable specific adapter*/
BT_error BT_EnableAdapter(tGetAdapter *p_get_adapter);

/* BT_DisableAdapter disable specific adapter*/
BT_error BT_DisableAdapter(tGetAdapter *p_get_adapter);


/* BT_SetDiscoverable set adapter as discoverable or not discoverable*/
BT_error BT_SetDiscoverable(tGetAdapter *p_get_adapter);


/* BT_SetDiscoverableTimeout set how long the adapter is discoverable*/
BT_error BT_SetDiscoverableTimeout(tGetAdapter *p_get_adapter);

/* BT_SetDeviceName set friendly name of BT adapter*/
BT_error BT_SetDeviceName(tGetAdapter *p_get_adapter);

/* BT_ResetAdapter reset specific adapter*/
BT_error BT_ResetAdapter(tGetAdapter *p_get_adapter);

/*BT_ConfigureAdapter... set a particular attribute for the adapter*/
BT_error BT_ConfigureAdapter(tGetAdapter *p_get_adapter);

/* BT_StartDiscovery - start the discovery process*/
BT_error BT_StartDiscovery(tStartDiscovery *p_start_discovery);

/* BT_AbortDiscovery - aborts the discovery process*/
BT_error BT_AbortDiscovery(tAbortDiscovery *p_abort_discovery);

/*BT_DiscoverServices - finds a service amongst discovered devices*/
BT_error BT_DiscoverServices(tFindService *p_find_service);

/*BT_AdvertiseService - Advertise Service on local SDP server*/
BT_error BT_AdvertiseService(tAdvertiseService *p_advertise_service);

/*BT_PairDevice*/
BT_error BT_PairDevice(tScannedDevices *p_scanned_device);

/*BT_FindDevice*/
BT_error BT_FindDevice(tScannedDevices *p_scanned_device);

/*BT_ConnectDevice*/
BT_error BT_ConnectDevice(tKnownDevices *p_known_device, eBtrDeviceType e_device_type);

/*BT_ConnectDevice*/
BT_error BT_DisconnectDevice(tKnownDevices *p_known_device, eBtrDeviceType e_device_type);

/*BT_ForgetDevice*/
BT_error BT_ForgetDevice(tKnownDevices *p_known_device);

/*BT_ListKnownDevices*/
BT_error BT_ListKnownDevices(tGetAdapter *p_get_adapter); /*- list previously Paired Devices*/

/*BT_FindService - confirm if a given service exists on a device*/
BT_error BT_FindService (tKnownDevices* p_known_device,const char * UUID,char * XMLdata, int * found);


/*BT_RegisterStatusCallback - callback for unsolicited status changes*/
BT_error BT_RegisterStatusCallback(void * cb);
#endif // _BTR_H_
