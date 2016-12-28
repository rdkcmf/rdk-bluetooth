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
 * btrCore_dbus_bluez5.c
 * Implementation of DBus layer abstraction for BT functionality (BlueZ 5.37)
 */

/* System Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Local Headers */
#include "btrCore_bt_ifce.h"

#define BD_NAME_LEN                     248

#define BT_MEDIA_A2DP_SINK_ENDPOINT     "/MediaEndpoint/A2DPSink"
#define BT_MEDIA_A2DP_SOURCE_ENDPOINT   "/MediaEndpoint/A2DPSource"

/* Static Function Prototypes */
static int btrCore_BTHandleDusError (DBusError* aDBusErr, const char* aErrfunc, int aErrline);

static DBusHandlerResult btrCore_BTDBusAgentFilter_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);
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
static int btrCore_BTGetScannedDeviceInfo (stBTDeviceInfo* apstBTScannedDeviceInfo);
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

char*         playerObjectPath = NULL;

/* Callbacks */
static fPtr_BtrCore_BTDevStatusUpdate_cB    gfpcBDevStatusUpdate = NULL;
static fPtr_BtrCore_BTNegotiateMedia_cB     gfpcBNegotiateMedia = NULL;
static fPtr_BtrCore_BTTransportPathMedia_cB gfpcBTransportPathMedia = NULL;
static fPtr_BtrCore_BTConnIntim_cB          gfpcBConnectionIntimation = NULL;
static fPtr_BtrCore_BTConnAuth_cB           gfpcBConnectionAuthentication = NULL;


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
    int             i32OpRet = -1;
    stBTDeviceInfo  lstBTDeviceInfo;

    memset (&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
    lstBTDeviceInfo.i32RSSI = INT_MIN;

    printf("agent filter activated....\n");

    if (dbus_message_is_signal(apDBusMsg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
        printf("Property Changed!\n");
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter1", "DeviceCreated")) {
        printf("Device Created!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStPaired, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.freedesktop.DBus.ObjectManager", "InterfacesAdded")) {
        printf("Device Found!\n");

        //i32OpRet = btrCore_BTParseDevice(apDBusMsg, &lstBTDeviceInfo);  //Removed and Modified with new function btrCore_BTGetScannedDeviceInfo compatible with BlueZ5
        i32OpRet = btrCore_BTGetScannedDeviceInfo(&lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate && !i32OpRet) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStFound, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter1","DeviceDisappeared")) {
        printf("Device DeviceDisappeared!\n");
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevUnknown, enBTDevStLost, NULL, NULL)) {
            }
        }
    }

    if (dbus_message_is_signal(apDBusMsg, "org.bluez.Adapter1","DeviceRemoved")) {
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
        btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevAudioSink, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
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
        btrCore_BTParsePropertyChange(apDBusMsg, &lstBTDeviceInfo);
        if (gfpcBDevStatusUpdate) {
            if(gfpcBDevStatusUpdate(enBTDevHFPHeadset, enBTDevStPropChanged, &lstBTDeviceInfo, gpcBDevStatusUserData)) {
            }
        }
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


    fprintf(stderr, "endpoint_handler: MediaEndpoint1\n");

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint1", "SelectConfiguration")) {
        fprintf(stderr, "endpoint_handler: MediaEndpoint1-SelectConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSelectConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint1", "SetConfiguration"))  {
        fprintf(stderr, "endpoint_handler: MediaEndpoint1-SetConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSetConfiguration(apDBusMsg);
    }
    else if (dbus_message_is_method_call(apDBusMsg, "org.bluez.MediaEndpoint1", "ClearConfiguration")) {
        fprintf(stderr, "endpoint_handler: MediaEndpoint1-ClearConfiguration\n");
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

    fprintf(stderr, "btrCore_BTAgentMessageHandler_cb\n");

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "Release"))
        return btrCore_BTAgentRelease (apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "RequestPinCode"))
        return btrCore_BTAgentRequestPincode(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "RequestPasskey"))
        return btrCore_BTAgentRequestPasskey(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "RequestConfirmation"))
        return btrCore_BTAgentRequestConfirmation(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "AuthorizeService"))
        return btrCore_BTAgentAuthorize(apDBusConn, apDBusMsg, userdata);

    if (dbus_message_is_method_call(apDBusMsg, "org.bluez.Agent1", "Cancel"))
        return btrCore_BTAgentCancelMessage(apDBusConn, apDBusMsg, userdata);

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static char*
btrCore_BTGetDefaultAdapterPath (
    void
) {
    int a = 0;
    int b = 0;
    DBusMessage* lpDBusReply;
    DBusMessageIter rootIter;
    bool adapterFound = FALSE;
    char* adapter_path;
    char  objectPath[256] = {'\0'};
    char  objectData[256] = {'\0'};


    lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

    if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
    {

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



        while(!adapterFound){
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
            {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                {

                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strcpy(objectPath, adapter_path);
                    ++a;
                }
                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)){
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                        {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                ////// getting default adapter path //////

                                if (strcmp(dbusObject, "org.bluez.Adapter1") == 0)
                                {
                                    gpcBTDAdapterPath = strdup(adapter_path);
                                    adapterFound = TRUE;
                                    break;
                                }


                            }


                            /////// NEW //////////


                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)){
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                    {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                        {
                                            char *dbusObject2;
                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }


                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            strcpy(objectData, dbusObject3);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                        }

                                    }
                                    if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                    else {dbus_message_iter_next(&innerArrayIter2);}

                                }

                            }

                            ////////// NEW ////////////
                        }
                        if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                        else {dbus_message_iter_next(&innerArrayIter);}
                    }
                }

                if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                else dbus_message_iter_next(&arrayElementIter);

            }//while loop end --used to traverse arra


        }

    }
    printf("\n\nDefault Adpater Path is: %s\n", gpcBTDAdapterPath);
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
        fprintf(stderr, "Invalid arguments for Release method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Unable to create lpDBusReply message\n");
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
        fprintf(stderr, "Invalid arguments for RequestPinCode method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (do_reject) {
        lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
        goto sendmsg;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        fprintf(stderr, "Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    printf("Pincode request for device %s\n", lpcPath);
    dbus_message_append_args(lpDBusReply, DBUS_TYPE_STRING, &gpcBTOutPassCode, DBUS_TYPE_INVALID);

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

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_INVALID))  {
        fprintf(stderr, "Incorrect args btrCore_BTAgentRequestPasskey");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        fprintf(stderr, "Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    printf("Pass code request for device %s\n", lpcPath);
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
    DBusMessage *lpDBusReply;
    const char *lpcPath;
    unsigned int ui32PassCode = 0;;

    const char *dev_name; //pass the dev name to the callback for app to use
    int yesNo;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_UINT32, &ui32PassCode, DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Invalid arguments for Authorize method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    printf("btrCore_BTAgentRequestConfirmation: PASS Code for %s is %6d\n", lpcPath, ui32PassCode);

    if (gfpcBConnectionIntimation) {
        printf("calling ConnIntimation cb with %s...\n",lpcPath);
        dev_name = "Bluetooth Device";//TODO connect device name with btrCore_GetKnownDeviceName

        if (dev_name != NULL) {
            yesNo = gfpcBConnectionIntimation(dev_name, ui32PassCode, gpcBConnIntimUserData);
        }
        else {
            //couldnt get the name, provide the bt address instead
            yesNo = gfpcBConnectionIntimation(lpcPath, ui32PassCode, gpcBConnIntimUserData);
        }

        if (yesNo == 0) {
            //printf("sorry dude, you cant connect....\n");
            lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
            goto sendReqConfError;
        }
    }

    gpcBConnAuthPassKey = ui32PassCode;

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        fprintf(stderr, "Can't create lpDBusReply message\n");
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
        fprintf(stderr, "Invalid arguments for Authorize method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (gfpcBConnectionAuthentication) {
        printf("calling ConnAuth cb with %s...\n",lpcPath);
        dev_name = "Bluetooth Device";//TODO connect device name with btrCore_GetKnownDeviceName

        if (dev_name != NULL) {
            yesNo = gfpcBConnectionAuthentication(dev_name, gpcBConnAuthUserData);
        }
        else {
            //couldnt get the name, provide the bt address instead
            yesNo = gfpcBConnectionAuthentication(lpcPath, gpcBConnAuthUserData);
        }

        if (yesNo == 0) {
            //printf("sorry dude, you cant connect....\n");
            lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
            goto sendAuthError;
        }
    }

    gpcBConnAuthPassKey = 0;

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        fprintf(stderr, "Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    printf("Authorizing request for %s\n", lpcPath);

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
        fprintf(stderr, "Invalid arguments for confirmation method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    printf("Request canceled\n");
    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Can't create lpDBusReply message\n");
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
        printf("Cannot allocate DBus message!\n");
        return NULL;
    }

    //Now do a sync call
    if (!dbus_connection_send_with_reply(gpDBusConn, methodcall, &pending, -1)) { //Send and expect lpDBusReply using pending call object
        printf("failed to send message!\n");
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(methodcall);
    methodcall = NULL;

    dbus_pending_call_block(pending);               //Now block on the pending call
    lpDBusReply = dbus_pending_call_steal_reply(pending); //Get the lpDBusReply message from the queue
    dbus_pending_call_unref(pending);               //Free pending call handle

    if (dbus_message_get_type(lpDBusReply) ==  DBUS_MESSAGE_TYPE_ERROR) {
        printf("Error : %s\n\n", dbus_message_get_error_name(lpDBusReply));
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
#if 0
    const char*     pcBTDevAddr = NULL;
#endif
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

    /*if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &pcBTDevAddr,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "%s:%d:%s - dbus_message_get_args Failed\n", __FILE__, __LINE__, __FUNCTION__);
        //return -1; Users of btrCore_BTParseDevice should not call it if the message contains no valid args
    }*/

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
                            printf ("UUID value is %s\n", pVal);
                            strncpy(apstBTDeviceInfo->aUUIDs[count], pVal, (BT_MAX_UUID_STR_LEN - 1));
                            count++;
                        }
                        dbus_message_iter_next (&variant_j);
                    }
                }
                else
                {
                    printf("apstBTDeviceInfo->Services; Not an Array\n");
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

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
       printf("GetProperties lpDBusReply has no arguments.");
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
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to select configuration");
    }

    if (gfpcBNegotiateMedia) {
        if(!(lpOutputMediaCaps = gfpcBNegotiateMedia(lpInputMediaCaps, gpcBNegMediaUserData))) {
            return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to select configuration");
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
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

    dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterProp);
    if (dbus_message_iter_get_arg_type(&lDBusMsgIterProp) != DBUS_TYPE_DICT_ENTRY)
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

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
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &uuid);
        }
        else if (strcasecmp(key, "Device") == 0) {
            if (ldBusType != DBUS_TYPE_OBJECT_PATH)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &dev_path);
        }
        else if (strcasecmp(key, "Configuration") == 0) {
            if (ldBusType != DBUS_TYPE_ARRAY)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

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
        if((lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, config, gpcBTransPathMediaUserData))) {
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
        if(!(lStoredDevTransportPath = gfpcBTransportPathMedia(lDevTransportPath, NULL, gpcBTransPathMediaUserData))) {
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

    dbus_bus_add_match(gpDBusConn, "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged'"",arg0='" "org.bluez" "'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',sender='" "org.bluez" "',interface='org.freedesktop.DBus.ObjectManager',member='InterfacesAdded'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',sender='" "org.bluez" "',interface='org.freedesktop.DBus.ObjectManager',""member='InterfacesRemoved'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',sender='" "org.bluez" "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'"",arg0='" "org.bluez.Adapter1" "'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',sender='" "org.bluez" "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'"",arg0='" "org.bluez.Device1" "'", NULL);
    dbus_bus_add_match(gpDBusConn, "type='signal',sender='" "org.bluez" "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'"",arg0='" "org.bluez.MediaTransport1" "'", NULL);

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

    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter1'", NULL);

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
    printf("\n\nAgent Path: %s", gpcBTAgentPath);
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
    DBusError lDBusErr;

    if (!dbus_connection_register_object_path(gpDBusConn, apBtAgentPath, &gDBusAgentVTable, NULL))  {
        fprintf(stderr, "Error registering object path for agent\n");
        return -1;
    }

    apDBusMsg = dbus_message_new_method_call("org.bluez", "/org/bluez","org.bluez.AgentManager1", "RegisterAgent");
    if (!apDBusMsg) {
        fprintf(stderr, "Error allocating new method call\n");
        return -1;
    }

    dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_STRING, &capabilities, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);

    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &lDBusErr);

    dbus_message_unref(apDBusMsg);
    if (!lpDBusReply) {
        fprintf(stderr, "Unable to register agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    apDBusMsg = dbus_message_new_method_call("org.bluez", "/org/bluez", "org.bluez.AgentManager1", "RequestDefaultAgent");
    if (!apDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);

    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &lDBusErr);

    dbus_message_unref(apDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Can't unregister agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;//this was an error case
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
    DBusError lDBusErr;

    apDBusMsg = dbus_message_new_method_call("org.bluez", "/org/bluez", "org.bluez.AgentManager1", "UnregisterAgent");
    if (!apDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(apDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);

    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, apDBusMsg, -1, &lDBusErr);

    dbus_message_unref(apDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "Can't unregister agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;//this was an error case
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    if (!dbus_connection_unregister_object_path(gpDBusConn, apBtAgentPath)) {
        fprintf(stderr, "Error unregistering object path for agent\n");
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
    DBusError   lDBusErr;
    int         c;
    int         rc = -1;
    int         a = 0;
    int         b = 0;
    int         d = 0;
    int         num = -1;
    char        paths[10][248];
    //char      **paths2 = NULL;

    DBusMessage*     lpDBusReply;
    DBusMessageIter rootIter;
    bool             adapterFound = FALSE;
    char*             adapter_path;
    char*             dbusObject2;
    char              objectPath[256] = {'\0'};
    char              objectData[256] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    dbus_error_init(&lDBusErr);
    lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!lpDBusReply) {
        printf("%s:%d - org.bluez.Manager.ListAdapters returned an error: '%s'\n", __FUNCTION__, __LINE__, lDBusErr.message);
        dbus_error_free(&lDBusErr);
    }

    if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
    {

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



        while(!adapterFound){
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
            {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                {

                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strcpy(objectPath, adapter_path);
                    ++a;
                }
                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)){
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                        {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                ////// getting all bluetooth adapters object paths //////

                                if (strcmp(dbusObject, "org.bluez.Adapter1") == 0 || strcmp(dbusObject, "org.bluez.Device1") == 0)
                                {
                                    strcpy(paths[d], adapter_path);
                                    //strcpy(paths2+d,adapter_path);
                                    //paths2[d] = strdup(adapter_path);
                                    //printf("\n\n test");
                                    //(paths2+2) = strdup(adapter_path);
                                    ++d;
                                }


                            }


                            /////// NEW //////////


                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)){
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                    {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                        {

                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }


                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            strcpy(objectData, dbusObject3);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                        }

                                    }
                                    if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                    else {dbus_message_iter_next(&innerArrayIter2);}

                                }

                            }

                            ////////// NEW ////////////
                        }
                        if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                        else {dbus_message_iter_next(&innerArrayIter);}
                    }
                }

                if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                else dbus_message_iter_next(&arrayElementIter);

            }//while loop end --used to traverse array

        }

    }
    num = d;
    if (apBtNumAdapters && apcArrBtAdapterPath) {
        *apBtNumAdapters = num;

        for (c = 0; c < num; c++) {
            if (*(apcArrBtAdapterPath + c)) {
                    printf("Adapter Path %d is: %s\n", c, paths[c]);
                    //strncpy(*(apcArrBtAdapterPath + c), paths[c], BD_NAME_LEN);
                    strncpy(apcArrBtAdapterPath[c], paths[c], BD_NAME_LEN);
                    rc = 0;
                }
        }
    }

    dbus_message_unref(lpDBusReply);

    return rc;
}


