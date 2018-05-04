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
/*
 * btrCore_bt_ifce_.h
 * DBus layer abstraction for BT functionality
 */

#ifndef __BTR_CORE_BT_IFCE_H__
#define __BTR_CORE_BT_IFCE_H__

#define BT_MAX_STR_LEN           256
#define BT_MAX_UUID_STR_LEN      64
#define BT_MAX_NUM_DEVICE        64
#define BT_MAX_DEVICE_PROFILE    32


/* Enum Types */
typedef enum _enBTDeviceType {
    enBTDevAudioSink,
    enBTDevAudioSource,
    enBTDevHFPHeadset,
    enBTDevHFPHeadsetGateway,
    enBTDevLE,
    enBTDevUnknown
} enBTDeviceType;

typedef enum _enBTDeviceClass {
    enBTDCTablet             = 0x11Cu,
    enBTDCSmartPhone         = 0x20Cu,
    enBTDCWearableHeadset    = 0x404u,
    enBTDCHandsfree          = 0x408u,
    enBTDCReserved           = 0x40Cu,
    enBTDCMicrophone         = 0x410u,
    enBTDCLoudspeaker        = 0x414u,
    enBTDCHeadphones         = 0x418u,
    enBTDCPortableAudio      = 0x41Cu,
    enBTDCCarAudio           = 0x420u,
    enBTDCSTB                = 0x424u,
    enBTDCHIFIAudioDevice    = 0x428u,
    enBTDCVCR                = 0x42Cu,
    enBTDCVideoCamera        = 0x430u,
    enBTDCCamcoder           = 0x434u,
    enBTDCVideoMonitor       = 0x438u,
    enBTDCTV                 = 0x43Cu,
    enBTDCVideoConference    = 0x440u,
    enBTDCUnknown            = 0x000u
} enBTDeviceClass;

typedef enum _enBTOpType {
    enBTAdapter,
    enBTDevice,
    enBTMediaTransport,
    enBTGattService,
    enBTGattCharacteristic,
    enBTGattDescriptor,
    enBTUnknown
} enBTOpIfceType;

typedef enum _enBTDeviceState {
    enBTDevStCreated,
    enBTDevStScanInProgress,
    enBTDevStFound,
    enBTDevStLost,
    enBTDevStPairingRequest,
    enBTDevStPairingInProgress,
    enBTDevStPaired,
    enBTDevStUnPaired,
    enBTDevStConnectInProgress,
    enBTDevStConnected,
    enBTDevStDisconnected,
    enBTDevStPropChanged,
    enBTDevStUnknown
} enBTDeviceState;

typedef enum _enBTAdapterOp {
    enBTAdpOpFindPairedDev,
    enBTAdpOpCreatePairedDev,
    enBTAdpOpRemovePairedDev,
    enBTAdpOpUnknown
} enBTAdapterOp;

typedef enum _enBTLeGattOp {
    enBTLeGattOpReadValue,
    enBTLeGattOpWriteValue,
    enBTLeGattOpStartNotify,
    enBTLeGattOpStopNotify,
    enBTLeGattOpUnknown
} enBTLeGattOp;


typedef enum _enBTAdapterProp {
    enBTAdPropName,
    enBTAdPropAddress,
    enBTAdPropPowered,
    enBTAdPropDiscoverable,
    enBTAdPropDiscoverableTimeOut,
    enBTAdPropUnknown
} enBTAdapterProp;

typedef enum _enBTDeviceProp {
    enBTDevPropPaired,
    enBTDevPropConnected,
    enBTDevPropVendor,
    enBTDevPropUnknown
} enBTDeviceProp;

typedef enum _enBTMediaTransportProp {
    enBTMedTPropDelay,
    enBTMedTPropUnknown
} enBTMediaTransportProp;

typedef enum _enBTGattServiceProp {
    enBTGattSPropUUID,
    enBTGattSPropPrimary,
    enBTGattSPropDevice,
    enBTGattSPropUnknown
} enBTGattServiceProp;

