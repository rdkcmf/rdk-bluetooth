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
 * @file btrCore_bt_ifce_.h
 * DBus layer abstraction for BT functionality
 */


#ifndef __BTR_CORE_BT_IFCE_H__
#define __BTR_CORE_BT_IFCE_H__

/**
 * @addtogroup BLUETOOTH_TYPES
 * @{
 */

/**
 * @brief Bluetooth max string length.
 *
 */
#define BT_MAX_STR_LEN           256

/**
 * @brief Bluetooth max device path length.
 *
 */
#define BT_MAX_DEV_PATH_LEN      64     //BT_MAX_STR_LEN/4

/**
 * @brief Bluetooth max uuid length.
 *
 * The data type uuid stores Universally Unique Identifiers (UUID) as defined by RFC 4122, 
 * ISO/IEC 9834-8:2005, and related standards.
 */
#define BT_MAX_UUID_STR_LEN      64     //BT_MAX_STR_LEN/4

/**
 * @brief Bluetooth max number of devices that can be connected.
 */
#define BT_MAX_NUM_DEVICE        32

/**
 * @brief Bluetooth max number of device profiles that are allowed.
 * Device Profiles are definitions of possible applications and specify general behaviors that
 * Bluetooth enabled devices use to communicate with other Bluetooth devices.
 */
#define BT_MAX_DEVICE_PROFILE    32

/**
 * @brief Bluetooth max number of bluetooth adapters that can be connected over USB/UART
 */
#define BT_MAX_NUM_ADAPTERS      4

/**
 * @brief Bluetooth Advertising Service Data max length.
 */
#define BT_MAX_SERVICE_DATA_LEN        32


/**
 * @brief Bluetooth A2DP Source UUID
 */
#define BT_UUID_A2DP_SOURCE     "0000110a-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth A2DP Sink UUID
 */
#define BT_UUID_A2DP_SINK       "0000110b-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth LE Tile 1 UUID
 */
#define BT_UUID_GATT_TILE_1     "0000feed-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth LE Tile 2 UUID
 */
#define BT_UUID_GATT_TILE_2     "0000feec-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth LE Tile 2 UUID
 */
#define BT_UUID_GATT_TILE_3     "0000febe-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth Hands Free Audio Gateway UUID
 */
#define BT_UUID_HFP_AG          "0000111f-0000-1000-8000-00805f9b34fb"

/**
 * @brief Bluetooth Hands Free Headset UUID
 */
#define BT_UUID_HFP_HS          "0000111e-0000-1000-8000-00805f9b34fb"


/**
 * @brief Bluetooth Media Codec SBC - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_SBC      0x00

/**
 * @brief  Bluetooth Media Codec MPEG12 - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_MPEG12   0x01

/**
 * @brief  Bluetooth Media Codec MPEG24 - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_MPEG24   0x02

/**
 * @brief  Bluetooth Media Codec ATRAC - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_ATRAC    0x03
/**
 * @brief  Bluetooth Media Codec Vendor - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_VENDOR   0xFF

/**
 * @brief  Bluetooth Media Codec PCM - Must be same as the Ifce
 */
#define BT_MEDIA_CODEC_PCM      0x00


#define BT_MAX_NUM_GATT_SERVICE     4
#define BT_MAX_NUM_GATT_CHAR        10
#define BT_MAX_NUM_GATT_DESC        4
#define BT_MAX_GATT_OP_DATA_LEN     BT_MAX_STR_LEN * 3


 /* Characteristic Property bit field and Characteristic Extented Property bit field Values */
#define BT_GATT_CHAR_FLAG_READ                         1 << 0
#define BT_GATT_CHAR_FLAG_WRITE                        1 << 1
#define BT_GATT_CHAR_FLAG_ENCRYPT_READ                 1 << 2
#define BT_GATT_CHAR_FLAG_ENCRYPT_WRITE                1 << 3
#define BT_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ   1 << 4
#define BT_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE  1 << 5
#define BT_GATT_CHAR_FLAG_SECURE_READ                  1 << 6          /* Server Mode only */
#define BT_GATT_CHAR_FLAG_SECURE_WRITE                 1 << 7          /* Server Mode only */
#define BT_GATT_CHAR_FLAG_NOTIFY                       1 << 8
#define BT_GATT_CHAR_FLAG_INDICATE                     1 << 9
#define BT_GATT_CHAR_FLAG_BROADCAST                    1 << 10
#define BT_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE       1 << 11
#define BT_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES  1 << 12
#define BT_GATT_CHAR_FLAG_RELIABLE_WRITE               1 << 13
#define BT_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES         1 << 14


#define BT_MAX_NUM_GATT_CHAR_FLAGS                     15
#define BT_MAX_NUM_GATT_DESC_FLAGS                     8

typedef unsigned long long int tBTMediaItemId;
/* Enum Types */
/**
 * @brief Bluetooth device types.
 *
 * This enumeration lists different bluetooth device types.
 */
typedef enum _enBTDeviceType {
    enBTDevAudioSink,
    enBTDevAudioSource,
    enBTDevHFPHeadset,
    enBTDevHFPAudioGateway,
    enBTDevLE,
    enBTDevHID,
    enBTDevUnknown
} enBTDeviceType;

/**
 * @brief Bluetooth device classes.
 *
 * This enumeration lists different bluetooth devices that represent the class of device (CoD) 
 * record as defined by the Bluetooth specification.
 */
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
    enBTDCKeyboard           = 0x540u,
    enBTDCMouse              = 0x580u,
    enBTDCMouseKeyBoard      = 0x5C0u,
    enBTDCJoystick           = 0x504u,
    enBTDCGamePad            = 0x508u,
    enBTDCAudioRemote        = 0x50Cu,

    enBTDCUnknown            = 0x000u
} enBTDeviceClass;

/**
 * @brief Bluetooth device operation types.
 *
 * This enumeration lists different operations a bluetooth device serves.
 */
typedef enum _enBTOpType {
    enBTAdapter,
    enBTDevice,
    enBTMediaTransport,
    enBTMediaControl,
    enBTMediaPlayer,
    enBTMediaItem,
    enBTMediaFolder,
    enBTGattService,
    enBTGattCharacteristic,
    enBTGattDescriptor,
    enBTAdvertisement,
    enBTUnknown
} enBTOpIfceType;