char*
BtrCore_BTGetAdapterPath (
    void*       apBtConn,
    const char* apBtAdapter
) {
    char* defaultAdapter1 = "/org/bluez/hci0";
    char* defaultAdapter2 = "/org/bluez/hci1";
    char* defaultAdapter3 = "/org/bluez/hci2";
    char* bt1 = "hci0";
    char* bt2 = "hci1";
    char* bt3 = "hci2";

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return NULL;

    if (!apBtAdapter)
        return btrCore_BTGetDefaultAdapterPath();


    if (gpcBTAdapterPath) {
        free(gpcBTAdapterPath);
        gpcBTAdapterPath = NULL;
    }

    if (strcmp(apBtAdapter, bt1) == 0)
    {
        gpcBTAdapterPath = strdup(defaultAdapter1);
    }

    if (strcmp(apBtAdapter, bt2) == 0)
    {
        gpcBTAdapterPath = strdup(defaultAdapter2);
    }

    if (strcmp(apBtAdapter, bt3) == 0)
    {
        gpcBTAdapterPath = strdup(defaultAdapter3);
    }


    //printf("\n\nPath is %s: ", gpcBTAdapterPath);
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
    const char*     apcPath,
    enBTOpType      aenBTOpType,
    const char*     pKey,
    void*           pValue
) {
    int             rc = 0;
    int             type;
    DBusMessage*    msg = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter args;
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    DBusError       lDBusErr;
    DBusPendingCall* pending;
    const char*     pParsedKey = NULL;
    const char*     pParsedValueString = NULL;
    int             parsedValueNumber = 0;
    unsigned int    parsedValueUnsignedNumber = 0;
    unsigned short  parsedValueUnsignedShort = 0;

    const char*     pInterface          = NULL;
    const char*     pAdapterInterface   = "org.bluez.Adapter1";
    const char*     pDeviceInterface    = "org.bluez.Device1";
    const char*     pMediaTransInterface= "org.bluez.MediaTransport1";

    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if ((!apcPath) || (!pKey) || (!pValue)) {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
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

    msg = dbus_message_new_method_call("org.bluez", apcPath, "org.freedesktop.DBus.Properties", "GetAll");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &pInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
    }


    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);


    if (!lpDBusReply) {
        printf("%s:%d - %s.GetProperties returned an error: '%s'\n", __FUNCTION__, __LINE__, pInterface, lDBusErr.message);
        rc = -1;
        dbus_error_free(&lDBusErr);
    }
    else {
        if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
            printf("GetProperties lpDBusReply has no arguments.");
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

        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);

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

    const char* defaultAdapterInterface = "org.bluez.Adapter1";

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apvVal)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                            "org.freedesktop.DBus.Properties",
                                            "Set");

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
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &defaultAdapterInterface);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &lDBusKey);
    dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
    dbus_message_iter_append_basic(&lDBusMsgIterValue, lDBusType, apvVal);
    dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);
    //dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &defaultAdapterInterface, DBUS_TYPE_STRING, &lDBusKey, lDBusType, apvVal, DBUS_TYPE_INVALID);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "lpDBusReply Null\n");
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
                                             "org.bluez.Adapter1",
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
                                             "org.bluez.Adapter1",
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


