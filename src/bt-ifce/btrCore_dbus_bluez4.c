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
 * btrCore_dbus_bluez4.c
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
#include "btrCore_priv.h"

#define BD_NAME_LEN                     248

#define BT_MEDIA_A2DP_SINK_ENDPOINT     "/MediaEndpoint/A2DPSink"
#define BT_MEDIA_A2DP_SOURCE_ENDPOINT   "/MediaEndpoint/A2DPSource"

/* Static Function Prototypes */
static int btrCore_BTHandleDusError (DBusError* aDBusErr, int aErrline, const char* aErrfunc);

static DBusHandlerResult btrCore_BTDBusConnectionFilter_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTMediaEndpointHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentMessageHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);

static char* btrCore_BTGetDefaultAdapterPath (void);
static int btrCore_BTReleaseDefaultAdapterPath (void);

static DBusHandlerResult btrCore_BTAgentRelease (DBusConnection* apDBusConn, DBusMessage*    apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentRequestPincode (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentRequestPasskey (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentRequestConfirmation(DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentAuthorize (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
static DBusHandlerResult btrCore_BTAgentCancelMessage (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);

static DBusMessage* btrCore_BTSendMethodCall (const char* objectpath, const char* interfacename, const char* methodname);
static int btrCore_BTParseDevice (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static int btrCore_BTParsePropertyChange (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static DBusMessage* btrCore_BTMediaEndpointSelectConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointSetConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointClearConfiguration (DBusMessage* apDBusMsg);



/* Static Global Variables Defs */
static char *gpcBTOutPassCode = NULL;
static int do_reject = 0;
static char gpcDeviceCurrState[BT_MAX_STR_LEN];
static DBusConnection*  gpDBusConn = NULL;
static char* gpcBTAgentPath = NULL;
static char* gpcBTDAdapterPath = NULL;
static char* gpcBTAdapterPath = NULL;
static char* gpcDevTransportPath = NULL;
static void* gpcBDevStatusUserData = NULL;
static void* gpcBNegMediaUserData = NULL;
static void* gpcBTransPathMediaUserData = NULL;
static void* gpcBConnIntimUserData = NULL;
static void* gpcBConnAuthUserData = NULL;

static unsigned int gpcBConnAuthPassKey = 0;

static const DBusObjectPathVTable gDBusMediaEndpointVTable = {
    .message_function = btrCore_BTMediaEndpointHandler_cb,
};

static const DBusObjectPathVTable gDBusAgentVTable = {
    .message_function = btrCore_BTAgentMessageHandler_cb,
};


/* Callbacks */
static fPtr_BtrCore_BTDevStatusUpdate_cB    gfpcBDevStatusUpdate = NULL;
static fPtr_BtrCore_BTNegotiateMedia_cB     gfpcBNegotiateMedia = NULL;
static fPtr_BtrCore_BTTransportPathMedia_cB gfpcBTransportPathMedia = NULL;
static fPtr_BtrCore_BTConnIntim_cB          gfpcBConnectionIntimation = NULL;
static fPtr_BtrCore_BTConnAuth_cB           gfpcBConnectionAuthentication = NULL;


/* Static Function Defs */
static inline int 
btrCore_BTHandleDusError (
    DBusError*  apDBusErr,
    int         aErrline,
    const char* apErrfunc
) {
    if (dbus_error_is_set(apDBusErr)) {
        BTRCORELOG_ERROR ("%d\t:%s - DBus Error is %s\n", aErrline, apErrfunc, apDBusErr->message);
        dbus_error_free(apDBusErr);
        return 1;
    }
    return 0;
}


static DBusHandlerResult
btrCore_BTDBusConnectionFilter_cb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    int             i32OpRet = -1;
    stBTDeviceInfo  lstBTDeviceInfo;

    memset (&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
    lstBTDeviceInfo.i32RSSI = INT_MIN;

    //BTRCORELOG_ERROR ("%d\t:%s - agent filter activated....\n", __LINE__, __FUNCTION__);
    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter", "DeviceCreated")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Created!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStPaired, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter", "DeviceFound")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Found!\n", __LINE__, __FUNCTION__);

        i32OpRet = btrCore_BTParseDevice(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate && !i32OpRet) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStFound, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter","DeviceDisappeared")) {
        BTRCORELOG_INFO ("%d\t:%s - Device DeviceDisappeared!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStLost, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter","DeviceRemoved")) {
        BTRCORELOG_ERROR ("%d\t:%s - Device Removed!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStUnPaired, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","Connected")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Connected - AudioSink!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","Disconnected")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Disconnected - AudioSink!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSink","PropertyChanged")) {
        BTRCORELOG_INFO ("%d\t:%s - Device PropertyChanged!\n", __LINE__, __FUNCTION__);
        btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","Connected")) {
        BTRCORELOG_ERROR ("%d\t:%s - Device Connected - AudioSource!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","Disconnected")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Disconnected - AudioSource!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.AudioSource","PropertyChanged")) {
        BTRCORELOG_INFO ("%d\t:%s - Device PropertyChanged!\n", __LINE__, __FUNCTION__);
        btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSource, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","Connected")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Connected - Headset!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset,enBTDevStConnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","Disconnected")) {
        BTRCORELOG_INFO ("%d\t:%s - Device Disconnected - Headset!\n", __LINE__, __FUNCTION__);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset, enBTDevStDisconnected, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Headset","PropertyChanged")) {
        BTRCORELOG_INFO ("%d\t:%s - Device PropertyChanged!\n", __LINE__, __FUNCTION__);
        btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }

    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.MediaTransport","PropertyChanged")) {
        BTRCORELOG_INFO ("%d\t:%s - MediaTransport PropertyChanged!\n", __LINE__, __FUNCTION__);
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
    const char*     lpcPath;

    lpcPath = dbus_message_get_path(apDBusMsg);
    dbus_error_init(&e);

    (void)lpcPath;
    (void)r;


    BTRCORELOG_INFO ("%d\t:%s - endpoint_handler: MediaEndpoint\n", __LINE__, __FUNCTION__);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "SelectConfiguration")) {
        BTRCORELOG_INFO ("%d\t:%s - endpoint_handler: MediaEndpoint-SelectConfiguration\n", __LINE__, __FUNCTION__);
        lpDBusReply = btrCore_BTMediaEndpointSelectConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "SetConfiguration"))  {
        BTRCORELOG_INFO ("%d\t:%s - endpoint_handler: MediaEndpoint-SetConfiguration\n", __LINE__, __FUNCTION__);
        lpDBusReply = btrCore_BTMediaEndpointSetConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint", "ClearConfiguration")) {
        BTRCORELOG_INFO ("%d\t:%s - endpoint_handler: MediaEndpoint-ClearConfiguration\n", __LINE__, __FUNCTION__);
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

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent",    "RequestPinCode"))
        return btrCore_BTAgentRequestPincode(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent",    "RequestPasskey"))
        return btrCore_BTAgentRequestPasskey(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "RequestConfirmation"))
        return btrCore_BTAgentRequestConfirmation(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "Authorize"))
        return btrCore_BTAgentAuthorize(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent", "Cancel"))
        return btrCore_BTAgentCancelMessage(apDBusConn, apDBusMsg, userdata);

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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't find Default adapter\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't get lpDBusReply arguments\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
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
btrCore_BTAgentRelease (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage *lpDBusReply;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for Release method", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Unable to create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);
       //return the result
    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentRequestPincode (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage*    lpDBusReply;
    const char*     lpcPath;

    if (!gpcBTOutPassCode)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for RequestPinCode method", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (do_reject) {
        lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
        goto sendmsg;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    BTRCORELOG_INFO ("%d\t:%s - Pincode request for device %s\n", __LINE__, __FUNCTION__, lpcPath);
    dbus_message_append_args(lpDBusReply, DBUS_TYPE_STRING, &gpcBTOutPassCode,DBUS_TYPE_INVALID);

sendmsg:
    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);

    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentRequestPasskey (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage*    lpDBusReply;
    const char*     lpcPath;
    unsigned int    ui32PassCode;

    if (!gpcBTOutPassCode)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args(apDBusMsg, NULL,DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_INVALID))  {
        BTRCORELOG_ERROR ("%d\t:%s - Incorrect args btrCore_BTAgentRequestPasskey", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    BTRCORELOG_INFO ("%d\t:%s - Pass code request for device %s\n", __LINE__, __FUNCTION__, lpcPath);
    ui32PassCode = strtoul(gpcBTOutPassCode, NULL, 10);
    dbus_message_append_args(lpDBusReply, DBUS_TYPE_UINT32, &ui32PassCode, DBUS_TYPE_INVALID);

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusReply);
    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentRequestConfirmation (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage*    lpDBusReply;
    const char*     lpcPath; 
    unsigned int    ui32PassCode = 0;;

    const char *dev_name; //pass the dev name to the callback for app to use
    int yesNo;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_UINT32, &ui32PassCode, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for Authorize method", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    BTRCORELOG_INFO ("%d\t:%s - btrCore_BTAgentRequestConfirmation: PASS Code for %s is %6d\n", __LINE__, __FUNCTION__ ,lpcPath, ui32PassCode);

    if (gfpcBConnectionIntimation) {
        BTRCORELOG_ERROR ("%d\t:%s - calling ConnIntimation cb with %s...\n", __LINE__, __FUNCTION__, lpcPath);
        dev_name = "Bluetooth Device";//TODO connect device name with btrCore_GetKnownDeviceName 

        if (dev_name != NULL) {
            yesNo = gfpcBConnectionIntimation(dev_name, ui32PassCode, gpcBConnIntimUserData);
        }
        else {
            //couldnt get the name, provide the bt address instead
            yesNo = gfpcBConnectionIntimation(lpcPath, ui32PassCode, gpcBConnIntimUserData);
        }

        if (yesNo == 0) {
            //BTRCORELOG_ERROR ("%d\t:%s - sorry dude, you cant connect....\n", __LINE__, __FUNCTION__);
            lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
            goto sendReqConfError;
        }
    }

    gpcBConnAuthPassKey = ui32PassCode;

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

sendReqConfError:
    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusReply);

    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentAuthorize (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage *lpDBusReply;
    const char *lpcPath, *uuid;
    const char *dev_name; //pass the dev name to the callback for app to use
    int yesNo;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_STRING, &uuid, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for Authorize method", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (gfpcBConnectionAuthentication) {
        BTRCORELOG_INFO ("%d\t:%s - calling ConnAuth cb with %s...\n", __LINE__, __FUNCTION__, lpcPath);
        dev_name = "Bluetooth Device";//TODO connect device name with btrCore_GetKnownDeviceName 

        if (dev_name != NULL) {
            yesNo = gfpcBConnectionAuthentication(dev_name, gpcBConnAuthUserData);
        }
        else {
            //couldnt get the name, provide the bt address instead
            yesNo = gfpcBConnectionAuthentication(lpcPath, gpcBConnAuthUserData);
        }

        if (yesNo == 0) {
            //BTRCORELOG_ERROR ("%d\t:%s - sorry dude, you cant connect....\n", __LINE__, __FUNCTION__);
            lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
            goto sendAuthError;
        }
    }

    gpcBConnAuthPassKey = 0;

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    BTRCORELOG_INFO ("%d\t:%s - Authorizing request for %s\n", __LINE__, __FUNCTION__, lpcPath);

sendAuthError:
    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusReply);

    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTAgentCancelMessage (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           userdata
) {
    DBusMessage *lpDBusReply;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for confirmation method", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    BTRCORELOG_INFO ("%d\t:%s - Request canceled\n", __LINE__, __FUNCTION__);
    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't create lpDBusReply message\n", __LINE__, __FUNCTION__);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);
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
    DBusMessage*     lpDBusReply;
    DBusMessage*     methodcall = dbus_message_new_method_call( busname,
                                                                objectpath,
                                                                interfacename,
                                                                methodname);

    if (methodcall == NULL) {
        BTRCORELOG_ERROR ("%d\t:%s - Cannot allocate DBus message!\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    //Now do a sync call
    if (!dbus_connection_send_with_reply(gpDBusConn, methodcall, &pending, -1)) { //Send and expect lpDBusReply using pending call object
        BTRCORELOG_ERROR ("%d\t:%s - failed to send message!\n", __LINE__, __FUNCTION__);
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(methodcall);
    methodcall = NULL;

    dbus_pending_call_block(pending);               //Now block on the pending call
    lpDBusReply = dbus_pending_call_steal_reply(pending); //Get the lpDBusReply message from the queue
    dbus_pending_call_unref(pending);               //Free pending call handle

    if (dbus_message_get_type(lpDBusReply) ==  DBUS_MESSAGE_TYPE_ERROR) {
        BTRCORELOG_ERROR ("%d\t:%s - Error : %s\n\n", __LINE__, __FUNCTION__, dbus_message_get_error_name(lpDBusReply));
        dbus_message_unref(lpDBusReply);
        lpDBusReply = NULL;
    }

    return lpDBusReply;
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
        BTRCORELOG_ERROR ("%d\t: %s - dbus_message_iter_init Failed\n", __LINE__, __FUNCTION__);
        return -1;
    }

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &pcBTDevAddr,
                                DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t: %s - dbus_message_get_args Failed\n", __LINE__, __FUNCTION__);
        //return -1; Users of btrCore_BTParseDevice should not call it if the message contains no valid args
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&arg_i);
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            BTRCORELOG_ERROR ("%d\t: %s - Unknown Prop structure from Bluez\n", __LINE__, __FUNCTION__);
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
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->pcAddress : %s\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->pcAddress);
            }
            else if (strcmp (pcKey, "Name") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcName);
                strncpy(apstBTDeviceInfo->pcName, pcName, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->pcName: %s\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->pcName);

            }
            else if (strcmp (pcKey, "Vendor") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Vendor);
                apstBTDeviceInfo->ui16Vendor = ui16Vendor;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->ui16Vendor = %d\n", __LINE__, __FUNCTION__ , apstBTDeviceInfo->ui16Vendor);
            }
            else if (strcmp (pcKey, "VendorSource") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16VendorSource);
                apstBTDeviceInfo->ui16VendorSource = ui16VendorSource;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->ui16VendorSource = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->ui16VendorSource);
            }
            else if (strcmp (pcKey, "Product") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Product);
                apstBTDeviceInfo->ui16Product = ui16Product;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->ui16Product = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->ui16Product);
            }
            else if (strcmp (pcKey, "Version") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Version);
                apstBTDeviceInfo->ui16Version = ui16Version;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->ui16Version = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->ui16Version);
            }
            else if (strcmp (pcKey, "Icon") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcIcon);
                strncpy(apstBTDeviceInfo->pcIcon, pcIcon, BT_MAX_STR_LEN);
                BTRCORELOG_ERROR ("%d\t:%s - apstBTDeviceInfo->pcIcon: %s\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->pcIcon);
            }
            else if (strcmp (pcKey, "Class") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui32Class);
                apstBTDeviceInfo->ui32Class = ui32Class;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->ui32Class: %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->ui32Class);
            }
            else if (strcmp (pcKey, "Paired") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bPaired);
                apstBTDeviceInfo->bPaired = bPaired;
                BTRCORELOG_ERROR ("%d\t:%s - apstBTDeviceInfo->bPaired = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->bPaired);
            }
            else if (strcmp (pcKey, "Connected") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bConnected);
                apstBTDeviceInfo->bConnected = bConnected;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->bConnected = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->bConnected);
            }
            else if (strcmp (pcKey, "Trusted") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bTrusted);
                apstBTDeviceInfo->bTrusted = bTrusted;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->bTrusted = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->bTrusted);
            }
            else if (strcmp (pcKey, "Blocked") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bBlocked);
                apstBTDeviceInfo->bBlocked = bBlocked;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->bBlocked = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->bBlocked);
            }
            else if (strcmp (pcKey, "Alias") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcAlias);
                strncpy(apstBTDeviceInfo->pcAlias, pcAlias, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->pcAlias: %s\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->pcAlias);
            }
            else if (strcmp (pcKey, "RSSI") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &i16RSSI);
                apstBTDeviceInfo->i32RSSI = i16RSSI;
                BTRCORELOG_INFO ("%d\t:%s - apstBTDeviceInfo->i32RSSI = %d i16RSSI = %d\n", __LINE__, __FUNCTION__, apstBTDeviceInfo->i32RSSI, i16RSSI);
            }
            else if (strcmp (pcKey, "UUIDs") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);

                dbus_type = dbus_message_iter_get_arg_type (&variant_i);
                if (dbus_type == DBUS_TYPE_ARRAY) {
                    int count = 0;
                    DBusMessageIter variant_j;
                    dbus_message_iter_recurse(&variant_i, &variant_j);

                    while ((dbus_type = dbus_message_iter_get_arg_type (&variant_j)) != DBUS_TYPE_INVALID)
                    {
                        if ((dbus_type == DBUS_TYPE_STRING) && (count < BT_MAX_DEVICE_PROFILE))
                        {
                            char *pVal = NULL;
                            dbus_message_iter_get_basic (&variant_j, &pVal);
                            BTRCORELOG_INFO ("%d\t:%s - UUID value is %s\n", __LINE__, __FUNCTION__, pVal);
                            strncpy(apstBTDeviceInfo->aUUIDs[count], pVal, (BT_MAX_UUID_STR_LEN - 1));
                            count++;
                        }
                        dbus_message_iter_next (&variant_j);
                    }
                }
                else
                {
                    BTRCORELOG_ERROR ("%d\t:%s - apstBTDeviceInfo->Services; Not an Array\n", __LINE__, __FUNCTION__);
                }
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

    const char* lpcDBusMsgObjPath= dbus_message_get_path(apDBusMsg);
    char*       lpcinBtDevAddr   = strstr(lpcDBusMsgObjPath, "/dev_") + strlen("/dev_");
    char*       lpcstBtDevAddr   = apstBTDeviceInfo->pcAddress;
    int         i32BtDevAddrLen  = strlen(lpcinBtDevAddr);
    int         i32LoopIdx       = 0;

    for (i32LoopIdx = 0; i32LoopIdx < i32BtDevAddrLen; i32LoopIdx++) {
        if (lpcinBtDevAddr[i32LoopIdx] == '_')
            lpcstBtDevAddr[i32LoopIdx] = ':';
        else
            lpcstBtDevAddr[i32LoopIdx] = lpcinBtDevAddr[i32LoopIdx];
    }

    BTRCORELOG_INFO(stderr, "%d\t:%s - Path = %s Address = %s\n", __LINE__, __FUNCTION__, lpcDBusMsgObjPath, lpcstBtDevAddr);

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
       BTRCORELOG_ERROR ("%d\t:%s - GetProperties lpDBusReply has no arguments.", __LINE__, __FUNCTION__);
    }

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &bd_addr,
                                DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("%d\t:%s - Invalid arguments for NameOwnerChanged signal", __LINE__, __FUNCTION__);
        return -1;
    }

    BTRCORELOG_INFO ("%d\t:%s -  Name: %s\n", __LINE__, __FUNCTION__, bd_addr);//"State" then the variant is a string
    if (strcmp(bd_addr,"State") == 0) {
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
        //BTRCORELOG_ERROR ("%d\t:%s - type is %d\n", __LINE__, __FUNCTION__, dbus_type);

        if (dbus_type == DBUS_TYPE_STRING) {
            dbus_message_iter_next(&arg_i);
            dbus_message_iter_recurse(&arg_i, &variant_i);
            dbus_message_iter_get_basic(&variant_i, &value);
             // BTRCORELOG_ERROR ("%d\t:%s -     the new state is: %s\n", __LINE__, __FUNCTION__, value);

            if (strcmp(gpcDeviceCurrState, value) != 0) {
                strncpy(apstBTDeviceInfo->pcDevicePrevState, gpcDeviceCurrState, BT_MAX_STR_LEN - 1);
                strncpy(apstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                strncpy(gpcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
            }
        }
    }
    else if (strcmp(bd_addr, "Connected") == 0) {
        int isConnected = 0;

        // The gpcDeviceCurrState could be either "connecting" or "playing"; just in case, if it comes in other scenario, just ignore
        if ((strcmp (gpcDeviceCurrState, "connecting") == 0) ||
            (strcmp (gpcDeviceCurrState, "playing") == 0))
        {
            dbus_type = dbus_message_iter_get_arg_type(&arg_i);
            if (dbus_type == DBUS_TYPE_BOOLEAN)
            {
                dbus_message_iter_next(&arg_i);
                dbus_message_iter_recurse(&arg_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &isConnected);

                if (1 == isConnected)
                    value = "connected";
                else if (0 == isConnected)
                    value = "disconnected";

                strncpy(apstBTDeviceInfo->pcDevicePrevState, gpcDeviceCurrState, BT_MAX_STR_LEN - 1);
                strncpy(apstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                strncpy(gpcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
            }
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
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to select configuration");
    }

    if (gfpcBNegotiateMedia) {
        if(!(lpOutputMediaCaps = gfpcBNegotiateMedia(lpInputMediaCaps, gpcBNegMediaUserData))) {
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
    const char* dev_path = NULL, *uuid = NULL, *routing = NULL;
    int codec = 0;
    unsigned char* config = NULL;
    int size = 0;
    int nrec= 0, inbandRingtone = 0;
    unsigned short int delay = 0;
    unsigned short int volume= 0;

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
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - UUID %s\n", __LINE__, __FUNCTION__, uuid);
        }
        else if (strcasecmp(key, "Device") == 0) {
            if (ldBusType != DBUS_TYPE_OBJECT_PATH)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &dev_path);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - Device %s\n", __LINE__, __FUNCTION__, dev_path);
        }
        else if (strcasecmp(key, "Codec") == 0) {
            if (ldBusType != DBUS_TYPE_BYTE)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &codec);
            BTRCORELOG_ERROR ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - Codec %d\n", __LINE__, __FUNCTION__, codec);
        }
        else if (strcasecmp(key, "Configuration") == 0) {
            if (ldBusType != DBUS_TYPE_ARRAY)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_recurse(&lDBusMsgIterValue, &lDBusMsgIterArr);
            dbus_message_iter_get_fixed_array(&lDBusMsgIterArr, &config, &size);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - Configuration \n", __LINE__, __FUNCTION__);
        }
        else if (strcasecmp(key, "Delay") == 0) {
            if (ldBusType != DBUS_TYPE_UINT16)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &delay);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - Delay %d\n", __LINE__, __FUNCTION__, delay);
        }
        else if (strcasecmp(key, "NREC") == 0) {
            if (ldBusType != DBUS_TYPE_BOOLEAN)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &nrec);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - NREC %d\n", __LINE__, __FUNCTION__, nrec);
        }
        else if (strcasecmp(key, "InbandRingtone") == 0) {
            if (ldBusType != DBUS_TYPE_BOOLEAN)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &inbandRingtone);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - InbandRingtone %d\n", __LINE__, __FUNCTION__, inbandRingtone);
        }
        else if (strcasecmp(key, "Routing") == 0) {
            if (ldBusType != DBUS_TYPE_STRING)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &routing);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - routing %s\n", __LINE__, __FUNCTION__, routing);
        }
        else if (strcasecmp(key, "Volume") == 0) {
            if (ldBusType != DBUS_TYPE_UINT16)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &volume);
            BTRCORELOG_INFO ("%d\t:%s - btrCore_BTMediaEndpointSetConfiguration - Volume %d\n", __LINE__, __FUNCTION__, volume);
        }

        dbus_message_iter_next(&lDBusMsgIterProp);
    }

    BTRCORELOG_INFO ("%d\t:%s - Set configuration - Transport Path %s\n", __LINE__, __FUNCTION__, lDevTransportPath);

    if (gpcDevTransportPath) {
        free(gpcDevTransportPath);
        gpcDevTransportPath = NULL;
    }

    gpcDevTransportPath = strdup(lDevTransportPath);

    if (gfpcBTransportPathMedia) {
        if((lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, config, gpcBTransPathMediaUserData))) {
            BTRCORELOG_INFO ("%d\t:%s - Stored - Transport Path 0x%p:%s\n", __LINE__, __FUNCTION__, lStoredDevTransportPath, lStoredDevTransportPath);
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
    BTRCORELOG_INFO ("%d\t:%s - Clear configuration - Transport Path %s\n", __LINE__, __FUNCTION__, lDevTransportPath);

    if (gpcDevTransportPath) {
        free(gpcDevTransportPath);
        gpcDevTransportPath = NULL;
    }

    if (gfpcBTransportPathMedia) {
        if(!(lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, NULL, gpcBTransPathMediaUserData))) {
            BTRCORELOG_INFO ("%d\t:%s - Cleared - Transport Path %s\n", __LINE__, __FUNCTION__, lDevTransportPath);
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
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return NULL;
    }

    BTRCORELOG_INFO ("%d\t:%s - DBus Debug DBus Connection Name %s\n", __LINE__, __FUNCTION__, dbus_bus_get_unique_name (lpDBusConn));
    gpDBusConn = lpDBusConn;

    if (!dbus_connection_add_filter(gpDBusConn, btrCore_BTDBusConnectionFilter_cb, NULL, NULL)) {
        BTRCORELOG_ERROR ("%d\t: %s - Can't add signal filter - BtrCore_BTInitGetConnection\n", __LINE__, __FUNCTION__);
        BtrCore_BTDeInitReleaseConnection(lpDBusConn);
        return NULL;
    }

    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    gpcBConnAuthPassKey             = 0;

    gpcBTransPathMediaUserData      = NULL;
    gpcBNegMediaUserData            = NULL;
    gpcBConnIntimUserData           = NULL;
    gpcBConnAuthUserData            = NULL;
    gpcBDevStatusUserData           = NULL;
    gfpcBDevStatusUpdate            = NULL;
    gfpcBNegotiateMedia             = NULL;
    gfpcBTransportPathMedia         = NULL;
    gfpcBConnectionIntimation       = NULL;
    gfpcBConnectionAuthentication   = NULL;

    return (void*)gpDBusConn;
}


