/*
 * btrCore_dbus.h
 * DBus layer abstraction for BT functionality
 */

#ifndef __BTR_CORE_DBUS_BT_H__
#define __BTR_CORE_DBUS_BT_H__


/*
 * Callbacks
 */
typedef void* (*fPtr_BtrCore_BTNegotiateMedia_cB)(void* apBtMediaCaps);

/* Interfaces */
void* BtrCore_BTInitGetConnection (void);
char* BtrCore_BTGetAgentPath (void);
char* BtrCore_BTGetDefaultAdapterPath (void* apBtConn);
char* BtrCore_BTGetAdapterPath (void* apBtConn, const char* apBtAdapter);
int   BtrCore_BTStartDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTStopDiscovery (void* apBtConn, const char* apBtAdapter, const char* apBtAgentPath);
int   BtrCore_BTRegisterMedia (void* apBtConn, const char* apBtAdapter, char* apBtMediaType,
                                void* apBtUUID, void* apBtMediaCodec, void* apBtMediaCapabilities, int apBtMediaCapabilitiesSize);
int   BtrCore_BTUnRegisterMedia (void* apBtConn, const char* apBtAdapter, char* apBtMediaType);
int   BtrCore_BTRegisterNegotiateMediacB (void* apBtConn, const char* apBtAdapter, char* apBtMediaType,
                                fPtr_BtrCore_BTNegotiateMedia_cB afpcBNegotiateMedia);



#endif // __BTR_CORE_DBUS_BT_H__
