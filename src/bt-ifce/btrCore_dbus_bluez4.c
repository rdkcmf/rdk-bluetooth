/*
 * btrCore_dbus_bt.c
 * Implementation of DBus layer abstraction for BT functionality
 */

/* System Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Local Headers */
#include "btrCore_bt_ifce.h"

#define BD_NAME_LEN     248

/* Static Function Prototypes */
static int btrCore_BTHandleDusError (DBusError* aDBusErr, const char* aErrfunc, int aErrline);

static DBusHandlerResult btrCore_BTDBusAgentFilter_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTMediaEndpointHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata); 
static DBusHandlerResult btrCore_BTAgentMessageHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);

static char* btrCore_BTGetDefaultAdapterPath (void);
static int btrCore_BTReleaseDefaultAdapterPath (void);
static DBusHandlerResult btrCore_BTAgentRequestPincode (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentRequestPasskey (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentCancelMessage (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentRelease (DBusConnection* apDBusConn, DBusMessage*    apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentAuthorize (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusMessage* btrCore_BTSendMethodCall (const char* objectpath, const char* interfacename, const char* methodname);
static int btrCore_BTParseDevice (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static int btrCore_BTParsePropertyChange (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static DBusMessage* btrCore_BTMediaEndpointSelectConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointSetConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointClearConfiguration (DBusMessage* apDBusMsg);



/* Static Global Variables Defs */
static char *passkey = NULL;
static int do_reject = 0;
static volatile sig_atomic_t __io_canceled = 0;
static volatile sig_atomic_t __io_terminated = 0;
static char gpcDeviceCurrState[BT_MAX_STR_LEN];
static DBusConnection*  gpDBusConn = NULL;
static char* gpcBTAgentPath = NULL;
static char* gpcBTDAdapterPath = NULL;
static char* gpcBTAdapterPath = NULL;
static char* gpcDevTransportPath = NULL;
static void* gpcBDevStatusUserData = NULL;
static void* gpcBConnAuthUserData = NULL;

static const DBusObjectPathVTable gDBusMediaEndpointVTable = {
    .message_function = btrCore_BTMediaEndpointHandler_cb,
};

static const DBusObjectPathVTable agent_table = {
	.message_function = btrCore_BTAgentMessageHandler_cb,
};


/* Callbacks */
static fPtr_BtrCore_BTDevStatusUpdate_cB gfpcBDevStatusUpdate = NULL;
static fPtr_BtrCore_BTNegotiateMedia_cB gfpcBNegotiateMedia = NULL;
static fPtr_BtrCore_BTTransportPathMedia_cB gfpcBTransportPathMedia = NULL;
static fPtr_BtrCore_BTConnAuth_cB gfpcBConnectionAuthentication = NULL;


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
btrCore_BTDBusAgentFilter_cb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    const char *name, *old, *new;
    int             i32OpRet = -1;
    stBTDeviceInfo  lstBTDeviceInfo;

    memset (&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
    lstBTDeviceInfo.i32RSSI = INT_MIN;

    //printf("agent filter activated....\n");
    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter", "DeviceCreated")) {
        printf("Device Created!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStPaired, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter", "DeviceFound")) {
        printf("Device Found!\n");

        i32OpRet = btrCore_BTParseDevice(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate && !i32OpRet) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStFound, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter","DeviceDisappeared")) {
        printf("Device DeviceDisappeared!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStLost, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter","DeviceRemoved")) {
        printf("Device Removed!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStUnPaired, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","Connected")) {
        printf("Device Connected - AudioSink!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","Disconnected")) {
        printf("Device Disconnected - AudioSink!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","PropertyChanged")) {
        printf("Device PropertyChanged!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStPropChanged, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","Connected")) {
        printf("Device Connected - AudioSource!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","Disconnected")) {
        printf("Device Disconnected - AudioSource!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","PropertyChanged")) {
        printf("Device PropertyChanged!\n");
         btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","Connected")) {
        printf("Device Connected - Headset!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset,enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","Disconnected")) {
        printf("Device Disconnected - Headset!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }
    
    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","PropertyChanged")) {
        printf("Device PropertyChanged!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset, enBTDevStPropChanged, NULL, NULL)) {
            }
        }
    }

    if (!dbus_message_is_signal(apDBusMsg, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &name,
                                DBUS_TYPE_STRING, &old,
                                DBUS_TYPE_STRING, &new,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Invalid arguments for NameOwnerChanged signal");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (!strcmp(name, "org.bluez") && *new == '\0') {
        fprintf(stderr, "Agent has been terminated\n");
    }

    if (!i32OpRet)
        return DBUS_HANDLER_RESULT_HANDLED;
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
btrCore_BTMediaEndpointHandler_cb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage*    lpDBusReply = NULL;
    DBusMessage*    r = NULL;
    DBusError       e;
    const char*     path;

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


static DBusHandlerResult
btrCore_BTAgentMessageHandler_cb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "Release"))
        return btrCore_BTAgentRelease (apDBusConn, apDBusMsg, userdata);

	if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent",	"RequestPinCode"))
		return btrCore_BTAgentRequestPincode(apDBusConn, apDBusMsg, userdata);

	if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent",	"RequestPasskey"))
		return btrCore_BTAgentRequestPasskey(apDBusConn, apDBusMsg, userdata);

	if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "Cancel"))
		return btrCore_BTAgentCancelMessage(apDBusConn, apDBusMsg, userdata);

	if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "Authorize"))
		return btrCore_BTAgentAuthorize(apDBusConn, apDBusMsg, userdata);

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static char*
btrCore_BTGetDefaultAdapterPath (
    void
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    const char*     lpReplyPath;
    dbus_bool_t     lDBusOp;

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

    if (gpcBTDAdapterPath) {
        free(gpcBTDAdapterPath);
        gpcBTDAdapterPath = NULL;
    }

    gpcBTDAdapterPath = strdup(lpReplyPath);
    return gpcBTDAdapterPath;
}


static int
btrCore_BTReleaseDefaultAdapterPath (
    void
) {
    if (gpcBTDAdapterPath) {
        free(gpcBTDAdapterPath);
        gpcBTDAdapterPath = NULL;
    }

    return 0;
}


static DBusHandlerResult
btrCore_BTAgentRequestPincode (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
	DBusMessage*    reply;
	const char*     path;

	if (!passkey)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID)) {
		fprintf(stderr, "Invalid arguments for RequestPinCode method");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (do_reject) {
		reply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
		goto sendmsg;
	}

	reply = dbus_message_new_method_return(apDBusMsg);
	if (!reply) {
		fprintf(stderr, "Can't create reply message\n");
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	printf("Pincode request for device %s\n", path);
	dbus_message_append_args(reply, DBUS_TYPE_STRING, &passkey,DBUS_TYPE_INVALID);

sendmsg:
	dbus_connection_send(apDBusConn, reply, NULL);
	dbus_connection_flush(apDBusConn);

	dbus_message_unref(reply);

	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentRequestPasskey (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
	DBusMessage*    reply;
	const char*     path;
	unsigned int    int_passkey;

	if (!passkey)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args(apDBusMsg, NULL,DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID))  {
		fprintf(stderr, "Invalid arguments for RequestPasskey method");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
        
	reply = dbus_message_new_method_return(apDBusMsg);
	if (!reply) {
		fprintf(stderr, "Can't create reply message\n");
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	printf("Passkey request for device %s\n", path);
	int_passkey = strtoul(passkey, NULL, 10);
	dbus_message_append_args(reply, DBUS_TYPE_UINT32, &int_passkey, DBUS_TYPE_INVALID);

	dbus_connection_send(apDBusConn, reply, NULL);
	dbus_connection_flush(apDBusConn);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentCancelMessage (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
	DBusMessage *reply;
	if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID))
          {
		fprintf(stderr, "Invalid arguments for passkey confirmation method");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	printf("Request canceled\n");
	reply = dbus_message_new_method_return(apDBusMsg);

	if (!reply) 
          {
		fprintf(stderr, "Can't create reply message\n");
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	dbus_connection_send(apDBusConn, reply, NULL);
	dbus_connection_flush(apDBusConn);

	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentRelease (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
	DBusMessage *reply;

	if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID))
         {
		fprintf(stderr, "Invalid arguments for Release method");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (!__io_canceled)
		fprintf(stderr, "Agent has been released\n");
	__io_terminated = 1;

	reply = dbus_message_new_method_return(apDBusMsg);

	if (!reply) 
         {
		fprintf(stderr, "Unable to create reply message\n");
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}
	dbus_connection_send(apDBusConn, reply, NULL);
	dbus_connection_flush(apDBusConn);

	dbus_message_unref(reply);
       //return the result
	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentAuthorize (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
	DBusMessage *reply;
	const char *path, *uuid;
        const char *dev_name;//pass the dev name to the callback for app to use
        int yesNo;
	if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_STRING, &uuid, DBUS_TYPE_INVALID)) 
            {
		fprintf(stderr, "Invalid arguments for Authorize method");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

    if (gfpcBConnectionAuthentication) {
        printf("calling ConnAuth cb with %s...\n",path);
        dev_name = "Bluetooth Device";//TODO connect device name with btrCore_GetKnownDeviceName 

        if (dev_name != NULL) {
            yesNo = gfpcBConnectionAuthentication(dev_name, gpcBConnAuthUserData);
        }
        else {
            //couldnt get the name, provide the bt address instead
            yesNo = gfpcBConnectionAuthentication(path, gpcBConnAuthUserData);
        }

        if (yesNo == 0) {
            //printf("sorry dude, you cant connect....\n");
            reply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
            goto send;
        }
    }

	reply = dbus_message_new_method_return(apDBusMsg);
	if (!reply) {
		fprintf(stderr, "Can't create reply message\n");
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	printf("Authorizing request for %s\n", path);

send:
	dbus_connection_send(apDBusConn, reply, NULL);
	dbus_connection_flush(apDBusConn);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusMessage*
btrCore_BTSendMethodCall (
    const char*     objectpath,
    const char*     interfacename,
    const char*     methodname
) {
    const char*     busname = "org.bluez";

    DBusPendingCall* pending;
    DBusMessage*     reply;
    DBusMessage*     methodcall = dbus_message_new_method_call( busname,
                                                                objectpath,
                                                                interfacename,
                                                                methodname);

    if (methodcall == NULL) {
        printf("Cannot allocate DBus message!\n");
        return NULL;
    }

    //Now do a sync call
    if (!dbus_connection_send_with_reply(gpDBusConn, methodcall, &pending, -1)) { //Send and expect reply using pending call object
        printf("failed to send message!\n");
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(methodcall);
    methodcall = NULL;

    dbus_pending_call_block(pending);               //Now block on the pending call
    reply = dbus_pending_call_steal_reply(pending); //Get the reply message from the queue
    dbus_pending_call_unref(pending);               //Free pending call handle

    if (dbus_message_get_type(reply) ==  DBUS_MESSAGE_TYPE_ERROR) {
        printf("Error : %s\n\n", dbus_message_get_error_name(reply));
        dbus_message_unref(reply);
        reply = NULL;
    }

    return reply;
}


static int
btrCore_BTParseDevice (
    DBusMessage*    apDBusMsg,
    stBTDeviceInfo* apstBTDeviceInfo
) {
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    const char*     pcKey = NULL;
    const char*     pcBTDevAddr = NULL;
    int             dbus_type;
    int             bPaired = 0;
    int             bConnected = 0;
    int             bTrusted = 0;
    int             bBlocked = 0;
    unsigned short  ui16Vendor = 0;
    unsigned short  ui16VendorSource = 0;
    unsigned short  ui16Product = 0;
    unsigned short  ui16Version = 0;
    unsigned int    ui32Class = 0;
    short           i16RSSI = 0;
    const char*     pcName = NULL;
    const char*     pcAddress = NULL;
    const char*     pcAlias = NULL;
    const char*     pcIcon = NULL;

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
        fprintf(stderr, "%s:%d:%s - dbus_message_iter_init Failed\n", __FILE__, __LINE__, __FUNCTION__);
        return -1;
    }

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &pcBTDevAddr,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "%s:%d:%s - dbus_message_get_args Failed\n", __FILE__, __LINE__, __FUNCTION__);
        //return -1; Users of btrCore_BTParseDevice should not call it if the message contains no valid args
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&arg_i);
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            fprintf(stderr, "%s:%d:%s - Unknown Prop structure from Bluez\n", __FILE__, __LINE__, __FUNCTION__);
            return -1;
        }
    }

    dbus_message_iter_recurse(&arg_i, &element_i);
    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter dict_i;

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_message_iter_get_basic(&dict_i, &pcKey);

            if (strcmp (pcKey, "Address") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcAddress);
                strncpy(apstBTDeviceInfo->pcAddress, pcAddress, BT_MAX_STR_LEN);
                printf("apstBTDeviceInfo->pcAddress : %s\n", apstBTDeviceInfo->pcAddress);
            }
            else if (strcmp (pcKey, "Name") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcName);
                strncpy(apstBTDeviceInfo->pcName, pcName, BT_MAX_STR_LEN);
                printf("apstBTDeviceInfo->pcName: %s\n", apstBTDeviceInfo->pcName);

            }
            else if (strcmp (pcKey, "Vendor") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Vendor);
                apstBTDeviceInfo->ui16Vendor = ui16Vendor;
                printf("apstBTDeviceInfo->ui16Vendor = %d\n", apstBTDeviceInfo->ui16Vendor);
            }
            else if (strcmp (pcKey, "VendorSource") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16VendorSource);
                apstBTDeviceInfo->ui16VendorSource = ui16VendorSource;
                printf("apstBTDeviceInfo->ui16VendorSource = %d\n", apstBTDeviceInfo->ui16VendorSource);
            }
            else if (strcmp (pcKey, "Product") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Product);
                apstBTDeviceInfo->ui16Product = ui16Product;
                printf("apstBTDeviceInfo->ui16Product = %d\n", apstBTDeviceInfo->ui16Product);
            }
            else if (strcmp (pcKey, "Version") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Version);
                apstBTDeviceInfo->ui16Version = ui16Version;
                printf("apstBTDeviceInfo->ui16Version = %d\n", apstBTDeviceInfo->ui16Version);
            }
            else if (strcmp (pcKey, "Icon") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcIcon);
                strncpy(apstBTDeviceInfo->pcIcon, pcIcon, BT_MAX_STR_LEN);
                printf("apstBTDeviceInfo->pcIcon: %s\n", apstBTDeviceInfo->pcIcon);
            }
            else if (strcmp (pcKey, "Class") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui32Class);
                apstBTDeviceInfo->ui32Class = ui32Class;
                printf("apstBTDeviceInfo->ui32Class: %d\n", apstBTDeviceInfo->ui32Class);
            }
            else if (strcmp (pcKey, "Paired") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bPaired);
                apstBTDeviceInfo->bPaired = bPaired;
                printf("apstBTDeviceInfo->bPaired = %d\n", apstBTDeviceInfo->bPaired);
            }
            else if (strcmp (pcKey, "Connected") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bConnected);
                apstBTDeviceInfo->bConnected = bConnected;
                printf("apstBTDeviceInfo->bConnected = %d\n", apstBTDeviceInfo->bConnected);
            }
            else if (strcmp (pcKey, "Trusted") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bTrusted);
                apstBTDeviceInfo->bTrusted = bTrusted;
                printf("apstBTDeviceInfo->bTrusted = %d\n", apstBTDeviceInfo->bTrusted);
            }
            else if (strcmp (pcKey, "Blocked") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bBlocked);
                apstBTDeviceInfo->bBlocked = bBlocked;
                printf("apstBTDeviceInfo->bBlocked = %d\n", apstBTDeviceInfo->bBlocked);
            }
            else if (strcmp (pcKey, "Alias") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcAlias);
                strncpy(apstBTDeviceInfo->pcAlias, pcAlias, BT_MAX_STR_LEN);
                printf("apstBTDeviceInfo->pcAlias: %s\n", apstBTDeviceInfo->pcAlias);
            }
            else if (strcmp (pcKey, "RSSI") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &i16RSSI);
                apstBTDeviceInfo->i32RSSI = i16RSSI;
                printf("apstBTDeviceInfo->i32RSSI = %d i16RSSI = %d\n", apstBTDeviceInfo->i32RSSI, i16RSSI);
            }
        }

        if (!dbus_message_iter_next(&element_i))
            break;
    }

    (void)dbus_type;

    if (strlen(apstBTDeviceInfo->pcAlias))
        strncpy(apstBTDeviceInfo->pcName, apstBTDeviceInfo->pcAlias, strlen(apstBTDeviceInfo->pcAlias));

    return 0;
}