int
BtrCore_BTDeInitReleaseConnection (
    void* apBtConn
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;


    if (gpcBTAgentPath) {
        free(gpcBTAgentPath);
        gpcBTAgentPath = NULL;
    }

    if (gpcBTDAdapterPath) {
        free(gpcBTDAdapterPath);
        gpcBTDAdapterPath = NULL;
    }

    if (gpcBTAdapterPath) {
        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }

    gfpcBConnectionAuthentication   = NULL;
    gfpcBConnectionIntimation       = NULL;
    gfpcBTransportPathMedia         = NULL;
    gfpcBNegotiateMedia             = NULL;
    gfpcBDevStatusUpdate            = NULL;
    gpcBDevStatusUserData           = NULL;
    gpcBConnAuthUserData            = NULL;
    gpcBConnIntimUserData           = NULL;
    gpcBNegMediaUserData            = NULL;
    gpcBTransPathMediaUserData      = NULL;

    gpcBConnAuthPassKey             = 0;

    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    dbus_connection_remove_filter(gpDBusConn, btrCore_BTDBusConnectionFilter_cb, NULL);

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

    DBusMessage *apDBusMsg, *lpDBusReply;
    DBusError err;

    if (!dbus_connection_register_object_path(gpDBusConn, apBtAgentPath, &gDBusAgentVTable, NULL))  {
        BTRCORELOG_ERROR ("%d\t:%s - Error registering object path for agent\n", __LINE__, __FUNCTION__);
        return -1;
    }

    apDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter,"org.bluez.Adapter", "RegisterAgent");
    if (!apDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Error allocating new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath,    DBUS_TYPE_STRING, &capabilities,DBUS_TYPE_INVALID);

    dbus_error_init(&err);

    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &err);

    dbus_message_unref(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Unable to register agent\n", __LINE__, __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            BTRCORELOG_ERROR ("%d\t:%s - %s\n", __LINE__, __FUNCTION__, err.message);
            dbus_error_free(&err);
        }
        return -1;
    }

    dbus_message_unref(lpDBusReply);

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

    DBusMessage *apDBusMsg, *lpDBusReply;
    DBusError err;

    apDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter, "org.bluez.Adapter", "UnregisterAgent");
    if (!apDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);

    dbus_error_init(&err);

    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &err);

    dbus_message_unref(apDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't unregister agent\n", __LINE__, __FUNCTION__);
        if (dbus_error_is_set(&err))  {
            BTRCORELOG_ERROR ("%d\t:%s - %s\n", __LINE__, __FUNCTION__, err.message);
            dbus_error_free(&err);
        }

        return -1;//this was an error case
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    if (!dbus_connection_unregister_object_path(gpDBusConn, apBtAgentPath)) {
        BTRCORELOG_ERROR ("%d\t:%s - Error unregistering object path for agent\n", __LINE__, __FUNCTION__);
        return -1;
    }

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
    DBusMessage *lpDBusReply = btrCore_BTSendMethodCall("/", "org.bluez.Manager", "ListAdapters");
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - org.bluez.Manager.ListAdapters returned an error: '%s'\n", __LINE__, __FUNCTION__, err.message);
        dbus_error_free(&err);
    }
    else {
        if (dbus_message_get_args(lpDBusReply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            if (apBtNumAdapters && apcArrBtAdapterPath) {
                *apBtNumAdapters = num;

                for (i = 0; i < num; i++) {
                    if (*(apcArrBtAdapterPath + i)) {
                        BTRCORELOG_INFO ("%d\t:%s - Adapter Path: %d is %s\n", __LINE__, __FUNCTION__, i, paths[i]);
                        strncpy(*(apcArrBtAdapterPath + i), paths[i], BD_NAME_LEN);
                        rc = 0;
                    }
                }
            }
        }
        else {
            BTRCORELOG_ERROR ("%d\t:%s - org.bluez.Manager.GetProperties parsing failed '%s'\n", __LINE__, __FUNCTION__, err.message);
            dbus_error_free(&err);
        }

        dbus_message_unref(lpDBusReply);
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &apBtAdapter, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't find adapter %s\n", __LINE__, __FUNCTION__, apBtAdapter);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return NULL;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_OBJECT_PATH, &lpReplyPath, DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't get lpDBusReply arguments\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);

    if (gpcBTAdapterPath) {
        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }

    gpcBTAdapterPath = strndup(lpReplyPath, strlen(lpReplyPath));
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
            BTRCORELOG_ERROR ("%d\t:%s - ERROR: Looks like Adapter path has been changed by User\n", __LINE__, __FUNCTION__);

        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }

    return 0;
}


