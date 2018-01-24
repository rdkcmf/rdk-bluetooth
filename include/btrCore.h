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


#define BTRCORE_MAX_NUM_BT_ADAPTERS 4   // TODO:Better to make this configurable at runtime
#define BTRCORE_MAX_NUM_BT_DEVICES  32  // TODO:Better to make this configurable at runtime
#define BTRCORE_STRINGS_MAX_LEN     32
#define BTRCORE_MAX_DEVICE_PROFILE  32
#define BTRCORE_MAX_STR_LEN         256


typedef enum _enBTRCoreDeviceType {
    enBTRCoreSpeakers,
    enBTRCoreHeadSet,
    enBTRCoreMobileAudioIn,
    enBTRCorePCAudioIn,
    enBTRCoreLE,
    enBTRCoreUnknown
} enBTRCoreDeviceType;

typedef enum _enBTRCoreDeviceClass {
    enBTRCore_DC_Tablet             = 0x11Cu,
    enBTRCore_DC_SmartPhone         = 0x20Cu,
    enBTRCore_DC_WearableHeadset    = 0x404u,
    enBTRCore_DC_Handsfree          = 0x408u,
    enBTRCore_DC_Reserved           = 0x40Cu,
    enBTRCore_DC_Microphone         = 0x410u,
    enBTRCore_DC_Loudspeaker        = 0x414u,
    enBTRCore_DC_Headphones         = 0x418u,
    enBTRCore_DC_PortableAudio      = 0x41Cu,
    enBTRCore_DC_CarAudio           = 0x420u,
    enBTRCore_DC_STB                = 0x424u,
    enBTRCore_DC_HIFIAudioDevice    = 0x428u,
    enBTRCore_DC_VCR                = 0x42Cu,
    enBTRCore_DC_VideoCamera        = 0x430u,
    enBTRCore_DC_Camcoder           = 0x434u,
    enBTRCore_DC_VideoMonitor       = 0x438u,
    enBTRCore_DC_TV                 = 0x43Cu,
    enBTRCore_DC_VideoConference    = 0x440u,
    enBTRCore_DC_Unknown            = 0x000u
} enBTRCoreDeviceClass;

typedef enum _enBTRCoreDeviceState {
    enBTRCoreDevStInitialized,
    enBTRCoreDevStFound,
    enBTRCoreDevStPaired,
    enBTRCoreDevStUnpaired,
    enBTRCoreDevStConnecting,
    enBTRCoreDevStConnected,
    enBTRCoreDevStDisconnecting,
    enBTRCoreDevStDisconnected,
    enBTRCoreDevStPlaying,
    enBTRCoreDevStLost,
    enBTRCoreDevStUnknown
} enBTRCoreDeviceState;

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
    enBTRCoreMediaCtrlPlay,
    enBTRCoreMediaCtrlPause,
    enBTRCoreMediaCtrlStop,
    enBTRCoreMediaCtrlNext,
    enBTRCoreMediaCtrlPrevious,
    enBTRCoreMediaCtrlFastForward,
    enBTRCoreMediaCtrlRewind,
    enBTRCoreMediaCtrlVolumeUp,
    enBTRCoreMediaCtrlVolumeDown
} enBTRCoreMediaCtrl;

typedef enum _eBTRCoreMediaStatusUpdate {
    eBTRCoreMediaTrkStStarted,
    eBTRCoreMediaTrkStPlaying,
    eBTRCoreMediaTrkStPaused,
    eBTRCoreMediaTrkStStopped,
    eBTRCoreMediaTrkStChanged,
    eBTRCoreMediaTrkPosition,
    eBTRCoreMediaPlaybackEnded,
    eBTRCoreMediaPlaylistUpdate,
    eBTRCoreMediaBrowserUpdate
} eBTRCoreMediaStatusUpdate;


/* bd addr length and type */
#ifndef BD_ADDR_LEN
#define BD_ADDR_LEN     6
typedef unsigned char BD_ADDR[BD_ADDR_LEN];
#endif


#define BD_NAME_LEN     248
typedef char BD_NAME[BD_NAME_LEN + 1];     /* Device name */
typedef char *BD_NAME_PTR;                 /* Pointer to Device name */

#define UUID_LEN 63
typedef char UUID[UUID_LEN+1];
      
/*BT getAdapters*/
typedef struct _stBTRCoreGetAdapters {
    unsigned char  number_of_adapters;
} stBTRCoreGetAdapters;