static int
btrCore_BTParsePropertyChange (
    DBusMessage*    apDBusMsg,
    stBTDeviceInfo* apstBTDeviceInfo
) {
     DBusMessageIter arg_i, variant_i;
    const char* value;
    const char* bd_addr;
    int dbus_type;

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
       printf("GetProperties reply has no arguments.");
    }

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &bd_addr,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Invalid arguments for NameOwnerChanged signal");
        return -1;
    }

    printf(" Name: %s\n",bd_addr);//"State" then the variant is a string
    if (strcmp(bd_addr,"State") == 0) {
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
        //printf("type is %d\n",dbus_type);

        if (dbus_type == DBUS_TYPE_STRING) {
            dbus_message_iter_next(&arg_i);
            dbus_message_iter_recurse(&arg_i, &variant_i);
            dbus_message_iter_get_basic(&variant_i, &value);
             // printf("    the new state is: %s\n",value);
            strncpy(apstBTDeviceInfo->pcDevicePrevState, gpcDeviceCurrState, BT_MAX_STR_LEN - 1);
            strncpy(apstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
          strncpy(gpcDeviceCurrState, value, BT_MAX_STR_LEN - 1);


        }
    }

    return 0;
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
    DBusMessage*    apDBusMsg
) {
    const char* lDevTransportPath = NULL;
    const char* lStoredDevTransportPath = NULL;
    const char* dev_path = NULL, *uuid = NULL;
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
    DBusMessage*    apDBusMsg
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

    if (!dbus_connection_add_filter(gpDBusConn, btrCore_BTDBusAgentFilter_cb, NULL, NULL)) {
        fprintf(stderr, "%s:%d:%s - Can't add signal filter - BtrCore_BTInitGetConnection\n", __FILE__, __LINE__, __FUNCTION__);
        BtrCore_BTDeInitReleaseConnection(lpDBusConn);
        return NULL;
    }

    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    gpcBConnAuthUserData            = NULL;
    gpcBDevStatusUserData           = NULL;
    gfpcBDevStatusUpdate            = NULL;
    gfpcBNegotiateMedia             = NULL;
    gfpcBTransportPathMedia         = NULL;
    gfpcBConnectionAuthentication   = NULL;

    return (void*)gpDBusConn;
}