int
BtrCore_BTGetIfceNameVersion (
    void* apBtConn,
    char* apBtOutIfceName,
    char* apBtOutVersion
) {
    FILE*   lfpVersion = NULL;
    char    lcpVersion[8] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtOutIfceName || !apBtOutVersion)
        return -1;

    lfpVersion = popen("/usr/sbin/bluetoothd --version", "r");
    if ((lfpVersion == NULL)) { 
        BTRCORELOG_ERROR ("%d\t:%s - Failed to run Version command\n", __LINE__, __FUNCTION__);
        strncpy(lcpVersion, "4.XXX", strlen("4.XXX"));
    }   
    else {
        if (fgets(lcpVersion, sizeof(lcpVersion)-1, lfpVersion) == NULL) {
            BTRCORELOG_ERROR ("%d\t:%s - Failed to Valid Version\n", __LINE__, __FUNCTION__);
            strncpy(lcpVersion, "4.XXX", strlen("4.XXX"));
        }
        
        pclose(lfpVersion);
    }


    strncpy(apBtOutIfceName, "Bluez", strlen("Bluez"));
    strncpy(apBtOutVersion, lcpVersion, strlen(lcpVersion));

    return 0;
}


int
BtrCore_BTGetProp (
    void*           apBtConn,
    const char*     apcPath,
    enBTOpType      aenBTOpType,
    const char*     pKey,
    void*           pValue
) {
    int             rc = 0;
    int             type;
    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    DBusError       err;
    const char*     pParsedKey = NULL;
    const char*     pParsedValueString = NULL;
    int             parsedValueNumber = 0;
    unsigned int    parsedValueUnsignedNumber = 0;
    unsigned short  parsedValueUnsignedShort = 0;

    const char*     pInterface          = NULL;
    const char*     pAdapterInterface   = "org.bluez.Adapter";
    const char*     pDeviceInterface    = "org.bluez.Device";
    const char*     pMediaTransInterface= "org.bluez.MediaTransport";

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if ((!apcPath) || (!pKey) || (!pValue)) {
        BTRCORELOG_ERROR ("%d\t:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __LINE__, __FUNCTION__);
        return -1;
    }

    switch (aenBTOpType) {
    case enBTAdapter:
        pInterface = pAdapterInterface;
        break;
    case enBTDevice:
        pInterface = pDeviceInterface;
        break;
    case enBTMediaTransport:
        pInterface = pMediaTransInterface;
        break;
    case enBTUnknown:
    default:
        pInterface = pAdapterInterface;
        break;
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
    else if (!strcmp(pKey, "Delay")) {
        type = DBUS_TYPE_UINT16;
    }
    else {
        type = DBUS_TYPE_INVALID;
        return -1;
    }

    dbus_error_init(&err);
    lpDBusReply = btrCore_BTSendMethodCall(apcPath, pInterface, "GetProperties");
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - %s.GetProperties returned an error: '%s'\n", __LINE__, __FUNCTION__, pInterface, err.message);
        rc = -1;
        dbus_error_free(&err);
    }
    else {
        if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
            BTRCORELOG_ERROR ("%d\t:%s - GetProperties lpDBusReply has no arguments.", __LINE__, __FUNCTION__);
            rc = -1;
        }
        else if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            BTRCORELOG_ERROR ("%d\t:%s - GetProperties argument is not an array.", __LINE__, __FUNCTION__);
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
                            //BTRCORELOG_ERROR ("%d\t:%s - Key is %s and the value in string is %s\n", __LINE__, __FUNCTION__, pParsedKey, pParsedValueString);
                            strncpy (pValue, pParsedValueString, BD_NAME_LEN);
                        }
                        else if (type == DBUS_TYPE_UINT16) {
                            unsigned short* ptr = (unsigned short*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedShort);
                            //BTRCORELOG_ERROR ("%d\t:%s - Key is %s and the value is %u\n", __LINE__, __FUNCTION__, pParsedKey, parsedValueUnsignedNumber);
                            *ptr = parsedValueUnsignedShort;
                        }
                        else if (type == DBUS_TYPE_UINT32) {
                            unsigned int* ptr = (unsigned int*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedNumber);
                            //BTRCORELOG_ERROR ("%d\t:%s - Key is %s and the value is %u\n", __LINE__, __FUNCTION__, pParsedKey, parsedValueUnsignedNumber);
                            *ptr = parsedValueUnsignedNumber;
                        }
                        else { /* As of now ints and bools are used. This function has to be extended for array if needed */
                            int* ptr = (int*) pValue;
                            dbus_message_iter_get_basic(&variant_i, &parsedValueNumber);
                            //BTRCORELOG_ERROR ("%d\t:%s - Key is %s and the value is %d\n", __LINE__, __FUNCTION__, pParsedKey, parsedValueNumber);
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
            BTRCORELOG_ERROR ("%d\t:%s - Some failure noticed and the err message is %s\n", __LINE__, __FUNCTION__, err.message);
            dbus_error_free(&err);
        }

        dbus_message_unref(lpDBusReply);
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Invalid Adaptre Property\n", __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Invalid DBus Type\n", __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Reply Null\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
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
    DBusMessage* lpDBusReply = btrCore_BTSendMethodCall(apBtAdapter, "org.bluez.Adapter", "ListDevices");
    if (lpDBusReply != NULL) {
        if (!dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            BTRCORELOG_ERROR ("%d\t:%s - org.bluez.Adapter.ListDevices returned an error: '%s'\n", __LINE__, __FUNCTION__, lDBusErr.message);
        }

        /* Update the number of devices */
        pPairedDeviceInfo->numberOfDevices = num;

        /* Update the paths of these devices */
        for ( i = 0; i < num; i++) {
            strcpy(pPairedDeviceInfo->devicePath[i], paths[i]);
        }
        dbus_message_unref(lpDBusReply);
    }

    for ( i = 0; i < num; i++) {
        lpDBusReply = btrCore_BTSendMethodCall(pPairedDeviceInfo->devicePath[i], "org.bluez.Device", "GetProperties");
        if (lpDBusReply != NULL) {
            memset (&apstBTDeviceInfo, 0, sizeof(apstBTDeviceInfo));
            if (0 != btrCore_BTParseDevice(lpDBusReply, &apstBTDeviceInfo)) {
                BTRCORELOG_ERROR ("%d\t:%s - Parsing the device %s failed..\n", __LINE__, __FUNCTION__, pPairedDeviceInfo->devicePath[i]);
                dbus_message_unref(lpDBusReply);
                return -1;
            }
            else {
                memcpy (&pPairedDeviceInfo->deviceInfo[i], &apstBTDeviceInfo, sizeof(apstBTDeviceInfo));
            }
        }
        dbus_message_unref(lpDBusReply);
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

    DBusMessage* lpDBusReply = btrCore_BTSendMethodCall(apBtAdapter, "org.bluez.Adapter", "ListDevices");
    if (lpDBusReply != NULL) {
        if (!dbus_message_get_args(lpDBusReply, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            BTRCORELOG_ERROR ("%d\t:%s - org.bluez.Adapter.ListDevices returned an error: '%s'\n", __LINE__, __FUNCTION__, lDBusErr.message);
        }

        for ( i = 0; i < num; i++) {
            if (apcArrPairedDevPath[i])
                strcpy(apcArrPairedDevPath[i], paths[i]);
        }

        *apui32PairedDevCnt = num;
        dbus_message_unref(lpDBusReply);

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
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;
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

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevPath,
                                             "org.bluez.Device",
                                             "DiscoverServices");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pSearchString, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(apBtConn, lpDBusMsg, 2500, &lDBusErr); /* Set the timeout as 2.5 sec */
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Discover Services FAILED\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
       BTRCORELOG_ERROR ("%d\t:%s - DiscoverServices information lpDBusReply empty", __LINE__, __FUNCTION__);
       dbus_message_unref(lpDBusReply);
       return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    // BTRCORELOG_ERROR ("%d\t:%s - type is %d\n", __LINE__, __FUNCTION__, dbus_type);

    dbus_message_iter_recurse(&arg_i, &element_i);
    dbus_type = dbus_message_iter_get_arg_type(&element_i);
    //BTRCORELOG_ERROR ("%d\t:%s - checking the type, it is %d\n", __LINE__, __FUNCTION__, dbus_type);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        dbus_type = dbus_message_iter_get_arg_type(&element_i);
        //BTRCORELOG_ERROR ("%d\t:%s - next element_i type is %d\n", __LINE__, __FUNCTION__, dbus_type);

        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
            isUUIDFound = 0;
            isProfileFound = 0;

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("%d\t:%s - checking the dict subtype, it is %d\n", __LINE__, __FUNCTION__, dbus_type);

            dbus_message_iter_next(&dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("%d\t:%s - interating the dict subtype, it is %d\n", __LINE__, __FUNCTION__, dbus_type);
            dbus_message_iter_get_basic(&dict_i, &value);

            //BTRCORELOG_ERROR ("%d\t:%s - Services: %s\n", __LINE__, __FUNCTION__, value);
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

    dbus_message_unref(lpDBusReply);
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
    DBusMessage *msg, *lpDBusReply;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;
    DBusError err;
    int match;
    const char* value;
    char* ret;

   //BTRCORELOG_ERROR ("%d\t:%s - apcDevPath is %s\n and service UUID is %s", __LINE__, __FUNCTION__, apcDevPath, apcSearchString);
    msg = dbus_message_new_method_call( "org.bluez",
                                        apcDevPath,
                                        "org.bluez.Device",
                                        "DiscoverServices");

    if (!msg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    match = 0; //assume it does not match
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &apcSearchString, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    lpDBusReply = dbus_connection_send_with_reply_and_block(apBtConn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Failure attempting to Discover Services\n", __LINE__, __FUNCTION__);

        if (dbus_error_is_set(&err)) {
            BTRCORELOG_ERROR ("%d\t:%s - %s\n", __LINE__, __FUNCTION__, err.message);
            dbus_error_free(&err);
        }

        return -1;
    }

    if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
       BTRCORELOG_ERROR ("%d\t:%s - DiscoverServices lpDBusReply has no information.", __LINE__, __FUNCTION__);
       return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    // BTRCORELOG_ERROR ("%d\t:%s - type is %d\n", __LINE__, __FUNCTION__, dbus_type);

    dbus_message_iter_recurse(&arg_i, &element_i);
    dbus_type = dbus_message_iter_get_arg_type(&element_i);
    //BTRCORELOG_ERROR ("%d\t:%s - checking the type, it is %d\n", __LINE__, __FUNCTION__, dbus_type);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        dbus_type = dbus_message_iter_get_arg_type(&element_i);
        //BTRCORELOG_ERROR ("%d\t:%s - next element_i type is %d\n", __LINE__, __FUNCTION__, dbus_type);

        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("%d\t:%s - checking the dict subtype, it is %d\n", __LINE__, __FUNCTION__, dbus_type);

            dbus_message_iter_next(&dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("%d\t:%s - interating the dict subtype, it is %d\n", __LINE__, __FUNCTION__, dbus_type);
            dbus_message_iter_get_basic(&dict_i, &value);

            // BTRCORELOG_ERROR ("%d\t:%s - Services: %s\n", __LINE__, __FUNCTION__, value);
            if (apcDataString != NULL) {
                strcpy(apcDataString, value);
            }

            // lets strstr to see if "uuid value="<UUID>" is there
            ret =  strstr(value, apcSearchString);
            if (ret !=NULL) {
                match = 1;//assume it does match
                // BTRCORELOG_ERROR ("%d\t:%s - match\n", __LINE__, __FUNCTION__);
            }
            else {
                //BTRCORELOG_ERROR ("%d\t:%s - NO match\n", __LINE__, __FUNCTION__);
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
BtrCore_BTPerformAdapterOp (
    void*           apBtConn,
    const char*     apBtAdapter,
    const char*     apBtAgentPath,
    const char*     apcDevPath,
    enBTAdapterOp   aenBTAdpOp
) {
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    char            deviceOpString[64] = {'\0'};
    int             rc = 0;

    /* We can enhance the BTRCore with passcode support later point in time */
#if 0
    const char*      capabilities = "NoInputNoOutput";
#else
    const char*      capabilities = "DisplayYesNo";
#endif

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !apBtAgentPath || !apcDevPath || (aenBTAdpOp == enBTAdpOpUnknown))
        return -1;


    switch (aenBTAdpOp) {
        case enBTAdpOpFindPairedDev:
            strcpy(deviceOpString, "FindDevice");
            break;
        case enBTAdpOpCreatePairedDev:
            strcpy(deviceOpString, "CreatePairedDevice");
            break;
        case enBTAdpOpRemovePairedDev:
            strcpy(deviceOpString, "RemoveDevice");
            break;
        case enBTAdpOpUnknown:
        default:
            rc = -1;
            break;
    }

    if (rc == -1)
        return rc;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Adapter",
                                             deviceOpString);

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    if (aenBTAdpOp == enBTAdpOpFindPairedDev) {
        dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &apcDevPath, DBUS_TYPE_INVALID);
    }
    else if (aenBTAdpOp == enBTAdpOpRemovePairedDev) {
        dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apcDevPath, DBUS_TYPE_INVALID);
    }
    else if (aenBTAdpOp == enBTAdpOpCreatePairedDev) {
        dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &apcDevPath,
                              DBUS_TYPE_OBJECT_PATH, &apBtAgentPath,
                              DBUS_TYPE_STRING, &capabilities,
                              DBUS_TYPE_INVALID);
    }

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Pairing failed...\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

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
    char         larDBusIfce[64] = {'\0'};

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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTDisconnectDevice (
    void*           apBtConn,
    const char*     apDevPath,
    enBTDeviceType  aenBTDevType
) {
    dbus_bool_t  lDBusOp;
    DBusMessage* lpDBusMsg;
    char         larDBusIfce[64] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apDevPath)
        return -1;

    switch (aenBTDevType) {
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTRegisterMedia (
    void*           apBtConn,
    const char*     apBtAdapter,
    enBTDeviceType  aenBTDevType,
    void*           apBtUUID,
    void*           apBtMediaCodec,
    void*           apBtMediaCapabilities,
    int             apBtMediaCapabilitiesSize,
    int             abBtMediaDelayReportEnable
) {
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterArr;
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;
    dbus_bool_t     lBtMediaDelayReport = FALSE;

    const char*     lpBtMediaType;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    switch (aenBTDevType) {
    case enBTDevAudioSink:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT;
        dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
        break;
    case enBTDevAudioSource:
        lpBtMediaType = BT_MEDIA_A2DP_SINK_ENDPOINT;
        dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSource'", NULL);
        break;
    case enBTDevHFPHeadset:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT; //TODO: Check if this is correct
        dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.Headset'", NULL);
        break;
    case enBTDevHFPHeadsetGateway:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT; //TODO: Check if this is correct
        dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.HeadsetGateway'", NULL);
        break;
    case enBTDevUnknown:
    default:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT;
        dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
        break;
    }

    if (abBtMediaDelayReportEnable)
        lBtMediaDelayReport = TRUE;

    lDBusOp = dbus_connection_register_object_path(gpDBusConn, lpBtMediaType, &gDBusMediaEndpointVTable, NULL);
    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't Register Media Object\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media",
                                             "RegisterEndpoint");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_OBJECT_PATH, &lpBtMediaType);
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
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
        char*   key = "DelayReporting";
        int     type = DBUS_TYPE_BOOLEAN;

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &lBtMediaDelayReport);
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
        BTRCORELOG_ERROR ("%d\t:%s - Reply Null\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    return 0;
}


int
BtrCore_BTUnRegisterMedia (
    void*           apBtConn,
    const char*     apBtAdapter,
    enBTDeviceType  aenBTDevType
) {
    DBusMessage*    lpDBusMsg;
    dbus_bool_t     lDBusOp;

    const char*      lpBtMediaType;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    switch (aenBTDevType) {
    case enBTDevAudioSink:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT;
        break;
    case enBTDevAudioSource:
        lpBtMediaType = BT_MEDIA_A2DP_SINK_ENDPOINT;
        break;
    case enBTDevHFPHeadset:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT; //TODO: Check if this is correct
        break;
    case enBTDevHFPHeadsetGateway:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT; //TODO: Check if this is correct
        break;
    case enBTDevUnknown:
    default:
        lpBtMediaType = BT_MEDIA_A2DP_SOURCE_ENDPOINT;
        break;
    }


    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media",
                                             "UnregisterEndpoint");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &lpBtMediaType, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_connection_unregister_object_path(gpDBusConn, lpBtMediaType);
    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't Register Media Object\n", __LINE__, __FUNCTION__);
        return -1;
    }

    switch (aenBTDevType) {
    case enBTDevAudioSink:
        dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
        break;
    case enBTDevAudioSource:
        dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSource'", NULL);
        break;
    case enBTDevHFPHeadset:
        dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Headset'", NULL);
        break;
    case enBTDevHFPHeadsetGateway:
        dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.HeadsetGateway'", NULL);
        break;
    case enBTDevUnknown:
    default:
        dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL);
        break;
    }

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

    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.MediaTransport',member='PropertyChanged'", NULL);

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevTransportPath,
                                             "org.bluez.MediaTransport",
                                             "Acquire");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_STRING, &access_type);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Reply Null\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    lDBusOp = dbus_message_get_args(lpDBusReply, &lDBusErr,
                                    DBUS_TYPE_UNIX_FD, dataPathFd,
                                    DBUS_TYPE_UINT16,  dataReadMTU,
                                    DBUS_TYPE_UINT16,  dataWriteMTU,
                                    DBUS_TYPE_INVALID);
    dbus_message_unref(lpDBusReply);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Can't get lpDBusReply arguments\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
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
        BTRCORELOG_ERROR ("%d\t:%s - Can't allocate new method call\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_STRING, &access_type);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("%d\t:%s - Reply Null\n", __LINE__, __FUNCTION__);
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.MediaTransport',member='PropertyChanged'", NULL);

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
BtrCore_BTRegisterConnIntimationcB (
    void*                       apBtConn,
    fPtr_BtrCore_BTConnIntim_cB afpcBConnIntim,
    void*                       apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!afpcBConnIntim)
        return -1;

    gfpcBConnectionIntimation = afpcBConnIntim;
    gpcBConnIntimUserData = apUserData;

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
    fPtr_BtrCore_BTNegotiateMedia_cB    afpcBNegotiateMedia,
    void*                               apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter || !afpcBNegotiateMedia)
        return -1;

    gfpcBNegotiateMedia  = afpcBNegotiateMedia;
    gpcBNegMediaUserData = apUserData;

    return 0;
}