typedef struct _stBTRCoreListAdapters {
   unsigned char    number_of_adapters;
   BD_NAME          adapter_path[BTRCORE_MAX_NUM_BT_ADAPTERS];
   BD_NAME          adapterAddr[BTRCORE_MAX_NUM_BT_ADAPTERS];
} stBTRCoreListAdapters;

typedef struct _stBTRCoreFilterMode {
   BD_ADDR  bd_address;
   BD_NAME  service_name;
   UUID     uuid;
} stBTRCoreFilterMode;

typedef struct _stBTRCoreDevStatusCBInfo {
    tBTRCoreDevId           deviceId;
    BD_NAME                 deviceName; 
    enBTRCoreDeviceType     eDeviceType;
    enBTRCoreDeviceClass    eDeviceClass;
    enBTRCoreDeviceState    eDevicePrevState;
    enBTRCoreDeviceState    eDeviceCurrState;
    unsigned char           isPaired;
} stBTRCoreDevStatusCBInfo;

typedef struct _stBTRCoreSupportedService {
    unsigned int    uuid_value;
    BD_NAME         profile_name;
} stBTRCoreSupportedService;

typedef struct _stBTRCoreSupportedServiceList {
    int                         numberOfService;
    stBTRCoreSupportedService   profile[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreSupportedServiceList;

/*BT Adapter*/
typedef struct _stBTRCoreAdapter {
    unsigned char   adapter_number;
    char*           pcAdapterPath;
    char*           pcAdapterDevName;
    BOOLEAN         enable;
    BOOLEAN         discoverable;
    BOOLEAN         bFirstAvailable; /*search for first available BT adapater*/
    unsigned int    DiscoverableTimeout;
} stBTRCoreAdapter;


typedef struct _stBTRCoreBTDevice { 
    tBTRCoreDevId                 tDeviceId;
    char                          pcDeviceName[BD_NAME_LEN+1];
    char                          pcDeviceAddress[BD_NAME_LEN+1];
    enBTRCoreDeviceClass          enDeviceType;
    stBTRCoreSupportedServiceList stDeviceProfile;
    int                           i32RSSI;
    unsigned int                  ui32VendorId;
    BOOLEAN                       bFound;
    BOOLEAN                       bDeviceConnected;
    char                          pcDevicePath[BD_NAME_LEN+1];
}stBTRCoreBTDevice;

typedef struct _stBTRCoreScannedDevicesCount {
    int                     numberOfDevices;
    stBTRCoreBTDevice       devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCoreScannedDevicesCount;

typedef struct _stBTRCorePairedDevicesCount {
    int                     numberOfDevices;
    stBTRCoreBTDevice       devices[BTRCORE_MAX_NUM_BT_DEVICES];
} stBTRCorePairedDevicesCount;

typedef struct _stBTRCoreConnCBInfo {
    unsigned int    ui32devPassKey;
    char            cConnAuthDeviceName[BTRCORE_STRINGS_MAX_LEN];
    union {
        stBTRCoreBTDevice        stFoundDevice;
        stBTRCoreBTDevice        stKnownDevice;
    };
} stBTRCoreConnCBInfo;


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

typedef struct _stBTRCoreMediaTrackInfo {
    char            pcAlbum[BTRCORE_MAX_STR_LEN];
    char            pcGenre[BTRCORE_MAX_STR_LEN];
    char            pcTitle[BTRCORE_MAX_STR_LEN];
    char            pcArtist[BTRCORE_MAX_STR_LEN];
    unsigned int    ui32TrackNumber;
    unsigned int    ui32Duration;
    unsigned int    ui32NumberOfTracks;
} stBTRCoreMediaTrackInfo;

typedef struct _stBTRCoreMediaPositionInfo {
    unsigned int    ui32Duration;
    unsigned int    ui32Position;
} stBTRCoreMediaPositionInfo;

typedef struct _stBTRCoreMediaStatusUpdate {
   eBTRCoreMediaStatusUpdate     eBTMediaStUpdate;

    union {
      stBTRCoreMediaTrackInfo       m_mediaTrackInfo;
      stBTRCoreMediaPositionInfo    m_mediaPositionInfo;
    };
} stBTRCoreMediaStatusUpdate;

typedef struct _stBTRCoreMediaStatusCBInfo {
    tBTRCoreDevId                   deviceId;
    BD_NAME                         deviceName;
    enBTRCoreDeviceClass            eDeviceClass;

    stBTRCoreMediaStatusUpdate      m_mediaStatusUpdate;
} stBTRCoreMediaStatusCBInfo;


/* Fptr Callbacks types */
typedef enBTRCoreRet (*fPtr_BTRCore_DeviceDiscoveryCb) (stBTRCoreBTDevice astBTRCoreScannedDevice, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_StatusCb) (stBTRCoreDevStatusCBInfo* apstDevStatusCbInfo, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_MediaStatusCb) (stBTRCoreMediaStatusCBInfo* apstMediaStatusCbInfo, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_ConnIntimCb) (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInIntimResp, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_ConnAuthCb) (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInAuthResp, void* apvUserData);


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

/* BTRCore_GetAdapterAddr Get Address of BT Adapter */
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

/* BTRCore_StartDiscovery - Start the discovery process*/
enBTRCoreRet BTRCore_StartDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath, enBTRCoreDeviceType aenBTRCoreDevType, unsigned int aui32DiscDuration);

/* BTRCore_StopDiscovery - aborts the discovery process*/
enBTRCoreRet BTRCore_StopDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath, enBTRCoreDeviceType aenBTRCoreDevType);

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

/* BTRCore_IsDeviceConnectable */
enBTRCoreRet BTRCore_IsDeviceConnectable (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/* BTRCore_ConnectDevice */
enBTRCoreRet BTRCore_ConnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_DisconnectDevice */
enBTRCoreRet BTRCore_DisconnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_GetDeviceConnected */
enBTRCoreRet BTRCore_GetDeviceConnected (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_GetDeviceDisconnected */
enBTRCoreRet BTRCore_GetDeviceDisconnected (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/* BTRCore_GetDeviceMediaInfo */
enBTRCoreRet BTRCore_GetDeviceMediaInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreDevMediaInfo*  apstBTRCoreDevMediaInfo);

/*BTRCore_AcquireDeviceDataPath*/
enBTRCoreRet BTRCore_AcquireDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, int* aiDataPath,
                                            int* aidataReadMTU, int* aidataWriteMTU); //TODO: Change to a unique device Identifier

/*BTRCore_ReleaseDeviceDataPath*/
enBTRCoreRet BTRCore_ReleaseDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType); //TODO: Change to a unique device Identifier

/* BTRCore_MediaControl */
enBTRCoreRet BTRCore_MediaControl(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, enBTRCoreMediaCtrl aenBTRCoreMediaCtrl);

/* BTRCore_GetTrackInformation */
enBTRCoreRet BTRCore_GetMediaTrackInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreMediaTrackInfo* apstBTMediaTrackInfo);