int
BtrCore_BTDeInitReleaseConnection (
    void* apBtConn
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    gfpcBConnectionAuthentication   = NULL;
    gfpcBTransportPathMedia         = NULL;
    gfpcBNegotiateMedia             = NULL;
    gfpcBDevStatusUpdate            = NULL;
    gpcBDevStatusUserData           = NULL;
    gpcBConnAuthUserData            = NULL;

    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    dbus_connection_remove_filter(gpDBusConn, btrCore_BTDBusAgentFilter_cb, NULL);

    gpDBusConn = NULL;

    return 0;
}


char*
BtrCore_BTGetAgentPath (
    void* apBtConn
) {
    char lDefaultBTPath[128] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return NULL;

    snprintf(lDefaultBTPath, sizeof(lDefaultBTPath), "/org/bluez/agent_%d", getpid());

    if (gpcBTAgentPath) {
        free(gpcBTAgentPath);
        gpcBTAgentPath = NULL;
    }

    gpcBTAgentPath = strdup(lDefaultBTPath);
    return gpcBTAgentPath;
}


int
BtrCore_BTReleaseAgentPath (
    void* apBtConn
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (gpcBTAgentPath) {
        free(gpcBTAgentPath);
        gpcBTAgentPath = NULL;
    }

    return 0;
}


