/*
 * btrCore_dbus.h
 * DBus layer abstraction for BT functionality
 */

#ifndef __BTR_CORE_DBUS_BT_H__
#define __BTR_CORE_DBUS_BT_H__

/* Interfaces */
void* BtrCore_BTInitGetConnection (void);
char* BtrCore_BTGetAgentPath (void);
char* BtrCore_BTGetDefaultAdapterPath (void* apBTConn);
char* BtrCore_BTGetAdapterPath (void* apBTConn, const char* apBtAdapter);
int BtrCore_BTStartDiscovery (void* apBTConn, const char* apBtAdapter, const char* apBtAgentPath);
int BtrCore_BTStopDiscovery (void* apBTConn, const char* apBtAdapter, const char* apBtAgentPath);

#endif // __BTR_CORE_DBUS_BT_H__