/**
 * @brief Bluetooth device state.
 *
 * This enumeration lists different states of a bluetooth device.
 */
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
    enBTDevStRSSIUpdate,
    enBTDevStUnknown
} enBTDeviceState;

/**
 * @brief Bluetooth Adapter operations.
 *
 * This enumeration lists different operations a bluetooth adapter serves.
 */
typedef enum _enBTAdapterOp {
    enBTAdpOpFindPairedDev,
    enBTAdpOpCreatePairedDev,
    enBTAdpOpCreatePairedDevASync,
    enBTAdpOpRemovePairedDev,
    enBTAdpOpUnknown
} enBTAdapterOp;

/**
 * @brief Bluetooth Gatt operations.
 *
 * This enumeration lists different operations a bluetooth Gatt serves.
 */
typedef enum _enBTLeGattOp {
    enBTLeGattOpReadValue,
    enBTLeGattOpWriteValue,
    enBTLeGattOpStartNotify,
    enBTLeGattOpStopNotify,
    enBTLeGattOpUnknown
} enBTLeGattOp;

/**
 * @brief Bluetooth adapter properties.
 *
 * This enumeration lists different properties a  bluetooth adapters possess.
 */
typedef enum _enBTAdapterProp {
    enBTAdPropName,
    enBTAdPropAddress,
    enBTAdPropPowered,
    enBTAdPropDiscoverable,
    enBTAdPropDiscoverableTimeOut,
    enBTAdPropDiscoveryStatus,
    enBTAdPropPairable,
    enBTAdPropUnknown
} enBTAdapterProp;

/**
 * @brief Bluetooth device properties.
 *
 * This enumeration lists different properties a  bluetooth device possesses.
 */
typedef enum _enBTDeviceProp {
    enBTDevPropPaired,
    enBTDevPropConnected,
    enBTDevPropVendor,
    enBTDevPropSrvRslvd,
    enBTDevPropUnknown
} enBTDeviceProp;

/**
 * @brief Bluetooth Media transport properties.
 *
 * This enumeration lists the transport properties of bluetooth media.
 */
typedef enum _enBTMediaTransportProp {
    enBTMedTPropDelay,
    enBTMedTPropState,
    enBTMedTPropVol,
    enBTMedTPropUnknown
} enBTMediaTransportProp;

/**
 * @brief Bluetooth Media Control Properties.
 *
 * This enumeration lists the property updates of a bluetooth media control.
 */
typedef enum _enBTMediaControlProp {
    enBTMedControlPropConnected,
    enBTMedControlPropPath,
    enBTMedControlUnknown
} enBTMediaControlProp;

/**
 * @brief Bluetooth Media Player Properties.
 *
 * This enumeration lists the property updates of a bluetooth media player.
 */
typedef enum _enBTMediaPlayerProp {

    enBTMedPlayerPropName,
    enBTMedPlayerPropType,
    enBTMedPlayerPropSubtype,
    enBTMedPlayerPropEqualizer,
    enBTMedPlayerPropShuffle,
    enBTMedPlayerPropScan,
    enBTMedPlayerPropRepeat,
    enBTMedPlayerPropPosition,
    enBTMedPlayerPropStatus,
    enBTMedPlayerPropTrack,
    enBTMedPlayerPropBrowsable,
    enBTMedPlayerPropSearchable,
    enBTMedPlayerPropPlaylist,
    enBTMedPlayerPropUnknown
} enBTMediaPlayerProp;

/**
 * @brief Bluetooth Media Folder Properties.
 *
 * This enumeration lists the property updates of a bluetooth media folder.
 */
typedef enum _enBTMediaFolderProp {
    enBTMedFolderPropName,
    enBTMedFolderPropNumberOfItems
} enBTMediaFolderProp;

/**
 * @brief Bluetooth Gatt service properties.
 *
 * This enumeration lists the properties of bluetooth Gatt services.
 */
typedef enum _enBTGattServiceProp {
    enBTGattSPropUUID,
    enBTGattSPropPrimary,
    enBTGattSPropDevice,
    enBTGattSPropServPath,
    enBTGattSPropUnknown
} enBTGattServiceProp;

/**
 * @brief Bluetooth Gatt characteristic  properties.
 *
 * This enumeration lists the properties of bluetooth Gatt characteristics.
 */
typedef enum _enBTGattCharProp {
    enBTGattCPropUUID,
    enBTGattCPropService,
    enBTGattCPropValue,
    enBTGattCPropNotifying,
    enBTGattCPropFlags,
    enBTGattCPropUnknown
} enBTGattCharProp;

/**
 * @brief Bluetooth Gatt characteristic Flag properties.
 *
 * This enumeration lists the flags of bluetooth Gatt characteristics.
 */
typedef enum _enBTGattCharFlagProp {
    enBTGattCharFlagRead,
    enBTGattCharFlagWrite,
    enBTGattCharFlagEncryptRead,
    enBTGattCharFlagEncryptWrite,
    enBTGattCharFlagEncryptAuthenticatedRead,
    enBTGattCharFlagEncryptAuthenticatedWrite,
    enBTGattCharFlagSecureRead,
    enBTGattCharFlagSecureWrite,
    enBTGattCharFlagNotify,
    enBTGattCharFlagIndicate,
    enBTGattCharFlagBroadcast,
    enBTGattCharFlagWriteWithoutResponse,
    enBTGattCharFlagAuthenticatedSignedWrites,
    enBTGattCharFlagReliableWrite,
    enBTGattCharFlagWritableAuxiliaries,
    enBTGattCharFlagUnknown
}enBTGattCharFlagProp;

/**
 * @brief Bluetooth Gatt Descriptor properties.
 *
 * This enumeration lists the properties of a bluetooth Gatt descriptors.
 */
typedef enum _enBTGattDescProp {
    enBTGattDPropUUID,
    enBTGattDPropCharacteristic,
    enBTGattDPropValue,
    enBTGattDPropFlags,
    enBTGattDPropUnknown
} enBTGattDescProp;