static int
btrCore_BTGetScannedDeviceInfo (
    stBTDeviceInfo* apstBTScannedDeviceInfo
) {
    DBusMessage*        msg;
    DBusMessageIter     rootIter;
    DBusMessageIter     args;
    DBusError           lDBusErr;
    DBusPendingCall*    pending;
    bool    adapterFound = FALSE;


    char*   pdeviceInterface = "org.bluez.Device1";
    char*   adapter_path;
    char*   dbusObject2;
    char    paths[32][256];
    char    objectPath[256] = {'\0'};
    char    objectData[256] = {'\0'};
    int     i = 0;
    int     num = 0;
    int     a = 0;
    int     b = 0;
    int     d = 0;


    if (!gpDBusConn)
        return -1;

    dbus_error_init(&lDBusErr);
    DBusMessage* lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!lpDBusReply) {
        printf("%s:%d - org.bluez.Manager.ListAdapters returned an error: '%s'\n", __FUNCTION__, __LINE__, lDBusErr.message);
        dbus_error_free(&lDBusErr);
    }

    if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
    {

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



        while(!adapterFound){
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
            {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                {

                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strcpy(objectPath, adapter_path);
                    ++a;
                }
                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)){
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                        {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);


                            }


                            /////// NEW //////////


                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)){
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                    {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                        {

                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }


                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            strcpy(objectData, dbusObject3);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            if (strcmp(dbusObject2, "Paired") == 0 && !device_prop)
                                            {
                                                strcpy(paths[d], adapter_path);
                                                ++d;
                                            }

                                        }

                                    }
                                    if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                    else {dbus_message_iter_next(&innerArrayIter2);}

                                }

                            }

                            ////////// NEW ////////////
                        }
                        if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                        else {dbus_message_iter_next(&innerArrayIter);}
                    }
                }

                if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                else dbus_message_iter_next(&arrayElementIter);

            }//while loop end --used to traverse array

        }

    }
    num = d;
    dbus_message_unref(lpDBusReply);

    for ( i = 0; i < num; i++) {
        msg = dbus_message_new_method_call("org.bluez", paths[i], "org.freedesktop.DBus.Properties", "GetAll");
        dbus_message_iter_init_append(msg, &args);
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &pdeviceInterface, DBUS_TYPE_INVALID);

        dbus_error_init(&lDBusErr);
        if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
        {
        printf("failed to send message");
        return -1;
        }

        dbus_connection_flush(gpDBusConn);
        dbus_message_unref(msg);
        msg = NULL;

        dbus_pending_call_block(pending);
        lpDBusReply =  dbus_pending_call_steal_reply(pending);
        dbus_pending_call_unref(pending);

        if (lpDBusReply != NULL) {
            if (0 != btrCore_BTParseDevice(lpDBusReply, apstBTScannedDeviceInfo)) {
                printf ("Parsing the device %s failed..\n", paths[i]);
                dbus_message_unref(lpDBusReply);
                return -1;
            }
            else {
                dbus_message_unref(lpDBusReply);
                return 0;
            }
        }
        dbus_message_unref(lpDBusReply);
    }

    return 0;
}