typedef enum _enBTGattCharProp {
    enBTGattCPropUUID,
    enBTGattCPropService,
    enBTGattCPropValue,
    enBTGattCPropNotifying,
    enBTGattCPropFlags,
    enBTGattCPropUnknown
} enBTGattCharProp;

typedef enum _enBTGattDescProp {
    enBTGattDPropUUID,
    enBTGattDPropCharacteristic,
    enBTGattDPropValue,
    enBTGattDPropFlags,
    enBTGattDPropUnknown
} enBTGattDescProp;

typedef enum _enBTMediaTransportState {
    enBTMTransportStNone,
    enBTMTransportStIdle,           /* Not Streaming and not Acquired                      */
    enBTMTransportStPending,        /* Streaming, but not acquire - acquire() to be called */
    enBTMTransportStActive          /* Streaming and Acquired                              */
} enBTMediaTransportState;

typedef enum _enBTMediaStatusUpdate { 
    enBTMediaTransportUpdate,       /* Transport path  Add/Rem        */
    enBTMediaPlayerUpdate,          /* MediaPlayer     Add/Rem        */
    enBTMediaTrackUpdate,           /* Track change    Add/Rem        */
    enBTMediaPlaylistUpdate,        /* NowPlaying list Add/Rem        */
    enBTMediaBrowserUpdate          /* Media Browser   Add/Rem        */
} enBTMediaStatusUpdate;

typedef enum _enBTMediaControl {
    enBTMediaCtrlPlay,
    enBTMediaCtrlPause,
    enBTMediaCtrlStop,
    enBTMediaCtrlNext,
    enBTMediaCtrlPrevious,
    enBTMediaCtrlFastForward,
    enBTMediaCtrlRewind,
    enBTMediaCtrlVolumeUp,
    enBTMediaCtrlVolumeDown
} enBTMediaControl;


/* Union Types */
typedef union _unBTOpIfceProp {
    enBTAdapterProp         enBtAdapterProp;
    enBTDeviceProp          enBtDeviceProp;
    enBTMediaTransportProp  enBtMediaTransportProp;
    enBTGattServiceProp     enBtGattServiceProp;
    enBTGattCharProp        enBtGattCharProp;
    enBTGattDescProp        enBtGattDescProp;
    // Add other enums which define the required properties 
} unBTOpIfceProp;


/* Structure Types */
typedef struct _stBTDeviceInfo {
    int             bPaired;
    int             bConnected;
    int             bTrusted;
    int             bBlocked;
    unsigned short  ui16Vendor;
    unsigned short  ui16VendorSource;
    unsigned short  ui16Product;
    unsigned short  ui16Version;
    unsigned int    ui32Class;
    int             i32RSSI;
    char            pcName[BT_MAX_STR_LEN];
    char            pcAddress[BT_MAX_STR_LEN];
    char            pcAlias[BT_MAX_STR_LEN];
    char            pcIcon[BT_MAX_STR_LEN];
    char            aUUIDs[BT_MAX_DEVICE_PROFILE][BT_MAX_UUID_STR_LEN];
    char            pcDevicePrevState[BT_MAX_STR_LEN];
    char            pcDeviceCurrState[BT_MAX_STR_LEN];
    char            pcDevicePath[BT_MAX_STR_LEN];
    // TODO: Array of objects Services;
    // TODO: Array of objects Nodes;
} stBTDeviceInfo;

typedef struct _stBTPairedDeviceInfo {
    unsigned short  numberOfDevices;
    char            devicePath[BT_MAX_NUM_DEVICE][BT_MAX_STR_LEN];
    stBTDeviceInfo  deviceInfo[BT_MAX_NUM_DEVICE];
} stBTPairedDeviceInfo;

typedef struct _stBTDeviceSupportedService {
    unsigned int    uuid_value;
    char            profile_name[BT_MAX_STR_LEN];
} stBTDeviceSupportedService;

typedef struct _stBTDeviceSupportedServiceList {
    int                         numberOfService;
    stBTDeviceSupportedService  profile[BT_MAX_DEVICE_PROFILE];
} stBTDeviceSupportedServiceList;

