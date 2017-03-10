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
/* Bluetooth Header file - June 04, 2015*/
/* Make header file for the various functions... include structures*/
/* commit 6aca47c658cf75cad0192a824915dabf82d3200f*/
#ifndef __BTR_CORE_H__
#define __BTR_CORE_H__

#include "btrCoreTypes.h"

#ifdef __cplusplus
extern "C" {
#endif


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
    enBTRCore_DC_WearableHeadset  = 0x04,
    enBTRCore_DC_Handsfree        = 0x08,
    enBTRCore_DC_Reserved         = 0x0C,
    enBTRCore_DC_Microphone       = 0x10,
    enBTRCore_DC_Loudspeaker      = 0x14,
    enBTRCore_DC_Headphones       = 0x18,
    enBTRCore_DC_PortableAudio    = 0x1C,
    enBTRCore_DC_CarAudio         = 0x20,
    enBTRCore_DC_STB              = 0x24,
    enBTRCore_DC_HIFIAudioDevice  = 0x28,
    enBTRCore_DC_VCR              = 0x2C,
    enBTRCore_DC_VideoCamera      = 0x30,
    enBTRCore_DC_Camcoder         = 0x34,
    enBTRCore_DC_VideoMonitor     = 0x38,
    enBTRCore_DC_TV               = 0x3C,
    enBTRCore_DC_VideoConference  = 0x40,
    enBTRCore_DC_Unknown          = 0x00,
} enBTRCoreDeviceClass;

typedef enum _enBTRCoreDeviceStatus {
    enBTRCore_DS_Inited,
    enBTRCore_DS_Connected,
    enBTRCore_DS_Disconnected,
    enBTRCore_DS_Playing
} enBTRCoreDeviceStatus;

typedef enum _eBTRCoreDevMediaType {
    eBTRCoreDevMediaTypePCM,
    eBTRCoreDevMediaTypeSBC,
    eBTRCoreDevMediaTypeMPEG,
    eBTRCoreDevMediaTypeAAC,
    eBTRCoreDevMediaTypeUnknown
} eBTRCoreDevMediaType;

typedef enum _eBTRCoreDevMediaAChan {
    eBTRCoreDevMediaAChanMono,
    eBTRCoreDevMediaAChanDualChannel,
    eBTRCoreDevMediaAChanStereo,
    eBTRCoreDevMediaAChanJointStereo,
    eBTRCoreDevMediaAChan5_1,
    eBTRCoreDevMediaAChan7_1,
    eBTRCoreDevMediaAChanUnknown
} eBTRCoreDevMediaAChan;

typedef enum _enBTRCoreMediaCtrl {
    enBTRCoreMediaPlay,
    enBTRCoreMediaPause,
    enBTRCoreMediaStop,
    enBTRCoreMediaNext,
    enBTRCoreMediaPrevious,
    enBTRCoreMediaFastForward,
    enBTRCoreMediaRewind,
    enBTRCoreMediaVolumeUp,
    enBTRCoreMediaVolumeDown
} enBTRCoreMediaCtrl;


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
   BD_NAME adapterAddr[BTRCORE_MAX_NUM_BT_ADAPTERS];
} stBTRCoreListAdapters;

typedef struct _stBTRCoreFilterMode {
   BD_ADDR  bd_address;
   BD_NAME  service_name;
   UUID     uuid;
} stBTRCoreFilterMode;

typedef struct _stBTRCoreDevStateCBInfo {
    enBTRCoreDeviceType     eDeviceType;
    enBTRCoreDeviceStatus   eDevicePrevState;
    enBTRCoreDeviceStatus   eDeviceCurrState;
} stBTRCoreDevStateCBInfo;

typedef struct _stBTRCoreConnCBInfo {
    unsigned int ui32devPassKey;
    char cConnAuthDeviceName[BTRCORE_STRINGS_MAX_LEN];
} stBTRCoreConnCBInfo;

typedef struct _stBTRCoreSupportedService {
    unsigned int uuid_value;
    BD_NAME profile_name;
} stBTRCoreSupportedService;