int
BtrCore_BTGetPairedDeviceInfo (
    void*                   apBtConn,
    const char*             apBtAdapter,
    stBTPairedDeviceInfo*   pPairedDeviceInfo
) {
    DBusMessage*        msg;
    DBusMessageIter     rootIter;
    DBusMessageIter     args;
    DBusError           lDBusErr;
    DBusPendingCall*    pending;
    bool                adapterFound = FALSE;


    char*         pdeviceInterface = "org.bluez.Device1";
    char*         adapter_path;
    char*        dbusObject2;
    char        paths[32][256];
    char          objectPath[256] = {'\0'};
    char          objectData[256] = {'\0'};
    int         i = 0;
    int         num = 0;
    int         a = 0;
    int         b = 0;
    int         d = 0;
    //char**      paths = NULL;
    stBTDeviceInfo apstBTDeviceInfo;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !pPairedDeviceInfo)
        return -1;

    memset (pPairedDeviceInfo, 0, sizeof (stBTPairedDeviceInfo));

    dbus_error_init(&lDBusErr);
    DBusMessage* lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!lpDBusReply) {
        printf("%s:%d - org.bluez.Manager.ListAdapters returned an error: '%s'\n", __FUNCTION__, __LINE__, lDBusErr.message);
        dbus_error_free(&lDBusErr);
    }

    if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
    {

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



        while(!adapterFound){
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
            {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                {

                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strcpy(objectPath, adapter_path);
                    ++a;
                }
                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)){
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                        {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);


                            }


                            /////// NEW //////////


                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                            {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)){
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                    {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                        {

                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }


                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            strcpy(objectData, dbusObject3);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                        {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            if (strcmp(dbusObject2, "Paired") == 0 && device_prop)
                                            {
                                                strcpy(paths[d], adapter_path);
                                                ++d;
                                            }

                                        }

                                    }
                                    if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                    else {dbus_message_iter_next(&innerArrayIter2);}

                                }

                            }

                            ////////// NEW ////////////
                        }
                        if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                        else {dbus_message_iter_next(&innerArrayIter);}
                    }
                }

                if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                else dbus_message_iter_next(&arrayElementIter);

            }//while loop end --used to traverse array

        }

    }
    num = d;

    /* Update the number of devices */
    pPairedDeviceInfo->numberOfDevices = num;

    /* Update the paths of these devices */
    for ( i = 0; i < num; i++) {
        memset(pPairedDeviceInfo->devicePath[i], '\0', sizeof(pPairedDeviceInfo->devicePath[i]));
        strcpy(pPairedDeviceInfo->devicePath[i], paths[i]);
    }
    dbus_message_unref(lpDBusReply);


    for ( i = 0; i < num; i++) {
        msg = dbus_message_new_method_call("org.bluez", pPairedDeviceInfo->devicePath[i], "org.freedesktop.DBus.Properties", "GetAll");
        dbus_message_iter_init_append(msg, &args);
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &pdeviceInterface, DBUS_TYPE_INVALID);

        dbus_error_init(&lDBusErr);
        if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
        {
        printf("failed to send message");
        return -1;
        }

        dbus_connection_flush(gpDBusConn);
        dbus_message_unref(msg);
        msg = NULL;

        dbus_pending_call_block(pending);
        lpDBusReply =  dbus_pending_call_steal_reply(pending);
        dbus_pending_call_unref(pending);

        if (lpDBusReply != NULL) {
            memset (&apstBTDeviceInfo, 0, sizeof(apstBTDeviceInfo));
            if (0 != btrCore_BTParseDevice(lpDBusReply, &apstBTDeviceInfo)) {
                printf ("Parsing the device %s failed..\n", pPairedDeviceInfo->devicePath[i]);
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
BtrCore_BTDiscoverDeviceServices (
    void*                           apBtConn,
    const char*                     apcDevPath,
    stBTDeviceSupportedServiceList* pProfileList
) {
    const char*     apcSearchString;
    DBusMessage*    msg;
    DBusMessage*    lpDBusReply;
    DBusError         lDBusErr;
    DBusMessageIter args;
    DBusMessageIter MsgIter;
    DBusPendingCall* pending;
    int             match = 0;


    msg = dbus_message_new_method_call("org.bluez", apcDevPath, "org.freedesktop.DBus.Properties", "Get");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, "org.bluez.Device1", DBUS_TYPE_STRING, "UUIDs", DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);



    dbus_message_iter_init(lpDBusReply, &MsgIter);//msg is pointer to dbus message received
    //dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge received
    /*if (!dbus_message_iter_init(lpDBusReply, &MsgIter))
    {
    fprintf(stderr, "Message has no arguments!\n");
    }*/

    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&MsgIter))
    {
        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&MsgIter, &arrayElementIter); //assign new iterator to first element of array
        while (dbus_message_iter_has_next(&arrayElementIter))
        {
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&arrayElementIter))
            {
                char *dbusObject2;
                dbus_message_iter_get_basic(&arrayElementIter, &dbusObject2);
                if (strcmp(apcSearchString, dbusObject2) == 0)
                {
                    match = 1;
                }

                else
                {
                    match = 0;
                }
            }
            if(!dbus_message_iter_has_next(&arrayElementIter)) {break;} //check to see if end of 3rd array
            else {dbus_message_iter_next(&arrayElementIter);}
        }

    }

    return match;
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
    DBusError lDBusErr;
    int match;
    const char* value;
    char* ret;

   //BTRCore_LOG("apcDevPath is %s\n and service UUID is %s", apcDevPath, apcSearchString);
    msg = dbus_message_new_method_call( "org.bluez",
                                        apcDevPath,
                                        "org.bluez.Device1",
                                        "DiscoverServices");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    match = 0; //assume it does not match
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &apcSearchString, DBUS_TYPE_INVALID);
    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(apBtConn, msg, -1, &lDBusErr);

    dbus_message_unref(msg);

    if (!lpDBusReply) {
        fprintf(stderr, "Failure attempting to Discover Services\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
       printf("DiscoverServices lpDBusReply has no information.");
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
BtrCore_BTPerformAdapterOp (
    void*           apBtConn,
    const char*     apBtAdapter,
    const char*     apBtAgentPath,
    const char*     apcDevPath,
    enBTAdapterOp   aenBTAdpOp
) {
    DBusMessage*     msg;
    DBusMessage*     lpDBusReply;
    DBusMessageIter rootIter;
    DBusError         lDBusErr;
    bool             adapterFound = FALSE;

    char*             adapter_path = NULL;
    char            deviceObjectPath[256] = {'\0'};
    char             deviceOpString[64] = {'\0'};
    char              objectPath[256] = {'\0'};
    char              objectData[256] = {'\0'};
    int             rc = 0;
    int             a = 0;
    int             b = 0;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apBtAdapter || !apBtAgentPath || !apcDevPath || (aenBTAdpOp == enBTAdpOpUnknown))
        return -1;


    switch (aenBTAdpOp) {
        case enBTAdpOpFindPairedDev:
        strcpy(deviceOpString, "FindDevice");
        break;
        case enBTAdpOpCreatePairedDev:
        strcpy(deviceOpString, "Pair");
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


    if (aenBTAdpOp == enBTAdpOpFindPairedDev) {
        lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

        if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
            DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
        {

            DBusMessageIter arrayElementIter;
            dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



            while(!adapterFound){
                if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
                {

                    DBusMessageIter dictEntryIter;
                    dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                    {

                        dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                        strcpy(objectPath, adapter_path);
                        ++a;
                    }
                    dbus_message_iter_next(&dictEntryIter);
                    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                    {
                        DBusMessageIter innerArrayIter;
                        dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                        while (dbus_message_iter_has_next(&innerArrayIter)){
                            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                            {
                                DBusMessageIter innerDictEntryIter;
                                dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                                if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                                {
                                    char *dbusObject;
                                    dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                    ////// getting default adapter path //////

                                }


                                /////// NEW //////////


                                dbus_message_iter_next(&innerDictEntryIter);
                                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                                {
                                    DBusMessageIter innerArrayIter2;
                                    dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                    while (dbus_message_iter_has_next(&innerArrayIter2)){
                                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                        {
                                            DBusMessageIter innerDictEntryIter2;
                                            dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                            {
                                                char *dbusObject2;
                                                dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                            }


                                            ////////////// NEW 2 ////////////
                                            dbus_message_iter_next(&innerDictEntryIter2);
                                            DBusMessageIter innerDictEntryIter3;
                                            char *dbusObject3;

                                            dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                            {
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                                strcpy(objectData, dbusObject3);
                                                if (strcmp(apcDevPath, objectData) == 0)
                                                {
                                                    ++b;
                                                    adapterFound = TRUE;
                                                    return 0;
                                                }
                                            }
                                            else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                            {
                                                bool *device_prop = FALSE;
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            }

                                        }
                                        if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                        else {dbus_message_iter_next(&innerArrayIter2);}

                                    }

                                }

                                ////////// NEW ////////////
                            }
                            if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                            else {dbus_message_iter_next(&innerArrayIter);}
                        }
                    }

                    if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                    else dbus_message_iter_next(&arrayElementIter);

                }//while loop end --used to traverse arra


            }

        }
        dbus_error_init(&lDBusErr);
        dbus_message_unref(lpDBusReply);
    }

    else if (aenBTAdpOp == enBTAdpOpRemovePairedDev) {
        msg = dbus_message_new_method_call("org.bluez",
        apBtAdapter,
        "org.bluez.Adapter1",
        deviceOpString);
        if (!msg) {
            fprintf(stderr, "Can't allocate new method call\n");
            return -1;
        }
        dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &apcDevPath, DBUS_TYPE_INVALID);
        lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, msg, -1, &lDBusErr);
        dbus_error_init(&lDBusErr);
        dbus_message_unref(msg);
    }



    else if (aenBTAdpOp == enBTAdpOpCreatePairedDev) {
        lpDBusReply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

        if (dbus_message_iter_init(lpDBusReply, &rootIter) && //point iterator to lpDBusReply message
            DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
        {

            DBusMessageIter arrayElementIter;
            dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array



            while(!adapterFound){
                if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
                {

                    DBusMessageIter dictEntryIter;
                    dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
                    {

                        dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                        strcpy(objectPath, adapter_path);
                        ++a;
                    }
                    dbus_message_iter_next(&dictEntryIter);
                    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
                    {
                        DBusMessageIter innerArrayIter;
                        dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                        while (dbus_message_iter_has_next(&innerArrayIter)){
                            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
                            {
                                DBusMessageIter innerDictEntryIter;
                                dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                                if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                                {
                                    char *dbusObject;
                                    dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                    ////// getting default adapter path //////

                                }


                                /////// NEW //////////


                                dbus_message_iter_next(&innerDictEntryIter);
                                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter))
                                {
                                    DBusMessageIter innerArrayIter2;
                                    dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                    while (dbus_message_iter_has_next(&innerArrayIter2)){
                                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2))
                                        {
                                            DBusMessageIter innerDictEntryIter2;
                                            dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
                                            {
                                                char *dbusObject2;
                                                dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                            }


                                            ////////////// NEW 2 ////////////
                                            dbus_message_iter_next(&innerDictEntryIter2);
                                            DBusMessageIter innerDictEntryIter3;
                                            char *dbusObject3;

                                            dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                            {
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                                strcpy(objectData, dbusObject3);
                                                if (strcmp(apcDevPath, objectData) == 0)
                                                {
                                                    ++b;
                                                    strcpy(deviceObjectPath,adapter_path);
                                                    adapterFound = TRUE;
                                                }
                                            }
                                            else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
                                            {
                                                bool *device_prop = FALSE;
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            }

                                        }
                                        if(!dbus_message_iter_has_next(&innerArrayIter2)) {break;} //check to see if end of 3rd array
                                        else {dbus_message_iter_next(&innerArrayIter2);}

                                    }

                                }

                                ////////// NEW ////////////
                            }
                            if(!dbus_message_iter_has_next(&innerArrayIter)) {break;} //check to see if end of 2nd array
                            else {dbus_message_iter_next(&innerArrayIter);}
                        }
                    }

                    if(!dbus_message_iter_has_next(&arrayElementIter)) break; //check to see if end of 1st array
                    else dbus_message_iter_next(&arrayElementIter);

                }//while loop end --used to traverse arra

            }
        }

        dbus_error_init(&lDBusErr);
        dbus_message_unref(lpDBusReply);

        msg = dbus_message_new_method_call("org.bluez",
        deviceObjectPath,
        "org.bluez.Device1",
        deviceOpString);
        if (!msg) {
            fprintf(stderr, "Can't allocate new method call\n");
            return -1;
        }
        lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, msg, -1, &lDBusErr);
        dbus_error_init(&lDBusErr);
        dbus_message_unref(msg);

        if (!lpDBusReply) {
            fprintf(stderr, "Pairing failed...\n");
            btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
            return -1;
        }

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
    //char         larDBusIfce[32] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apDevPath)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apDevPath,
                                             "org.bluez.Device1",
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
    //char         larDBusIfce[32] = {'\0'};

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apDevPath)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                        apDevPath,
                                        "org.bluez.Device1",
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
        fprintf(stderr, "Can't Register Media Object\n");
        return -1;
    }

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media1",
                                             "RegisterEndpoint");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
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
        fprintf(stderr, "lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
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
                                             "org.bluez.Media1",
                                             "UnregisterEndpoint");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &lpBtMediaType, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(gpDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    lDBusOp = dbus_connection_unregister_object_path(gpDBusConn, lpBtMediaType);
    if (!lDBusOp) {
        fprintf(stderr, "Can't Register Media Object\n");
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
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apcDevTransportPath)
        return -1;

    dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.MediaTransport1',member='PropertyChanged'", NULL);

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevTransportPath,
                                             "org.bluez.MediaTransport1",
                                             "Acquire");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "lpDBusReply Null\n");
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
        fprintf(stderr, "Can't get lpDBusReply arguments\n");
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
    DBusError       lDBusErr;


    if (!gpDBusConn || (gpDBusConn != apBtConn) || !apcDevTransportPath)
        return -1;

    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apcDevTransportPath,
                                             "org.bluez.MediaTransport1",
                                             "Release");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.MediaTransport1',member='PropertyChanged'", NULL);

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

    gfpcBNegotiateMedia = afpcBNegotiateMedia;
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

    gfpcBTransportPathMedia = afpcBTransportPathMedia;
    gpcBTransPathMediaUserData = apUserData;

    return 0;
}