/**
 * @brief Bluetooth Media transport states.
 *
 * This enumeration lists the transport states of a bluetooth media.
 */
typedef enum _enBTMediaTransportState {
    enBTMedTransportStNone,
    enBTMedTransportStIdle,           /* Not Streaming and not Acquired                      */
    enBTMedTransportStPending,        /* Streaming, but not acquire - acquire() to be called */
    enBTMedTransportStActive          /* Streaming and Acquired                              */
} enBTMediaTransportState;

/**
 * @brief Bluetooth Media Player Status.
 *
 * This enumeration lists the status of the media player.
 */
typedef enum _enBTMediaPlayerStatus {
    enBTMedPlayerStPlaying,
    enBTMedPlayerStStopped,
    enBTMedPlayerStPaused,
    enBTMedPlayerStForwardSeek,
    enBTMedPlayerStReverseSeek,
    enBTMedPlayerStError
} enBTMediaPlayerStatus;

/**
 * @brief Bluetooth Media Players' Equalizer State.
 *
 * This enumeration lists the equalizer states of the media player.
 */
typedef enum _enBTMediaPlayerEqualizer {
    enBTMedPlayerEqualizerOff,
    enBTMedPlayerEqualizerOn
} enBTMediaPlayerEqualizer;

/**
 * @brief Bluetooth Media Players' Shuffle State.
 *
 * This enumeration lists the shuffle states of the media player.
 */
typedef enum _enBTMediaPlayerShuffle {
    enBTMedPlayerShuffleOff,
    enBTMedPlayerShuffleAllTracks,
    enBTMedPlayerShuffleGroup
} enBTMediaPlayerShuffle;

/**
 * @brief Bluetooth Media Players' Scan State.
 *
 * This enumeration lists the scan states of the media player.
 */
typedef enum _enBTMediaPlayerScan {
    enBTMedPlayerScanOff,
    enBTMedPlayerScanAllTracks,
    enBTMedPlayerScanGroup
} enBTMediaPlayerScan;

/**
 * @brief Bluetooth Media Players' Repeat States.
 *
 * This enumeration lists the repeat states of the media player.
 */
typedef enum _enBTMediaPlayerRepeat {
    enBTMedPlayerRpOff,
    enBTMedPlayerRpSingleTrack,
    enBTMedPlayerRpAllTracks,
    enBTMedPlayerRpGroup
} enBTMediaPlayerRepeat;

/**
 * @brief Bluetooth Media Players' Type.
 *
 * This enumeration lists the type of the media player.
 */
typedef enum _enBTMediaPlayerType {
    enBTMedPlayerTypAudio,
    enBTMedPlayerTypVideo,
    enBTMedPlayerTypAudioBroadcasting,
    enBTMedPlayerTypVideoBroadcasting
} enBTMediaPlayerType;

/**
 * @brief Bluetooth Media Players' Subtype.
 *
 * This enumeration lists the subtype of the media player.
 */
typedef enum _enBTMediaPlayerSubtype {
    enBTMedPlayerSbTypAudioBook,
    enBTMedPlayerSbTypPodcast
} enBTMediaPlayerSubtype;

/**
 * @brief Bluetooth Media Items' Type.
 *
 * This enumeration lists the type of media item.
 */
typedef enum _enBTMediaItemType {
    enBTMediaItemTypUnknown,
    enBTMediaItemTypAudio,
    enBTMediaItemTypVideo,
    enBTMediaItemTypFolder
} enBTMediaItemType;

#if 0
/**
 * @brief Bluetooth Media Status updates.
 *
 * This enumeration lists the status updates of a bluetooth media.
 */
typedef enum _enBTMediaStatusUpdate { 
    enBTMediaTransportUpdate,       /* Transport path  Add/Rem        */
    enBTMediaPlayerUpdate,          /* MediaPlayer     Add/Rem        */
    enBTMediaPlaylistUpdate,        /* NowPlaying list Add/Rem        */
    enBTMediaBrowserUpdate          /* Media Browser   Add/Rem        */
} enBTMediaStatusUpdate;
#endif

/**
 * @brief Bluetooth Media types.
 *
 * This enumeration lists different Bluetooth Media types.
 */
typedef enum _enBTMediaType {
    enBTMediaTypePCM,
    enBTMediaTypeSBC,
    enBTMediaTypeMP3,
    enBTMediaTypeAAC,
    enBTMediaTypeUnknown
} enBTMediaType;

/**
 * @brief Bluetooth Media Controls.
 *
 * This enumeration lists the properties of a bluetooth media transport.
 */
typedef enum _enBTMediaControlCmd {
    enBTMediaCtrlPlay,
    enBTMediaCtrlPause,
    enBTMediaCtrlStop,
    enBTMediaCtrlNext,
    enBTMediaCtrlPrevious,
    enBTMediaCtrlFastForward,
    enBTMediaCtrlRewind,
    enBTMediaCtrlVolumeUp,
    enBTMediaCtrlVolumeDown,
    enBTMediaCtrlEqlzrOff,
    enBTMediaCtrlEqlzrOn,
    enBTMediaCtrlShflOff,
    enBTMediaCtrlShflAllTracks,
    enBTMediaCtrlShflGroup,
    enBTMediaCtrlRptOff,
    enBTMediaCtrlRptSingleTrack,
    enBTMediaCtrlRptAllTracks,
    enBTMediaCtrlRptGroup,
    enBTMediaCtrlUnknown
} enBTMediaControlCmd;

typedef enum _enBTMediaFolderType {
    enBTMediaFldTypAlbum,
    enBTMediaFldTypArtist,
    enBTMediaFldTypGenre,
    enBTMediaFldTypCompilation,
    //enBTMediaFldTypMixed,
    enBTMediaFldTypPlayList,
    enBTMediaFldTypTrackList,
    enBTMediaFldTypTrack
} enBTMediaFolderType;

/* Union Types */
typedef union _unBTOpIfceProp {
    enBTAdapterProp         enBtAdapterProp;
    enBTDeviceProp          enBtDeviceProp;
    enBTMediaTransportProp  enBtMediaTransportProp;
    enBTMediaControlProp    enBtMediaControlProp;
    enBTMediaPlayerProp     enBtMediaPlayerProp;
    //enBTMediaItemProp       enBtMediaItemProp;
    enBTMediaFolderProp     enBtMediaFolderProp;
    enBTGattServiceProp     enBtGattServiceProp;
    enBTGattCharProp        enBtGattCharProp;
    enBTGattDescProp        enBtGattDescProp;
    // Add other enums which define the required properties 
} unBTOpIfceProp;


