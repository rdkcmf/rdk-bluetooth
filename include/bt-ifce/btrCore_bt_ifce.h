/*
 * btrCore_dbus.h
 * DBus layer abstraction for BT functionality
 */

#ifndef __BTR_CORE_DBUS_BT_H__
#define __BTR_CORE_DBUS_BT_H__

#define BT_MAX_STR_LEN          256
#define BT_MAX_NUM_DEVICE        32
#define BT_MAX_DEVICE_PROFILE    32

/* Enum Types */
typedef enum _enBTDeviceType {
    enBTDevAudioSink,
    enBTDevAudioSource,
    enBTDevHFPHeadset,
    enBTDevHFPHeadsetGateway,
    enBTDevUnknown
} enBTDeviceType;

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

typedef enum _enBTDeviceOp {
    enBTDevOpFindPairedDev,
    enBTDevOpCreatePairedDev,
    enBTDevOpRemovePairedDev,
    enBTDevOpUnknown
} enBTDeviceOp;

typedef enum _enBTAdapterProp {
    enBTAdPropName,
    enBTAdPropPowered,
    enBTAdPropDiscoverable,
    enBTAdPropDiscoverableTimeOut,
    enBTAdPropUnknown
} enBTAdapterProp;

typedef enum _enBTMediaControl {
	enBTMediaPlay,
	enBTMediaPause,
	enBTMediaStop,
	enBTMediaNext,
	enBTMediaPrevious,
	enBTMediaFastForward,
	enBTMediaRewind,
	enBTMediaVolumeUp,
	enBTMediaVolumeDown
} enBTMediaControl;

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
    char pcDevicePrevState[BT_MAX_STR_LEN];
    char pcDeviceCurrState[BT_MAX_STR_LEN];
    // TODO: Array of strings UUIDs;
    // TODO: Array of objects Services;
    // TODO: Array of objects Nodes;
} stBTDeviceInfo;

typedef struct _stBTPairedDeviceInfo {
    unsigned short numberOfDevices;
    char devicePath[BT_MAX_NUM_DEVICE][BT_MAX_STR_LEN];
    stBTDeviceInfo deviceInfo[BT_MAX_NUM_DEVICE];
} stBTPairedDeviceInfo;

typedef struct _stBTDeviceSupportedService
{
    unsigned int uuid_value;
    char profile_name[BT_MAX_STR_LEN];
} stBTDeviceSupportedService;

typedef struct _stBTDeviceSupportedServiceList
{
    int numberOfService;
    stBTDeviceSupportedService profile[BT_MAX_DEVICE_PROFILE];
} stBTDeviceSupportedServiceList;


/* Callbacks Types */
typedef int (*fPtr_BtrCore_BTDevStatusUpdate_cB)(enBTDeviceType aeBtDeviceType, enBTDeviceState aeBtDeviceState, stBTDeviceInfo* apstBTDeviceInfo, void* apUserData);
typedef void* (*fPtr_BtrCore_BTNegotiateMedia_cB)(void* apBtMediaCaps);
typedef const char* (*fPtr_BtrCore_BTTransportPathMedia_cB)(const char* apBtMediaTransportPath, void* apBtMediaCaps);
typedef int (*fPtr_BtrCore_BTConnAuth_cB)(const char* apBtDeviceName, void* apUserData);

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
int   BtrCore_BTGetProp (void* apBtConn, const char* pDevicePath, const char* pInterface, const char* pKey, void* pValue);
int   BtrCore_BTSetAdapterProp (void* apBtConn, const char* apBtAdapter, enBTAdapterProp aenBTAdapterProp, void* apvVal);
int   BtrCore_BTStartDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStopDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTGetPairedDevices (void* apBtConn, const char* apBtAdapter, unsigned int* apui32PairedDevCnt, char** apcArrPairedDevPath);
int   BtrCore_BTGetPairedDeviceInfo (void* apBtConn, const char* apBtAdapter, stBTPairedDeviceInfo *pPairedDeviceInfo);
int   BtrCore_BTDiscoverDeviceServices (void* apBtConn, const char* apcDevPath, stBTDeviceSupportedServiceList *pProfileList);
int   BtrCore_BTFindServiceSupported(void* apBtConn, const char* apcDevPath, const char* apcSearchString, char* apcDataString);
int   BtrCore_BTPerformDeviceOp (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath, const char* apcDevPath, enBTDeviceOp aenBTDevOp);
int   BtrCore_BTConnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);
int   BtrCore_BTDisconnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType aenBTDevType);
int   BtrCore_BTRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType,
                                void* apBtUUID, void* apBtMediaCodec, void* apBtMediaCapabilities, int apBtMediaCapabilitiesSize);
int   BtrCore_BTUnRegisterMedia (void* apBtConn, const char* apBtAdapter, enBTDeviceType aenBTDevType);
int   BtrCore_BTAcquireDevDataPath (void* apBtConn, char* apcDevTransportPath, int* dataPathFd, int* dataReadMTU, int* dataWriteMTU);
int   BtrCore_BTReleaseDevDataPath (void* apBtConn, char* apcDevTransportPath);
int   BtrCore_BTSendReceiveMessages (void* apBtConn);
int   BtrCore_BTRegisterDevStatusUpdatecB (void* apBtConn, fPtr_BtrCore_BTDevStatusUpdate_cB afpcBDevStatusUpdate, void* apUserData);
int   BtrCore_BTRegisterConnAuthcB (void* apBtConn, fPtr_BtrCore_BTConnAuth_cB afpcBConnAuth, void* apUserData);
int   BtrCore_BTRegisterNegotiateMediacB (void* apBtConn, const char* apBtAdapter,
                                            fPtr_BtrCore_BTNegotiateMedia_cB afpcBNegotiateMedia, void* apUserData);
int   BtrCore_BTRegisterTransportPathMediacB (void* apBtConn, const char* apBtAdapter,
                                                fPtr_BtrCore_BTTransportPathMedia_cB afpcBTransportPathMedia, void* apUserData);

/////////////////////////////////////////////////////         AVRCP Functions         ////////////////////////////////////////////////////
int   BtrCore_MediaPlayControl (void* apBtConn, const char* apBtAdapterPath, enBTMediaControl aenBTMediaOper);
char* BtrCore_GetPlayerObjectPath (void* apBtConn, const char* apBtAdapterPath);
char* BTrCoreGetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty);
int   BtrCoreSetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty, char* pValue);
int   BtrCoreGetTrackInformation (void* apBtConn, const char* apBtAdapterPath);
int   BtrCoreCheckPlayerBrowsable(void* apBtConn, const char* apBtAdapterPath);


#endif // __BTR_CORE_DBUS_BT_H__