/////////////////////////////////////////////////////         AVRCP Functions         ////////////////////////////////////////////////////



/* Get Player Object Path on Remote BT Device*/

char* BtrCore_GetPlayerObjectPath (void* apBtConn, const char* apBtAdapterPath)
{
    DBusMessage*     msg;
    DBusMessage*     lpDBusReply;
    DBusPendingCall* pending;
    DBusError         lDBusErr;
    DBusMessageIter  args;


    if (!gpDBusConn || (gpDBusConn != apBtConn))
    {
        return NULL;
    }

    msg = dbus_message_new_method_call("org.bluez",
    apBtAdapterPath,
    "org.freedesktop.DBus.Properties",
    "Get");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, "org.bluez.MediaControl1", DBUS_TYPE_STRING, "Player", DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!lpDBusReply) {
        printf("%s:%d GetPlayerObject returned an error: '%s'\n", __FUNCTION__, __LINE__, lDBusErr.message);
        dbus_error_free(&lDBusErr);
        return NULL;
    }

    DBusMessageIter MsgIter;
    dbus_message_iter_init(lpDBusReply, &MsgIter);//pointer to dbus message received

    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&MsgIter))
    {

        dbus_message_iter_get_basic(&MsgIter, &playerObjectPath);
        return playerObjectPath;
    }

    else
    {
        return NULL;
    }

    return playerObjectPath;
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


    lpDBusMsg = dbus_message_new_method_call("org.bluez", apDevPath, "org.bluez.MediaControl1", mediaOper);

    if (lpDBusMsg == NULL) {
        printf("Cannot allocate Dbus message to play media file\n\n");
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

/* Get Media Property on Remote BT Device*/

char* BtrCoreGetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty)