/* Structure Types */
typedef struct _stBTAdapterInfo {
    char            pcAddress[BT_MAX_STR_LEN];                              // Bluetooth device address
    char            pcName[BT_MAX_STR_LEN];                                 // Bluetooth system name (pretty hostname)
    char            pcAlias[BT_MAX_STR_LEN];                                // Bluetooth friendly name
    unsigned int    ui32Class;                                              // Bluetooth class of device
    int             bPowered;                                               // Bluetooth adapter 'powered' state
    int             bDiscoverable;                                          // Bluetooth adapter visible/hidden
    int             bPairable;                                              // Bluetooth adapter pairable or non-pairable
    unsigned int    ui32PairableTimeout;                                    // 0 (default) = timeout disabled; stays in pairable mode forever
    unsigned int    ui32DiscoverableTimeout;                                // default = 180s (3m). 0 = timeout disabled; stays in discoverable/limited mode forever
    int             bDiscovering;                                           // device discovery procedure active?
    char            ppcUUIDs[BT_MAX_DEVICE_PROFILE][BT_MAX_UUID_STR_LEN];   // List of 128-bit UUIDs that represents the available local services
    char            pcModalias[BT_MAX_STR_LEN];                             // Local Device ID information in modalias format used by the kernel and udev
    char            pcPath[BT_MAX_DEV_PATH_LEN];                            // Bluetooth adapter path
} stBTAdapterInfo;

typedef struct _stBTAdServiceData {
    char            pcUUIDs[BT_MAX_UUID_STR_LEN];;
    uint8_t         pcData[BT_MAX_SERVICE_DATA_LEN];
    size_t          len;
} stBTAdServiceData;

typedef struct _stBTDeviceInfo {
    int             bPaired;
    int             bConnected;
    int             bTrusted;
    int             bBlocked;
    int             bServiceResolved;
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
    char            pcDevicePath[BT_MAX_DEV_PATH_LEN];
    stBTAdServiceData  saServices[BT_MAX_DEVICE_PROFILE];
    // TODO: Array of objects Services;
    // TODO: Array of objects Nodes;
} stBTDeviceInfo;

typedef struct _stBTPairedDeviceInfo {
    unsigned short  numberOfDevices;
    char            devicePath[BT_MAX_NUM_DEVICE][BT_MAX_DEV_PATH_LEN];
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
    enBTOpIfceType          aenBtOpIfceType;
    unBTOpIfceProp          aunBtOpIfceProp;

    union {
      enBTMediaTransportState   m_mediaTransportState;
      unsigned char             m_mediaTransportVolume;
      enBTMediaPlayerType       enMediaPlayerType;
      enBTMediaPlayerSubtype    enMediaPlayerSubtype;
      enBTMediaPlayerEqualizer  enMediaPlayerEqualizer;
      enBTMediaPlayerShuffle    enMediaPlayerShuffle;
      enBTMediaPlayerScan       enMediaPlayerScan;
      enBTMediaPlayerRepeat     enMediaPlayerRepeat;
      enBTMediaPlayerStatus     enMediaPlayerStatus;
      unsigned char             m_mediaPlayerBrowsable;
      unsigned char             m_mediaPlayerSearchable;
      unsigned char             m_mediaPlayerConnected;
      unsigned int              m_mediaPlayerPosition;
      stBTMediaTrackInfo        m_mediaTrackInfo;
      char                      m_mediaPlayerPath[BT_MAX_STR_LEN];
      char                      m_mediaPlayerName[BT_MAX_STR_LEN];
      char                      m_mediaFolderName[BT_MAX_STR_LEN];
      unsigned int              m_mediaFolderNumberOfItems;
    };
} stBTMediaStatusUpdate;

typedef struct _stBTMediaBrowserUpdate {
    char                    pcMediaItemName[BT_MAX_STR_LEN];
    char                    pcMediaItemPath[BT_MAX_STR_LEN];
    enBTMediaItemType       eMediaItemType;
    enBTMediaFolderType     eMediaFolderType;
    tBTMediaItemId          ui32BTMediaItemId;
    unsigned int            ui32NumberOfItems;
    stBTMediaTrackInfo      mediaTrackInfo;
} stBTMediaBrowserUpdate;


/* GattDescriptor1 Properties */
typedef struct _stBTLeGattDesc {
    char                    descPath[BT_MAX_STR_LEN];                       /* Descriptor Path */
    char                    descUuid[BT_MAX_UUID_STR_LEN];                  /* 128-bit service UUID */
    unsigned short          descFlags;                                      /* Descriptor Flags - bit field values */
    char                    propertyValue[BT_MAX_GATT_OP_DATA_LEN];         /* value of the descriptor */
} stBTLeGattDesc;

/* GattCharacteristic1 Path and Properties */
typedef struct _stBTLeGattChar {
    char                    charPath[BT_MAX_STR_LEN];                       /* Characteristic Path */
    char                    charUuid[BT_MAX_UUID_STR_LEN];                  /* 128-bit service UUID */
    stBTLeGattDesc          atBTRGattDesc[BT_MAX_NUM_GATT_DESC];            /* Max of 8 Gatt Descriptor array */
    unsigned short          ui16NumberOfGattDesc;                           /* Number of Gatt Service ID */
    unsigned short          charFlags;                                      /* Characteristic Flags - bit field values */
    char                    value[BT_MAX_GATT_OP_DATA_LEN];                 /* value of the characteristic */
} stBTLeGattChar;

/* GattService Path and Properties */
typedef struct _stBTLeGattService {
    char                    servicePath[BT_MAX_STR_LEN];                    /* Service Path */
    char                    serviceUuid[BT_MAX_UUID_STR_LEN];               /* 128-bit service UUID */
    unsigned char           serviceType;                                    /* Primary(True) or secondary(False) gatt service*/
    stBTLeGattChar          astBTRGattChar[BT_MAX_NUM_GATT_CHAR];           /* Max of 6 Gatt Charactristic array */
    unsigned short          ui16NumberOfGattChar;                           /* Number of Gatt Charactristics */
} stBTLeGattService;

