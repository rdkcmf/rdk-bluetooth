/*
 * btrCore_dbus_bt.c
 * Implementation of DBus layer abstraction for BT functionality
 */

/* System Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Local Headers */
#include "btrCore_dbus_bt.h"



/* Static Function Prototypes */
static int btrCore_BTHandleDusError(DBusError* aDBusErr, const char* aErrfunc, int aErrline);
static DBusHandlerResult btrCore_BTMediaEndpointHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata); 
static DBusMessage* btrCore_BTMediaEndpointSelectConfiguration (DBusMessage *apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointSetConfiguration (DBusMessage *apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointClearConfiguration (DBusMessage *apDBusMsg);


/* Static Global Variables Defs */
static DBusConnection*  gpDBusConn = NULL;
static char*            gpcDevTransportPath = NULL;
static const DBusObjectPathVTable gDBusMediaEndpointVTable = {
    .message_function = btrCore_BTMediaEndpointHandler_cb,
};


/* Callbacks */
static fPtr_BtrCore_BTNegotiateMedia_cB gfpcBNegotiateMedia = NULL;
static fPtr_BtrCore_BTTransportPathMedia_cB gfpcBTransportPathMedia = NULL;


/* Static Function Defs */
static inline
int btrCore_BTHandleDusError (
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


static DBusHandlerResult
btrCore_BTMediaEndpointHandler_cb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage*    lpDBusReply = NULL;
    DBusMessage *r = NULL;
    DBusError e;
    const char *path;

    path = dbus_message_get_path(apDBusMsg);
    dbus_error_init(&e);

    (void)path;
    (void)r;


    fprintf(stderr, "endpoint_handler: MediaEndpoint\n");

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "SelectConfiguration")) {
        fprintf(stderr, "endpoint_handler: MediaEndpoint-SelectConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSelectConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "SetConfiguration"))  {
        fprintf(stderr, "endpoint_handler: MediaEndpoint-SetConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSetConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "ClearConfiguration")) {
        fprintf(stderr, "endpoint_handler: MediaEndpoint-ClearConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointClearConfiguration(apDBusMsg);
    }
    else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (lpDBusReply) {
        dbus_connection_send(apDBusConn, lpDBusReply, NULL);
        dbus_message_unref(lpDBusReply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusMessage* 
btrCore_BTMediaEndpointSelectConfiguration (
    DBusMessage*    apDBusMsg
) {
    int             lDBusArgsSize;
    void*           lpInputMediaCaps;
    void*           lpOutputMediaCaps;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;

    dbus_error_init(&lDBusErr);

    if (!dbus_message_get_args(apDBusMsg, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpInputMediaCaps, &lDBusArgsSize, DBUS_TYPE_INVALID)) {
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to select configuration");
    }

    if (gfpcBNegotiateMedia) {
        if(!(lpOutputMediaCaps = gfpcBNegotiateMedia(lpInputMediaCaps))) {
            return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to select configuration");
        }
    }


    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    dbus_message_append_args (lpDBusReply, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpOutputMediaCaps, lDBusArgsSize, DBUS_TYPE_INVALID);
    return lpDBusReply;
}


static DBusMessage*
btrCore_BTMediaEndpointSetConfiguration (
    DBusMessage *apDBusMsg
) {
    const char *lDevTransportPath = NULL;
    const char *lStoredDevTransportPath = NULL;
    const char *dev_path = NULL, *uuid = NULL;
    unsigned char* config = NULL;
    int size = 0; 

    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterProp;
    DBusMessageIter lDBusMsgIterEntry;
    DBusMessageIter lDBusMsgIterValue;
    DBusMessageIter lDBusMsgIterArr;


    dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
    dbus_message_iter_get_basic(&lDBusMsgIter, &lDevTransportPath);
    if (!dbus_message_iter_next(&lDBusMsgIter))
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

    dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterProp);
    if (dbus_message_iter_get_arg_type(&lDBusMsgIterProp) != DBUS_TYPE_DICT_ENTRY)
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

    while (dbus_message_iter_get_arg_type(&lDBusMsgIterProp) == DBUS_TYPE_DICT_ENTRY) {
        const char *key;
        int ldBusType; 

        dbus_message_iter_recurse(&lDBusMsgIterProp, &lDBusMsgIterEntry);
        dbus_message_iter_get_basic(&lDBusMsgIterEntry, &key);

        dbus_message_iter_next(&lDBusMsgIterEntry);
        dbus_message_iter_recurse(&lDBusMsgIterEntry, &lDBusMsgIterValue);

        ldBusType = dbus_message_iter_get_arg_type(&lDBusMsgIterValue);
        if (strcasecmp(key, "UUID") == 0) { 
            if (ldBusType != DBUS_TYPE_STRING)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &uuid);
        } 
        else if (strcasecmp(key, "Device") == 0) { 
            if (ldBusType != DBUS_TYPE_OBJECT_PATH)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &dev_path);
        } 
        else if (strcasecmp(key, "Configuration") == 0) { 
            if (ldBusType != DBUS_TYPE_ARRAY)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_recurse(&lDBusMsgIterValue, &lDBusMsgIterArr);
            dbus_message_iter_get_fixed_array(&lDBusMsgIterArr, &config, &size);
        }
        dbus_message_iter_next(&lDBusMsgIterProp);
    }    

    fprintf(stderr, "Set configuration - Transport Path %s\n", lDevTransportPath);

    if (gpcDevTransportPath) {
        free(gpcDevTransportPath);
        gpcDevTransportPath = NULL;
    }

    gpcDevTransportPath = strdup(lDevTransportPath);

    if (gfpcBTransportPathMedia) {
        if((lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, config))) {
            fprintf(stderr, "Stored - Transport Path 0x%8x:%s\n", (unsigned int)lStoredDevTransportPath, lStoredDevTransportPath);
        }
    }

    return dbus_message_new_method_return(apDBusMsg);
}


static DBusMessage* 
btrCore_BTMediaEndpointClearConfiguration (
    DBusMessage *apDBusMsg
) {

    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    DBusMessageIter lDBusMsgIter;
    const char*     lDevTransportPath = NULL;
    const char*     lStoredDevTransportPath = NULL;

    dbus_error_init(&lDBusErr);
    dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
    dbus_message_iter_get_basic(&lDBusMsgIter, &lDevTransportPath);
    fprintf(stderr, "Clear configuration - Transport Path %s\n", lDevTransportPath);

    if (gpcDevTransportPath) {
        free(gpcDevTransportPath);
        gpcDevTransportPath = NULL;
    }

    if (gfpcBTransportPathMedia) {
        if(!(lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, NULL))) {
            fprintf(stderr, "Cleared - Transport Path %s\n", lDevTransportPath);
        }
    }
    
    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    return lpDBusReply;
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
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
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
    void* apBtConn
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    const char*     lpReplyPath;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
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
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        fprintf(stderr, "Can't get reply arguments\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);

    return strdup(lpReplyPath); //Caller Should free the pointer returned
}


char*
BtrCore_BTGetAdapterPath (
    void*       apBtConn,
    const char* apBtAdapter
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    const char*     lpReplyPath;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
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
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        fprintf(stderr, "Can't get reply arguments\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);

    return strdup(lpReplyPath); //Caller Should free the pointer returned
}


int
BtrCore_BTSetAdapterProp (
    void*           apBtConn,
    const char*     apBtAdapter,
    enBTAdapterProp aenBTAdapterProp,
    void*           apvVal
) {

    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterValue;
    DBusError       lDBusErr;
    int             lDBusType;
    const char*     lDBusTypeAsString;
    const char*     lDBusKey;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apvVal)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                            "org.bluez.Adapter",
                                            "SetProperty");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    switch (aenBTAdapterProp) {
    case enBTAdPropName:
        lDBusType = DBUS_TYPE_STRING;
        lDBusKey  = "Name";
        break;
    case enBTAdPropPowered:
        lDBusType = DBUS_TYPE_BOOLEAN;
        lDBusKey  = "Powered";
        break;
    case enBTAdPropDiscoverable:
        lDBusType = DBUS_TYPE_BOOLEAN;
        lDBusKey  = "Discoverable";
        break;
    case enBTAdPropDiscoverableTimeOut:
        lDBusType = DBUS_TYPE_UINT32;
        lDBusKey  = "DiscoverableTimeout";
        break;
    case enBTAdPropUnknown:
    default:
        fprintf(stderr, "Invalid Adaptre Property\n");
        return -1;
    }

    switch (lDBusType) {
    case DBUS_TYPE_BOOLEAN:
        lDBusTypeAsString = DBUS_TYPE_BOOLEAN_AS_STRING;
        break;
    case DBUS_TYPE_UINT32:
        lDBusTypeAsString = DBUS_TYPE_UINT32_AS_STRING;
        break;
    case DBUS_TYPE_STRING:
        lDBusTypeAsString = DBUS_TYPE_STRING_AS_STRING;
        break;
    default:
        fprintf(stderr, "Invalid DBus Type\n");
        return -1;
    }

    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &lDBusKey);
    dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
        dbus_message_iter_append_basic(&lDBusMsgIterValue, lDBusType, apvVal);
    dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Reply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    return 0;
}



