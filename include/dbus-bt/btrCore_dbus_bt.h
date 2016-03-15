/*
 * btrCore_dbus.h
 * DBus layer abstraction for BT functionality
 */

#ifndef __BTR_CORE_DBUS_BT_H__
#define __BTR_CORE_DBUS_BT_H__

typedef enum _enBTDeviceType {
    enBTDevAudioSink,
    enBTDevAudioSource,
    enBTDevHFPHeadset,
    enBTDevHFPHeadsetGateway,
    enBTDevUnknown
} enBTDeviceType;


/*
 * Callbacks
 */
typedef void* (*fPtr_BtrCore_BTNegotiateMedia_cB)(void* apBtMediaCaps);
typedef const char* (*fPtr_BtrCore_BTTransportPathMedia_cB)(const char* apBtMediaTransportPath);


/* Interfaces */
void* BtrCore_BTInitGetConnection (void);
char* BtrCore_BTGetAgentPath (void);
char* BtrCore_BTGetDefaultAdapterPath (void* apBtConn);
char* BtrCore_BTGetAdapterPath (void* apBtConn, const char* apBtAdapter);
int   BtrCore_BTStartDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStopDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTConnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType enBTDevType);
int   BtrCore_BTDisconnectDevice (void* apBtConn, const char* apDevPath, enBTDeviceType enBTDevType);
int   BtrCore_BTRegisterMedia (void* apBtConn, const char* apBtAdapter, char* apBtMediaType,
                                void* apBtUUID, void* apBtMediaCodec, void* apBtMediaCapabilities, int apBtMediaCapabilitiesSize);
int   BtrCore_BTUnRegisterMedia (void* apBtConn, const char* apBtAdapter, char* apBtMediaType);
int   BtrCore_BTAcquireDevDataPath (void* apBtConn, char* apcDevTransportPath, int* dataPathFd, int* dataReadMTU, int* dataWriteMTU);
int   BtrCore_BTReleaseDevDataPath (void* apBtConn, char* apcDevTransportPath);
int   BtrCore_BTRegisterNegotiateMediacB (void* apBtConn, const char* apBtAdapter, char* apBtMediaType,
                                            fPtr_BtrCore_BTNegotiateMedia_cB afpcBNegotiateMedia);
int   BtrCore_BTRegisterTransportPathMediacB (void* apBtConn, const char* apBtAdapter, char* apBtMediaType,
                                                fPtr_BtrCore_BTTransportPathMedia_cB afpcBTransportPathMedia);

#endif // __BTR_CORE_DBUS_BT_H__