int
BtrCore_BTRegisterAgent (
    void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath,
    const char* capabilities
) {

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    DBusMessage *apDBusMsg, *reply;
	DBusError myerr;

	if (!dbus_connection_register_object_path(gpDBusConn, apBtAgentPath, &agent_table, NULL))  {
		fprintf(stderr, "Error registering object path for agent\n");
		return -1;//an error occured
	}

	apDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter,"org.bluez.Adapter", "RegisterAgent");
    if (!apDBusMsg) {
		fprintf(stderr, "Error allocating new method call\n");
		return -1;
    }

	dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath,	DBUS_TYPE_STRING, &capabilities,DBUS_TYPE_INVALID);

	dbus_error_init(&myerr);

	reply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &myerr);

	dbus_message_unref(apDBusMsg);
	if (!reply) {
		fprintf(stderr, "Unable to register agent\n");
		if (dbus_error_is_set(&myerr)) {
			fprintf(stderr, "%s\n", myerr.message);
			dbus_error_free(&myerr);
        }
		return -1;//an error occurred
	}

	dbus_message_unref(reply);

	dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTUnregisterAgent (
    void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

	DBusMessage *apDBusMsg, *reply;
	DBusError err;
	apDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter, "org.bluez.Adapter", "UnregisterAgent");
	if (!apDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

	dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &err);

	dbus_message_unref(apDBusMsg);

	if (!reply) {
		fprintf(stderr, "Can't unregister agent\n");
		if (dbus_error_is_set(&err))  {
			fprintf(stderr, "%s\n", err.message);
			dbus_error_free(&err);
		}

		return -1;//this was an error case
	}

	dbus_message_unref(reply);

	dbus_connection_flush(gpDBusConn);

	dbus_connection_unregister_object_path(gpDBusConn, apBtAgentPath);


    return 0;
}