{
    DBusMessage*     msg;
    DBusMessage*     lpDBusReply;
    DBusPendingCall* pending;
    DBusError         lDBusErr;
    DBusMessageIter  args;
    DBusMessageIter  element;
    char*              mediaPlayerObjectPath = NULL;
    char*              mediaPropertyValue = NULL;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
    {
        return NULL;
    }

    mediaPlayerObjectPath = BtrCore_GetPlayerObjectPath (apBtConn, apBtAdapterPath);

    if (mediaPlayerObjectPath == NULL)
    {
        return NULL;
    }



    msg = dbus_message_new_method_call("org.bluez",
    mediaPlayerObjectPath,
    "org.freedesktop.DBus.Properties",
    "Get");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, "org.bluez.MediaPlayer1", DBUS_TYPE_STRING, mediaProperty, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
        return NULL;
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    DBusMessageIter MsgIter;
    dbus_message_iter_init(lpDBusReply, &MsgIter);//msg is pointer to dbus message received
    dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge receive
    if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&element))
    {
        dbus_message_iter_get_basic(&element, &mediaPropertyValue);
        return mediaPropertyValue;
    }

    return mediaPropertyValue;
}




/* Set Media Property on Remote BT Device (Equalizer, Repeat, Shuffle, Scan, Status)*/