int
BtrCore_BTRegisterTransportPathMediacB (
    void*                                   apBtConn,
    const char*                             apBtAdapter,
    fPtr_BtrCore_BTTransportPathMedia_cB    afpcBTransportPathMedia,
    void*                                   apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!apBtAdapter || !afpcBTransportPathMedia)
        return -1;

    gfpcBTransportPathMedia    = afpcBTransportPathMedia;
    gpcBTransPathMediaUserData = apUserData;

    return 0;
}


/* Control Media on Remote BT Device*/
int
BtrCore_BTDevMediaPlayControl (
    void*            apBtConn,
    const char*      apDevPath,
    enBTDeviceType   aenBTDevType,
    enBTMediaControl aenBTMediaOper
) {
    DBusMessage*    lpDBusMsg;
    dbus_bool_t     lDBusOp;
    char            mediaOper[16] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    switch (aenBTMediaOper) {
    case enBTMediaPlay:
        strcpy(mediaOper, "Play");
        break;
    case enBTMediaPause:
        strcpy(mediaOper, "Pause");
        break;
    case enBTMediaStop:
        strcpy(mediaOper, "Stop");
        break;
    case enBTMediaNext:
        strcpy(mediaOper, "Next");
        break;
    case enBTMediaPrevious:
        strcpy(mediaOper, "Previous");
        break;
    case enBTMediaFastForward:
        strcpy(mediaOper, "FastForward");
        break;
    case enBTMediaRewind:
        strcpy(mediaOper, "Rewind");
        break;
    case enBTMediaVolumeUp:
        strcpy(mediaOper, "VolumeUp");
        break;
    case enBTMediaVolumeDown:
        strcpy(mediaOper, "VolumeDown");
        break;
    }


    lpDBusMsg = dbus_message_new_method_call("org.bluez", apDevPath, "org.bluez.Control", mediaOper);

    if (lpDBusMsg == NULL) {
        BTRCORELOG_ERROR ("%d\t:%s - Cannot allocate Dbus message to play media file\n\n", __LINE__, __FUNCTION__);
    }

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("%d\t:%s - Not enough memory for message send\n", __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(gpDBusConn);

    return 0;
}