int
BtrCore_BTGetAdapterList (
    void*           apBtConn,
    unsigned int*   apBtNumAdapters,
    char**          apcArrBtAdapterPath
) {
    DBusError err;
    char **paths = NULL;
    int i, rc = -1;
    int num = -1;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;
    
    dbus_error_init(&err);
    DBusMessage *reply = btrCore_BTSendMethodCall("/", "org.bluez.Manager", "ListAdapters");
    if (!reply) {
        printf("%s:%d - org.bluez.Manager.ListAdapters returned an error: '%s'\n", __FUNCTION__, __LINE__, err.message);
        dbus_error_free(&err);
    }
    else {
        if (dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            if (apBtNumAdapters && apcArrBtAdapterPath) {
                *apBtNumAdapters = num;

                for (i = 0; i < num; i++) {
                    if (*(apcArrBtAdapterPath + i)) {
                        printf("Adapter Path: %d is %s\n", i, paths[i]);
                        strncpy(*(apcArrBtAdapterPath + i), paths[i], BD_NAME_LEN);
                        rc = 0;
                    }
                }
            }
        }
        else {
            printf("%s:%d - org.bluez.Manager.GetProperties parsing failed '%s'\n", __FUNCTION__, __LINE__, err.message);
            dbus_error_free(&err);
        }

        dbus_message_unref(reply);
    }

    return rc;
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
        return btrCore_BTGetDefaultAdapterPath();

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

    if (gpcBTAdapterPath) {
        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }

    gpcBTAdapterPath = strdup(lpReplyPath);
    return gpcBTAdapterPath;
}


int
BtrCore_BTReleaseAdapterPath (
    void*       apBtConn,
    const char* apBtAdapter
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter) {
        return btrCore_BTReleaseDefaultAdapterPath();
    }

    if (gpcBTAdapterPath) {

         if (gpcBTAdapterPath != apBtAdapter)
            fprintf(stderr, "ERROR: Looks like Adapter path has been changed by User\n");

        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }
    
    return 0;
}


