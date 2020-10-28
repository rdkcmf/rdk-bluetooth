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

/*
 * @file btrCore.h
 * Core abstraction for BT functionality
 */

#ifndef __BTR_CORE_H__
#define __BTR_CORE_H__

#include "btrCoreTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 @defgroup BLUETOOTH_CORE Bluetooth Core
* Bluetooth Manager implements the Bluetooth HAL i.e. Bluetooth Core[BTRCore] API.
* Bluetooth HAL interface provides a software abstraction layer that interfaces
* with the actual Bluetooth implementation and/or drivers. RDK Bluetooth HAL layer
* enables projects to pick any Bluetooth profiles as per their requirements.
* Bluetooth HAL uses BlueZ5.42 stack which is a quite popular Linux Bluetooth library.
* - Bluetooth HAL - Provides APIs to perform Bluetooth operations by abstracting and simplifying the complexities
*  of Bt-Ifce (& BLuez)
* - Bt-Ifce - Abstracts Bluez versions and serves as a HAL for other Bluetooth stacks - Interacts with Bluez over DBus
* - Bluez -  Interacts with kernel layer bluetooth modules
* @defgroup BLUETOOTH_TYPES Bluetooth Core Types
* @ingroup BLUETOOTH_CORE
*
* @defgroup BLUETOOTH_APIS Bluetooth Core APIs
* @ingroup BLUETOOTH_CORE
*
**/

/**
 * @addtogroup BLUETOOTH_TYPES
 * @{
 */

#define BTRCORE_MAX_NUM_BT_ADAPTERS 4   // TODO:Better to make this configurable at runtime
#define BTRCORE_MAX_NUM_BT_DEVICES  64  // TODO:Better to make this configurable at runtime
#define BTRCORE_MAX_DEVICE_PROFILE  32
#define BTRCORE_MAX_MEDIA_ELEMENTS  64

#define BTRCORE_UUID_LEN            BTRCORE_STR_LEN
#define BTRCORE_MAX_DEV_OP_DATA_LEN BTRCORE_MAX_STR_LEN * 3
#define BTRCORE_MAX_SERVICE_DATA_LEN  32

typedef unsigned long long int tBTRCoreMediaElementId;

typedef enum _enBTRCoreOpType {
    enBTRCoreOpTypeAdapter,
    enBTRCoreOpTypeDevice
} enBTRCoreOpType;

typedef enum _enBTRCoreDeviceType {
    enBTRCoreSpeakers,
    enBTRCoreHeadSet,
    enBTRCoreMobileAudioIn,
    enBTRCorePCAudioIn,
    enBTRCoreLE,
    enBTRCoreHID,
    enBTRCoreUnknown
} enBTRCoreDeviceType;

typedef enum _enBTRCoreDeviceClass {
    /* AV DeviceClass */
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

    /* LE DeviceClass */
    enBTRCore_DC_Tile               = 0xfeedu, //0xfeecu
    enBTRCore_DC_HID_AudioRemote    = 0x50Cu,
    enBTRCore_DC_HID_Keyboard       = 0x540u,
    enBTRCore_DC_HID_Mouse          = 0x580u,
    enBTRCore_DC_HID_MouseKeyBoard  = 0x5C0u,
    enBTRCore_DC_HID_Joystick       = 0x504u,
    enBTRCore_DC_HID_GamePad        = 0x508u,

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
    enBTRCoreDevStOpReady,
    enBTRCoreDevStOpInfo,
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
    enBTRCoreMediaCtrlVolumeDown,
    enBTRCoreMediaCtrlEqlzrOff,
    enBTRCoreMediaCtrlEqlzrOn,
    enBTRCoreMediaCtrlShflOff,
    enBTRCoreMediaCtrlShflAllTracks,
    enBTRCoreMediaCtrlShflGroup,
    enBTRCoreMediaCtrlRptOff,
    enBTRCoreMediaCtrlRptSingleTrack,
    enBTRCoreMediaCtrlRptAllTracks,
    enBTRCoreMediaCtrlRptGroup,
    enBTRCoreMediaCtrlScanOff,
    enBTRCoreMediaCtrlScanAllTracks,
    enBTRCoreMediaCtrlScanGroup,
    enBTRCoreMediaCtrlMute,
    enBTRCoreMediaCtrlUnMute,
    enBTRCoreMediaCtrlUnknown
} enBTRCoreMediaCtrl;