/* BTRCore_GetMediaPositionInfo */
enBTRCoreRet BTRCore_GetMediaPositionInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreMediaPositionInfo* apstBTMediaPositionInfo);

/* BTRCore_GetMediaProperty */
enBTRCoreRet BTRCore_GetMediaProperty ( tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, const char* mediaPropertyKey, void* mediaPropertyValue); 

/* BTRCore_ReportMediaPosition */
enBTRCoreRet BTRCore_ReportMediaPosition (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

// Outgoing callbacks Registration Interfaces
/* Callback to notify the application every time when a new device is found and added to discovery list */
enBTRCoreRet BTRCore_RegisterDiscoveryCb (tBTRCoreHandle  hBTRCore, fPtr_BTRCore_DeviceDiscoveryCb afpcBBTRCoreDeviceDiscovery, void* apUserData);

/*BTRCore_RegisterStatusCallback - callback for unsolicited status changes*/
enBTRCoreRet BTRCore_RegisterStatusCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_StatusCb afpcBBTRCoreStatus, void* apUserData);

/*BTRCore_RegisterMediaStatusCallback - callback for media state changes*/
enBTRCoreRet BTRCore_RegisterMediaStatusCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_MediaStatusCb afpcBBTRCoreMediaStatus, void* apUserData);

/*BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device*/
enBTRCoreRet BTRCore_RegisterConnectionIntimationCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_ConnIntimCb afpcBBTRCoreConnAuth, void* apUserData);

/*BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device*/
enBTRCoreRet BTRCore_RegisterConnectionAuthenticationCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_ConnAuthCb afpcBBTRCoreConnAuth, void* apUserData);


#ifdef __cplusplus
}
#endif

#endif // __BTR_CORE_H__