int
BtrCore_BTGetProp (
    void*           apBtConn,
    const char*     pDevicePath,
    const char*     pInterface,
    const char*     pKey,
    void*           pValue
) {
    int             rc = 0;
    int             type;
    DBusMessage*    reply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    DBusError       err;
    const char*     pParsedKey = NULL;
    const char*     pParsedValueString = NULL;
    int             parsedValueNumber = 0;
    unsigned int    parsedValueUnsignedNumber = 0;
    unsigned short  parsedValueUnsignedShort = 0;


    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if ((!pDevicePath) || (!pInterface) || (!pKey) || (!pValue)) {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!strcmp(pKey, "Name")) {
        type = DBUS_TYPE_STRING;
    }
    else if (!strcmp(pKey, "Address")) {
        type = DBUS_TYPE_STRING;
    }
    else if (!strcmp(pKey, "Powered")) {
        type = DBUS_TYPE_BOOLEAN;
    }
    else if (!strcmp(pKey, "Paired")) {
        type = DBUS_TYPE_BOOLEAN;
    }
    else if (!strcmp(pKey, "Connected")) {
        type = DBUS_TYPE_BOOLEAN;
    }
    else if (!strcmp(pKey, "Discoverable")) {
        type = DBUS_TYPE_BOOLEAN;
    }
    else if (!strcmp(pKey, "Vendor")) {
        type = DBUS_TYPE_UINT16;
    }
    else {
        type = DBUS_TYPE_INVALID;
        return -1;
    }

    dbus_error_init(&err);
    reply = btrCore_BTSendMethodCall(pDevicePath, pInterface, "GetProperties");
    if (!reply) {
        printf("%s:%d - %s.GetProperties returned an error: '%s'\n", __FUNCTION__, __LINE__, pInterface, err.message);
        rc = -1;
        dbus_error_free(&err);
    }
    else {
        if (!dbus_message_iter_init(reply, &arg_i)) {
            printf("GetProperties reply has no arguments.");
            rc = -1;
        }
        else if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            printf("GetProperties argument is not an array.");
            rc = -1;
        }
        else {
            dbus_message_iter_recurse(&arg_i, &element_i);
            while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
                if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter dict_i;
                    dbus_message_iter_recurse(&element_i, &dict_i);
                    dbus_message_iter_get_basic(&dict_i, &pParsedKey);

                    if ((pParsedKey) && (strcmp (pParsedKey, pKey) == 0)) {
                        dbus_message_iter_next(&dict_i);
                        dbus_message_iter_recurse(&dict_i, &variant_i);
                        if (type == DBUS_TYPE_STRING) {
                            dbus_message_iter_get_basic(&variant_i, &pParsedValueString);
                            //printf("Key is %s and the value in string is %s\n", pParsedKey, pParsedValueString);
                            strncpy (pValue, pParsedValueString, BD_NAME_LEN);
                        }
                        else if (type == DBUS_TYPE_UINT16) {
                            unsigned short* ptr = (unsigned short*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedShort);
                            //printf("Key is %s and the value is %u\n", pParsedKey, parsedValueUnsignedNumber);
                            *ptr = parsedValueUnsignedShort;
                        }
                        else if (type == DBUS_TYPE_UINT32) {
                            unsigned int* ptr = (unsigned int*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedNumber);
                            //printf("Key is %s and the value is %u\n", pParsedKey, parsedValueUnsignedNumber);
                            *ptr = parsedValueUnsignedNumber;
                        }
                        else { /* As of now ints and bools are used. This function has to be extended for array if needed */
                            int* ptr = (int*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueNumber);
                            //printf("Key is %s and the value is %d\n", pParsedKey, parsedValueNumber);
                            *ptr = parsedValueNumber;
                        }
                        rc = 0;
                        break;
                    }
                }

                if (!dbus_message_iter_next(&element_i))
                    break;
            }
        }

        if (dbus_error_is_set(&err)) {
            printf("%s:%d - Some failure noticed and the err message is %s\n", __FUNCTION__, __LINE__, err.message);
            dbus_error_free(&err);
        }

        dbus_message_unref(reply);
    }

    return rc;
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
BtrCore_BTGetPairedDeviceInfo (
    void*                   apBtConn,
    const char*             apBtAdapter,
    stBTPairedDeviceInfo*   pPairedDeviceInfo
) {
    int         i = 0;
    int         num = 0;
    char**      paths = NULL;
    DBusError   lDBusErr;
    stBTDeviceInfo apstBTDeviceInfo;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !pPairedDeviceInfo)
        return -1;

    memset (pPairedDeviceInfo, 0, sizeof (stBTPairedDeviceInfo));

    dbus_error_init(&lDBusErr);
    DBusMessage* reply = btrCore_BTSendMethodCall(apBtAdapter, "org.bluez.Adapter", "ListDevices");
    if (reply != NULL) {
        if (!dbus_message_get_args(reply, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Adapter.ListDevices returned an error: '%s'\n", lDBusErr.message);
        }

        /* Update the number of devices */
        pPairedDeviceInfo->numberOfDevices = num;

        /* Update the paths of these devices */
        for ( i = 0; i < num; i++) {
            strcpy(pPairedDeviceInfo->devicePath[i], paths[i]);
        }
        dbus_message_unref(reply);
    }

    for ( i = 0; i < num; i++) {
        reply = btrCore_BTSendMethodCall(pPairedDeviceInfo->devicePath[i], "org.bluez.Device", "GetProperties");
        if (reply != NULL) {
            memset (&apstBTDeviceInfo, 0, sizeof(apstBTDeviceInfo));
            if (0 != btrCore_BTParseDevice(reply, &apstBTDeviceInfo)) {
                printf ("Parsing the device %s failed..\n", pPairedDeviceInfo->devicePath[i]);
                dbus_message_unref(reply);
                return -1;
            }
            else {
                memcpy (&pPairedDeviceInfo->deviceInfo[i], &apstBTDeviceInfo, sizeof(apstBTDeviceInfo));
            }
        }
        dbus_message_unref(reply);
    }

    return 0;
}