typedef enum _eBTRCoreMediaStatusUpdate {
    eBTRCoreMediaTrkStStarted,
    eBTRCoreMediaTrkStPlaying,
    eBTRCoreMediaTrkStPaused,
    eBTRCoreMediaTrkStStopped,
    eBTRCoreMediaTrkStChanged,
    eBTRCoreMediaTrkPosition,
    eBTRCoreMediaPlaybackEnded,
    eBTRCoreMediaPlyrName,
    eBTRCoreMediaPlyrVolume,
    eBTRCoreMediaPlyrEqlzrStOff,
    eBTRCoreMediaPlyrEqlzrStOn,
    eBTRCoreMediaPlyrShflStOff,
    eBTRCoreMediaPlyrShflStAllTracks,
    eBTRCoreMediaPlyrShflStGroup,
    eBTRCoreMediaPlyrRptStOff,
    eBTRCoreMediaPlyrRptStSingleTrack,
    eBTRCoreMediaPlyrRptStAllTracks,
    eBTRCoreMediaPlyrRptStGroup,
    eBTRCoreMediaPlyrScanStOff,
    eBTRCoreMediaPlyrScanStAllTracks,
    eBTRCoreMediaPlyrScanStGroup,
    eBTRCoreMediaElementInScope,
    eBTRCoreMediaElementOofScope,
    eBTRCoreMediaStUnknown
} eBTRCoreMediaStatusUpdate;

typedef enum _eBTRCoreMedElementType {
    enBTRCoreMedETypeUnknown,
    enBTRCoreMedETypeAlbum,
    enBTRCoreMedETypeArtist,
    enBTRCoreMedETypeGenre,
    enBTRCoreMedETypeCompilation,
    enBTRCoreMedETypePlayList,
    enBTRCoreMedETypeTrackList,
    enBTRCoreMedETypeTrack
} eBTRCoreMedElementType;

typedef enum _enBTRCoreLeOp {
    enBTRCoreLeOpGReady,
    enBTRCoreLeOpGReadValue,    // G Referring to Gatt
    enBTRCoreLeOpGWriteValue,
    enBTRCoreLeOpGStartNotify,
    enBTRCoreLeOpGStopNotify,
    enBTRCoreLeOpUnknown       // Add enBTRCoreLeOpXXXXX Later if needed
} enBTRCoreLeOp;

typedef enum _enBTRCoreLeProp {
    enBTRCoreLePropGUUID,
    enBTRCoreLePropGPrimary,
    enBTRCoreLePropGDevice,
    enBTRCoreLePropGService,
    enBTRCoreLePropGValue,
    enBTRCoreLePropGNotifying,
    enBTRCoreLePropGFlags,
    enBTRCoreLePropGChar,
    enBTRCoreLEGPropGDesc,
    enBTRCoreLePropUnknown
} enBTRCoreLeProp;


/* bd addr length and type */
#ifndef BD_ADDR_LEN
#define BD_ADDR_LEN     6
typedef unsigned char BD_ADDR[BD_ADDR_LEN];
#endif


#define BD_NAME_LEN     BTRCORE_STR_LEN - 1
typedef char BD_NAME[BD_NAME_LEN + 1];     /* Device name */
typedef char *BD_NAME_PTR;                 /* Pointer to Device name */

#define UUID_LEN        BTRCORE_UUID_LEN - 1
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
    char                    deviceAddress[BTRCORE_MAX_STR_LEN];
    enBTRCoreDeviceType     eDeviceType;
    enBTRCoreDeviceClass    eDeviceClass;
    enBTRCoreDeviceState    eDevicePrevState;
    enBTRCoreDeviceState    eDeviceCurrState;
    unsigned char           isPaired;
    unsigned int            ui32DevClassBtSpec;
    char                    uuid[BTRCORE_UUID_LEN];
    char                    devOpResponse[BTRCORE_MAX_DEV_OP_DATA_LEN];
    enBTRCoreLeProp         eCoreLeProp;
    enBTRCoreLeOp           eCoreLeOper;
} stBTRCoreDevStatusCBInfo;