int BtrCoreSetMediaProperty (void* apBtConn, const char* apBtAdapterPath, char* mediaProperty, char* pValue)
{
    DBusMessage*    lpDBusMsg;
    DBusMessage*    lpDBusReply;
    DBusMessageIter lDBusMsgIter;
    //DBusMessageIter lDBusMsgIter2;
    DBusMessageIter lDBusMsgIterValue;
    DBusError       lDBusErr;
    char*             mediaPlayerObjectPath = NULL;
    const char*     lDBusTypeAsString = DBUS_TYPE_STRING_AS_STRING;

    if (!gpDBusConn || (gpDBusConn != apBtConn) || !pValue)
        return -1;

    mediaPlayerObjectPath = BtrCore_GetPlayerObjectPath (apBtConn, apBtAdapterPath);

    if (mediaPlayerObjectPath == NULL)
    {
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call("org.bluez",
    mediaPlayerObjectPath,
    "org.freedesktop.DBus.Properties",
    "Set");

    if (!lpDBusMsg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, "org.bluez.MediaPlayer1");
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaProperty);
    dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
    dbus_message_iter_append_basic(&lDBusMsgIterValue, DBUS_TYPE_STRING, pValue);
    dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(gpDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        fprintf(stderr, "lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(gpDBusConn);

    return 0;
}



/* Get Track information and place them in an array (Title, Artists, Album, number of tracks, tracknumber, duration)*/

int BtrCoreGetTrackInformation (void* apBtConn, const char* apBtAdapterPath)

{
    DBusMessage*     msg;
    DBusMessage*     lpDBusReply;
    DBusPendingCall* pending;
    DBusError         lDBusErr;
    DBusMessageIter  args;
    DBusMessageIter  element;
    char*              mediaPlayerObjectPath = NULL;
    char*              trackPropertyValue = NULL;
    char*             trackInfo[4];
    unsigned int*     trackData[3];
    unsigned int*    trackUint32;
    int              a = 0;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
    {
        return -1;
    }

    mediaPlayerObjectPath = BtrCore_GetPlayerObjectPath (apBtConn, apBtAdapterPath);

    if (mediaPlayerObjectPath == NULL)
    {
        return -1;
    }



    msg = dbus_message_new_method_call("org.bluez",
    mediaPlayerObjectPath,
    "org.freedesktop.DBus.Properties",
    "Get");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, "org.bluez.MediaPlayer1", DBUS_TYPE_STRING, "Track", DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
        return -1;
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    DBusMessageIter MsgIter;
    dbus_message_iter_init(lpDBusReply, &MsgIter);//msg is pointer to dbus message received
    //dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge receive
    while (dbus_message_iter_has_next(&MsgIter)){

        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&MsgIter))
        {
            dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge receive

            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&element))
            {
                dbus_message_iter_get_basic(&element, &trackPropertyValue);
                trackInfo[a]=strdup(trackPropertyValue);
            }

            if (DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(&element))
            {
                dbus_message_iter_get_basic(&element, &trackUint32);
                trackData[a]=trackUint32;
            }

        }
        if(!dbus_message_iter_has_next(&MsgIter)) break; //check to see if end of dict
        else
        {
            a++;
            dbus_message_iter_next(&MsgIter);
        }

    }

    (void)trackInfo;
    (void)trackData;

    return 0;

}