typedef struct _stBTLeGattInfo {
    stBTLeGattService       astBTRGattService[BT_MAX_NUM_GATT_SERVICE];
    int                     nNumGattServices;
}stBTLeGattInfo;

/* Advertisement data structure */
typedef struct _stBTLeManfData {
    unsigned short  ManfID;                                                 /* Manufacturer ID Key */
    unsigned int    lenManfData;                                            /* Length of data associated with the manufacturer ID */
    unsigned char   data[BT_MAX_GATT_OP_DATA_LEN];                          /* Data associated with the manufacturer ID */
} stBTLeManfData;

typedef struct _stBTLeServData {
    char    UUID[BT_MAX_STR_LEN];                                           /* UUID of the service data - Key */
    uint8_t data[BT_MAX_GATT_OP_DATA_LEN];                                  /* Data associated with the service UUID */
} stBTLeServData;

typedef struct _stBTLeCustomAdv {
    char            pui8AdvertPath[BT_MAX_STR_LEN];                         /* Service Path */
    char            AdvertisementType[BT_MAX_STR_LEN];                      /* Type of advertising packet : "Broadcast"/"Peripheral" */
    char            ServiceUUID[BT_MAX_NUM_GATT_SERVICE][BT_MAX_STR_LEN];   /* List of the UUIDs of the services supported by the device */
    int             numServiceUUID;                                         /* Number of Services supported by the device */
    char            SolicitUUID[BT_MAX_NUM_GATT_SERVICE][BT_MAX_STR_LEN];   /* Services the Peripheral device maybe interested in when it is a Gatt client too */
    int             numSolicitUUID;                                         /* Number of solicit services of the device */
    stBTLeManfData  ManfData;                                               /* Manufacturer Id and the data assosciated */
    stBTLeServData  ServiceData;                                            /* Arbitary data associated with the UUID */
    unsigned char   bTxPower;                                               /* Includes Tx power in advertisement */
} stBTLeCustomAdv;


/* Fptr Callbacks types */
typedef int (*fPtr_BtrCore_BTAdapterStatusUpdateCb)(enBTAdapterProp aeBtAdapterProp, stBTAdapterInfo* apstBTAdapterInfo, void* apUserData);
typedef int (*fPtr_BtrCore_BTDevStatusUpdateCb)(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);
typedef int (*fPtr_BtrCore_BTMediaStatusUpdateCb)(enBTDeviceType aeBtDeviceType, stBTMediaStatusUpdate* apstBtMediaStUpdate, const char* apcBtDevAddr, void* apUserData);
typedef int (*fPtr_BtrCore_BTNegotiateMediaCb)(void* apBtMediaCapsInput, void** appBtMediaCapsOutput, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apUserData);
typedef int (*fPtr_BtrCore_BTTransportPathMediaCb)(const char* apBtMediaTransportPath, const char* apBtMediaUUID, void* apBtMediaCaps, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apUserData);
typedef int (*fPtr_BtrCore_BTMediaPlayerPathCb)(const char* apcBTMediaPlayerPath, void* apUserData);
typedef int (*fPtr_BtrCore_BTConnIntimCb)(enBTDeviceType aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, unsigned int aui32devPassKey, unsigned char ucIsReqConfirmation, void* apUserData);
typedef int (*fPtr_BtrCore_BTConnAuthCb)(enBTDeviceType aeBtDeviceType, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);
typedef int (*fPtr_BtrCore_BTMediaBrowserPathCb)(stBTMediaBrowserUpdate* apstBtMediaBsrUpdate, unsigned char ucItemScope, const char* apcBtDevAddr, void* apUserData);
typedef int (*fPtr_BtrCore_BTLeGattPathCb)(enBTOpIfceType enBtOpIfceType, enBTLeGattOp aenGattOp, const char* apBtGattPath, const char* apcBtDevAddr, enBTDeviceState aenBTDeviceState, void* apLeCbData, void* apUserData);
typedef int (*fPtr_BtrCore_BTLeAdvertisementCb)(const char* apBtAdvPath, stBTLeCustomAdv** appstBtCoreLeCustomAdv, void* apUserData);
/* @} */ // End of group BLUETOOTH_TYPES

/**
 * @addtogroup BLUETOOTH_APIS
 * @{
 */

//callback to process connection requests:
int (*p_ConnAuth_callback) ();

/* Interfaces */

/**
 * @brief This API Initializes the Bluetooth core and fetches DBus connection and returns a handle to the instance.
 *
 * This handle will be used in all future communication with the Bluetooth core.
 *
 * @retval Returns NULL if DBus connection establishment fails.
 */
void* BtrCore_BTInitGetConnection (void);

/**
 * @brief This API DeInitializes the Bluetooth core and releases the DBus connection.
 *
 * This handle will be used in all future communication with the Bluetooth core.
 * This function will release the memory allocated toBT Adapters  & BT agent.
 *
 * @param[in]  apBtConn   The Dbus connection handle as returned by BtrCore_BTInitGetConnection. NULL is valid for this API.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTDeInitReleaseConnection (void* apBtConn);

/**
 * @brief  Using this API, a default Path is assigned to the  Bluetooth Agent.
 *
 * @param[in]  apBtConn   The Dbus connection handle as returned by BtrCore_BTInitGetConnection. NULL is valid for this API.
 *
 * @return  assigned bluetooth agent path is returned.
 */
char* BtrCore_BTGetAgentPath (void* apBtConn);