typedef struct _stBTMediaTrackInfo {
    char            pcAlbum[BT_MAX_STR_LEN];
    char            pcGenre[BT_MAX_STR_LEN];
    char            pcTitle[BT_MAX_STR_LEN];
    char            pcArtist[BT_MAX_STR_LEN];
    unsigned int    ui32TrackNumber;
    unsigned int    ui32Duration;
    unsigned int    ui32NumberOfTracks;
} stBTMediaTrackInfo;

typedef struct _stBTMediaStatusUpdate {
    enBTMediaStatusUpdate  aeBtMediaStatus;

    union {
      enBTMediaTransportState m_mediaTransportState;
      stBTMediaTrackInfo*     m_mediaTrackInfo;
      //MediaBrowser
      //Playlist
    };
} stBTMediaStatusUpdate;



/* Fptr Callbacks types */
typedef int (*fPtr_BtrCore_BTDevStatusUpdateCb)(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);
typedef int (*fPtr_BtrCore_BTMediaStatusUpdateCb)(enBTDeviceType aeBtDeviceType, stBTMediaStatusUpdate* apstBtMediaStUpdate, const char* apcBtDevAddr, void* apUserData);
typedef int (*fPtr_BtrCore_BTNegotiateMediaCb)(void* apBtMediaCapsInput, void** appBtMediaCapsOutput, void* apUserData);
typedef int (*fPtr_BtrCore_BTTransportPathMediaCb)(const char* apBtMediaTransportPath, void* apBtMediaCaps, void* apUserData);
typedef int (*fPtr_BtrCore_BTMediaPlayerPathCb)(const char* apcBTMediaPlayerPath, void* apUserData);
typedef int (*fPtr_BtrCore_BTConnIntimCb)(enBTDeviceType aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, unsigned int aui32devPassKey, void* apUserData);
typedef int (*fPtr_BtrCore_BTConnAuthCb)(enBTDeviceType aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);
typedef int (*fPtr_BtrCore_BTLeGattPathCb)(enBTOpIfceType enBtOpIfceType, const char* apBtGattPath, const char* apcBtDevAddr, enBTDeviceState aenBTDeviceState, void* apConnHdl, void* apUserData);

//callback to process connection requests:
int (*p_ConnAuth_callback) ();

/* Interfaces */
void* BtrCore_BTInitGetConnection (void);
int   BtrCore_BTDeInitReleaseConnection (void* apBtConn);
char* BtrCore_BTGetAgentPath (void* apBtConn);
int   BtrCore_BTReleaseAgentPath (void* apBtConn);
int   BtrCore_BTRegisterAgent (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath, const char *capabilities);
int   BtrCore_BTUnregisterAgent (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTGetAdapterList (void* apBtConn, unsigned int *apBtNumAdapters, char** apcArrBtAdapterPath);
char* BtrCore_BTGetAdapterPath (void* apBtConn, const char* apBtAdapter);
int   BtrCore_BTReleaseAdapterPath (void* apBtConn, const char* apBtAdapter);
int   BtrCore_BTGetIfceNameVersion (void* apBtConn, char* apBtOutIfceName, char* apBtOutVersion);
int   BtrCore_BTGetProp (void* apBtConn, const char* apcOpIfcePath, enBTOpIfceType aenBtOpIfceType, unBTOpIfceProp aunBtOpIfceProp, void* apvVal);
int   BtrCore_BTSetProp (void* apBtConn, const char* apcOpIfcePath, enBTOpIfceType aenBtOpIfceType, unBTOpIfceProp aunBtOpIfceProp, void* apvVal);
int   BtrCore_BTStartDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStopDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStartLEDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStopLEDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTGetPairedDevices (void* apBtConn, const char* apBtAdapter, unsigned int* apui32PairedDevCnt, char** apcArrPairedDevPath);
int   BtrCore_BTGetPairedDeviceInfo (void* apBtConn, const char* apBtAdapter, stBTPairedDeviceInfo *pPairedDeviceInfo);
int   BtrCore_BTDiscoverDeviceServices (void* apBtConn, const char* apcDevPath, stBTDeviceSupportedServiceList *pProfileList);
int   BtrCore_BTFindServiceSupported(void* apBtConn, const char* apcDevPath, const char* apcSearchString, char* apcDataString);
int   BtrCore_BTPerformAdapterOp (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath, const char* apcDevPath, enBTAdapterOp aenBTAdpOp);
int   BtrCore_BTIsDeviceConnectable (void* apBtConn, const char* apcDevPath);
int   BtrCore_BTConnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);
int   BtrCore_BTDisconnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);
int   BtrCore_BTRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType, void* apBtUUID,
                                void* apBtMediaCodec, void* apBtMediaCapabilities, int apBtMediaCapabilitiesSize,int abBtMediaDelayReportEnable);
