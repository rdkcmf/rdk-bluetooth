/*
 * btrCore_dbus_bt.c
 * Implementation of DBus layer abstraction for BT functionality
 */

/* System Headers */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Local Headers */
#include "btrCore_dbus_bt.h"



static DBusConnection *gpDBusConn = NULL;

static int btrCore_HandleDusError(DBusError *aDBusErr, const char *aErrfunc, int aErrline);


static inline
int btrCore_HandleDusError (
    DBusError*  apDBusErr,
    const char* apErrfunc,
    int         aErrline
) {
    if (dbus_error_is_set(apDBusErr)) {
        fprintf(stderr, "DBus Error is %s at %u: %s\n", apErrfunc, aErrline, apDBusErr->message);
        dbus_error_free(apDBusErr);
        return 1;
    }
    return 0;
}




/* Interfaces */
void*
BtrCore_BTInitGetConnection (
    void
) {
    DBusError       lDBusErr;
    DBusConnection* lpDBusConn = NULL;

    dbus_error_init(&lDBusErr);
    lpDBusConn = dbus_bus_get(DBUS_BUS_SYSTEM, &lDBusErr);

    if (lpDBusConn == NULL) {
        btrCore_HandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    fprintf(stderr, "DBus Debug DBus Connection Name %s\n", dbus_bus_get_unique_name (lpDBusConn));
    gpDBusConn = lpDBusConn;

    return (void*)gpDBusConn;
}


char*
BtrCore_BTGetAgentPath (
    void
) {
    char lDefaultBTPath[128];
    snprintf(lDefaultBTPath, sizeof(lDefaultBTPath), "/org/bluez/agent_%d", getpid());
    return strdup(lDefaultBTPath);
}


char*
BtrCore_BTGetDefaultAdapterPath (
    void* apBTConn
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    const char*     lpReplyPath;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBTConn))
        return NULL;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             "/",
                                             "org.bluez.Manager",
                                             "DefaultAdapter");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return NULL;
    }

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Can't find Default adapter\n");
        btrCore_HandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        fprintf(stderr, "Can't get reply arguments\n");
        btrCore_HandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);

    return strdup(lpReplyPath); //Caller Should free the pointer returned
}


char*
BtrCore_BTGetAdapterPath (
    void*       apBTConn,
    const char* apBtAdapter
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    const char*     lpReplyPath;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBTConn))
        return NULL;

    if (!apBtAdapter)
        return BtrCore_BTGetDefaultAdapterPath(gpDBusConn);

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             "/",
                                             "org.bluez.Manager",
                                             "FindAdapter");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return NULL;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &apBtAdapter, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Can't find adapter %s\n", apBtAdapter);
        btrCore_HandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        fprintf(stderr, "Can't get reply arguments\n");
        btrCore_HandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);

    return strdup(lpReplyPath); //Caller Should free the pointer returned
}


int
BtrCore_BTStartDiscovery (
    void*       apBTConn,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    dbus_bool_t     lDBusOp;
    DBusMessage*    lpDBusMsg;

    if (!gpDBusConn || (gpDBusConn != apBTConn))
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Adapter",
                                             "StartDiscovery");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTStopDiscovery (
    void*       apBTConn,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    dbus_bool_t     lDBusOp;
    DBusMessage*    lpDBusMsg;

    if (!gpDBusConn || (gpDBusConn != apBTConn))
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Adapter",
                                             "StopDiscovery");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}