typedef struct _stBTRCoreSupportedServiceList {
    int numberOfService;
    stBTRCoreSupportedService profile[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreSupportedServiceList;

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
   tBTRCoreDevId deviceId;
   BD_NAME device_name;
   BD_NAME device_address;
   enBTRCoreDeviceClass device_type;
   stBTRCoreSupportedServiceList device_profile;
   int RSSI;
   unsigned int vendor_id;
   BOOLEAN found;
} stBTRCoreScannedDevices;

typedef struct _stBTRCoreKnownDevice {
   tBTRCoreDevId deviceId;
   BD_NAME device_name;
   BD_NAME device_address;
   enBTRCoreDeviceClass device_type;
   stBTRCoreSupportedServiceList device_profile;
   BD_NAME bd_path;
   int RSSI;
   unsigned int vendor_id;
   BOOLEAN found;
   BOOLEAN device_connected;
} stBTRCoreKnownDevice;

typedef struct _stBTRCoreScannedDevicesCount {
    int numberOfDevices;
    stBTRCoreScannedDevices devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCoreScannedDevicesCount;

typedef struct _stBTRCorePairedDevicesCount {
    int numberOfDevices;
    stBTRCoreKnownDevice devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCorePairedDevicesCount;


typedef struct _stBTRCoreDevMediaPcmInfo {
    eBTRCoreDevMediaAChan   eDevMAChan;               // channel_mode
    unsigned int            ui32DevMSFreq;            // frequency
    unsigned int            ui32DevMSFmt;
} stBTRCoreDevMediaPcmInfo;

typedef struct _stBTRCoreDevMediaSbcInfo {
    eBTRCoreDevMediaAChan   eDevMAChan;               // channel_mode
    unsigned int            ui32DevMSFreq;            // frequency
    unsigned char           ui8DevMSbcAllocMethod;    // allocation_method
    unsigned char           ui8DevMSbcSubbands;       // subbands
    unsigned char           ui8DevMSbcBlockLength;    // block_length
    unsigned char           ui8DevMSbcMinBitpool;     // min_bitpool
    unsigned char           ui8DevMSbcMaxBitpool;     // max_bitpool
    unsigned short          ui16DevMSbcFrameLen;      // frameLength
    unsigned short          ui16DevMSbcBitrate;       // bitrate
} stBTRCoreDevMediaSbcInfo;

typedef struct _stBTRCoreDevMediaMpegInfo {
    eBTRCoreDevMediaAChan   eDevMAChan;               // channel_mode
    unsigned int            ui32DevMSFreq;            // frequency
    unsigned char           ui8DevMMpegCrc;           // crc
    unsigned char           ui8DevMMpegLayer;         // layer
    unsigned char           ui8DevMMpegMpf;           // mpf
    unsigned char           ui8DevMMpegRfa;           // rfa
    unsigned short          ui16DevMMpegFrameLen;     // frameLength
    unsigned short          ui16DevMMpegBitrate;      // bitrate
} stBTRCoreDevMediaMpegInfo;

typedef struct _stBTRCoreDevMediaInfo {
    eBTRCoreDevMediaType eBtrCoreDevMType;
    void*                pstBtrCoreDevMCodecInfo;
} stBTRCoreDevMediaInfo;


typedef void (*BTRCore_DeviceDiscoveryCb) (stBTRCoreScannedDevices astBTRCoreScannedDevice);
typedef void (*BTRCore_StatusCb) (stBTRCoreDevStateCBInfo* apstDevStateCbInfo, void* apvUserData);
typedef int  (*BTRCore_ConnAuthCb) (stBTRCoreConnCBInfo* apstConnCbInfo);
typedef int  (*BTRCore_ConnIntimCb) (stBTRCoreConnCBInfo* apstConnCbInfo);


/*
 * Interfaces
 */
// TODO: Reduce the number of interfaces exposed to the outside world
// TODO: Combine interfaces which perform the same functionality

/* Generic call to init any needed stack.. may be called during powerup*/
enBTRCoreRet BTRCore_Init (tBTRCoreHandle* phBTRCore);

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

enBTRCoreRet BTRCore_GetAdapterAddr (tBTRCoreHandle hBTRCore, unsigned char aui8adapterIdx, char* apui8adapterAddr);

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

/* BTRCore_GetVersionInfo Get BT Version */
enBTRCoreRet BTRCore_GetVersionInfo(tBTRCoreHandle hBTRCore, char* apcBtVersion);

/* BTRCore_StartDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDiscovery (tBTRCoreHandle hBTRCore, stBTRCoreStartDiscovery* pstStartDiscovery);

/* BTRCore_StartDeviceDiscovery - start the discovery process*/
enBTRCoreRet BTRCore_StartDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath);

/* BTRCore_StopDeviceDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_StopDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath);

/* BTRCore_GetListOfScannedDevices - gets the discovered devices list */
enBTRCoreRet BTRCore_GetListOfScannedDevices (tBTRCoreHandle hBTRCore, stBTRCoreScannedDevicesCount *pListOfScannedDevices);

/* BTRCore_PairDevice*/
enBTRCoreRet BTRCore_PairDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/* BTRCore_UnPairDevice is similar to BTRCore_ForgetDevice */
enBTRCoreRet BTRCore_UnPairDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/* BTRCore_GetListOfPairedDevices - gets the paired devices list */
enBTRCoreRet BTRCore_GetListOfPairedDevices (tBTRCoreHandle hBTRCore, stBTRCorePairedDevicesCount *pListOfDevices);

/*BTRCore_FindDevice*/
enBTRCoreRet BTRCore_FindDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId); //TODO: Change to a unique device Identifier

/*BTRCore_FindServiceByIndex - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_FindService (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* UUID, char* XMLdata, int* found); //TODO: Change to a unique device Identifier

/* BTRCore_GetSupportedServices - confirm if a given service exists on a device*/
enBTRCoreRet BTRCore_GetSupportedServices (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, stBTRCoreSupportedServiceList *pProfileList);

/* BTRCore_ConnectDevice */
enBTRCoreRet BTRCore_ConnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_DisconnectDevice */
enBTRCoreRet BTRCore_DisconnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_GetDeviceMediaInfo */
enBTRCoreRet BTRCore_GetDeviceMediaInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreDevMediaInfo*  apstBTRCoreDevMediaInfo);