/**
 * @brief  Using this API the path assigned to the  Bluetooth Agent is released.
 *
 * @param[in]  apBtConn   The Dbus connection handle as returned by BtrCore_BTInitGetConnection. NULL is valid for this API.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTReleaseAgentPath (void* apBtConn);

/**
 * @brief  using this API, DBus object path is registered with bluetooth agent path and current adapter's path.
 *
 * @param[in]  apBtConn       The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[in]  apBtAdapter    The current bluetooth Adapter path.
 * @param[in]  apBtAgentPath  The bluetooth agent path.
 * @param[in]  capabilities   Bluetooth core capabilities.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTRegisterAgent (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath, const char *capabilities);

/**
 * @brief  Using this API, DBus object path is unregistered with bluetooth agent path and current adapter's path.
 *
 * @param[in]  apBtConn       The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[in]  apBtAdapter    The current bluetooth Adapter path.
 * @param[in]  apBtAgentPath  The bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
*/
int   BtrCore_BTUnregisterAgent (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API obtains adapter list from Dbus object path.
 *
 * @param[in]  apBtConn        The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[out] apBtNumAdapters Total number of bluetooth adapters available.
 * @param[out] apcArrBtAdapterPath  Array of bluetooth adapter paths.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
*/
int   BtrCore_BTGetAdapterList (void* apBtConn, unsigned int *apBtNumAdapters, char* apcArrBtAdapterPath[BT_MAX_NUM_ADAPTERS]);

/**
 * @brief  Using this API adapter path is fetched from Dbus object path.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Current bluetooth adapter whose path has to be fetched.
 *
 * @return  Retrieved bluetooth adapter path is returned.
 */
char* BtrCore_BTGetAdapterPath (void* apBtConn, const char* apBtAdapter);

/**
 * @brief  Using this API the path assigned to the current Bluetooth Adapter is released.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                               NULL is valid for this API.
 * @param[in]  apBtAdapter       Current bluetooth adapter whose path has to be released.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTReleaseAdapterPath (void* apBtConn, const char* apBtAdapter);

/**
 * @brief  using this API, Bluetooth interface version is obtained from bluetooth daemon of the kernel
 * and default name "Bluez" is assigned as interface name.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                               NULL is valid for this API.
 * @param[out] apBtOutIfceName   Bluetooth interface name that has to be fetched.
 * @param[out] apBtOutVersion    Bluetooth interface version that has to be fetched.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropiate error code otherwise.
 */
int   BtrCore_BTGetIfceNameVersion (void* apBtConn, char* apBtOutIfceName, char* apBtOutVersion);

/**
 * @brief  This API gets different properties of different BT devices and services.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                               NULL is valid for this API.
 * @param[in] apcOpIfcePath      Bluetooth interface path.
 * @param[in] aenBtOpIfceType    Bluetooth interface type.
 * @param[in] aunBtOpIfceProp    Bluetooth interface property whose property value has to be fetched.
 * @param[out] apvVal            Property value which has to be fetched from Dbus iterator.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetProp (void* apBtConn, const char* apcOpIfcePath, enBTOpIfceType aenBtOpIfceType, unBTOpIfceProp aunBtOpIfceProp, void* apvVal);

/**
 * @brief  This API sets different properties of different BT devices and services.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                               NULL is valid for this API.
 * @param[in] apcOpIfcePath      Bluetooth interface path.
 * @param[in] aenBtOpIfceType    Bluetooth interface type.
 * @param[in] aunBtOpIfceProp    Bluetooth interface property whose property value has to be fetched.
 * @param[in] apvVal             Property value which has to be set to Dbus iterator.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTSetProp (void* apBtConn, const char* apcOpIfcePath, enBTOpIfceType aenBtOpIfceType, unBTOpIfceProp aunBtOpIfceProp, void* apvVal);

/**
 * @brief  This API is used to discover the Bluetooth adapter.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth adapter.
 * @param[in] apBtAgentPath     Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStartDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API is used to stop discovering Bluetooth adapter.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth adapter.
 * @param[in] apBtAgentPath     Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStopDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API is used to discover the low energy  Bluetooth adapter.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth adapter.
 * @param[in] apBtAgentPath     Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStartLEDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API is used to stop discovering low energy Bluetooth adapter.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                               NULL is valid for this API.
 * @param[in] apBtAdapter        Bluetooth adapter.
 * @param[in] apBtAgentPath      Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStopLEDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API is used to discover the Classic Bluetooth Devices.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth adapter.
 * @param[in] apBtAgentPath     Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStartClassicDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API is used to stop discovering Classic Bluetooth Devices.
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                               NULL is valid for this API.
 * @param[in] apBtAdapter        Bluetooth adapter.
 * @param[in] apBtAgentPath      Bluetooth agent path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTStopClassicDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);

/**
 * @brief  This API fetches all the paired devices' paths and number of paired devices.
 *
 * @param[in]  apBtConn             The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                                  NULL is valid for this API.
 * @param[in]  apBtAdapter          Bluetooth adapter.
 * @param[out] apui32PairedDevCnt   Number of paired devices that are found.
 * @param[out] apcArrPairedDevPath  Array that stores the device paths of paired devices found.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetPairedDevices (void* apBtConn, const char* apBtAdapter, unsigned int* apui32PairedDevCnt, char** apcArrPairedDevPath);

/**
 * @brief  This API fetches all BT paired devices' device information.
 *
 * @param[in]  apBtConn            The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                 NULL is valid for this API.
 * @param[in]  apBtAdapter         Bluetooth adapter.
 * @param[out] pPairedDeviceInfo   A structure that fetches device information  of all the paired devices.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetPairedDeviceInfo (void* apBtConn, const char* apBtAdapter, stBTPairedDeviceInfo *pPairedDeviceInfo);

/**
 * @brief  This API is used to discover the supported services and fetch the profiles of all those devices.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 * @param[in] apcDevPath       Bluetooth device path.
 * @param[out] pProfileList    A structure that fetches Profile information  of all the supported services.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTDiscoverDeviceServices (void* apBtConn, const char* apcDevPath, stBTDeviceSupportedServiceList *pProfileList);

/**
 * @brief  This API is used to find all supported services and fetch the profiles of all those devices.
 *
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in]  apcDevPath       Bluetooth device path.
 * @param[in]  apcSearchString  Usually, the UUID that has to be searched for.
 * @param[out] apcDataString    The actual string that is fetched from Dbus iterator which matches the provided UUID.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropiate error code otherwise.
 */
int   BtrCore_BTFindServiceSupported(void* apBtConn, const char* apcDevPath, const char* apcSearchString, char* apcDataString);

/**
 * @brief  This API is used to perform BT adapter operations.
 *
 * @param[in] apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth Adapter
 * @param[in] apBtAgentPath     BT agent path.
 * @param[in] apcDevPath        Bluetooth device path.
 * @param[in] aenBTAdpOp        Adapter operation that has to be performed.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTPerformAdapterOp (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath, const char* apcDevPath, enBTAdapterOp aenBTAdpOp);

/**
 * @brief  This API is used to run device connectable command.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 * @param[in] apcDevPath       Bluetooth device path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTIsDeviceConnectable (void* apBtConn, const char* apcDevPath);

/**
 * @brief  This API is used to establish the connection with a BT device.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 * @param[in] apDevPath        Bluetooth device path.
 * @param[in] aenBTDevType     Bluetooth device type.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTConnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);

/**
 * @brief  This API is used to diconnect a BT device.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 * @param[in] apDevPath        Bluetooth device path.
 * @param[in] aenBTDevType     Bluetooth device type.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTDisconnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);

/**
 * @brief  This API is used to Enable Enhanced Retransmission on all BT adapter.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTEnableEnhancedRetransmissionMode (void* apstBtIfceHdl);

/**
 * @brief  This API is used to Disable Enhanced Retransmission on all BT adapter.
 *
 * @param[in] apBtConn         The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                             NULL is valid for this API.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTDisableEnhancedRetransmissionMode (void* apstBtIfceHdl);

/**
 * @brief  This API is used to register a media device.
 *
 * @param[in] apBtConn                    The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                        NULL is valid for this API.
 * @param[in] apBtAdapter                 Bluetooth device path.
 * @param[in] aenBTDevType                Device type.
 * @param[in] aenMediaType                Media codec type.
 * @param[in] apBtUUID                    Bluetooth UUID
 * @param[in] apBtMediaCapabilities       Media capabilities like frequency, block length, Min bitpool, Max bitpool etc.
 * @param[in] apBtMediaCapabilitiesSize   size of apBtMediaCapabilities.
 * @param[in] abBtMediaDelayReportEnable  Flag that indicates if any delay.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
*/
int   BtrCore_BTRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType, enBTMediaType aenBTMediaType,
                                const char* apBtUUID, void* apBtMediaCapabilities, int apBtMediaCapabilitiesSize,int abBtMediaDelayReportEnable);