int BtrCoreCheckPlayerBrowsable(void* apBtConn, const char* apBtAdapterPath)
{
    DBusMessage*     msg;
    DBusMessage*     lpDBusReply;
    DBusPendingCall* pending;
    DBusError        lDBusErr;
    DBusMessageIter  args;
    DBusMessageIter  element;
    char*            mediaPlayerObjectPath = NULL;
    bool*            browsable = FALSE;

    if (!gpDBusConn || (gpDBusConn != apBtConn))
    {
        return -1;
    }

    mediaPlayerObjectPath = BtrCore_GetPlayerObjectPath (apBtConn, apBtAdapterPath);

    if (mediaPlayerObjectPath == NULL)
    {
        return -1;
    }



    msg = dbus_message_new_method_call("org.bluez",
    mediaPlayerObjectPath,
    "org.freedesktop.DBus.Properties",
    "Get");

    dbus_message_iter_init_append(msg, &args);
    dbus_message_append_args(msg, DBUS_TYPE_STRING, "org.bluez.MediaPlayer1", DBUS_TYPE_STRING, "Browsable", DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
    {
        printf("failed to send message");
        return -1;
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    lpDBusReply =  dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    DBusMessageIter MsgIter;
    dbus_message_iter_init(lpDBusReply, &MsgIter);//msg is pointer to dbus message received
    dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge receive
    if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&element))
    {

        dbus_message_iter_get_basic(&element, &browsable);

    }

    if (browsable)
    {
        return 0;
    }

    else
    {
        return -1;
    }
}