int
BtrCore_BTGetPairedDevices (
    void*           apBtConn,
    const char*     apBtAdapter,
    unsigned int*   apui32PairedDevCnt,
    char**          apcArrPairedDevPath
) {
    int         rc = -1;
    int         i = 0;
    int         num = 0;
    char**      paths = NULL;
    DBusError   lDBusErr;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !apui32PairedDevCnt || !apcArrPairedDevPath)
        return -1;

    dbus_error_init(&lDBusErr);
    //path  busname interface  method

    DBusMessage* reply = btrCore_BTSendMethodCall(apBtAdapter, "org.bluez.Adapter", "ListDevices");
    if (reply != NULL) {
        if (!dbus_message_get_args(reply, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Adapter.ListDevices returned an error: '%s'\n", lDBusErr.message);
        }

        for ( i = 0; i < num; i++) {
            if (apcArrPairedDevPath[i])
                strcpy(apcArrPairedDevPath[i], paths[i]);
        }

        *apui32PairedDevCnt = num;
        dbus_message_unref(reply);

        rc = 0;
    }

    return rc;
}


int
BtrCore_BTDiscoverDeviceServices (
    void*                           apBtConn,
    const char*                     apcDevPath,
    stBTDeviceSupportedServiceList* pProfileList
) {
    DBusMessage *msg, *reply;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;
    DBusError err;
    const char* value;
    char* ret;

    int count = 0;
    char* pSearchString = "";
    char* pUUIDValue = "uuid value=\"0x";
    char* pProfileName = "text value=\"";
    int lengthOfUUID = strlen (pUUIDValue);
    int lengthOfProfile = strlen (pProfileName);
    char buff[10] = "";

    int isUUIDFound = 0;
    int isProfileFound = 0;
    msg = dbus_message_new_method_call( "org.bluez", apcDevPath, "org.bluez.Device", "DiscoverServices");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &pSearchString, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(apBtConn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!reply) {
        fprintf(stderr, "Failure attempting to Discover Services\n");

        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return -1;
    }

    if (!dbus_message_iter_init(reply, &arg_i)) {
       printf("DiscoverServices reply has no information.");
       return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    // printf("type is %d\n", dbus_type);

    dbus_message_iter_recurse(&arg_i, &element_i);
    dbus_type = dbus_message_iter_get_arg_type(&element_i);
    //printf("checking the type, it is %d\n",dbus_type);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        dbus_type = dbus_message_iter_get_arg_type(&element_i);
        //printf("next element_i type is %d\n",dbus_type);

        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
            isUUIDFound = 0;
            isProfileFound = 0;

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // printf("checking the dict subtype, it is %d\n",dbus_type);

            dbus_message_iter_next(&dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // printf("interating the dict subtype, it is %d\n",dbus_type);
            dbus_message_iter_get_basic(&dict_i, &value);

            //printf("Services: %s\n",value);
            ret =  strstr(value, pUUIDValue);
            if (ret != NULL) {
                ret += lengthOfUUID;

                buff[0] = ret[0];
                buff[1] = ret[1];
                buff[2] = ret[2];
                buff[3] = ret[3];
                buff[4] = '\0';
                pProfileList->profile[count].uuid_value = strtol(buff, NULL, 16);
                isUUIDFound = 1;
            }

            ret =  strstr(value, pProfileName);
            if (ret != NULL) {
                char *ptr = NULL;
                int index = 0;
                ret += lengthOfProfile;
                ptr = strchr(ret, '"');
                if (ptr != NULL) {
                    /* shorten the string */
                    index = ptr - ret;
                    if (index < BT_MAX_STR_LEN) {
                        strncpy (pProfileList->profile[count].profile_name, ret, index);
                        isProfileFound = 1;
                    }
                }
            }

            /* increase the Profile/Service Count by 1 */
            if ((isUUIDFound) && (isProfileFound)) {
                count++;
                pProfileList->numberOfService = count;
            }
        }

        //load the new device into our list of scanned devices
        if (!dbus_message_iter_next(&element_i))
            break;

    }

    (void)dbus_type;

    return 0;
}


int 
BtrCore_BTFindServiceSupported (
    void*           apBtConn,
    const char*     apcDevPath,
    const char*     apcSearchString,
    char*           apcDataString
) {
    DBusMessage *msg, *reply;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;
    DBusError err;
    int match;
    const char* value;
    char* ret;
        
   //BTRCore_LOG("apcDevPath is %s\n and service UUID is %s", apcDevPath, apcSearchString);
    msg = dbus_message_new_method_call( "org.bluez",
                                        apcDevPath,
                                        "org.bluez.Device",
                                        "DiscoverServices");
    
    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }
    
    match = 0; //assume it does not match
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &apcSearchString, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(apBtConn, msg, -1, &err);
    
    dbus_message_unref(msg);
    
    if (!reply) {
        fprintf(stderr, "Failure attempting to Discover Services\n");

        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return -1;
    }

    if (!dbus_message_iter_init(reply, &arg_i)) {
       printf("DiscoverServices reply has no information.");
       return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    // printf("type is %d\n", dbus_type);
    
    dbus_message_iter_recurse(&arg_i, &element_i);
    dbus_type = dbus_message_iter_get_arg_type(&element_i);
    //printf("checking the type, it is %d\n",dbus_type);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        dbus_type = dbus_message_iter_get_arg_type(&element_i);
        //printf("next element_i type is %d\n",dbus_type);

        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // printf("checking the dict subtype, it is %d\n",dbus_type);

            dbus_message_iter_next(&dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // printf("interating the dict subtype, it is %d\n",dbus_type);
            dbus_message_iter_get_basic(&dict_i, &value);
            
            // printf("Services: %s\n",value);
            if (apcDataString != NULL) {
                strcpy(apcDataString, value);
            }

            // lets strstr to see if "uuid value="<UUID>" is there
            ret =  strstr(value, apcSearchString);
            if (ret !=NULL) {
                match = 1;//assume it does match
                // printf("match\n");
            }
            else {
                //printf("NO match\n");
                match = 0;//assume it does not match
            }
        }

        //load the new device into our list of scanned devices
        if (!dbus_message_iter_next(&element_i))
            break;

    }

    (void)dbus_type;
    
    return match;
}