typedef struct _stBTRCoreSupportedService {
    unsigned int    uuid_value;
    BD_NAME         profile_name;
} stBTRCoreSupportedService;

typedef struct _stBTRCoreSupportedServiceList {
    int                         numberOfService;
    stBTRCoreSupportedService   profile[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreSupportedServiceList;

typedef struct _stBTRCoreAdServiceData {
    char            pcUUIDs[BTRCORE_UUID_LEN];;
    unsigned char   pcData[BTRCORE_MAX_SERVICE_DATA_LEN];
    unsigned int    len;
} stBTRCoreAdServiceData;

/*BT Adapter*/
typedef struct _stBTRCoreAdapter {
    unsigned char   adapter_number;
    char*           pcAdapterPath;
    char*           pcAdapterDevName;
    BOOLEAN         enable;
    BOOLEAN         discoverable;
    BOOLEAN         bFirstAvailable; /*search for first available BT adapater*/
    unsigned int    DiscoverableTimeout;
    BOOLEAN         bDiscovering;
} stBTRCoreAdapter;


typedef struct _stBTRCoreBTDevice { 
    tBTRCoreDevId                 tDeviceId;
    enBTRCoreDeviceClass          enDeviceType;
    BOOLEAN                       bFound;
    BOOLEAN                       bDeviceConnected;
    int                           i32RSSI;
    unsigned int                  ui32VendorId;
    unsigned int                  ui32DevClassBtSpec;  
    char                          pcDeviceName[BD_NAME_LEN+1];
    char                          pcDeviceAddress[BD_NAME_LEN+1];
    char                          pcDevicePath[BD_NAME_LEN+1];
    stBTRCoreSupportedServiceList stDeviceProfile;
    stBTRCoreAdServiceData        stAdServiceData[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreBTDevice;

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
    unsigned char   ucIsReqConfirmation;
    char            cConnAuthDeviceName[BTRCORE_STR_LEN];
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

typedef struct _stBTRCoreMediaElementInfo {
    eBTRCoreMedElementType       eAVMedElementType;
    tBTRCoreMediaElementId       ui32MediaElementId;
    unsigned char                bIsPlayable;
    char                         m_mediaElementName[BTRCORE_MAX_STR_LEN];
    stBTRCoreMediaTrackInfo      m_mediaTrackInfo;
} stBTRCoreMediaElementInfo;

typedef struct _stBTRCoreMediaElementInfoList {
    unsigned short               m_numOfElements;
    stBTRCoreMediaElementInfo    m_mediaElementInfo[BTRCORE_MAX_MEDIA_ELEMENTS];
} stBTRCoreMediaElementInfoList;

typedef struct _stBTRCoreMediaStatusUpdate {
   eBTRCoreMediaStatusUpdate     eBTMediaStUpdate;

    union {
      stBTRCoreMediaTrackInfo         m_mediaTrackInfo;
      stBTRCoreMediaPositionInfo      m_mediaPositionInfo;
      stBTRCoreMediaElementInfo       m_mediaElementInfo;
      char                            m_mediaPlayerName[BTRCORE_MAX_STR_LEN];
      unsigned char                   m_mediaPlayerVolumePercentage;
    };
} stBTRCoreMediaStatusUpdate;

typedef struct _stBTRCoreMediaStatusCBInfo {
    tBTRCoreDevId                   deviceId;
    BD_NAME                         deviceName;
    enBTRCoreDeviceClass            eDeviceClass;

    stBTRCoreMediaStatusUpdate      m_mediaStatusUpdate;
} stBTRCoreMediaStatusCBInfo;

typedef struct _stBTRCoreUUID {
    unsigned short  flags;
    char            uuid[BTRCORE_UUID_LEN];
} stBTRCoreUUID;

typedef struct _stBTRCoreUUIDList {
    unsigned char   numberOfUUID;
    stBTRCoreUUID   uuidList[BTRCORE_MAX_DEVICE_PROFILE];
} stBTRCoreUUIDList;

typedef struct _stBTRCoreDiscoveryCBInfo {
    enBTRCoreOpType type;
    stBTRCoreAdapter adapter;
    stBTRCoreBTDevice device;
} stBTRCoreDiscoveryCBInfo;



/* Fptr Callbacks types */
typedef enBTRCoreRet (*fPtr_BTRCore_DeviceDiscCb) (stBTRCoreDiscoveryCBInfo* astBTRCoreDiscoveryCbInfo, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_StatusCb) (stBTRCoreDevStatusCBInfo* apstDevStatusCbInfo, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_MediaStatusCb) (stBTRCoreMediaStatusCBInfo* apstMediaStatusCbInfo, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_ConnIntimCb) (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInIntimResp, void* apvUserData);
typedef enBTRCoreRet (*fPtr_BTRCore_ConnAuthCb) (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInAuthResp, void* apvUserData);

/* @} */ // End of group BLUETOOTH_TYPES


/**
 * @addtogroup BLUETOOTH_APIS
 * @{
 */


/*
 * Interfaces
 */
// TODO: Reduce the number of interfaces exposed to the outside world
// TODO: Combine interfaces which perform the same functionality

/**
 * @brief This API connects to a bus daemon and registers the client with it.
 *
 * @param[in]  phBTRCore       Bluetooth core handle.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_Init (tBTRCoreHandle* phBTRCore);

/**
 * @brief This APi deinitialzes and free BTRCore.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_DeInit (tBTRCoreHandle hBTRCore);

/**
 * @brief This API registers an agent handler.
 *
 * Every application can register its own agent and for all actions triggered by that application its
 * agent is used.
 * If an application chooses to not register an agent, the default agent is used.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 * @param[in]  iBTRCapMode     Capabilities can be "DisplayOnly", "DisplayYesNo", "KeyboardOnly",
 *                             "NoInputNoOutput" and "KeyboardDisplay" which
 *                             reflects the input and output capabilities of the agent.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 * @note An application can only register one agent. Multiple agents per application is not supported.
 *
 */
enBTRCoreRet BTRCore_RegisterAgent (tBTRCoreHandle hBTRCore, int iBTRCapMode);

/**
 * @brief This unregisters the agent that has been previously registered.
 *
 * The object path parameter must match the same value that has been used on registration.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_UnregisterAgent (tBTRCoreHandle hBTRCore);


/**
 * @brief Returns list of adapter object paths under /org/bluez
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[out] pstListAdapters  List of adapters.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetListOfAdapters (tBTRCoreHandle hBTRCore, stBTRCoreListAdapters* pstListAdapters);

/**
 * @brief  This API sets the bluetooth adapter power as ON/OFF.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 * @param[in]  pAdapterPath    Bluetooth adapter address.
 * @param[in]  powerStatus     Bluetooth adapter power status.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char powerStatus);

/**
 * @brief This API returns the value of  org.bluez.Adapter.powered .
 *
 * @param[in]  hBTRCore       Bluetooth core handle.
 * @param[in]  pAdapterPath   Bluetooth adapter address.
 * @param[out] pAdapterPower  Value of bluetooth adapter.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pAdapterPower);

/**
 * @brief This API returns the value of org.bluez.Manager.Getadapters .
 *
 * @param[in]  hBTRCore          Bluetooth core handle.
 * @param[out] pstGetAdapters    Adapter value.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapters (tBTRCoreHandle hBTRCore, stBTRCoreGetAdapters* pstGetAdapters);

/**
 * @brief  This API returns the bluetooth adapter path.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[out] apstBTRCoreAdapter  Adapter path.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/**
 * @brief  This API sets Current Bluetooth Adapter to use.
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[in]  adapter_number   Bluetooth adapter number.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapter (tBTRCoreHandle hBTRCore, int adapter_number);

/**
 * @brief  This API enables specific adapter.
 *
 * @param[in]  hBTRCore             Bluetooth core handle.
 * @param[in]  apstBTRCoreAdapter   Structure which holds the adapter info.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_EnableAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/**
 * @brief  This API disables specific adapter.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[in]  apstBTRCoreAdapter  Structure which holds the adapter info.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_DisableAdapter (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/**
 * @brief   This API gets Address of BT Adapter.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[in]  aui8adapterIdx      Adapter index.
 * @param[out] apui8adapterAddr    Adapter address.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapterAddr (tBTRCoreHandle hBTRCore, unsigned char aui8adapterIdx, char* apui8adapterAddr);

/**
 * @brief  This API sets adapter as discoverable.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  pAdapterPath       Adapter path.
 * @param[in]  discoverable       Value that sets the device discoverable or not.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapterDiscoverable (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char discoverable);

/**
 * @brief  This API sets how long the adapter is discoverable.
 *
 * @param[in]  hBTRCore          Bluetooth core handle.
 * @param[in]  pAdapterPath      Adapter path.
 * @param[in]  timeout           Time out value.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapterDiscoverableTimeout (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned short timeout);

/**
 * @brief  This API checks whether the discovery is in progress or not.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[in]  pAdapterPath        Adapter path.
 * @param[in]  pDiscoverable       Indicates discoverable or not.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapterDiscoverableStatus (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pDiscoverable);

/**
 * @brief  This API sets a friendly name to BT adapter device.
 *
 * @param[in]  hBTRCore             Bluetooth core handle.
 * @param[in]  apstBTRCoreAdapter   Adapter path.
 * @param[in]  apcAdapterDeviceName Adapter device name.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapterDeviceName (tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter, char* apcAdapterDeviceName);

/**
 * @brief  This API sets a friendly name to BT adapter.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[in]  pAdapterPath        Adapter path.
 * @param[in]  pAdapterName        Adapter name.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pAdapterName);

/**
 * @brief  This API gets the name of BT adapter.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  pAdapterPath       Adapter path.
 * @param[out] pAdapterName       Adapter name.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, char* pAdapterName);

/**
 * @brief  This API resets specific adapter.
 *
 * @param[in]  hBTRCore            Bluetooth core handle.
 * @param[in]  apstBTRCoreAdapter  Adapter to be reset.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_ResetAdapter(tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);

/**
 * @brief  This API gets BT Version.
 *
 * @param[in]  hBTRCore      Bluetooth core handle.
 * @param[out] apcBtVersion  Bluetooth version.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_GetVersionInfo(tBTRCoreHandle hBTRCore, char* apcBtVersion);

/**
 * @brief  This method starts the device discovery session.
 *
 * This includes an inquiry procedure and remote device name resolving.
 * This process will start emitting DeviceFound and PropertyChanged "Discovering" signals.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  pAdapterPath       Adapter path the message should be sent to.
 * @param[in]  aenBTRCoreDevType  Bluetooth device types like headset, speakers, Low energy devices etc.
 * @param[in]  aui32DiscDuration  Timeout for the discovery.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_StartDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath, enBTRCoreDeviceType aenBTRCoreDevType, unsigned int aui32DiscDuration);

/**
 * @brief  This method will cancel any previous StartDiscovery transaction.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  pAdapterPath       Adapter path where the message should be sent to.
 * @param[in]  aenBTRCoreDevType  Bluetooth device types like headset, speakers, Low energy devices etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 * @note Discovery procedure is shared between all discovery sessions thus calling StopDiscovery will only
 * release a single session.
 */
enBTRCoreRet BTRCore_StopDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath, enBTRCoreDeviceType aenBTRCoreDevType);

/**
 * @brief  This API returns the number of devices scanned.
 *
 * This includes the Device name, MAC address, Signal strength etc.
 *
 * @param[in]   hBTRCore               Bluetooth core handle.
 * @param[out]  pListOfScannedDevices  Structure which holds the count and the device info.
 */
enBTRCoreRet BTRCore_GetListOfScannedDevices (tBTRCoreHandle hBTRCore, stBTRCoreScannedDevicesCount *pListOfScannedDevices);

/**
 * @brief This API initiates the pairing of the device.
 *
 * This method will connect to the remote device and retrieve all SDP records and then initiate the pairing.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 * @param[in]  aBTRCoreDevId   Device ID for pairing.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_PairDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/**
 * @brief  This API removes the remote device object at the given path.
 *
 * It will remove also the pairing information.
 * BTRCore_UnPairDevice is similar to BTRCore_ForgetDevice.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 * @param[in]  aBTRCoreDevId   Device ID for pairing.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_UnPairDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/**
 * @brief   Gets the paired devices list.
 *
 * @param[in]  hBTRCore        Bluetooth core handle.
 * @param[out] pListOfDevices  List of paired devices that has to be fetched.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetListOfPairedDevices (tBTRCoreHandle hBTRCore, stBTRCorePairedDevicesCount *pListOfDevices);

/**
 * @brief   This API checks the device entry in the scanned device list.
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[in]  aBTRCoreDevId    Device ID to be checked.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_FindDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/**
 * @brief   This API is used to confirm if a given service exists on a device.
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[in]  aBTRCoreDevId    Device ID to be checked.
 * @param[in]  UUID             UUID  of the bluetooth device.
 * @param[in]  XMLdata          Service name.
 * @param[out] found            Indicates service found or not.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_FindService (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* UUID, char* XMLdata, int* found);

/**
 * @brief   This API retuns the list of services supported by the device.
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[in]  aBTRCoreDevId    Device ID to be checked.
 * @param[out] pProfileList     List of supported services.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetSupportedServices (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, stBTRCoreSupportedServiceList *pProfileList);

/**
 * @brief   This API checks the device is connectable.
 *
 * It uses ping utility to check the connection with the remote device.
 *
 * @param[in]  hBTRCore         Bluetooth core handle.
 * @param[in]  aBTRCoreDevId    Device ID to be checked.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_IsDeviceConnectable (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId);

/**
 * @brief  This method connect any profiles the remote device supports.
 *
 * It is been flagged as auto-connectable on adapter side. If only subset of profiles is already
 * connected it will try to connect currently disconnected ones.
 * If at least one profile was connected successfully this method will indicate success.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType  Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_ConnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/**
 * @brief  This method gracefully disconnects all connected profiles and then terminates  connection.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType  Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_DisconnectDevice (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/**
 * @brief  This method checks the current device that is connected.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType  Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetDeviceConnected (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/**
 * @brief  This method checks the current device that is disconnected.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType  Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetDeviceDisconnected (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType);

/**
 * @brief  This API returns current media info that includes the codec info, channel modes, subbands etc.
 *
 * @param[in]  hBTRCore                Bluetooth core handle.
 * @param[in]  aBTRCoreDevId           Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType       Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] apstBTRCoreDevMediaInfo Structure which stores the media info.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetDeviceTypeClass (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType* apenBTRCoreDevTy, enBTRCoreDeviceClass* apenBTRCoreDevCl);

/**
 * @brief  This API returns current media info that includes the codec info, channel modes, subbands etc.
 *
 * @param[in]  hBTRCore                Bluetooth core handle.
 * @param[in]  aBTRCoreDevId           Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType       Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] apstBTRCoreDevMediaInfo Structure which stores the media info.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetDeviceMediaInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreDevMediaInfo*  apstBTRCoreDevMediaInfo);

/**
 * @brief  This API returns the bluetooth device address.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType  Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] aiDataPath         Device address.
 * @param[in]  aidataReadMTU      Read data length.
 * @param[in]  aidataWriteMTU     Write data length.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AcquireDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, int* aiDataPath, int* aidataReadMTU, int* aidataWriteMTU);

/**
 * @brief  This API release the bluetooth device address.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aBTRCoreDevId      Device Id of the remote device.
 * @param[in]  enDeviceType       Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_ReleaseDeviceDataPath(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType enDeviceType);

/**
 * @brief  This API release the bluetooth device address.
 *
 * @param[in]  hBTRCore           Bluetooth core handle.
 * @param[in]  aui32AckTOutms     Data write acknowledgment timeout
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetDeviceDataAckTimeout(tBTRCoreHandle hBTRCore, unsigned int aui32AckTOutms);

/**
 * @brief  This API is used to perform media control operations like play, pause, NExt, Previous, Rewind etc.
 *
 * @param[in]  hBTRCore             Bluetooth core handle.
 * @param[in]  aBTRCoreDevId        Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType    Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[in]  aenBTRCoreMediaCtrl  Indicates which operation needs to be performed.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_MediaControl(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, enBTRCoreMediaCtrl aenBTRCoreMediaCtrl);

/**
 * @brief  This API is used to retrieve the media track information.
 *
 * @param[in]  hBTRCore              Bluetooth core handle.
 * @param[in]  aBTRCoreDevId         Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType     Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] apstBTMediaTrackInfo  Structure which represents the media track information.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_GetMediaTrackInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreMediaTrackInfo* apstBTMediaTrackInfo);

/**
 * @brief  This API is used to retrieve the media track information.
 *
 * @param[in]  hBTRCore              Bluetooth core handle.
 * @param[in]  aBTRCoreDevId         Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType     Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[in]  aBtrMediaElementId           Media Element Id
 * @param[out] apstBTMediaTrackInfo  Structure which represents the media track information.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropiate error code otherwise.
 */
enBTRCoreRet BTRCore_GetMediaElementTrackInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType,tBTRCoreMediaElementId  aBtrMediaElementId, stBTRCoreMediaTrackInfo* apstBTMediaTrackInfo);


/**
 * @brief  This API returns the duration and the current position of the media.
 *
 * @param[in]  hBTRCore                  Bluetooth core handle.
 * @param[in]  aBTRCoreDevId             Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType         Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] apstBTMediaPositionInfo  Structure which represents the position information.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetMediaPositionInfo (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, stBTRCoreMediaPositionInfo* apstBTMediaPositionInfo);

/**
 * @brief  This API returns the media file properties of the Bluetooth device.
 *
 * As of now, it is implemented to return dummy value.
 *
 * @param[in]  hBTRCore                 Bluetooth core handle.
 * @param[in]  aBTRCoreDevId            Device Id of the remote device.
 * @param[in]  aenBTRCoreDevType        Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[out] mediaPropertyKey         Key to the property.
 * @param[out] mediaPropertyValue       Value to the property.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetMediaProperty ( tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, enBTRCoreDeviceType aenBTRCoreDevType, const char* mediaPropertyKey, void* mediaPropertyValue);

/**
 * @brief  This API sets the mentioned media list active/in_scope at the lower to allow further operations on the elements in the list.
 *
 * @param[in]  hBTRCore                     Bluetooth core handle.
 * @param[in]  aBTRCoreDevId                Device Id of the remote device.
 * @param[in]  aBtrMediaElementId           Media Element Id
 * @param[in]  aenBTRCoreDevType            Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[in]  aeBTRCoreMedElementType      Media Element type (Albums, Artists, ...)
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetMediaElementActive (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, tBTRCoreMediaElementId aBtrMediaElementId, enBTRCoreDeviceType aenBTRCoreDevType, eBTRCoreMedElementType aeBTRCoreMedElementType);

/**
 * @brief  This API returns the mentioned media list.
 *
 * @param[in]  hBTRCore                     Bluetooth core handle.
 * @param[in]  aBTRCoreDevId                Device Id of the remote device.
 * @param[in]  aBtrMediaElementId           Media Element Id
 * @param[in]  aui16BtrMedElementStartIdx   Starting index of the list.
 * @param[in]  aui16BtrMedElementEndIdx     ending index of the list
 * @param[in]  aenBTRCoreDevType            Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[in]  aenBTRCoreMedElementType      Media Element type (Albums, Artists, ...)
 * @param[out] apstMediaElementListInfo     Retrived Media Element List.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetMediaElementList (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, tBTRCoreMediaElementId aBtrMediaElementId, unsigned short aui16BtrMedElementStartIdx, unsigned short aui16BtrMedElementEndIdx, enBTRCoreDeviceType aenBTRCoreDevType, eBTRCoreMedElementType aenBTRCoreMedElementType, stBTRCoreMediaElementInfoList* apstMediaElementListInfo);

/**
 * @brief  This API performs operation according to the element type selected.
 * @param[in]  hBTRCore                     Bluetooth core handle.
 * @param[in]  aBTRCoreDevId                Device Id of the remote device.
 * @param[in]  aBtrMediaElementId           Media Element Id
 * @param[in]  aenBTRCoreDevType            Type of bluetooth device HFP(Hands Free Profile) headset, audio source etc.
 * @param[in]  aeBTRCoreMedElementType      Media Element type (Albums, Artists, ...)
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SelectMediaElement (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, tBTRCoreMediaElementId aBtrMediaElementId, enBTRCoreDeviceType aenBTRCoreDevType, eBTRCoreMedElementType  aenBTRCoreMedElementType);

/**
 * @brief  This API returns the Low energy profile device name and address.
 *
 * @param[in]  hBTRCore                 Bluetooth core handle.
 * @param[in]  aBTRCoreDevId            Device Id of the remote device.
 * @param[in]  apcBTRCoreLEUuid         UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLeProp         Indicates the property name.
 * @param[out] apvBTRCorePropVal        LE device property value.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetLEProperty(tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* apcBTRCoreLEUuid, enBTRCoreLeProp aenBTRCoreLeProp, void* apvBTRCorePropVal);

/**
 * @brief  This API is used to perform read, write, notify operations on LE devices.
 *
 * @param[in]  hBTRCore                 Bluetooth core handle.
 * @param[in]  aBTRCoreDevId            Device Id of the remote device.
 * @param[in]  apcBTRCoreLEUuid         UUID to distinguish the devices.
 * @param[in]  aenBTRCoreLeOp           Indicates the operation to be performed.
 * @param[in]  apUserData               Data to perform the operation.
 * @param[out] rpLeOpRes                        LE operation result.
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_PerformLEOp (tBTRCoreHandle hBTRCore, tBTRCoreDevId aBTRCoreDevId, const char* apcBTRCoreLEUuid, enBTRCoreLeOp aenBTRCoreLeOp, char* apLeOpArg, char* rpLeOpRes);

/**
 * @brief  This API is used to start advertisement registration
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_StartAdvertisement(tBTRCoreHandle hBTRCore);

/**
 * @brief  This API is used to stop advertisement registration
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_StopAdvertisement(tBTRCoreHandle  hBTRCore);

/**
 * @brief  This API is used to set advertisement type
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetAdvertisementType(tBTRCoreHandle hBTRCore, char *aAdvtType);

/**
 * @brief  This API is used to set service UUIDs
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetServiceUUIDs(tBTRCoreHandle hBTRCore, char *aUUID);

/**
 * @brief  This API is used to set manufacturer data
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetManufacturerData(tBTRCoreHandle hBTRCore, unsigned short aManfId, unsigned char *aDeviceDetails, int aLenManfData);

/**
 * @brief  This API is used to Enable Tx Power transmission
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetEnableTxPower(tBTRCoreHandle hBTRCore, BOOLEAN lTxPower);

/**
 * @brief  This API is used to Get Property value
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_GetPropertyValue(tBTRCoreHandle hBTRCore, char *aUUID, char *aValue, enBTRCoreLeProp aElement);

/**
 * @brief  This API is used to Set Service Info value
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetServiceInfo(tBTRCoreHandle hBTRCore, char *aUUID, BOOLEAN aServiceType);

/**
 * @brief  This API is used to Set Gatt Info value
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetGattInfo(tBTRCoreHandle hBTRCore, char *aParentUUID, char *aCharUUID, unsigned short aFlags, char *aValue, enBTRCoreLeProp aElement);

/**
 * @brief  This API is used to Set Property value
 *
 * @param[in]  hBTRCore                 Bluetooth core handle 
 *
 * @return  Returns the status of the operation.
 * @retval  Returns enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_SetPropertyValue(tBTRCoreHandle hBTRCore, char *aUUID, char *aValue, enBTRCoreLeProp aElement);

// Outgoing callbacks Registration Interfaces
/* BTRCore_RegisterDiscoveryCb - Callback to notify the application every time when a new device is found and added to discovery list */
enBTRCoreRet BTRCore_RegisterDiscoveryCb (tBTRCoreHandle  hBTRCore, fPtr_BTRCore_DeviceDiscCb afpcBBTRCoreDeviceDiscovery, void* apUserData);

/* BTRCore_RegisterStatusCallback - callback for unsolicited status changes */
enBTRCoreRet BTRCore_RegisterStatusCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_StatusCb afpcBBTRCoreStatus, void* apUserData);

/* BTRCore_RegisterMediaStatusCallback - callback for media state changes */
enBTRCoreRet BTRCore_RegisterMediaStatusCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_MediaStatusCb afpcBBTRCoreMediaStatus, void* apUserData);

/* BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device */
enBTRCoreRet BTRCore_RegisterConnectionIntimationCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_ConnIntimCb afpcBBTRCoreConnAuth, void* apUserData);

/* BTRCore_RegisterConnectionAuthenticationCallback - callback for receiving a connection request from another device */
enBTRCoreRet BTRCore_RegisterConnectionAuthenticationCb (tBTRCoreHandle hBTRCore, fPtr_BTRCore_ConnAuthCb afpcBBTRCoreConnAuth, void* apUserData);

/* @} */    //BLUETOOTH_APIS

#ifdef __cplusplus
}
#endif

#endif // __BTR_CORE_H__