/*BTRCore_AcquireDeviceDataPath*/
enBTRCoreRet BTRCore_AcquireDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, int* aiDataPath,
                                            int* aidataReadMTU, int* aidataWriteMTU); //TODO: Change to a unique device Identifier

/*BTRCore_ReleaseDeviceDataPath*/
enBTRCoreRet BTRCore_ReleaseDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

enBTRCoreRet BTRCore_MediaPlayControl(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, enBTRCoreMediaCtrl  aenBTRCoreDMCtrl);

/* Callback to notify the application every time when a new device is found and added to discovery list */
enBTRCoreRet BTRCore_RegisterDiscoveryCallback (tBTRCoreHandle  hBTRCore, BTRCore_DeviceDiscoveryCb afptrBTRCoreDeviceDiscoveryCB, void* apUserData);

/*BTRCore_RegisterStatusCallback - callback for unsolicited status changes*/
enBTRCoreRet BTRCore_RegisterStatusCallback (tBTRCoreHandle hBTRCore, BTRCore_StatusCb afptrBTRCoreStatusCB, void* apUserData);

/*BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device*/
enBTRCoreRet BTRCore_RegisterConnectionIntimationCallback (tBTRCoreHandle hBTRCore, BTRCore_ConnIntimCb afptrBTRCoreConnAuthCB, void* apUserData);

/*BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device*/
enBTRCoreRet BTRCore_RegisterConnectionAuthenticationCallback (tBTRCoreHandle hBTRCore, BTRCore_ConnAuthCb afptrBTRCoreConnAuthCB, void* apUserData);


#ifdef __cplusplus
}
#endif

#endif // __BTR_CORE_H__