int
BtrCore_BTPerformDeviceOp (
    void*           apBtConn,
    const char*     apBtAdapter,
    const char*     apBtAgentPath,
    const char*     apcDevPath,
    enBTDeviceOp    aenBTDevOp
) {
    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;
    char deviceOpString[64] = {'\0'}; 
    int rc = 0;

    /* We can enhance the BTRCore with passcode support later point in time */
    const char *capabilities = "NoInputNoOutput";

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !apBtAgentPath || !apcDevPath || (aenBTDevOp == enBTDevOpUnknown))
        return -1;


    switch (aenBTDevOp) {
        case enBTDevOpFindPairedDev:
            strcpy(deviceOpString, "FindDevice");
            break;
        case enBTDevOpCreatePairedDev:
            strcpy(deviceOpString, "CreatePairedDevice");
            break;
        case enBTDevOpRemovePairedDev:
            strcpy(deviceOpString, "RemoveDevice");
            break;
        case enBTDevOpUnknown:
        default:
            rc = -1;  
            break;
    }

    if (rc == -1)
        return rc;

    msg = dbus_message_new_method_call( "org.bluez",
                                        apBtAdapter,
                                        "org.bluez.Adapter",
                                        deviceOpString);

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    if (aenBTDevOp == enBTDevOpFindPairedDev) {
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &apcDevPath, DBUS_TYPE_INVALID);
    }
    else if (aenBTDevOp == enBTDevOpRemovePairedDev) {
        dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &apcDevPath, DBUS_TYPE_INVALID);
    }
    else if (aenBTDevOp == enBTDevOpCreatePairedDev) {
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &apcDevPath,
                              DBUS_TYPE_OBJECT_PATH, &apBtAgentPath,
                              DBUS_TYPE_STRING, &capabilities,
                              DBUS_TYPE_INVALID);
    }

    dbus_error_init(&err);

    reply = dbus_connection_send_with_reply_and_block(gpDBusConn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!reply) {

        fprintf(stderr, "Pairing failed...\n");

        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }
        return -1;
    }

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
    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSource'", NULL);
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
        char*   key = "UUID";
        int     type = DBUS_TYPE_STRING;

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtUUID);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
    {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
        char*   key = "Codec";
        int     type = DBUS_TYPE_BYTE;

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtMediaCodec);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
    {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant, lDBusMsgIterSubArray;
        char*   key = "Capabilities";
        int     type = DBUS_TYPE_BYTE;

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
    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSource'", NULL);
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
BtrCore_BTSendReceiveMessages (
    void*   apBtConn
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if(dbus_connection_read_write_dispatch(gpDBusConn, 25) != TRUE) {
        return -1;
    }
    
    return 0;
}


int
BtrCore_BTRegisterDevStatusUpdatecB (
    void*                               apBtConn,
    fPtr_BtrCore_BTDevStatusUpdate_cB   afpcBDevStatusUpdate,
    void*                               apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!afpcBDevStatusUpdate)
        return -1;

    gfpcBDevStatusUpdate    = afpcBDevStatusUpdate;
    gpcBDevStatusUserData   = apUserData;

    return 0;
}


int
BtrCore_BTRegisterConnAuthcB (
    void*                       apBtConn,
    fPtr_BtrCore_BTConnAuth_cB  afpcBConnAuth,
    void*                       apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!afpcBConnAuth)
        return -1;

    gfpcBConnectionAuthentication = afpcBConnAuth;
    gpcBConnAuthUserData = apUserData;

    return 0;
}


int
BtrCore_BTRegisterNegotiateMediacB (
    void*                               apBtConn, 
    const char*                         apBtAdapter, 
    char*                               apBtMediaType,
    fPtr_BtrCore_BTNegotiateMedia_cB    afpcBNegotiateMedia,
    void*                               apUserData
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
    void*                                   apBtConn,
    const char*                             apBtAdapter,
    char*                                   apBtMediaType,
    fPtr_BtrCore_BTTransportPathMedia_cB    afpcBTransportPathMedia,
    void*                                   apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter || !apBtMediaType || !afpcBTransportPathMedia)
        return -1;

    gfpcBTransportPathMedia = afpcBTransportPathMedia;

    return 0;
}