int   BtrCore_BTUnRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType);
int   BtrCore_BTAcquireDevDataPath (void* apBtConn, char* apcDevTransportPath, int* dataPathFd, int* dataReadMTU, int* dataWriteMTU);
int   BtrCore_BTReleaseDevDataPath (void* apBtConn, char* apcDevTransportPath);
// AVRCP Interfaces
int   BtrCore_BTDevMediaControl (void* apBtConn, const char* apmediaPlayerPath, enBTMediaControl  aenBTMediaOper);
char* BtrCore_BTGetMediaPlayerPath (void* apBtConn, const char* apBtDevPath);
int   BtrCore_BTGetMediaPlayerProperty (void* apBtConn, const char* apBtObjectPath, const char* mediaProperty, void* mediaPropertyValue);
int   BtrCore_BTGetTransportState (void* apBtConn, const char* apBtDataPath, void* state);
int   BtrCore_BTSetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty, char* pValue);
int   BtrCore_BTGetTrackInformation (void* apBtConn, const char* apBtmediaPlayerObjectPath, stBTMediaTrackInfo* lpstBTMediaTrackInfo);

int   BtrCore_BTSendReceiveMessages (void* apBtConn);
// Outgoing callbacks Registration Interfaces
int   BtrCore_BTRegisterDevStatusUpdateCb (void* apBtConn, fPtr_BtrCore_BTDevStatusUpdateCb afpcBDevStatusUpdate, void* apUserData);
int   BtrCore_BTRegisterMediaStatusUpdateCb (void* apBtConn, fPtr_BtrCore_BTMediaStatusUpdateCb afpcBMediaStatusUpdate, void* apUserData);
int   BtrCore_BTRegisterConnIntimationCb (void* apBtConn, fPtr_BtrCore_BTConnIntimCb afpcBConnIntim, void* apUserData);
int   BtrCore_BTRegisterConnAuthCb (void* apBtConn, fPtr_BtrCore_BTConnAuthCb afpcBConnAuth, void* apUserData);
int   BtrCore_BTRegisterNegotiateMediaCb (void* apBtConn, const char* apBtAdapter,
                                            fPtr_BtrCore_BTNegotiateMediaCb afpcBNegotiateMedia, void* apUserData);
int   BtrCore_BTRegisterTransportPathMediaCb (void* apBtConn, const char* apBtAdapter,
                                                fPtr_BtrCore_BTTransportPathMediaCb afpcBTransportPathMedia, void* apUserData);
int   BtrCore_BTRegisterMediaPlayerPathCb (void* apBtConn, const char* apBtAdapter,
                                                fPtr_BtrCore_BTMediaPlayerPathCb afpcBTMediaPlayerPath, void* apUserData); 
/******************************************
*    LE Functions
*******************************************/
int   BtrCore_BTPerformLeGattOp (void* apBtConn, const char* apBtLePath, enBTOpIfceType aenBTOpIfceType, enBTLeGattOp aenBTLeGattOp, void* apLeGatOparg, void* rpLeOpRes);
int   BtrCore_BTRegisterLEGattInfoCb (void* apBtConn, const char* apBtAdapter, fPtr_BtrCore_BTLeGattPathCb afpcBLeGattPath, void* apUserData);
#endif // __BTR_CORE_BT_IFCE_H__