/**
 * @brief  This API is used to unregister the media device.
 *
 * @param[in] apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                              NULL is valid for this API.
 * @param[in] apBtAdapter       Bluetooth device path.
 * @param[in] aenBTDevType      Bluetooth device type.
 * @param[in] aenMediaType      Media codec type.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTUnRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType, enBTMediaType aenBTMediaType);

/**
 * @brief  This API is used to acquire device data path.
 *
 * @param[in] apBtConn             The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                                 NULL is valid for this API.
 * @param[in] apcDevTransportPath  Bluetooth device transport path.
 * @param[out] dataPathFd          Data path file descriptor that has to be fetched.
 * @param[out] dataReadMTU         Fetches MTU of data reading.
 * @param[out] dataWriteMTU        Fetches MTU of data writing.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTAcquireDevDataPath (void* apBtConn, char* apcDevTransportPath, int* dataPathFd, int* dataReadMTU, int* dataWriteMTU);

/**
 * @brief  This API is used to release the acquired device data path.
 *
 * @param[in] apBtConn              The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                  NULL is valid for this API.
 * @param[in] apcDevTransportPath   Bluetooth device transport path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTReleaseDevDataPath (void* apBtConn, char* apcDevTransportPath);

/**
 * @brief  This API is used to release the acquired device data path.
 *
 * @param[in] apBtConn              The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                  NULL is valid for this API.
 * @param[in] aui32AckTOutms        Data write acknowledgment timeout
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTSetDevDataAckTimeout (void* apBtConn, unsigned int aui32AckTOutms);


// AVRCP Interfaces

/**
 * @brief  A Path is assigned to Media player using  Bluetooth device path.
 *
 * @param[in]  apBtConn            The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                 NULL is valid for this API.
 * @param[in]  apBtDevPath         Bluetooth device path.
 *
 * @return  assigned Media player path is returned.
 */
char* BtrCore_BTGetMediaPlayerPath (void* apBtConn, const char* apBtDevPath);

/**
 * @brief  This API is used to control the media device.
 *
 * @param[in]  apBtConn             The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                  NULL is valid for this API.
 * @param[in]  apmediaPlayerPath    Media player path.
 * @param[in]  aenBTMediaOper       Media operation that has to be performed.
 *
 * @return  assigned Media player path is returned.
 */
int   BtrCore_BTDevMediaControl (void* apBtConn, const char* apmediaPlayerPath, enBTMediaControlCmd  aenBTMediaOper);

/**
 * @brief  This API is used to get the state of the BT device .
 *
 * @param[in]  apBtConn           The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                NULL is valid for this API.
 * @param[in]  apBtDataPath       Bt Data path.
 * @param[out] state              Transport state of the BT device.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetTransportState (void* apBtConn, const char* apBtDataPath, void* state);

/**
 * @brief  This API is used to get media player property value using the object path of BT device and media property.
 *
 * @param[in]  apBtConn           The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                                NULL is valid for this API.
 * @param[in]  apBtObjectPath     Bluetooth device path.
 * @param[in]  mediaProperty      Property of the Mediaplayer.
 * @param[out] mediaPropertyValue Property value of media player.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetMediaPlayerProperty (void* apBtConn, const char* apBtObjectPath, const char* mediaProperty, void* mediaPropertyValue);

/**
 * @brief  This API is used to get the number of items in the current media folder.
 *
 * @param[in]  apBtConn            The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                                 NULL is valid for this API.
 * @param[in]  apBtMediaPlayerPath Media Player path.
 * @param[out] aui32NumberOfItems  number of items.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetMediaFolderNumberOfItems (void* apBtConn, const char* apBtMediaPlayerPath, unsigned int* aui32NumberOfItems);

/**
 * @brief  This API is used to set the media property of the BT device .
 *
 * @param[in]  apBtConn          The Dbus connection handle as returned by BtrCore_BTInitGetConnection. 
 *                               NULL is valid for this API.
 * @param[in]  apBtAdapterPath   Blue tooth adapter path.
 * @param[in]  mediaProperty     Media Property that has to be set to BT device.
 * @param[in]  pValue            Value of the media property that has been set.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTSetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty, char* pValue);

/**
 * @brief  This API is used to retrieve the information about the track that is being played on BT media device.
 *
 * @param[in]  apBtConn                   The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                        NULL is valid for this API.
 * @param[in]  apBtmediaPlayerObjectPath  Object path of the BT media player.
 * @param[out] lpstBTMediaTrackInfo       Track information that has to be retrieved.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTGetTrackInformation (void* apBtConn, const char* apBtmediaPlayerObjectPath, stBTMediaTrackInfo* lpstBTMediaTrackInfo);

/**
 * @brief  This API is used to change from the current media folder path to another path.
 *
 * @param[in]  apBtConn                   The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                        NULL is valid for this API.
 * @param[in]  apBtmediaPlayerObjectPath  Object path of the BT media player.
 * @param[in]  apBtdestMediaFolderPath    Destination Folder path.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */ 