int
BtrCore_BTStartDiscovery (
    void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    dbus_bool_t     lDBusOp;
    DBusMessage*    lpDBusMsg;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
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
    void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    dbus_bool_t     lDBusOp;
    DBusMessage*    lpDBusMsg;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
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


int
BtrCore_BTConnectDevice (
    void*           apBtConn,
    const char*     apDevPath,
    enBTDeviceType  aenBTDeviceType
) {
    dbus_bool_t  lDBusOp;
    DBusMessage* lpDBusMsg;
    char         larDBusIfce[32] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apDevPath)
        return -1;

    switch (aenBTDeviceType) {
    case enBTDevAudioSink:
        strncpy(larDBusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    case enBTDevAudioSource:
        strncpy(larDBusIfce, "org.bluez.AudioSource", strlen("org.bluez.AudioSource"));
        break;
    case enBTDevHFPHeadset:
        strncpy(larDBusIfce, "org.bluez.Headset", strlen("org.bluez.Headset"));
        break;
    case enBTDevHFPHeadsetGateway:
        strncpy(larDBusIfce, "org.bluez.HeadsetGateway", strlen("org.bluez.HeadsetGateway"));
        break;
    case enBTDevUnknown:
    default:
        strncpy(larDBusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    }

    lpDBusMsg = dbus_message_new_method_call( "org.bluez",
                                        apDevPath,
                                        larDBusIfce,
                                        "Connect");

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
BtrCore_BTDisconnectDevice (
    void*           apBtConn,
    const char*     apDevPath,
    enBTDeviceType  aenBTDeviceType
) {
    dbus_bool_t  lDBusOp;
    DBusMessage* lpDBusMsg;
    char         larDBusIfce[32] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apDevPath)
        return -1;

    switch (aenBTDeviceType) {
    case enBTDevAudioSink:
        strncpy(larDBusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    case enBTDevAudioSource:
        strncpy(larDBusIfce, "org.bluez.AudioSource", strlen("org.bluez.AudioSource"));
        break;
    case enBTDevHFPHeadset:
        strncpy(larDBusIfce, "org.bluez.Headset", strlen("org.bluez.Headset"));
        break;
    case enBTDevHFPHeadsetGateway:
        strncpy(larDBusIfce, "org.bluez.HeadsetGateway", strlen("org.bluez.HeadsetGateway"));
        break;
    case enBTDevUnknown:
    default:
        strncpy(larDBusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    }

    lpDBusMsg = dbus_message_new_method_call( "org.bluez",
                                        apDevPath,
                                        larDBusIfce,
                                        "Disconnect");

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
BtrCore_BTRegisterMedia (
    void*       apBtConn,
    const char* apBtAdapter,
    char*       apBtMediaType,
    void*       apBtUUID,
    void*       apBtMediaCodec,
    void*       apBtMediaCapabilities,
    int         apBtMediaCapabilitiesSize
) {
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterArr;
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    //TODO: Check the Mediatype and then add the match
    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.Headset'", NULL);

    lDBusOp = dbus_connection_register_object_path(gpDBusConn, apBtMediaType, &gDBusMediaEndpointVTable, NULL);
    if (!lDBusOp) {
        fprintf(stderr, "Can't Register Media Object\n");
        return -1;
    }

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media",
                                             "RegisterEndpoint");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_OBJECT_PATH, &apBtMediaType);
    dbus_message_iter_open_container (&lDBusMsgIter, DBUS_TYPE_ARRAY, "{sv}", &lDBusMsgIterArr);
    {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
        char *key = "UUID";
        int  type = DBUS_TYPE_STRING;

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtUUID);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
    {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
        char *key = "Codec";
        int  type = DBUS_TYPE_BYTE;

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtMediaCodec);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
    {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant, lDBusMsgIterSubArray;
        char *key = "Capabilities";
        int  type = DBUS_TYPE_BYTE;

        char array_type[5] = "a";
        strncat (array_type, (char*)&type, sizeof(array_type));

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                dbus_message_iter_open_container (&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                    dbus_message_iter_append_fixed_array (&lDBusMsgIterSubArray, type, &apBtMediaCapabilities, apBtMediaCapabilitiesSize);
                dbus_message_iter_close_container (&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterArr);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Reply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTUnRegisterMedia (
    void*       apBtConn,
    const char* apBtAdapter,
    char*       apBtMediaType
) {
    DBusMessage*    lpDBusMsg;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;


    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media",
                                             "UnregisterEndpoint");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtMediaType, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    lDBusOp = dbus_connection_unregister_object_path(gpDBusConn, apBtMediaType);
    if (!lDBusOp) {
        fprintf(stderr, "Can't Register Media Object\n");
        return -1;
    }


    //TODO: Check the Mediatype and then remove the match
    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Headset'", NULL);

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTAcquireDevDataPath (
    void*   apBtConn,
    char*   apcDevTransportPath,
    int*    dataPathFd,
    int*    dataReadMTU,
    int*    dataWriteMTU
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusMessageIter lDBusMsgIter;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;

    //TODO: There is no point in always acquire a rw socket/fd/anything else
    //Decide the Access type based on the current Device type
    char *access_type = "rw";

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apcDevTransportPath)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevTransportPath,
                                             "org.bluez.MediaTransport",
                                             "Acquire");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_STRING, &access_type);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Reply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr,
                                    DBUS_TYPE_UNIX_FD, dataPathFd,
                                    DBUS_TYPE_UINT16,  dataReadMTU,
                                    DBUS_TYPE_UINT16,  dataWriteMTU,
                                    DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        fprintf(stderr, "Can't get reply arguments\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTReleaseDevDataPath (
    void*   apBtConn,
    char*   apcDevTransportPath
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusMessageIter lDBusMsgIter;
    DBusError       lDBusErr;

    //TODO: There is no point in always acquire a rw socket/fd/anything else
    //Decide the Access type based on the current Device type
    char *access_type = "rw";

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apcDevTransportPath)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevTransportPath,
                                             "org.bluez.MediaTransport",
                                             "Release");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_STRING, &access_type);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Reply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTRegisterNegotiateMediacB (
    void*       apBtConn, 
    const char* apBtAdapter, 
    char*       apBtMediaType,
    fPtr_BtrCore_BTNegotiateMedia_cB afpcBNegotiateMedia
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter || !apBtMediaType || !afpcBNegotiateMedia)
        return -1;

    gfpcBNegotiateMedia = afpcBNegotiateMedia;

    return 0;
}


int
BtrCore_BTRegisterTransportPathMediacB (
    void*       apBtConn,
    const char* apBtAdapter,
    char*       apBtMediaType,
    fPtr_BtrCore_BTTransportPathMedia_cB afpcBTransportPathMedia
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter || !apBtMediaType || !afpcBTransportPathMedia)
        return -1;

    gfpcBTransportPathMedia = afpcBTransportPathMedia;

    return 0;
}