int   BtrCore_BTChangeMediaFolder (void* apstBtIfceHdl, const char* apBtmediaPlayerObjectPath, const char* apBtdestMediaFolderPath);

/**
 * @brief  This API is used to list the media items in the current folder.
 *
 * @param[in]  apBtConn                   The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                        NULL is valid for this API.
 * @param[in]  apBtmediaPlayerObjectPath  Object path of the BT media player.
 * @param[in]  apBtMediaFolderStartIndex  Starting index of the list.
 * @param[in]  apBtMediaFolderEndIndex    Ending indx of the list.
 * @param[in]  apBtMediaFilter            Filter to be applied.
 * @param[in]  apBtMediaFilterSize        Filter size.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTSelectMediaFolderItems (void* apstBtIfceHdl, const char* apBtMediaPlayerObjectPath, unsigned int apBtMediaFolderStartIndex, unsigned int apBtMediaFolderEndIndex, const char* apBtMediaFilter, int apBtMediaFilterSize);

/**
 * @brief  This API is used to Play the mentioned media track.
 *
 * @param[in]  apBtConn                   The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                        NULL is valid for this API.
 * @param[in]  apBtmediaItemObjectPath    Object path of the media Track.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTPlayMediaTrackItem (void* apstBtIfceHdl, const char* apBtMediaItemObjectPath);

/**
 * @brief  This API is used to get the mediaItemID of the node which is the immediate common parent of the src and dest nodes.
 *
 * @param[in]  aBTcurrMediaItemId         mediaItemId of the current browsing folder.
 * @param[in]  aBTdestMediaItemId         mediaItemId of the destination browsing folder, we need to switch.
 *
 * @return Returns the result of the operation.
 * @return Returns the common parents' mediaItemId.
 */
tBTMediaItemId BtrCore_BTGetCommonParentMediaItemId (tBTMediaItemId aBTcurrMediaItemId, tBTMediaItemId aBTdestMediaItemId);

/******************************************
*    LE Functions
*******************************************/

/**
 * @brief  This API is used to invoke the bluez API advertisement method call RegisterAdvertisment
 *
 * @param[in]  apstBtIfceHdl           The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[in]  apBtAdapter             Bluetooth adapter
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int BtrCore_BTRegisterLeAdvertisement( void* apstBtIfceHdl, const char* apBtAdapter);

/**
 * @brief  This API is used to invoke the bluez API gatt service method call RegisterApplication
 *
 * @param[in]  apstBtIfceHdl           The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[in]  apBtAdapter             Bluetooth adapter
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int BtrCore_BTRegisterLeGatt (void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API is used to invoke the bluez API gatt service method call UnRegisterApplication
 *
 * @param[in]  apstBtIfceHdl           The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 * @param[in]  apBtAdapter             Bluetooth adapter
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int BtrCore_BTUnRegisterLeGatt (void* apBtConn, const char* apBtAdapter);

/**
 * @brief  This API is used to perform gatt services of the BT device .
 *
 * @param[in]  apBtConn           The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                NULL is valid for this API.
 * @param[in]  apBtLePath         LE Blue tooth device path.
 * @param[in]  aenBTOpIfceType    Bluetooth interface type.
 * @param[in]  aenBTLeGattOp      Bluetooth interface property whose propert value has to be fetched.
 * @param[in]  apUserdata1        LE gatt operation userdata.
 * @param[in]  apUserdata2        LE gatt operation userdata.
 * @param[out] rpLeOpRes          LE operation result.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTPerformLeGattOp (void* apBtConn, const char* apBtLePath, enBTOpIfceType aenBTOpIfceType, enBTLeGattOp aenBTLeGattOp, char* apLeGatOparg1, char* apLeGatOparg2, char* rpLeOpRes);

/**
 * @brief  This API is used to read, write and dispatch BT device information.
 *
 * @param[in]  apBtConn                The Dbus connection handle as returned by BtrCore_BTInitGetConnection.
 *                                     NULL is valid for this API.
 *
 * @return Returns the status of the operation.
 * @retval Returns 0 on success, appropriate error code otherwise.
 */
int   BtrCore_BTSendReceiveMessages (void* apBtConn);

// Outgoing callbacks Registration Interfaces
int   BtrCore_BTRegisterAdapterStatusUpdateCb (void* apBtConn, fPtr_BtrCore_BTAdapterStatusUpdateCb afpcBAdapterStatusUpdate, void* apUserData);
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
int   BtrCore_BTRegisterMediaBrowserUpdateCb (void* apBtConn, fPtr_BtrCore_BTMediaBrowserPathCb afpcBTMediaBrowserPath, void* apUserData);
int   BtrCore_BTRegisterLEGattInfoCb (void* apBtConn, const char* apBtAdapter, fPtr_BtrCore_BTLeGattPathCb afpcBLeGattPath, void* apUserData);
int   BtrCore_BTRegisterLEAdvInfoCb(void* apBtConn, const char* apBtAdapter, fPtr_BtrCore_BTLeAdvertisementCb afpcBLeAdvPath, void* apUserData);

#endif // __BTR_CORE_BT_IFCE_H__
