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
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Interface lib Headers */
#include "btrCore_logger.h"

/* Local Headers */
#include "btrCore_bt_ifce.h"

#ifdef LIBSYSWRAPPER_BUILD
#include "secure_wrapper.h"
#endif

#define DBUS_INTERFACE_OBJECT_MANAGER       "org.freedesktop.DBus.ObjectManager"

#define BT_DBUS_BLUEZ_PATH                  "org.bluez"

#define BT_DBUS_BLUEZ_ADAPTER_PATH          "org.bluez.Adapter1"

#define BT_DBUS_BLUEZ_DEVICE_PATH           "org.bluez.Device1"

#define BT_DBUS_BLUEZ_AGENT_PATH            "org.bluez.Agent1"
#define BT_DBUS_BLUEZ_AGENT_MGR_PATH        "org.bluez.AgentManager1"

#define BT_DBUS_BLUEZ_MEDIA_PATH            "org.bluez.Media1"
#define BT_DBUS_BLUEZ_MEDIA_ENDPOINT_PATH   "org.bluez.MediaEndpoint1"
#define BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH  "org.bluez.MediaTransport1"
#define BT_DBUS_BLUEZ_MEDIA_CTRL_PATH       "org.bluez.MediaControl1"
#define BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH     "org.bluez.MediaPlayer1"
#define BT_DBUS_BLUEZ_MEDIA_ITEM_PATH       "org.bluez.MediaItem1"
#define BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH     "org.bluez.MediaFolder1"

#define BT_DBUS_BLUEZ_GATT_SERVICE_PATH     "org.bluez.GattService1"
#define BT_DBUS_BLUEZ_GATT_CHAR_PATH        "org.bluez.GattCharacteristic1"
#define BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH  "org.bluez.GattDescriptor1"
#define BT_DBUS_BLUEZ_GATT_MGR_PATH         "org.bluez.GattManager1"

#define BT_DBUS_BLUEZ_LE_ADV_PATH           "org.bluez.LEAdvertisement1"
#define BT_DBUS_BLUEZ_LE_ADV_MGR_PATH       "org.bluez.LEAdvertisingManager1"

#define BT_MEDIA_SBC_A2DP_SINK_ENDPOINT     "/MediaEndpoint/SBC/A2DP/Sink"
#define BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT   "/MediaEndpoint/SBC/A2DP/Source"
#define BT_MEDIA_MP3_A2DP_SINK_ENDPOINT     "/MediaEndpoint/Mp3/A2DP/Sink"
#define BT_MEDIA_MP3_A2DP_SOURCE_ENDPOINT   "/MediaEndpoint/Mp3/A2DP/Source"
#define BT_MEDIA_AAC_A2DP_SINK_ENDPOINT     "/MediaEndpoint/AAC/A2DP/Sink"
#define BT_MEDIA_AAC_A2DP_SOURCE_ENDPOINT   "/MediaEndpoint/AAC/A2DP/Source"
#define BT_MEDIA_PCM_HFP_AG_ENDPOINT        "/MediaEndpoint/PCM/HFP/AudioGateway"
#define BT_MEDIA_SBC_HFP_AG_ENDPOINT        "/MediaEndpoint/SBC/HFP/AudioGateway"
#define BT_MEDIA_PCM_HFP_HS_ENDPOINT        "/MediaEndpoint/PCM/HFP/Headset"

#define BT_LE_GATT_SERVER_ENDPOINT          "/org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX"
#define BT_LE_GATT_SERVER_ADVERTISEMENT     "/LeGattAdvert"

#define BT_BLUEZ_VERSION_5_45               "5.45"
#define BT_BLUEZ_VERSION_5_48               "5.48"
#define BT_BLUEZ_VERSION_5_54               "5.54"


typedef struct _stBTMediaInfo {
    unsigned char   ui8Codec;
    char            pcState[BT_MAX_STR_LEN];
    char            pcUUID[BT_MAX_STR_LEN];
    unsigned short  ui16Delay;
    unsigned short  ui16Volume;
} stBTMediaInfo;


typedef struct _stBtIfceHdl {

    DBusConnection*                         pDBusConn;

    char*                                   pcBTAgentPath;
    char*                                   pcBTDAdapterPath;
    char*                                   pcBTDAdapterAddr;
    char*                                   pcBTAdapterPath;
    char*                                   pcDevTransportPath;

    char*                                   pcBTOutPassCode;

    void*                                   pcBAdapterStatusUserData;
    void*                                   pcBDevStatusUserData;
    void*                                   pcBMediaStatusUserData;
    void*                                   pcBNegMediaUserData;
    void*                                   pcBTransPathMediaUserData;
    void*                                   pcBMediaPlayerPathUserData;
    void*                                   pcBMediaBrowserPathUserData;
    void*                                   pcBConnIntimUserData;
    void*                                   pcBConnAuthUserData;
    void*                                   pcBLePathUserData;
    void*                                   pcBLeAdvUserData;

    int                                     i32DoReject;

    unsigned int                            ui32cBConnAuthPassKey;
    unsigned int                            ui32DevLost;

    unsigned int                            ui32IsAdapterDiscovering;

    char                                    pcBTVersion[BT_MAX_STR_LEN];
    char                                    pcDeviceCurrState[BT_MAX_STR_LEN];
    char                                    pcLeDeviceCurrState[BT_MAX_STR_LEN];
    char                                    pcLeDeviceAddress[BT_MAX_STR_LEN];
    char                                    pcMediaCurrState[BT_MAX_STR_LEN];

    stBTAdapterInfo                         lstBTAdapterInfo;
    stBTDeviceInfo                          lstBTDeviceInfo;
    stBTMediaInfo                           lstBTMediaInfo;

    stBTLeGattInfo                          stBTLeGatt;
    stBTLeCustomAdv                         stBTLeCustAdvt;
    char                                    pcBTAdapterGattSrvEpPath[BT_MAX_DEV_PATH_LEN];

    fPtr_BtrCore_BTAdapterStatusUpdateCb    fpcBAdapterStatusUpdate;
    fPtr_BtrCore_BTDevStatusUpdateCb        fpcBDevStatusUpdate;
    fPtr_BtrCore_BTMediaStatusUpdateCb      fpcBMediaStatusUpdate;
    fPtr_BtrCore_BTNegotiateMediaCb         fpcBNegotiateMedia;
    fPtr_BtrCore_BTTransportPathMediaCb     fpcBTransportPathMedia;
    fPtr_BtrCore_BTMediaPlayerPathCb        fpcBTMediaPlayerPath;
    fPtr_BtrCore_BTMediaBrowserPathCb       fpcBTMediaBrowserPath;
    fPtr_BtrCore_BTConnIntimCb              fpcBConnectionIntimation;
    fPtr_BtrCore_BTConnAuthCb               fpcBConnectionAuthentication;
    fPtr_BtrCore_BTLeGattPathCb             fpcBTLeGattPath;
    fPtr_BtrCore_BTLeAdvertisementCb        fpcBTLeAdvPath;

} stBtIfceHdl;


/* Static Function Prototypes */
static int btrCore_BTHandleDusError (DBusError* aDBusErr, int aErrline, const char* aErrfunc);
static const char* btrCore_DBusType2Name (int ai32DBusMessageType);
static enBTDeviceType btrCore_BTMapServiceClasstoDevType(unsigned int aui32Class);
static enBTDeviceType btrCore_BTMapDevClasstoDevType(unsigned int aui32Class);
static char* btrCore_BTGetDefaultAdapterPath (stBtIfceHdl* apstlhBtIfce);
static int btrCore_BTReleaseDefaultAdapterPath (stBtIfceHdl* apstlhBtIfce);
static int btrCore_BTGetDevAddressFromDevPath (const char* deviceIfcePath, char* devAddr);
static tBTMediaItemId btrCore_BTGetMediaItemIdFromMediaPath (const char* mediaIfcePath);
static enBTMediaFolderType btrCore_BTGetMediaFolderType (const char* apcFolderType);
static const char* btrCore_BTGetMediaItemAbsoluteName (const char* apcMediaItemName);

static DBusHandlerResult btrCore_BTAgentRelease (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentRequestPincode (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentRequestPasskey (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentRequestConfirmation(DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentAuthorize (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentDisplayPinCodePassKey (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, bool abPinCode, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentCancelMessage (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);

static DBusMessage* btrCore_BTSendMethodCall (DBusConnection* apDBusConn, const char* objectpath, const char* interfacename, const char* methodname);
static void btrCore_BTPendingCallCheckReply (DBusPendingCall* apDBusPendC, void* apvUserData);

static int btrCore_BTParseAdapter (DBusMessageIter* apDBusMsgIter, stBTAdapterInfo* apstBTAdapterInfo);
static int btrCore_BTGetDeviceInfo (DBusConnection* apDBusConn, stBTDeviceInfo* apstBTDeviceInfo, const char* apcIface);
static int btrCore_BTParseDevice (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);

#if 0
static int btrCore_BTParsePropertyChange (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static int btrCore_BTGetGattInfo (enBTOpIfceType aenBTOpIfceType, void* apvGattInfo, const char* apcIface);
#endif

static int btrCore_BTGetMediaInfo (DBusConnection* apDBusConn, stBTMediaInfo* apstBTDeviceInfo, const char* apcIface);
static int btrCore_BTParseMediaTransport (DBusMessage* apDBusMsg, stBTMediaInfo*  apstBTMediaInfo); 
static int btrCore_BTParseMediaTrackMetadata (DBusMessage* apDBusMsg, stBTMediaTrackInfo* apstBTMediaTrackInfo);

static DBusMessage* btrCore_BTMediaEndpointSelectConfiguration (DBusMessage* apDBusMsg, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apvUserData);
static DBusMessage* btrCore_BTMediaEndpointSetConfiguration (DBusMessage* apDBusMsg, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apvUserData);
static DBusMessage* btrCore_BTMediaEndpointClearConfiguration (DBusMessage* apDBusMsg, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apvUserData);

static int btrCore_BTGetMediaIfceProperty (DBusConnection* apDBusConn, const char* apBtObjectPath, const char* apBtInterfacePath, const char* mediaProperty, void* mediaPropertyValue);

static DBusMessage* btrCore_BTRegisterGattService (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, stBtIfceHdl* apstlhBtIfce, stBTLeGattService  *lpstBTRCoreLeGattService, int aNumOfGattServices);
static int btrCore_BTUnRegisterGattService (DBusConnection* apDBusConn, const char* apui8SCDGattPath);
static int btrCore_BTRegisterLeAdvGetProp (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, stBtIfceHdl* apstlhBtIfce);
static DBusMessage* btrCore_BTLEGattOps (DBusMessage* apDBusMsg, stBtIfceHdl* apstlhBtIfce, enBTOpIfceType  aenIfceType, enBTLeGattOp aenGattOp);
static int btrCore_BTReleaseLEGattObjPath(char* apstObjPath, void* apvUserData);
/* Incoming Callbacks Prototypes */
static DBusHandlerResult btrCore_BTDBusConnectionFilterCb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTMediaEndpointHandlerCb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTAgentMessageHandlerCb  (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTLeGattEndpointHandlerCb(DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);
static DBusHandlerResult btrCore_BTLeGattMessageHandlerCb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* apvUserData);


static const DBusObjectPathVTable gDBusMediaEndpointVTable = {
    .message_function = btrCore_BTMediaEndpointHandlerCb,
};

static const DBusObjectPathVTable gDBusAgentVTable = {
    .message_function = btrCore_BTAgentMessageHandlerCb,
};

static const DBusObjectPathVTable gDBusLeGattEndpointVTable = {
    .message_function = btrCore_BTLeGattEndpointHandlerCb,
};

static const DBusObjectPathVTable gDBusLeGattSCDVTable = {
    .message_function = btrCore_BTLeGattMessageHandlerCb,
};


/* Static Function Defs */
static inline int 
btrCore_BTHandleDusError (
    DBusError*  apDBusErr,
    int         aErrline, 
    const char* apErrfunc
) {
    if (dbus_error_is_set(apDBusErr)) {
        BTRCORELOG_ERROR ("%d\t: %s - DBus Error is %s - Name: %s \n", aErrline, apErrfunc, apDBusErr->message, apDBusErr->name);
        dbus_error_free(apDBusErr);
        return 1;
    }
    return 0;
}


static const char*
btrCore_DBusType2Name (
    int ai32MessageType
) {
    switch (ai32MessageType) {
    case DBUS_MESSAGE_TYPE_SIGNAL:
      return "Signal ";
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
      return "MethodCall";
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      return "MethodReturn";
    case DBUS_MESSAGE_TYPE_ERROR:
      return "Error";
    default:
      return "Unknown";
    }
}

static enBTDeviceType
btrCore_BTMapServiceClasstoDevType (
    unsigned int aui32Class
) {
    enBTDeviceType lenBtDevType = enBTDevUnknown;

    /* Refer https://www.bluetooth.com/specifications/assigned-numbers/baseband
     * The bit 18 set to represent AUDIO OUT service Devices.
     * The bit 19 can be set to represent AUDIO IN Service devices
     * The bit 21 set to represent AUDIO Services (Mic, Speaker, headset).
     * The bit 22 set to represent Telephone Services (headset).
     */

    if (0x40000u & aui32Class) {
        BTRCORELOG_DEBUG ("Its a enBTDevAudioSink : Rendering Class of Service\n");
        lenBtDevType = enBTDevAudioSink;
    }
    else if (0x80000u & aui32Class) {
        if (enBTDCMicrophone && aui32Class) {
            BTRCORELOG_DEBUG ("Its a enBTDevAudioSource : Capturing Service and Mic Device\n");
            lenBtDevType = enBTDevAudioSource;
        }
    }
    //Audio device but not a peripheral  (mouse, joystick, keyboards etc )
    else if ((0x200000u & aui32Class) && ((0x500u & aui32Class) != 0x500u ))  {
        if (enBTDCMicrophone && aui32Class) {
            BTRCORELOG_DEBUG ("Its a enBTDevAudioSource : Audio Class of Service and Mic Device\n");
            lenBtDevType = enBTDevAudioSource;
        }
        else {
            BTRCORELOG_DEBUG ("Its a enBTDevAudioSink : Audio Class of Service. Not a Mic\n");
            lenBtDevType = enBTDevAudioSink;
        }
    }
    else if (0x400000u & aui32Class) {
        BTRCORELOG_DEBUG ("Its a enBTDevAudioSink : Telephony Class of Service\n");
        lenBtDevType = enBTDevAudioSink;
    }

    return lenBtDevType;
}

static enBTDeviceType
btrCore_BTMapDevClasstoDevType (
    unsigned int    aui32Class
) {
    enBTDeviceType lenBtDevType = enBTDevUnknown;

    if ((lenBtDevType = btrCore_BTMapServiceClasstoDevType(aui32Class)) != enBTDevUnknown)
        return lenBtDevType;


    if (((aui32Class & 0x100u) == 0x100u) ||
        ((aui32Class & 0x200u) == 0x200u) ||
        ((aui32Class & 0x400u) == 0x400u) ||
        ((aui32Class & 0x500u) == 0x500u) ||
        ((aui32Class & 0x540u) == 0x540u) ||
        ((aui32Class & 0x580u) == 0x580u) ||
        ((aui32Class & 0x5C0u) == 0x5C0u) ||
        ((aui32Class & 0x504u) == 0x504u) ||
        ((aui32Class & 0x508u) == 0x508u) || 
        ((aui32Class & 0x50Cu) == 0x50Cu)) {

        unsigned int ui32DevClassID = aui32Class & 0xFFFu;

        switch (ui32DevClassID){
        case enBTDCSmartPhone:
        case enBTDCTablet:
        case enBTDCMicrophone:
            BTRCORELOG_DEBUG ("Its a enBTDevAudioSource\n");
            lenBtDevType = enBTDevAudioSource;
            break;
        case enBTDCWearableHeadset:
        case enBTDCHeadphones:
        case enBTDCLoudspeaker:
        case enBTDCHIFIAudioDevice:
            BTRCORELOG_DEBUG ("Its a enBTDevAudioSink\n");
            lenBtDevType = enBTDevAudioSink;
            break;
        case enBTDCKeyboard:
        case enBTDCMouse:
        case enBTDCMouseKeyBoard:
        case enBTDCJoystick:
        case enBTDCGamePad:
        case enBTDCAudioRemote:
            BTRCORELOG_DEBUG ("Its a enBTDevHID\n");
            lenBtDevType = enBTDevHID;
            break;
        default:
            BTRCORELOG_DEBUG ("Its a enBTDevUnknown\n");                   
            lenBtDevType = enBTDevUnknown;
            break;
        }
    }

    return lenBtDevType;
}

static int
btrCore_BTGetDevAddressFromDevPath (
    const char*     deviceIfcePath,
    char*           devAddr
) {
    //  DevIfcePath format /org/bluez/hci0/dev_DC_1A_C5_62_F5_EA
    deviceIfcePath = strstr(deviceIfcePath, "dev") + 4;

    devAddr[0]   = deviceIfcePath[0];
    devAddr[1]   = deviceIfcePath[1];
    devAddr[2]   = ':';
    devAddr[3]   = deviceIfcePath[3];
    devAddr[4]   = deviceIfcePath[4];
    devAddr[5]   = ':';
    devAddr[6]   = deviceIfcePath[6];
    devAddr[7]   = deviceIfcePath[7];
    devAddr[8]   = ':';
    devAddr[9]   = deviceIfcePath[9];
    devAddr[10]  = deviceIfcePath[10];
    devAddr[11]  = ':';
    devAddr[12]  = deviceIfcePath[12];
    devAddr[13]  = deviceIfcePath[13];
    devAddr[14]  = ':';
    devAddr[15]  = deviceIfcePath[15];
    devAddr[16]  = deviceIfcePath[16];

    devAddr[17]  = '\0';

    return 0;
}

static tBTMediaItemId
btrCore_BTGetMediaItemIdFromMediaPath (
    const char*     mediaIfcePath
) {
    tBTMediaItemId  mediaItemId = 0;
    char*           ptr         = 0;
    unsigned int    ui32Index   = 0;
    char            iArray[BT_MAX_STR_LEN]  = {'\0'};
    /* To change the logic later to enable index based searching for better performance */
    ptr = strstr(mediaIfcePath, "item");

    if (ptr != NULL) {

        while (ptr && *ptr) {
          if (*ptr<=57 && *ptr>=48) {
              iArray[ui32Index++] = *ptr;
          }
          ptr++;
        }

        iArray[ui32Index] = '\0';

        /* In mediaIfcePath if big value comes,
         * mediaItemId is limiting the range from the 1 to LLONG_MAX.
         * mediaItemId MSB bit is used for differentiating the items from which path
         * either NowPlaying or Filesystem.
         */
        ui32Index = 0;
        do {
            mediaItemId = strtoull (iArray + ui32Index++, NULL, 10);
        } while ((mediaItemId > LLONG_MAX) || (mediaItemId == 0));
    }

    if (strstr(mediaIfcePath, "NowPlaying")) {
        mediaItemId |= 0x8000000000000000;
    }
    else if (!strstr(mediaIfcePath, "Filesystem")) {
        mediaItemId  = 0xFFFFFFFFFFFFFFFF;
    }

    return mediaItemId;
}

static const char*
btrCore_BTGetMediaItemAbsoluteName (
    const char*     apcMediaItemName
) {
    const char*     ptr = 0;
    /* /Filesystem/Album/XYZ_Album   should return  XYZ_Album    */
    /* /Filesystem                   should return  Filesystem   */
    /* ABC_SongName                  should return  ABC_SongName */

    if (!apcMediaItemName) {
        return NULL;
    }

    ptr = &apcMediaItemName[strlen(apcMediaItemName)];

    while (--ptr >= apcMediaItemName && *ptr != '/');

    return ptr +1;
}

static enBTMediaFolderType
btrCore_BTGetMediaFolderType (
    const char*     apcFolderType
) {
    enBTMediaFolderType     eMediaFolderType;

    if (!strncmp(apcFolderType, "albums",      strlen("albums"))) {
        eMediaFolderType    = enBTMediaFldTypAlbum;
    } else
    if (!strncmp(apcFolderType, "artists",     strlen("artists"))) {
        eMediaFolderType    = enBTMediaFldTypArtist;
    } else
    if (!strncmp(apcFolderType, "genres",      strlen("genres"))) {
        eMediaFolderType    = enBTMediaFldTypGenre;
    } else
    if (!strncmp(apcFolderType, "compilation", strlen("compilation"))) {
        eMediaFolderType    = enBTMediaFldTypCompilation;
    } else
    if (!strncmp(apcFolderType, "playlists",    strlen("playlists"))) {
        eMediaFolderType    = enBTMediaFldTypPlayList;
    } else
    if (!strncmp(apcFolderType, "titles",      strlen("titles"))) {
        eMediaFolderType    = enBTMediaFldTypTrackList;
    } else
    if (!strncmp(apcFolderType, "mixed",       strlen("mixed"))) {
        eMediaFolderType    = enBTMediaFldTypTrackList;
    } else
    if (!strncmp(apcFolderType, "track",       strlen("track"))) {
        eMediaFolderType    = enBTMediaFldTypTrack;
    }
    else {
        eMediaFolderType    = enBTMediaFldTypTrackList;
    }

    return eMediaFolderType;
}
    
static char*
btrCore_BTGetDefaultAdapterPath (
    stBtIfceHdl*    apstlhBtIfce
) {
    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter rootIter;
    int             a = 0;
    int             b = 0;
    bool            adapterFound = FALSE;
    char*           adapter_path;
    char            objectPath[BT_MAX_DEV_PATH_LEN] = {'\0'};
    char            objectData[BT_MAX_DEV_PATH_LEN] = {'\0'};


    lpDBusReply = btrCore_BTSendMethodCall(apstlhBtIfce->pDBusConn, "/", DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects");

    if (lpDBusReply && 
        dbus_message_iter_init(lpDBusReply, &rootIter) &&               //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) {  //get the type of message that iter points to

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array

        while (!adapterFound) {

            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {
                DBusMessageIter dictEntryIter;

                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)
                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strncpy(objectPath, adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                    ++a;
                }

                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    DBusMessageIter innerArrayIter;

                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)) {

                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter)) {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                ////// getting default adapter path //////

                                if (strcmp(dbusObject, BT_DBUS_BLUEZ_ADAPTER_PATH) == 0) {
                                    apstlhBtIfce->pcBTDAdapterPath = strndup(adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                                    adapterFound = TRUE;
                                    break;
                                }
                            }

                            /////// NEW //////////
                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)) {
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2)) {
                                        DBusMessageIter innerDictEntryIter2;

                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2)) {
                                            char *dbusObject2;
                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }

                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);
                                            strncpy(objectData, dbusObject3, (strlen(dbusObject3) < BT_MAX_DEV_PATH_LEN) ? strlen(dbusObject3) : BT_MAX_DEV_PATH_LEN - 1);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                        }

                                    }

                                    if (!dbus_message_iter_has_next(&innerArrayIter2)) {
                                        break; //check to see if end of 3rd array
                                    }
                                    else {
                                        dbus_message_iter_next(&innerArrayIter2);
                                    }
                                }
                            }
                        }

                        if (!dbus_message_iter_has_next(&innerArrayIter)) {
                            break; //check to see if end of 2nd array
                        }
                        else {
                            dbus_message_iter_next(&innerArrayIter);
                        }
                    }
                }

                if (!dbus_message_iter_has_next(&arrayElementIter)) {
                    break; //check to see if end of 1st array
                }
                else {
                    dbus_message_iter_next(&arrayElementIter);
                }
            } //while loop end --used to traverse arra
        }

        dbus_message_unref(lpDBusReply);
    }

    if (apstlhBtIfce->pcBTDAdapterPath) {
        BTRCORELOG_WARN ("Default Adapter Path is : %s - %p\n", apstlhBtIfce->pcBTDAdapterPath, apstlhBtIfce->pcBTDAdapterPath);

    }
    return apstlhBtIfce->pcBTDAdapterPath;
}


static int
btrCore_BTReleaseDefaultAdapterPath (
    stBtIfceHdl*    apstlhBtIfce
) {
    if (apstlhBtIfce->pcBTDAdapterAddr) {
        BTRCORELOG_WARN ("Adapter Addr is : %s - %p\n", apstlhBtIfce->pcBTDAdapterAddr, apstlhBtIfce->pcBTDAdapterAddr);
        free(apstlhBtIfce->pcBTDAdapterAddr);
        apstlhBtIfce->pcBTDAdapterAddr = NULL;
    }

    if (apstlhBtIfce->pcBTDAdapterPath) {
        BTRCORELOG_WARN ("Default Adapter Path is : %s - %p\n", apstlhBtIfce->pcBTDAdapterPath, apstlhBtIfce->pcBTDAdapterPath);
        free(apstlhBtIfce->pcBTDAdapterPath);
        apstlhBtIfce->pcBTDAdapterPath = NULL;
    }

    return 0;
}


static DBusHandlerResult
btrCore_BTAgentRelease (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    (void)pstlhBtIfce;


    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for Release method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Unable to create lpDBusReply message\n");
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
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    if (!pstlhBtIfce->pcBTOutPassCode)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for RequestPinCode method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (pstlhBtIfce->i32DoReject) {
        lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
        goto sendmsg;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    BTRCORELOG_INFO ("Pincode request for device %s\n", lpcPath);
    dbus_message_append_args(lpDBusReply, DBUS_TYPE_STRING, &pstlhBtIfce->pcBTOutPassCode, DBUS_TYPE_INVALID);

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
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    unsigned int    ui32PassCode= 0;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    if (!pstlhBtIfce->pcBTOutPassCode)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_INVALID))  {
        BTRCORELOG_ERROR ("Incorrect args btrCore_BTAgentRequestPasskey");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    BTRCORELOG_INFO ("Pass code request for device %s\n", lpcPath);
    ui32PassCode = strtoul(pstlhBtIfce->pcBTOutPassCode, NULL, 10);
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
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    unsigned int    ui32PassCode= 0;
    int             yesNo       = 0;
    int             i32OpRet    = -1;
    stBTDeviceInfo  lstBTDeviceInfo;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    memset(&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));


    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_UINT32, &ui32PassCode, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for Authorize method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    BTRCORELOG_INFO ("btrCore_BTAgentRequestConfirmation: PASS Code for %s is %6d\n", lpcPath, ui32PassCode);

    if (lpcPath) {
        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, &lstBTDeviceInfo, lpcPath);
        enBTDeviceType  lenBTDevType  = btrCore_BTMapDevClasstoDevType(lstBTDeviceInfo.ui32Class);

        /* Set the ucIsReqConfirmation as 1; as we expect confirmation from the user */
        if (pstlhBtIfce->fpcBConnectionIntimation) {
            BTRCORELOG_INFO ("calling ConnIntimation cb for %s - OpRet = %d\n", lpcPath, i32OpRet);
            yesNo = pstlhBtIfce->fpcBConnectionIntimation(lenBTDevType, &lstBTDeviceInfo, ui32PassCode, 1, pstlhBtIfce->pcBConnIntimUserData);
        }
    }

    pstlhBtIfce->ui32cBConnAuthPassKey = ui32PassCode;


    if (yesNo == 0) {
        BTRCORELOG_ERROR ("Sorry, you cant connect....\n");
        lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
    }
    else {
        lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    }


    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }
    else {
        BTRCORELOG_INFO ("Intimating request for %s\n", lpcPath);
        dbus_connection_send(apDBusConn, lpDBusReply, NULL);
        dbus_connection_flush(apDBusConn);
        dbus_message_unref(lpDBusReply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
}

static DBusHandlerResult
btrCore_BTAgentDisplayPinCodePassKey (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    bool            abPinCode,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    const char*     pinCode     = NULL;
    unsigned int    ui32PassCode= 0;
    enBTDeviceType  lenBTDevType= enBTDevUnknown;
    stBTDeviceInfo  lstBTDeviceInfo;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    memset(&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));

    if (abPinCode == TRUE) {
        if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_STRING, &pinCode, DBUS_TYPE_INVALID)) {
            BTRCORELOG_ERROR ("Invalid arguments for PINCode Display method");
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        BTRCORELOG_INFO ("btrCore_BTAgentDisplayPinCode: PINCode is @@%s@@\n", pinCode);
        ui32PassCode = (unsigned int) atoi(pinCode);
        BTRCORELOG_DEBUG ("btrCore_BTAgentDisplayPinCode: PINCode in decimal @@%06d@@\n", ui32PassCode);
    }
    else {
        unsigned short ui16Entered = 0;
        if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_UINT32, &ui32PassCode, DBUS_TYPE_UINT16, &ui16Entered, DBUS_TYPE_INVALID)) {
            BTRCORELOG_ERROR ("Invalid arguments for PINCode Display method");
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        BTRCORELOG_DEBUG ("btrCore_BTAgentDisplayPassKey : PassKey in decimal @@%06d@@  Entered is %d\n", ui32PassCode, ui16Entered);
    }

    if (lpcPath) {
        btrCore_BTGetDeviceInfo(apDBusConn, &lstBTDeviceInfo, lpcPath);
        lenBTDevType  = btrCore_BTMapDevClasstoDevType(lstBTDeviceInfo.ui32Class);
        pstlhBtIfce->ui32cBConnAuthPassKey = ui32PassCode;

        /* Set the ucIsReqConfirmation as 0; as we do not expect confirmation */
        if (pstlhBtIfce->fpcBConnectionIntimation) {
            pstlhBtIfce->fpcBConnectionIntimation(lenBTDevType, &lstBTDeviceInfo, ui32PassCode, 0, pstlhBtIfce->pcBConnIntimUserData);
        }
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusReply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
btrCore_BTAgentAuthorize (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    const char*     uuid        = NULL;
    int             yesNo       = 0;
    int             i32OpRet    = -1;
    enBTDeviceType  lenBTDevType= enBTDevUnknown;
    stBTDeviceInfo  lstBTDeviceInfo;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;
    

    memset(&lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));


    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_OBJECT_PATH, &lpcPath, DBUS_TYPE_STRING, &uuid, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for Authorize method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (lpcPath) {
        i32OpRet      = btrCore_BTGetDeviceInfo(apDBusConn, &lstBTDeviceInfo, lpcPath);
        lenBTDevType  = btrCore_BTMapDevClasstoDevType(lstBTDeviceInfo.ui32Class);

        if (pstlhBtIfce->fpcBConnectionAuthentication) {
            BTRCORELOG_INFO ("calling ConnAuth cb for %s - OpRet = %d\n", lpcPath, i32OpRet);
            yesNo = pstlhBtIfce->fpcBConnectionAuthentication(lenBTDevType, &lstBTDeviceInfo, pstlhBtIfce->pcBConnAuthUserData);
        }
    }

    pstlhBtIfce->ui32cBConnAuthPassKey = 0;


    if (yesNo == 0) {
        BTRCORELOG_ERROR ("Sorry, you cant connect....\n");
        lpDBusReply = dbus_message_new_error(apDBusMsg, "org.bluez.Error.Rejected", "");
    }
    else {
        lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    }


    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }
    else {
        BTRCORELOG_INFO ("Authorizing request for %s\n", lpcPath);
        if (enBTDevAudioSource == lenBTDevType && yesNo) {
            strcpy(lstBTDeviceInfo.pcDeviceCurrState,"connected");

            if (pstlhBtIfce->fpcBDevStatusUpdate) {
                pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, enBTDevStPropChanged, &lstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData);
            }
        }

        dbus_connection_send(apDBusConn, lpDBusReply, NULL);
        dbus_connection_flush(apDBusConn);
        dbus_message_unref(lpDBusReply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
}


static DBusHandlerResult
btrCore_BTAgentCancelMessage (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    (void)pstlhBtIfce;

    if (!dbus_message_get_args(apDBusMsg, NULL, DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for confirmation method");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    BTRCORELOG_INFO ("Request canceled\n");
    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't create lpDBusReply message\n");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);
    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusMessage*
btrCore_BTSendMethodCall (
    DBusConnection*     apDBusConn,
    const char*         apcObjectPath,
    const char*         apcInterface,
    const char*         apcMethod
) {
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusPendingCall*    lpDBusPendC = NULL;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcObjectPath,
                                             apcInterface,
                                             apcMethod);

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Cannot allocate DBus message!\n");
        return NULL;
    }

    if (!dbus_connection_send_with_reply(apDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message!\n");
        return NULL;
    }

    dbus_connection_flush(apDBusConn);

    if (lpDBusPendC != NULL) {
        dbus_pending_call_block(lpDBusPendC);

        if ((lpDBusReply = dbus_pending_call_steal_reply(lpDBusPendC))) {
            if (dbus_message_get_type(lpDBusReply) ==  DBUS_MESSAGE_TYPE_ERROR) {
                BTRCORELOG_ERROR ("Error : %s\n\n", dbus_message_get_error_name(lpDBusReply));
                dbus_message_unref(lpDBusReply);
                lpDBusReply = NULL;
            }
        }

        dbus_pending_call_unref(lpDBusPendC);
        lpDBusPendC = NULL;
    }

    dbus_message_unref(lpDBusMsg);
    lpDBusMsg = NULL;

    return lpDBusReply;
}


static void
btrCore_BTPendingCallCheckReply (
    DBusPendingCall*    apDBusPendC,
    void*               apvUserData
) {
    DBusMessage *lpDBusReply = NULL; 
    DBusError lDBusErr;

    BTRCORELOG_DEBUG("btrCore_BTPendingCallCheckReply\n");

    if ((lpDBusReply = dbus_pending_call_steal_reply(apDBusPendC))) {
        dbus_error_init(&lDBusErr);
        if (dbus_set_error_from_message(&lDBusErr, lpDBusReply) == TRUE) {
            BTRCORELOG_ERROR ("Error : %s\n\n", dbus_message_get_error_name(lpDBusReply));
            btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        }

        dbus_message_unref(lpDBusReply);
    }

    dbus_pending_call_unref(apDBusPendC);   //Free pending call handle
}


static int
btrCore_BTParseAdapter (
    DBusMessageIter*    apDBusMsgIter,
    stBTAdapterInfo*    apstBTAdapterInfo
) {
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    DBusMessageIter dict_entry_value_iter;
    int             dbus_type;
    const char*     pcKey = NULL;
    const char*     pcVal = NULL;

    // 'out' parameter of org.freedesktop.DBus.Properties.GetAll call has format 'DICT<STRING,VARIANT> props'
    for (dbus_message_iter_recurse(apDBusMsgIter, &dict_iter); // recurse into array (of dictionary entries)
            (dbus_type = dbus_message_iter_get_arg_type(&dict_iter)) != DBUS_TYPE_INVALID;
                dbus_message_iter_next(&dict_iter)) {

        if (dbus_type == DBUS_TYPE_DICT_ENTRY) {
            dbus_message_iter_recurse(&dict_iter, &dict_entry_iter); // recurse into dictionary entry at this position in dictionary

            dbus_message_iter_get_basic(&dict_entry_iter, &pcKey); // get dictionary entry's first item (key) which is a string

            // move to dictionary entry's next item (value associated with key) which should be a variant
            if (!dbus_message_iter_next(&dict_entry_iter) || dbus_message_iter_get_arg_type(&dict_entry_iter) != DBUS_TYPE_VARIANT)
                continue; // either this dictionary entry does not have a value (or) the value is not a variant

            dbus_message_iter_recurse(&dict_entry_iter, &dict_entry_value_iter); // recurse into dictionary entry's variant value
            dbus_type = dbus_message_iter_get_arg_type (&dict_entry_value_iter);

            if (strcmp (pcKey, "Address") == 0 && dbus_type == DBUS_TYPE_STRING) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &pcVal);
                strncpy(apstBTAdapterInfo->pcAddress, pcVal, BT_MAX_STR_LEN-1);  // TODO strncpy is unsafe; use snprintf instead
                BTRCORELOG_TRACE ("pcAddress               = %s\n", apstBTAdapterInfo->pcAddress);
            }
            else if (strcmp (pcKey, "Name") == 0 && dbus_type == DBUS_TYPE_STRING) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &pcVal);
                strncpy(apstBTAdapterInfo->pcName, pcVal, BT_MAX_STR_LEN-1);
                BTRCORELOG_TRACE ("pcName                  = %s\n", apstBTAdapterInfo->pcName);
            }
            else if (strcmp (pcKey, "Alias") == 0 && dbus_type == DBUS_TYPE_STRING) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &pcVal);
                strncpy(apstBTAdapterInfo->pcAlias, pcVal, BT_MAX_STR_LEN-1);
                BTRCORELOG_TRACE ("pcAlias                 = %s\n", apstBTAdapterInfo->pcAlias);
            }
            else if (strcmp (pcKey, "Class") == 0 && dbus_type == DBUS_TYPE_UINT32) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->ui32Class);
                BTRCORELOG_TRACE ("ui32Class               = %u\n", apstBTAdapterInfo->ui32Class);
            }
            else if (strcmp (pcKey, "Powered") == 0 && dbus_type == DBUS_TYPE_BOOLEAN) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->bPowered);
                BTRCORELOG_TRACE ("bPowered                = %d\n", apstBTAdapterInfo->bPowered);
            }
            else if (strcmp (pcKey, "Discoverable") == 0 && dbus_type == DBUS_TYPE_BOOLEAN) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->bDiscoverable);
                BTRCORELOG_TRACE ("bDiscoverable           = %d\n", apstBTAdapterInfo->bDiscoverable);
            }
            else if (strcmp (pcKey, "DiscoverableTimeout") == 0 && dbus_type == DBUS_TYPE_UINT32) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->ui32DiscoverableTimeout);
                BTRCORELOG_TRACE ("ui32DiscoverableTimeout = %u\n", apstBTAdapterInfo->ui32DiscoverableTimeout);
            }
            else if (strcmp (pcKey, "Pairable") == 0 && dbus_type == DBUS_TYPE_BOOLEAN) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->bPairable);
                BTRCORELOG_TRACE ("bPairable               = %d\n", apstBTAdapterInfo->bPairable);
            }
            else if (strcmp (pcKey, "PairableTimeout") == 0 && dbus_type == DBUS_TYPE_UINT32) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->ui32PairableTimeout);
                BTRCORELOG_TRACE ("ui32PairableTimeout     = %u\n", apstBTAdapterInfo->ui32PairableTimeout);
            }
            else if (strcmp (pcKey, "Discovering") == 0 && dbus_type == DBUS_TYPE_BOOLEAN) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &apstBTAdapterInfo->bDiscovering);
                BTRCORELOG_DEBUG ("bDiscovering            = %d\n", apstBTAdapterInfo->bDiscovering);
            }
            else if (strcmp (pcKey, "Modalias") == 0 && dbus_type == DBUS_TYPE_STRING) {
                dbus_message_iter_get_basic(&dict_entry_value_iter, &pcVal);
                strncpy(apstBTAdapterInfo->pcModalias, pcVal, BT_MAX_STR_LEN-1);
                BTRCORELOG_TRACE ("pcModalias              = %s\n", apstBTAdapterInfo->pcModalias);
            }
            else if (strcmp (pcKey, "UUIDs") == 0 && dbus_type == DBUS_TYPE_ARRAY) {
                DBusMessageIter uuid_array_iter;
                int count = 0;
                for (dbus_message_iter_recurse(&dict_entry_value_iter, &uuid_array_iter);
                        (dbus_type = dbus_message_iter_get_arg_type (&uuid_array_iter)) != DBUS_TYPE_INVALID;
                        dbus_message_iter_next (&uuid_array_iter)) {
                    if ((dbus_type == DBUS_TYPE_STRING) && (count < BT_MAX_DEVICE_PROFILE)) {
                        dbus_message_iter_get_basic (&uuid_array_iter, &pcVal);
                        strncpy(apstBTAdapterInfo->ppcUUIDs[count], pcVal, (BT_MAX_UUID_STR_LEN - 1));
                        BTRCORELOG_TRACE ("UUID value is %s\n", apstBTAdapterInfo->ppcUUIDs[count]);
                        count++;
                    }
                }
            }
        }
    }

    return 0;
}


static int
btrCore_BTGetDeviceInfo (
    DBusConnection*     apDBusConn,
    stBTDeviceInfo*     apstBTDeviceInfo,
    const char*         apcIface
) {
    char*               pdeviceInterface = BT_DBUS_BLUEZ_DEVICE_PATH;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusError           lDBusErr;
    DBusPendingCall*    lpDBusPendC;


    if (!apcIface)
        return -1;

    BTRCORELOG_DEBUG ("Getting properties for the device %s\n", apcIface);

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcIface,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "GetAll");

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pdeviceInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(apDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message");
        return -1;
    }

    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusMsg);
    lpDBusMsg = NULL;

    dbus_pending_call_block(lpDBusPendC);
    lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);
    dbus_pending_call_unref(lpDBusPendC);

    if (lpDBusReply != NULL) {
        if (0 != btrCore_BTParseDevice(lpDBusReply, apstBTDeviceInfo)) {
            BTRCORELOG_ERROR ("Parsing the device %s failed..\n", apcIface);
            dbus_message_unref(lpDBusReply);
            return -1;
        }
        else {
            strncpy(apstBTDeviceInfo->pcDevicePath, apcIface, (strlen(apcIface) < BT_MAX_DEV_PATH_LEN) ? strlen(apcIface) : BT_MAX_DEV_PATH_LEN - 1 );
            dbus_message_unref(lpDBusReply);
            return 0;
        }
    }

    dbus_message_unref(lpDBusReply);
    return 0;
}


static int
btrCore_BTParseDevice (
    DBusMessage*    apDBusMsg,
    stBTDeviceInfo* apstBTDeviceInfo
) {
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    int             dbus_type;

    const char*     pcKey = NULL;
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
    const char*     pcDevAdapterObjPath = NULL;
    const char*     pcIcon = NULL;

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
        BTRCORELOG_ERROR ("dbus_message_iter_init Failed\n");
        return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&arg_i);
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            BTRCORELOG_ERROR ("Unknown Prop structure from Bluez\n");
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
                BTRCORELOG_TRACE ("pcAddress       = %s\n", apstBTDeviceInfo->pcAddress);
               
 #if 1
                char lcDevVen[4] = {'\0'};

                lcDevVen[0]=pcAddress[12];
                lcDevVen[1]=pcAddress[15];
                lcDevVen[2]=pcAddress[16];
                lcDevVen[3]='\0';
                ui16Vendor =  strtoll(lcDevVen, NULL, 16);
                apstBTDeviceInfo->ui16Vendor = ui16Vendor;
                BTRCORELOG_TRACE ("ui16Vendor      = %d\n", apstBTDeviceInfo->ui16Vendor);
 #endif
            }
            else if (strcmp (pcKey, "Name") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcName);
                strncpy(apstBTDeviceInfo->pcName, pcName, BT_MAX_STR_LEN);
                BTRCORELOG_TRACE ("pcName          = %s\n", apstBTDeviceInfo->pcName);

            }
            else if (strcmp (pcKey, "Vendor") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Vendor);
                apstBTDeviceInfo->ui16Vendor = ui16Vendor;
                BTRCORELOG_TRACE ("ui16Vendor      = %d\n", apstBTDeviceInfo->ui16Vendor);
            }
            else if (strcmp (pcKey, "VendorSource") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16VendorSource);
                apstBTDeviceInfo->ui16VendorSource = ui16VendorSource;
                BTRCORELOG_TRACE ("ui16VendorSource= %d\n", apstBTDeviceInfo->ui16VendorSource);
            }
            else if (strcmp (pcKey, "Product") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Product);
                apstBTDeviceInfo->ui16Product = ui16Product;
                BTRCORELOG_TRACE ("ui16Product     = %d\n", apstBTDeviceInfo->ui16Product);
            }
            else if (strcmp (pcKey, "Version") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Version);
                apstBTDeviceInfo->ui16Version = ui16Version;
                BTRCORELOG_TRACE ("ui16Version     = %d\n", apstBTDeviceInfo->ui16Version);
            }
            else if (strcmp (pcKey, "Icon") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcIcon);
                strncpy(apstBTDeviceInfo->pcIcon, pcIcon, BT_MAX_STR_LEN);
                BTRCORELOG_TRACE ("pcIcon          = %s\n", apstBTDeviceInfo->pcIcon);
            }
            else if (strcmp (pcKey, "Class") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui32Class);
                apstBTDeviceInfo->ui32Class = ui32Class;
                BTRCORELOG_DEBUG ("ui32Class       = %d\n", apstBTDeviceInfo->ui32Class);
            }
            else if (strcmp (pcKey, "Paired") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bPaired);
                apstBTDeviceInfo->bPaired = bPaired;
                BTRCORELOG_DEBUG ("bPaired         = %d\n", apstBTDeviceInfo->bPaired);
            }
            else if (strcmp (pcKey, "Connected") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bConnected);
                apstBTDeviceInfo->bConnected = bConnected;
                BTRCORELOG_DEBUG ("bConnected      = %d\n", apstBTDeviceInfo->bConnected);
            }
            else if (strcmp (pcKey, "Trusted") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bTrusted);
                apstBTDeviceInfo->bTrusted = bTrusted;
                BTRCORELOG_TRACE ("bTrusted        = %d\n", apstBTDeviceInfo->bTrusted);
            }
            else if (strcmp (pcKey, "Blocked") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &bBlocked);
                apstBTDeviceInfo->bBlocked = bBlocked;
                BTRCORELOG_TRACE ("bBlocked        = %d\n", apstBTDeviceInfo->bBlocked);
            }
            else if (strcmp (pcKey, "Alias") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcAlias);
                strncpy(apstBTDeviceInfo->pcAlias, pcAlias, BT_MAX_STR_LEN);
                BTRCORELOG_TRACE ("pcAlias         = %s\n", apstBTDeviceInfo->pcAlias);
            }
            else if (strcmp (pcKey, "Adapter") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcDevAdapterObjPath);
                BTRCORELOG_TRACE ("pcDevAdapterObjPath = %s\n", pcDevAdapterObjPath);
            }
            else if (strcmp (pcKey, "RSSI") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &i16RSSI);
                apstBTDeviceInfo->i32RSSI = i16RSSI;
                BTRCORELOG_DEBUG ("i32RSSI         = %d\n", apstBTDeviceInfo->i32RSSI);
            }
            else if (strcmp (pcKey, "UUIDs") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);

                dbus_type = dbus_message_iter_get_arg_type (&variant_i);
                if (dbus_type == DBUS_TYPE_ARRAY) {
                    int count = 0;
                    DBusMessageIter variant_j;
                    dbus_message_iter_recurse(&variant_i, &variant_j);

                    while ((dbus_type = dbus_message_iter_get_arg_type (&variant_j)) != DBUS_TYPE_INVALID) {
                        if ((dbus_type == DBUS_TYPE_STRING) && (count < BT_MAX_DEVICE_PROFILE)) {
                            char *pVal = NULL;
                            dbus_message_iter_get_basic (&variant_j, &pVal);
                            BTRCORELOG_TRACE ("UUID value is %s\n", pVal);
                            strncpy(apstBTDeviceInfo->aUUIDs[count], pVal, (BT_MAX_UUID_STR_LEN - 1));
                            count++;
                        }
                        dbus_message_iter_next (&variant_j);
                    }
                }
                else {
                    BTRCORELOG_ERROR ("Services; Not an Array\n");
                }
            }
            else if (strcmp (pcKey, "ServiceData") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_type = dbus_message_iter_get_arg_type (&variant_i);

                if (dbus_type == DBUS_TYPE_ARRAY) {
                    DBusMessageIter variant_j;

                    dbus_message_iter_recurse(&variant_i, &variant_j);
                    //dbus_type = dbus_message_iter_get_arg_type (&variant_j);

                    DBusMessageIter dict_j;
                    int count = 0;


                    while ((dbus_message_iter_get_arg_type (&variant_j) == DBUS_TYPE_DICT_ENTRY) &&
                           (count < BT_MAX_DEVICE_PROFILE)){
                        const char* uuid_str = NULL;
                        dbus_message_iter_recurse(&variant_j, &dict_j);
                        dbus_message_iter_get_basic(&dict_j, &uuid_str);

                        if (uuid_str) {
                            strncpy(apstBTDeviceInfo->saServices[count].pcUUIDs, uuid_str, (BT_MAX_UUID_STR_LEN - 1));
	                    //BTRCORELOG_TRACE ("Service advertisement data, Key UUID is [%s]\n", apstBTDeviceInfo->saServices[count].pcUUIDs);

	                    DBusMessageIter variant_k;
	                    dbus_message_iter_next(&dict_j);
	                    dbus_message_iter_recurse(&dict_j, &variant_k);

                            dbus_type = dbus_message_iter_get_arg_type (&variant_k);
                            if (dbus_type == DBUS_TYPE_ARRAY) {
                                DBusMessageIter variant_l;
                                uint8_t *service_data;
                                int len;

                                dbus_message_iter_recurse(&variant_k, &variant_l);
                                dbus_message_iter_get_fixed_array(&variant_l, &service_data, &len);

                                if(service_data && len) {
                                    if(len <= BT_MAX_SERVICE_DATA_LEN) {
	                                apstBTDeviceInfo->saServices[count].len = len;
	                                memcpy(apstBTDeviceInfo->saServices[count].pcData, service_data, len);
                                    }
                                    else {
                                        BTRCORELOG_ERROR ("Failed to populate the ServiceData since the ServiceData length [%d] is more than BT_MAX_SERVICE_DATA_LEN [%d].\n", len, BT_MAX_SERVICE_DATA_LEN);
                                    }

                                    //for (int i=0; i<len; i++) {
                                    //BTRCORELOG_TRACE ("ServiceData [%d] [%x]\n",i, *(service_data+i));
                                    //}
                                }
                            }

                            BTRCORELOG_TRACE ("Service advertisement data, Data is: \n "  );
                            BTRCORELOG_TRACE ("------------------\n" );
                            for (int i =0; i < apstBTDeviceInfo->saServices[count].len; i++){
                                BTRCORELOG_TRACE ("ServiceData[%d] = [%x]\n ", i, apstBTDeviceInfo->saServices[count].pcData[i]);
                            }
                            BTRCORELOG_TRACE ("------------------\n" );
                        }
                        count++;
                        dbus_message_iter_next (&variant_j);
                    }
                }
            }
        }

        if (!dbus_message_iter_next(&element_i)) {
            break;
        }
    }
    (void)dbus_type;

    if (strlen(apstBTDeviceInfo->pcAlias))
        strncpy(apstBTDeviceInfo->pcName, apstBTDeviceInfo->pcAlias, BT_MAX_STR_LEN-1);

    return 0;
}

#if 0
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
       BTRCORELOG_ERROR ("GetProperties lpDBusReply has no arguments.");
    }

    if (!dbus_message_get_args( apDBusMsg, NULL,
                                DBUS_TYPE_STRING, &bd_addr,
                                DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid arguments for NameOwnerChanged signal");
        return -1;
    }

    BTRCORELOG_ERROR (" Name: %s\n",bd_addr);//"State" then the variant is a string
    if (strcmp(bd_addr,"State") == 0) {
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
        //BTRCORELOG_ERROR ("type is %d\n", dbus_type);

        if (dbus_type == DBUS_TYPE_STRING) {
            dbus_message_iter_next(&arg_i);
            dbus_message_iter_recurse(&arg_i, &variant_i);
            dbus_message_iter_get_basic(&variant_i, &value);
             // BTRCORELOG_ERROR ("    the new state is: %s\n", value);
            strncpy(apstBTDeviceInfo->pcDevicePrevState, gpcDeviceCurrState, BT_MAX_STR_LEN - 1);
            strncpy(apstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
            strncpy(gpcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
        }
    }

    return 0;
}
#endif

#if 0
static int
btrCore_BTGetGattInfo (
    enBTOpIfceType aenBTOpIfceType,
    void*          apvGattInfo,
    const char*    apcIface
) {
    char*               pGattInterface = NULL;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusError           lDBusErr;
    DBusPendingCall*    lpDBusPendC;

    if (!apcIface)
        return -1;

    BTRCORELOG_DEBUG ("Getting properties for the Gatt Ifce %s\n", apcIface);

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcIface,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "GetAll");

    if (aenBTOpIfceType == enBTGattService) {
       pGattInterface = BT_DBUS_BLUEZ_GATT_SERVICE_PATH;
    } else if (aenBTOpIfceType == enBTGattCharacteristic) {
       pGattInterface = BT_DBUS_BLUEZ_GATT_CHAR_PATH;
    } else if (aenBTOpIfceType == enBTGattDescriptor) {
       pGattInterface = BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pGattInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(gpDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message");
        return -1;
    }

    dbus_connection_flush(gpDBusConn);
    dbus_message_unref(lpDBusMsg);
    lpDBusMsg = NULL;

    dbus_pending_call_block(lpDBusPendC);
    lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);
    dbus_pending_call_unref(lpDBusPendC);

    if (lpDBusReply != NULL) {
   
        DBusMessageIter arg_i;
        DBusMessageIter element_i;
        DBusMessageIter variant_i;
        int             dbus_type;

        char *pcKey               = NULL;
        char *devicePath          = NULL; 
        char *servicePath         = NULL; 
        char *characteristicPath  = NULL; 
        char uuid[BT_MAX_UUID_STR_LEN] = "\0";
        char flags[16][BT_MAX_STR_LEN];
        //char value[10][BT_MAX_STR_LEN];
        int  primary   = 0;
        int  notifying = 0;
        int  fCount = 0, vCount = 0;
      
        if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
            BTRCORELOG_ERROR ("dbus_message_iter_init Failed\n");
            return -1;
        }

        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            dbus_message_iter_next(&arg_i);
            dbus_type = dbus_message_iter_get_arg_type(&arg_i);

            if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
                BTRCORELOG_ERROR ("Unknown Prop structure from Bluez\n");
                return -1;
            }
         }

         dbus_message_iter_recurse(&arg_i, &element_i);
         while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
            if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter dict_i;

                dbus_message_iter_recurse(&element_i, &dict_i);
                dbus_message_iter_get_basic(&dict_i, &pcKey);

                if (strcmp (pcKey, "UUID") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &uuid);
                BTRCORELOG_INFO ("UUID : %s\n", uuid);
            }
            else if (strcmp (pcKey, "Device") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &devicePath);
                //strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("Device : %s\n", devicePath);
            }
            else if (strcmp (pcKey, "Service") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &servicePath);
                //strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("Service : %s\n", servicePath);
            }
            else if (strcmp (pcKey, "Characteristic") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &characteristicPath);
                //strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("Characteristic : %s\n", characteristicPath);
            }
            else if (strcmp (pcKey, "Primary") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &primary);
                //strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("Primary : %d\n", primary);
            }
            else if (strcmp (pcKey, "Notifying") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &notifying);
                //strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("notifying : %d\n", notifying);
            }
            else if (strcmp (pcKey, "Flags") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);

                dbus_type = dbus_message_iter_get_arg_type (&variant_i);
                if (dbus_type == DBUS_TYPE_ARRAY) {
                    DBusMessageIter variant_j;
                    dbus_message_iter_recurse(&variant_i, &variant_j);

                    while ((dbus_type = dbus_message_iter_get_arg_type (&variant_j)) != DBUS_TYPE_INVALID) {
                        if ((dbus_type == DBUS_TYPE_STRING)) {
                            char *pVal = NULL;
                            dbus_message_iter_get_basic (&variant_j, &pVal);
                            BTRCORELOG_INFO ("Flags value is %s\n", pVal);
                            strncpy(flags[fCount], pVal, (BT_MAX_UUID_STR_LEN - 1));
                            fCount++;
                        }
                        dbus_message_iter_next (&variant_j);
                    }
                }
                else {
                    BTRCORELOG_ERROR ("Services; Not an Array\n");
                }
             }
             else if (strcmp (pcKey, "Value") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);

                dbus_type = dbus_message_iter_get_arg_type (&variant_i);
                if (dbus_type == DBUS_TYPE_ARRAY) {
                    DBusMessageIter variant_j;
                    dbus_message_iter_recurse(&variant_i, &variant_j);

                    while ((dbus_type = dbus_message_iter_get_arg_type (&variant_j)) != DBUS_TYPE_INVALID) {
                        if ((dbus_type == DBUS_TYPE_BYTE)) {
                            char *pVal = NULL;
                            dbus_message_iter_get_basic (&variant_j, &pVal);
                            BTRCORELOG_INFO ("Value is %s\n", pVal);
                            //strncpy(value[count], pVal, (BT_MAX_UUID_STR_LEN - 1));
                            vCount++;
                        }
                        dbus_message_iter_next (&variant_j);
                    }
                }
                else {
                    BTRCORELOG_ERROR ("Services; Not an Array\n");
                }
             }
          }

          if (!dbus_message_iter_next(&element_i)) {
              break;
          }
       }
         
       dbus_message_unref(lpDBusReply);

       if (aenBTOpIfceType == enBTGattService) {
          stBTGattServiceInfo* gattServiceInfo = (stBTGattServiceInfo*)apvGattInfo;

          strncpy(gattServiceInfo->uuid, uuid, BT_MAX_UUID_STR_LEN - 1);
          strncpy(gattServiceInfo->gattDevicePath, devicePath, BT_MAX_STR_LEN - 1);
          gattServiceInfo->ui16Primary = primary;
       }
       else if (aenBTOpIfceType == enBTGattCharacteristic) {
          stBTGattCharInfo* gattCharInfo = (stBTGattCharInfo*)apvGattInfo;
          int i=0;

          strncpy(gattCharInfo->uuid, uuid, BT_MAX_UUID_STR_LEN - 1);
          strncpy(gattCharInfo->gattServicePath, servicePath, BT_MAX_STR_LEN - 1);
          gattCharInfo->ui16Notifying = notifying;
          for (;i<fCount;i++) {
              strncpy(gattCharInfo->flags[i], flags[i], BT_MAX_STR_LEN - 1);
          }/*
          for (;i<fCount;i++) {
              strncpy(gattCharInfo->value, value[i], BT_MAX_STR_LEN - 1);
          }*/
       }
       else if (aenBTOpIfceType == enBTGattDescriptor) {
          stBTGattDescInfo* gattDescInfo = (stBTGattDescInfo*)apvGattInfo;
          int i=0;

          strncpy(gattDescInfo->uuid, uuid, BT_MAX_UUID_STR_LEN - 1);
          strncpy(gattDescInfo->gattCharPath, characteristicPath, BT_MAX_STR_LEN - 1);
          for (;i<fCount;i++) {
              strncpy(gattDescInfo->flags[i], flags[i], BT_MAX_STR_LEN - 1);
          }/*
          for (;i<fCount;i++) {
              strncpy(gattCharInfo->value, value[i], BT_MAX_STR_LEN - 1);
          }*/
       }
    }

    return 0;
}
#endif


static int
btrCore_BTGetMediaInfo (
    DBusConnection*     apDBusConn,
    stBTMediaInfo*      apstBTMediaInfo,
    const char*         apcIface
) {
    char*               pdeviceInterface = BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusError           lDBusErr;
    DBusPendingCall*    lpDBusPendC;


    if (!apcIface)
        return -1;

    BTRCORELOG_DEBUG ("Getting properties for the Media Ifce %s\n", apcIface);

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcIface,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "GetAll");

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pdeviceInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(apDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message");
        return -1;
    }

    dbus_connection_flush(apDBusConn);
    dbus_message_unref(lpDBusMsg);
    lpDBusMsg = NULL;

    dbus_pending_call_block(lpDBusPendC);
    lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);
    dbus_pending_call_unref(lpDBusPendC);

    if (lpDBusReply != NULL) {
        if (0 != btrCore_BTParseMediaTransport(lpDBusReply, apstBTMediaInfo)) {
            BTRCORELOG_ERROR ("Parsing the device %s failed..\n", apcIface);
            dbus_message_unref(lpDBusReply);
            return -1;
        }
        else {
            dbus_message_unref(lpDBusReply);
            return 0;
        }
    }

    dbus_message_unref(lpDBusReply);
    return 0;

}


static int
btrCore_BTParseMediaTransport (
    DBusMessage*    apDBusMsg,
    stBTMediaInfo*  apstBTMediaInfo
) {
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    int             dbus_type;

    char*           pcKey = NULL;
    unsigned char   ui8Codec = 0;
    char*           pcState = NULL;
    char*           pcUUID = NULL;
    unsigned short  ui16Delay = 0;
    unsigned short  ui16Volume = 0;

    if (!dbus_message_iter_init(apDBusMsg, &arg_i)) {
        BTRCORELOG_ERROR ("dbus_message_iter_init Failed\n");
        return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&arg_i);
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            BTRCORELOG_ERROR ("Unknown Prop structure from Bluez\n");
            return -1;
        }
    }

    dbus_message_iter_recurse(&arg_i, &element_i);
    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter dict_i;

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_message_iter_get_basic(&dict_i, &pcKey);

            if (strcmp (pcKey, "Codec") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui8Codec);
                apstBTMediaInfo->ui8Codec = ui8Codec;
                BTRCORELOG_INFO ("apstBTMediaInfo->ui8Codec : %d\n", apstBTMediaInfo->ui8Codec);
            }
            else if (strcmp (pcKey, "State") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcState);
                strncpy(apstBTMediaInfo->pcState, pcState, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("apstBTMediaInfo->pcState: %s\n", apstBTMediaInfo->pcState);

            }
            else if (strcmp (pcKey, "UUID") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &pcUUID);
                strncpy(apstBTMediaInfo->pcUUID, pcUUID, BT_MAX_STR_LEN);
                BTRCORELOG_INFO ("apstBTMediaInfo->pcUUID: %s\n", apstBTMediaInfo->pcUUID);
            }
            else if (strcmp (pcKey, "Delay") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Delay);
                apstBTMediaInfo->ui16Delay = ui16Delay;
                BTRCORELOG_INFO ("apstBTMediaInfo->ui16Delay = %d\n", apstBTMediaInfo->ui16Delay);
            }
            else if (strcmp (pcKey, "Volume") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &ui16Volume);
                apstBTMediaInfo->ui16Volume = ui16Volume;
                BTRCORELOG_INFO ("apstBTMediaInfo->ui16Volume = %d\n", apstBTMediaInfo->ui16Volume);
            }
        }

        if (!dbus_message_iter_next(&element_i)) {
            break;
        }
    }

    (void)dbus_type;

    return 0;
}


static int
btrCore_BTParseMediaTrackMetadata (
    DBusMessage*        apDBusMsg,
    stBTMediaTrackInfo* apstBTMediaTrackInfo
) {
    int                 dbus_type = DBUS_TYPE_INVALID;
    DBusMessageIter     lDBusReplyIter;
    DBusMessageIter     arrayMsgIter;
    DBusMessageIter     dictMsgIter;
    DBusMessageIter     element;
    DBusMessageIter     elementBasic;

    unsigned int        ui32Value   = 0;
    char*               pcKey       = "\0";
    char*               pcValue     = "\0";

    if (!apDBusMsg) {
        BTRCORELOG_ERROR ("DBusMessage is NULL !\n");
        return -1;
    }

    if (!dbus_message_iter_init(apDBusMsg, &lDBusReplyIter)) {           // lpDBusMsg is pointer to dbus message received
        BTRCORELOG_ERROR ("dbus_message_iter_init Failed\n");
        return -1;
    }

    if ((dbus_message_iter_get_arg_type (&lDBusReplyIter)) == DBUS_TYPE_INVALID) {
        BTRCORELOG_ERROR ("DBUS_TYPE_INVALID\n");
        return -1;
    }

    dbus_message_iter_recurse(&lDBusReplyIter, &arrayMsgIter);      // pointer to first element ARRAY of the dbus messge received
    if ((dbus_message_iter_get_arg_type (&arrayMsgIter)) == DBUS_TYPE_INVALID) {
        BTRCORELOG_ERROR ("DBUS_TYPE_INVALID\n");
        return -1;
    }

    dbus_message_iter_recurse(&arrayMsgIter, &dictMsgIter);         // pointer to first element DICTIONARY of the dbus messge received


    while ((dbus_type = dbus_message_iter_get_arg_type(&dictMsgIter)) != DBUS_TYPE_INVALID) {
        if (DBUS_TYPE_DICT_ENTRY == dbus_type) {
            dbus_message_iter_recurse(&dictMsgIter,&element);         // pointer to element STRING of the dbus messge received

            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&element)) {
                dbus_message_iter_get_basic(&element, &pcKey);

                if (!strcmp("Album", pcKey)) {        
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, &pcValue); 
                    strncpy(apstBTMediaTrackInfo->pcAlbum, pcValue, BT_MAX_STR_LEN);
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->pcAlbum : %s\n", apstBTMediaTrackInfo->pcAlbum);
                }
                else if (!strcmp("Artist", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, &pcValue);
                    strncpy(apstBTMediaTrackInfo->pcArtist, pcValue, BT_MAX_STR_LEN);
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->pcArtist : %s\n", apstBTMediaTrackInfo->pcArtist);
                }
                else if (!strcmp("Genre", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, &pcValue);
                    strncpy(apstBTMediaTrackInfo->pcGenre, pcValue, BT_MAX_STR_LEN);
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->pcGenre : %s\n", apstBTMediaTrackInfo->pcGenre);
                }
                else if (!strcmp("Title", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, &pcValue);
                    strncpy(apstBTMediaTrackInfo->pcTitle, pcValue, BT_MAX_STR_LEN);
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->pcTitle : %s\n", apstBTMediaTrackInfo->pcTitle);
                }
                else if (!strcmp("NumberOfTracks", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, (void*)&ui32Value);
                    apstBTMediaTrackInfo->ui32NumberOfTracks = ui32Value;
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->ui32NumberOfTracks : %d\n", apstBTMediaTrackInfo->ui32NumberOfTracks);
                }
                else if (!strcmp("TrackNumber", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, (void*)&ui32Value);
                    apstBTMediaTrackInfo->ui32TrackNumber = ui32Value;
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->ui32TrackNumber : %d\n", apstBTMediaTrackInfo->ui32TrackNumber);
                }
                else if (!strcmp("Duration", pcKey)) {
                    dbus_message_iter_next(&element);                   // next element is VARIANT of the dbus messge received
                    dbus_message_iter_recurse(&element, &elementBasic); // pointer to element STRING/UINT32 w.r.t dbus messge received here
                    dbus_message_iter_get_basic(&elementBasic, (void*)&ui32Value);
                    apstBTMediaTrackInfo->ui32Duration = ui32Value;
                    BTRCORELOG_DEBUG ("lpstBTMediaTrackInfo->ui32Duration : %d\n", apstBTMediaTrackInfo->ui32Duration);
                }
            }
        }

        if (!dbus_message_iter_has_next(&dictMsgIter)) {
            break;
        }
        else {
            dbus_message_iter_next(&dictMsgIter);
        }
    }

    return 0;
}


static DBusMessage*
btrCore_BTMediaEndpointSelectConfiguration (
    DBusMessage*    apDBusMsg,
    enBTDeviceType  aenBTDeviceType,
    enBTMediaType   aenBTMediaType,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply      = NULL;
    DBusError       lDBusErr;
    void*           lpMediaCapsInput = NULL;
    void*           lpMediaCapsOutput= NULL;
    int             lDBusArgsSize;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    dbus_error_init(&lDBusErr);

    if (!dbus_message_get_args(apDBusMsg, &lDBusErr, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpMediaCapsInput, &lDBusArgsSize, DBUS_TYPE_INVALID)) {
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to select configuration");
    }

    if (pstlhBtIfce->fpcBNegotiateMedia) {
        if(pstlhBtIfce->fpcBNegotiateMedia(lpMediaCapsInput, &lpMediaCapsOutput, aenBTDeviceType, aenBTMediaType, pstlhBtIfce->pcBNegMediaUserData)) {
            return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to select configuration");
        }
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);
    dbus_message_append_args (lpDBusReply, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpMediaCapsOutput, lDBusArgsSize, DBUS_TYPE_INVALID);

    return lpDBusReply;
}


static DBusMessage*
btrCore_BTMediaEndpointSetConfiguration (
    DBusMessage*    apDBusMsg,
    enBTDeviceType  aenBTDeviceType,
    enBTMediaType   aenBTMediaType,
    void*           apvUserData
) {
    const char*     lpcDevTransportPath = NULL;
    const char*     lpcDevPath = NULL;
    const char*     lpcUuid = NULL;
    unsigned char*  lpui8Config = NULL;
    int             i32ConfSize = 0;

    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterProp;
    DBusMessageIter lDBusMsgIterEntry;
    DBusMessageIter lDBusMsgIterValue;
    DBusMessageIter lDBusMsgIterArr;

    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
    dbus_message_iter_get_basic(&lDBusMsgIter, &lpcDevTransportPath);
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

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &lpcUuid);
        }
        else if (strcasecmp(key, "Device") == 0) {
            if (ldBusType != DBUS_TYPE_OBJECT_PATH)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_get_basic(&lDBusMsgIterValue, &lpcDevPath);
        }
        else if (strcasecmp(key, "Configuration") == 0) {
            if (ldBusType != DBUS_TYPE_ARRAY)
                return dbus_message_new_error(apDBusMsg, "org.bluez.MediaEndpoint1.Error.InvalidArguments", "Unable to set configuration");

            dbus_message_iter_recurse(&lDBusMsgIterValue, &lDBusMsgIterArr);
            dbus_message_iter_get_fixed_array(&lDBusMsgIterArr, &lpui8Config, &i32ConfSize);
        }
        dbus_message_iter_next(&lDBusMsgIterProp);
    }

    BTRCORELOG_INFO ("Set configuration - Transport Path %s\n", lpcDevTransportPath);
    BTRCORELOG_INFO ("Set configuration - Transport Path UUID %s\n", lpcUuid);
    BTRCORELOG_TRACE("Set configuration - Device Path %s\n", lpcDevPath);

    if (pstlhBtIfce->pcDevTransportPath) {
        free(pstlhBtIfce->pcDevTransportPath);
        pstlhBtIfce->pcDevTransportPath = NULL;
    }

    pstlhBtIfce->pcDevTransportPath = strdup(lpcDevTransportPath);

    if (pstlhBtIfce->fpcBTransportPathMedia) {
        if(!pstlhBtIfce->fpcBTransportPathMedia(lpcDevTransportPath, lpcUuid, lpui8Config, aenBTDeviceType, aenBTMediaType, pstlhBtIfce->pcBTransPathMediaUserData)) {
            BTRCORELOG_INFO ("Stored - Transport Path: %s\n", lpcDevTransportPath);
        }
    }

    return dbus_message_new_method_return(apDBusMsg);
}


static DBusMessage*
btrCore_BTMediaEndpointClearConfiguration (
    DBusMessage*    apDBusMsg,
    enBTDeviceType  aenBTDeviceType,
    enBTMediaType   aenBTMediaType,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply;
    DBusError       lDBusErr;
    DBusMessageIter lDBusMsgIter;
    const char*     lDevTransportPath = NULL;

    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    dbus_error_init(&lDBusErr);
    dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
    dbus_message_iter_get_basic(&lDBusMsgIter, &lDevTransportPath);
    BTRCORELOG_DEBUG ("Clear configuration - Transport Path %s\n", lDevTransportPath);

    if (pstlhBtIfce->pcDevTransportPath) {
        free(pstlhBtIfce->pcDevTransportPath);
        pstlhBtIfce->pcDevTransportPath = NULL;
    }

    if (pstlhBtIfce->fpcBTransportPathMedia) {
        if(!pstlhBtIfce->fpcBTransportPathMedia(lDevTransportPath, NULL, NULL, aenBTDeviceType, aenBTMediaType, pstlhBtIfce->pcBTransPathMediaUserData)) {
            BTRCORELOG_INFO ("Cleared - Transport Path %s\n", lDevTransportPath);
        }
    }

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    return lpDBusReply;
}


static int 
btrCore_BTGetMediaIfceProperty (
    DBusConnection*     apDBusConn,
    const char*         apBtObjectPath,
    const char*         apBtInterfacePath,
    const char*         property,
    void*               propertyValue
) {
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusError           lDBusErr;
    DBusMessageIter     lDBusMsgIter;
    DBusMessageIter     lDBusReplyIter;
    DBusMessageIter     element;
    int                 dbus_type = DBUS_TYPE_INVALID;

  
    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtObjectPath,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "Get");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    
    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &apBtInterfacePath);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &property);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(apDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply NULL\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(apDBusConn);

    dbus_message_iter_init(lpDBusReply, &lDBusReplyIter);            // lpDBusMsg is pointer to dbus message received
    if ((dbus_message_iter_get_arg_type (&lDBusReplyIter)) == DBUS_TYPE_INVALID) {
        BTRCORELOG_ERROR ("DBUS_TYPE_INVALID\n");
        dbus_message_unref(lpDBusReply);
        return -1;
    }

    dbus_message_iter_recurse(&lDBusReplyIter, &element);            // pointer to first element of the dbus messge received
    dbus_type = dbus_message_iter_get_arg_type(&element);

    if (DBUS_TYPE_STRING      == dbus_type ||
        DBUS_TYPE_UINT32      == dbus_type ||
        DBUS_TYPE_BOOLEAN     == dbus_type ||
        DBUS_TYPE_OBJECT_PATH == dbus_type ||
        DBUS_TYPE_UINT16      == dbus_type ||
        DBUS_TYPE_UINT64      == dbus_type ||
        DBUS_TYPE_INT16       == dbus_type ||
        DBUS_TYPE_INT32       == dbus_type ||
        DBUS_TYPE_INT64       == dbus_type ||
        DBUS_TYPE_BYTE        == dbus_type ||
        DBUS_TYPE_DOUBLE      == dbus_type) {
          dbus_message_iter_get_basic(&element, propertyValue);
    }

    dbus_message_unref(lpDBusReply);
    return 0;
}

static int
btrCore_BTGattGetCharFlag (
    unsigned short  aflag,
    char            (*aFlagProp)[BT_MAX_UUID_STR_LEN],
    int             nMaxFlags,
    int*            apnFlags
) {
    int index = 0;
    int lIdx = 0;
    
    const char *lGattFlags[BT_MAX_UUID_STR_LEN] = { "read", "write", "encrypt-read", "encrypt-write", "encrypt-authenticated-read", 
                                                    "encrypt-authenticated-write", "secure-read", "secure-write", "notify", "indicate",
                                                    "broadcast", "write-without-response", "authenticated-signed-writes", "reliable-write",
                                                    "writable-auxiliaries"
                                                  };
    
    for (index = 0; index < nMaxFlags; index++) {
        if ((aflag & (1 << index)) != 0) {
            strncpy(aFlagProp[lIdx], lGattFlags[index], (BT_MAX_UUID_STR_LEN-1));
            lIdx += 1;
        }
    }
    
    *apnFlags = lIdx;
    return 0;
}

static DBusMessage*
btrCore_BTRegisterGattService (
    DBusConnection*     apDBusConn,
    DBusMessage*        apDBusMsg,
    stBtIfceHdl*        apstlhBtIfce,
    stBTLeGattService*  apstBTRCoreLeGattService,
    int                 aNumOfGattServices
) {
    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterDict;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;
    const char*     lui8ConnName = NULL;
    char* param = "";

    // read the arguments
    if (!dbus_message_iter_init(apDBusMsg, &lDBusMsgIter)) {
        BTRCORELOG_TRACE("Message has no arguments!\n"); 
    }
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&lDBusMsgIter))  {
        BTRCORELOG_TRACE("Argument is not string!\n"); 
    }
    else {
        dbus_message_iter_get_basic(&lDBusMsgIter, &param);
    }

    BTRCORELOG_TRACE("Method called with %s\n", param);

    lui8ConnName = dbus_bus_get_unique_name(apDBusConn);
    BTRCORELOG_DEBUG("!!! Gatt connection name : %s\n", lui8ConnName);

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    dbus_message_iter_init_append(lpDBusReply, &lDBusMsgIter);
    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_OBJECT_PATH_AS_STRING
                                     DBUS_TYPE_ARRAY_AS_STRING
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_ARRAY_AS_STRING
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);

    for (int serviceIndex = 0; serviceIndex < aNumOfGattServices; serviceIndex++) {
        stBTLeGattService *lpstBTRCoreLeGattService = &apstBTRCoreLeGattService[serviceIndex];

        const char* lpui8SrvGattPath = strdup(lpstBTRCoreLeGattService->servicePath);
        BTRCORELOG_INFO("Inside btrCore_BTRegisterGattService - %s \n", lpui8SrvGattPath);

        /* Register service path */
        dbus_error_init(&lDBusErr);
        lDBusOp = dbus_connection_try_register_object_path(apDBusConn, lpui8SrvGattPath, &gDBusLeGattSCDVTable, apstlhBtIfce, &lDBusErr);
        if (!lDBusOp) {
            btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
            BTRCORELOG_ERROR("Can't Register Le Gatt Object - %s\n", lpui8SrvGattPath);
        }

        {   // Service
            DBusMessageIter lDBusMsgIterDictObjPath, lDBusMsgIterDictArr;

            dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictObjPath);
                dbus_message_iter_append_basic(&lDBusMsgIterDictObjPath, DBUS_TYPE_OBJECT_PATH, &lpui8SrvGattPath);
                dbus_message_iter_open_container(&lDBusMsgIterDictObjPath,
                                                 DBUS_TYPE_ARRAY,
                                                 DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                 DBUS_TYPE_STRING_AS_STRING
                                                 DBUS_TYPE_ARRAY_AS_STRING
                                                 DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                 DBUS_TYPE_STRING_AS_STRING
                                                 DBUS_TYPE_VARIANT_AS_STRING
                                                 DBUS_DICT_ENTRY_END_CHAR_AS_STRING
                                                 DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                 &lDBusMsgIterDictArr);
            {
                DBusMessageIter lDBusMsgIterDictStrIfce, lDBusMsgIterDictArrStr;
                char* lpcIfce = BT_DBUS_BLUEZ_GATT_SERVICE_PATH;

                dbus_message_iter_open_container(&lDBusMsgIterDictArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrIfce);
                    dbus_message_iter_append_basic(&lDBusMsgIterDictStrIfce, DBUS_TYPE_STRING, &lpcIfce);
                    dbus_message_iter_open_container(&lDBusMsgIterDictStrIfce,
                                                     DBUS_TYPE_ARRAY,
                                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                     DBUS_TYPE_STRING_AS_STRING
                                                     DBUS_TYPE_VARIANT_AS_STRING
                                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                     &lDBusMsgIterDictArrStr);
                {
                    DBusMessageIter lDBusMsgIterDictStrUUID;
                    DBusMessageIter lDBusMsgIterValue;
                    char*   key = "UUID";
                    int     type = DBUS_TYPE_STRING;
                    const char* lpServiceUUID = lpstBTRCoreLeGattService->serviceUuid;
                    BTRCORELOG_DEBUG(" Service UUID is %s\n", lpServiceUUID );

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                            dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lpServiceUUID);
                        dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterValue);
                    dbus_message_iter_close_container (&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                }
                {
                    DBusMessageIter lDBusMsgIterDictBoolPri;
                    DBusMessageIter lDBusMsgIterValue;
                    char*   key = "Primary";
                    int     type = DBUS_TYPE_BOOLEAN;
                    dbus_bool_t     lbBtPrimaryGatt = lpstBTRCoreLeGattService->serviceType;

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictBoolPri);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictBoolPri, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictBoolPri, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                            dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lbBtPrimaryGatt);
                        dbus_message_iter_close_container(&lDBusMsgIterDictBoolPri, &lDBusMsgIterValue);
                    dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictBoolPri);
                }
                dbus_message_iter_close_container(&lDBusMsgIterDictStrIfce, &lDBusMsgIterDictArrStr);
                dbus_message_iter_close_container(&lDBusMsgIterDictArr, &lDBusMsgIterDictStrIfce);
            }
            dbus_message_iter_close_container(&lDBusMsgIterDictObjPath, &lDBusMsgIterDictArr);
            dbus_message_iter_close_container(&lDBusMsgIterDict, &lDBusMsgIterDictObjPath);
        }

        // Characteristic
        for (int index = 0; index < lpstBTRCoreLeGattService->ui16NumberOfGattChar; index++) {
            stBTLeGattChar *lpstBTRCoreLeGattChar = &lpstBTRCoreLeGattService->astBTRGattChar[index];
            const char*     lpui8ChrGattPath = strdup(lpstBTRCoreLeGattChar->charPath);

            DBusMessageIter lDBusMsgIterDictObjPath, lDBusMsgIterDictArr;
            BTRCORELOG_DEBUG(" Char path is %s\n", lpui8ChrGattPath);
            /* Register char path */
            dbus_error_init(&lDBusErr);
            lDBusOp = dbus_connection_try_register_object_path(apDBusConn, lpui8ChrGattPath, &gDBusLeGattSCDVTable, apstlhBtIfce, &lDBusErr);
            if (!lDBusOp) {
                btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
                BTRCORELOG_ERROR("Can't Register Le Gatt Object - %s\n", lpui8ChrGattPath);
            }

            dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictObjPath);
                dbus_message_iter_append_basic(&lDBusMsgIterDictObjPath, DBUS_TYPE_OBJECT_PATH, &lpui8ChrGattPath);
                dbus_message_iter_open_container(&lDBusMsgIterDictObjPath,
                                                DBUS_TYPE_ARRAY,
                                                DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                DBUS_TYPE_STRING_AS_STRING
                                                DBUS_TYPE_ARRAY_AS_STRING
                                                DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                DBUS_TYPE_STRING_AS_STRING
                                                DBUS_TYPE_VARIANT_AS_STRING
                                                DBUS_DICT_ENTRY_END_CHAR_AS_STRING
                                                DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                &lDBusMsgIterDictArr);
            {
                DBusMessageIter lDBusMsgIterDictStrIfce, lDBusMsgIterDictArrStr;
                char* lpcIfce = BT_DBUS_BLUEZ_GATT_CHAR_PATH;

                dbus_message_iter_open_container(&lDBusMsgIterDictArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrIfce);
                    dbus_message_iter_append_basic(&lDBusMsgIterDictStrIfce, DBUS_TYPE_STRING, &lpcIfce);
                    dbus_message_iter_open_container(&lDBusMsgIterDictStrIfce,
                                                     DBUS_TYPE_ARRAY,
                                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                     DBUS_TYPE_STRING_AS_STRING
                                                     DBUS_TYPE_VARIANT_AS_STRING
                                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                     &lDBusMsgIterDictArrStr);
                {
                    DBusMessageIter lDBusMsgIterDictStrUUID;
                    DBusMessageIter lDBusMsgIterValue;
                    char*   key = "UUID";
                    int     type = DBUS_TYPE_STRING;
                    const char* lpCharUUID = lpstBTRCoreLeGattChar->charUuid;

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                            dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lpCharUUID);
                        dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterValue);
                    dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                }
                {
                    DBusMessageIter lDBusMsgIterDictStrUUID;
                    DBusMessageIter lDBusMsgIterValue;
                    char*   key = "Service";
                    int     type = DBUS_TYPE_OBJECT_PATH;

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                            dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lpui8SrvGattPath);
                        dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterValue);
                    dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                }
                {
                    DBusMessageIter lDBusMsgIterDictStrUUID;
                    DBusMessageIter lDBusMsgIterVariant;
                    DBusMessageIter lDBusMsgIterSubArray;
                    char*   key = "Flags";
                    int     type = DBUS_TYPE_STRING;
                    int nFlags = 0;
                    char lppui8Props[BT_MAX_NUM_GATT_CHAR_FLAGS][BT_MAX_UUID_STR_LEN] = { {""} };
                    char array_type[5] = "a";

                    strncat (array_type, (char*)&type, sizeof(array_type) - sizeof(type));

                    btrCore_BTGattGetCharFlag(lpstBTRCoreLeGattChar->charFlags, lppui8Props, BT_MAX_NUM_GATT_CHAR_FLAGS, &nFlags);

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                            dbus_message_iter_open_container(&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                        for (int index = 0; index < nFlags; index++) {
                            const char* lpCharFlag = lppui8Props[index];
                            dbus_message_iter_append_basic(&lDBusMsgIterSubArray, type, &lpCharFlag);
                        }
                        dbus_message_iter_close_container (&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
                      dbus_message_iter_close_container (&lDBusMsgIterDictStrUUID, &lDBusMsgIterVariant);
                    dbus_message_iter_close_container (&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                }
                {   /* SM- TODO Check if this is required */
                    DBusMessageIter lDBusMsgIterDictBoolPri;
                    DBusMessageIter lDBusMsgIterValue;
                    char*   key = "Notifying";
                    int     type = DBUS_TYPE_BOOLEAN;
                    dbus_bool_t     lbNotify = FALSE;
                    
                    BTRCORELOG_WARN("Characteristics flag is - %d\n", lpstBTRCoreLeGattChar->charFlags);
                    if ((lpstBTRCoreLeGattChar->charFlags & BT_GATT_CHAR_FLAG_NOTIFY) == BT_GATT_CHAR_FLAG_NOTIFY) {
                        lbNotify = TRUE;
                        BTRCORELOG_WARN("Notification flag is - %d\n", (int)lbNotify);
                    }

                    dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictBoolPri);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictBoolPri, DBUS_TYPE_STRING, &key);
                        dbus_message_iter_open_container(&lDBusMsgIterDictBoolPri, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                            dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lbNotify);
                        dbus_message_iter_close_container(&lDBusMsgIterDictBoolPri, &lDBusMsgIterValue);
                    dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictBoolPri);
                }

                dbus_message_iter_close_container(&lDBusMsgIterDictStrIfce, &lDBusMsgIterDictArrStr);
                dbus_message_iter_close_container(&lDBusMsgIterDictArr, &lDBusMsgIterDictStrIfce);
            }
            dbus_message_iter_close_container(&lDBusMsgIterDictObjPath, &lDBusMsgIterDictArr);
            dbus_message_iter_close_container(&lDBusMsgIterDict, &lDBusMsgIterDictObjPath);

            // Descriptor
            for (int DescIndex = 0; DescIndex < lpstBTRCoreLeGattChar->ui16NumberOfGattDesc; DescIndex++) {
                stBTLeGattDesc *lpstCoreLeGattDesc = &lpstBTRCoreLeGattChar->atBTRGattDesc[DescIndex];
                const char* lpui8DescGattPath = strdup(lpstCoreLeGattDesc->descPath);
                DBusMessageIter lDBusMsgIterDictObjPath, lDBusMsgIterDictArr;
                /* Register char path */
                dbus_error_init(&lDBusErr);
                lDBusOp = dbus_connection_try_register_object_path(apDBusConn, lpui8DescGattPath, &gDBusLeGattSCDVTable, apstlhBtIfce, &lDBusErr);
                if (!lDBusOp) {
                    btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
                    BTRCORELOG_ERROR("Can't Register Le Gatt Object - %s\n", lpui8DescGattPath);
                }

                dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictObjPath);
                    dbus_message_iter_append_basic(&lDBusMsgIterDictObjPath, DBUS_TYPE_OBJECT_PATH, &lpui8DescGattPath);
                    dbus_message_iter_open_container(&lDBusMsgIterDictObjPath,
                                                     DBUS_TYPE_ARRAY,
                                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                     DBUS_TYPE_STRING_AS_STRING
                                                     DBUS_TYPE_ARRAY_AS_STRING
                                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                     DBUS_TYPE_STRING_AS_STRING
                                                     DBUS_TYPE_VARIANT_AS_STRING
                                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING
                                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                     &lDBusMsgIterDictArr);
                {
                    DBusMessageIter lDBusMsgIterDictStrIfce, lDBusMsgIterDictArrStr;
                    char* lpcIfce = BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH;

                    dbus_message_iter_open_container(&lDBusMsgIterDictArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrIfce);
                        dbus_message_iter_append_basic(&lDBusMsgIterDictStrIfce, DBUS_TYPE_STRING, &lpcIfce);
                        dbus_message_iter_open_container(&lDBusMsgIterDictStrIfce,
                                                         DBUS_TYPE_ARRAY,
                                                         DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                         DBUS_TYPE_STRING_AS_STRING
                                                         DBUS_TYPE_VARIANT_AS_STRING
                                                         DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                         &lDBusMsgIterDictArrStr);
                    {
                        DBusMessageIter lDBusMsgIterDictStrUUID;
                        DBusMessageIter lDBusMsgIterValue;
                        char*   key = "UUID";
                        int     type = DBUS_TYPE_STRING;
                        const char* lpDescUUID = lpstCoreLeGattDesc->descUuid;

                        dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                            dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                            dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                                dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lpDescUUID);
                            dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterValue);
                        dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                    }
                    {
                        DBusMessageIter lDBusMsgIterDictStrUUID;
                        DBusMessageIter lDBusMsgIterValue;
                        char*   key = "Characteristic";
                        int     type = DBUS_TYPE_OBJECT_PATH;

                        dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                            dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                            dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                                dbus_message_iter_append_basic(&lDBusMsgIterValue, type, &lpui8ChrGattPath);
                            dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterValue);
                        dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                    }
                    {
                        DBusMessageIter lDBusMsgIterDictStrUUID;
                        DBusMessageIter lDBusMsgIterVariant;
                        DBusMessageIter lDBusMsgIterSubArray;
                        char*   key = "Flags";
                        int     type = DBUS_TYPE_STRING;
                        char lppui8Props[BT_MAX_NUM_GATT_DESC_FLAGS][BT_MAX_UUID_STR_LEN] = { {""} };
                        int nFlags = 0;
                        char array_type[5] = "a";
                        strncat(array_type, (char*)&type, sizeof(array_type) - sizeof(type));

                        btrCore_BTGattGetCharFlag(lpstCoreLeGattDesc->descFlags, lppui8Props, BT_MAX_NUM_GATT_DESC_FLAGS, &nFlags);

                        dbus_message_iter_open_container(&lDBusMsgIterDictArrStr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStrUUID);
                            dbus_message_iter_append_basic(&lDBusMsgIterDictStrUUID, DBUS_TYPE_STRING, &key);
                            dbus_message_iter_open_container(&lDBusMsgIterDictStrUUID, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                            dbus_message_iter_open_container(&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                            for (int index = 0; index < nFlags; index++)
                            {
                                const char* lpCharFlag = lppui8Props[index];
                                dbus_message_iter_append_basic(&lDBusMsgIterSubArray, type, &lpCharFlag);
                            }
                            dbus_message_iter_close_container(&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
                            dbus_message_iter_close_container(&lDBusMsgIterDictStrUUID, &lDBusMsgIterVariant);
                        dbus_message_iter_close_container(&lDBusMsgIterDictArrStr, &lDBusMsgIterDictStrUUID);
                    }


                    dbus_message_iter_close_container(&lDBusMsgIterDictStrIfce, &lDBusMsgIterDictArrStr);
                    dbus_message_iter_close_container(&lDBusMsgIterDictArr, &lDBusMsgIterDictStrIfce);
                }
                dbus_message_iter_close_container(&lDBusMsgIterDictObjPath, &lDBusMsgIterDictArr);
                dbus_message_iter_close_container(&lDBusMsgIterDict, &lDBusMsgIterDictObjPath);

                free((void*)lpui8DescGattPath);
            }
            free((void*)lpui8ChrGattPath);
        }
        free((void*)lpui8SrvGattPath);
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);

    BTRCORELOG_ERROR ("RETURNING - %p\n", lpDBusReply);
    return lpDBusReply;
}


static int
btrCore_BTUnRegisterGattService (
    DBusConnection* apDBusConn,
    const char*     apui8SCDGattPath
) {
    dbus_bool_t     lDBusOp;

    (void)lDBusOp;
    BTRCORELOG_TRACE("Inside btrCore_BTUnRegisterGattService\n");

    return 0;
}

static int
btrCore_BTReleaseLEGattObjPath (
    char*           apstObjPath,
    void*           apvUserData
) {
    dbus_bool_t     lDBusOp;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    lDBusOp = dbus_connection_unregister_object_path(pstlhBtIfce->pDBusConn, apstObjPath);
    if (!lDBusOp) {
        BTRCORELOG_ERROR("Can't unregister object path %s for Gatt!\n", apstObjPath);
        return -1;
    }

    return 0;
}

/* Function to read/write characteristic/descriptor value to/from Bluez */
static DBusMessage*
btrCore_BTLEGattOps (
    DBusMessage*    apDBusMsg,
    stBtIfceHdl*    apstlhBtIfce,
    enBTOpIfceType  aenIfceType,
    enBTLeGattOp    aenGattOp
) {
    DBusMessage*            lpDBusReply = NULL;
    //DBusError             lDBusErr;
    void*                   lpLEGattOutput = NULL;
    DBusMessageIter         lDBusReplyIter, lDBusNextIter;
    DBusMessageIter         element;
    int                     dbus_type = DBUS_TYPE_INVALID;
    char                    propertyValue[BT_MAX_GATT_OP_DATA_LEN] = {'\0'};
    char                    data = '\0';
    int                     index = 0;
    char                    objectPath[BT_MAX_STR_LEN] = "\0";

    const char *path = dbus_message_get_path(apDBusMsg);

    switch (aenGattOp) {
    case enBTLeGattOpWriteValue: {
        BTRCORELOG_INFO("enBTLeGattOpWriteValue\n");
        /* Read the data from the bus */
        dbus_message_iter_init(apDBusMsg, &lDBusReplyIter);            // apDBusMsg is pointer to dbus message received
        if ((dbus_message_iter_get_arg_type(&lDBusReplyIter)) == DBUS_TYPE_INVALID) {
            BTRCORELOG_ERROR("DBUS_TYPE_INVALID\n");
            dbus_message_unref(apDBusMsg);
        }

        dbus_message_iter_recurse(&lDBusReplyIter, &element);            // pointer to first element of the dbus messge received
        /* Get the array of bytes */
        while ((dbus_type = dbus_message_iter_get_arg_type(&element)) != DBUS_TYPE_INVALID) {
            if (dbus_type == DBUS_TYPE_BYTE) {
                dbus_message_iter_get_basic(&element, &data);
                propertyValue[index++] = data;
            }

            if (!dbus_message_iter_next(&element)) {
                break;
            }
        }
        propertyValue[index] = '\0';

        dbus_message_iter_recurse(&element, &lDBusNextIter);
        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&lDBusNextIter)) {
            DBusMessageIter dictEntryIter;
            dbus_message_iter_recurse(&lDBusNextIter, &dictEntryIter);
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                char *dbusObject;

                dbus_message_iter_get_basic(&dictEntryIter, &dbusObject);

                if (0 == strcmp(dbusObject, "device")) {
                    if (true == dbus_message_iter_next(&dictEntryIter)) {
                        DBusMessageIter variantArray;
                        dbus_message_iter_recurse(&dictEntryIter, &variantArray);
                        dbus_type = dbus_message_iter_get_arg_type(&variantArray);
                        if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&variantArray)) {
                            char *path;
                            dbus_message_iter_get_basic(&variantArray, &path);
                            strcpy(objectPath, path);
                            BTRCORELOG_DEBUG("object path is %s\n", objectPath);
                        }
                    }
                }
            }
        }

        stBTDeviceInfo  lstBTDeviceInfo;
        btrCore_BTGetDeviceInfo(apstlhBtIfce->pDBusConn, &lstBTDeviceInfo, objectPath);
        BTRCORELOG_DEBUG("device address is %s\n", lstBTDeviceInfo.pcAddress);
        BTRCORELOG_DEBUG("Received data is %s with length %lu\n", propertyValue, (unsigned long)strlen(propertyValue));

        /* Update data to the LE layer */
        if (apstlhBtIfce->fpcBTLeGattPath) {
            apstlhBtIfce->fpcBTLeGattPath(aenIfceType, aenGattOp, path, lstBTDeviceInfo.pcAddress, enBTDevStPropChanged, propertyValue, apstlhBtIfce->pcBLePathUserData);
        }
    }
        break;
    case enBTLeGattOpReadValue: {
        int lDataLength = 0;
        /* Read the options from the dbus message */
        dbus_message_iter_init(apDBusMsg, &lDBusReplyIter);
        if ((dbus_message_iter_get_arg_type(&lDBusReplyIter)) == DBUS_TYPE_INVALID) {
            BTRCORELOG_ERROR("DBUS_TYPE_INVALID\n");
            dbus_message_unref(apDBusMsg);
        }

        dbus_message_iter_recurse(&lDBusReplyIter, &lDBusNextIter);            // pointer to first element of the dbus messge received
        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&lDBusNextIter)) {
            DBusMessageIter dictEntryIter;
            dbus_message_iter_recurse(&lDBusNextIter, &dictEntryIter);

            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                char *dbusObject;
                dbus_message_iter_get_basic(&dictEntryIter, &dbusObject);

                if (0 == strcmp(dbusObject, "device")) {
                    if (true == dbus_message_iter_next(&dictEntryIter)) {
                        DBusMessageIter variantArray;
                        dbus_message_iter_recurse(&dictEntryIter, &variantArray);

                        if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&variantArray)) {
                            char *path;
                            dbus_message_iter_get_basic(&variantArray, &path);
                            strcpy(objectPath, path);
                            BTRCORELOG_DEBUG("object path is %s\n", objectPath);
                        }
                    }
                }
            }
        }

        stBTDeviceInfo  lstBTDeviceInfo;
        btrCore_BTGetDeviceInfo(apstlhBtIfce->pDBusConn, &lstBTDeviceInfo, objectPath);
        BTRCORELOG_DEBUG("device address is %s\n", lstBTDeviceInfo.pcAddress);

        /* Get data from LE layer */
        if (apstlhBtIfce->fpcBTLeGattPath) {
            apstlhBtIfce->fpcBTLeGattPath(aenIfceType, aenGattOp, path, lstBTDeviceInfo.pcAddress, enBTDevStPropChanged, propertyValue, apstlhBtIfce->pcBLePathUserData);
        }
        BTRCORELOG_INFO("Property value is %s\n", propertyValue);
        lDataLength = strlen(propertyValue);
        lpLEGattOutput = (void*)propertyValue;

        /* Write the data to the bus */
        lpDBusReply = dbus_message_new_method_return(apDBusMsg);
        dbus_message_append_args(lpDBusReply, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpLEGattOutput, lDataLength, DBUS_TYPE_INVALID);
    }
        break;
    default:
        break;
    }

    return lpDBusReply;
}


/* Function to send advertisement data to Bluez */
static int
btrCore_BTRegisterLeAdvGetProp (
    DBusConnection*     apDBusConn,
    DBusMessage*        apDBusMsg,
    stBtIfceHdl*        apstlhBtIfce
) {
    DBusMessage*        lpDBusReply = NULL;
    DBusMessageIter     lDBusMsgIterAdv;
    DBusMessageIter     lDBusMsgIterDictAdv;
    stBTLeCustomAdv*    lpstCustomAdv = &apstlhBtIfce->stBTLeCustAdvt;

    lpDBusReply = dbus_message_new_method_return(apDBusMsg);

    dbus_message_iter_init_append(lpDBusReply, &lDBusMsgIterAdv);
    dbus_message_iter_open_container(&lDBusMsgIterAdv,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDictAdv);
    {
        /* Get advertisement data */
        apstlhBtIfce->fpcBTLeAdvPath(NULL, &lpstCustomAdv, apstlhBtIfce->pcBLeAdvUserData);

        {
            DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
            char*   lpcKey = "Type";
            char*   lui8Value = "";
            int     i32DBusType = DBUS_TYPE_STRING;
            
            /* Get advertisement type */
            lui8Value = lpstCustomAdv->AdvertisementType;
            BTRCORELOG_INFO("Advertisement type is %s\n", lui8Value);

            dbus_message_iter_open_container(&lDBusMsgIterDictAdv, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                dbus_message_iter_append_basic(&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
                dbus_message_iter_open_container(&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                    dbus_message_iter_append_basic(&lDBusMsgIterVariant, i32DBusType, &lui8Value);
                dbus_message_iter_close_container(&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
            dbus_message_iter_close_container(&lDBusMsgIterDictAdv, &lDBusMsgIterDictStr);
        }

        {
            if (lpstCustomAdv->numServiceUUID > 0) {
                DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant, lDBusMsgIterSubArray;
                char*   key = "ServiceUUIDs";
                int     type = DBUS_TYPE_STRING;

                char array_type[5] = "a";
                strncat(array_type, (char*)&type, sizeof(array_type) - sizeof(type));

                dbus_message_iter_open_container(&lDBusMsgIterDictAdv, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                    dbus_message_iter_append_basic(&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &key);
                    dbus_message_iter_open_container(&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                    dbus_message_iter_open_container(&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                    for (int index = 0; index < lpstCustomAdv->numServiceUUID; index++) {
                        const char *lppui8Props = lpstCustomAdv->ServiceUUID[index];
                        BTRCORELOG_INFO("Service UUID is %s\n", lpstCustomAdv->ServiceUUID[index]);
                        dbus_message_iter_append_basic(&lDBusMsgIterSubArray, type, &lppui8Props);
                    }
                    dbus_message_iter_close_container(&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
                    dbus_message_iter_close_container(&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
                dbus_message_iter_close_container(&lDBusMsgIterDictAdv, &lDBusMsgIterDictStr);
            }
        }

        {
            if (lpstCustomAdv->numSolicitUUID > 0) {
                DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant, lDBusMsgIterSubArray;
                char*   key = "SolicitUUIDs";
                int     type = DBUS_TYPE_STRING;

                char array_type[5] = "a";
                strncat(array_type, (char*)&type, sizeof(array_type) - sizeof(type));

                dbus_message_iter_open_container(&lDBusMsgIterDictAdv, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                    dbus_message_iter_append_basic(&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &key);
                    dbus_message_iter_open_container(&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                    dbus_message_iter_open_container(&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                    for (int index = 0; index < lpstCustomAdv->numSolicitUUID; index++) {
                        const char *lppui8Props = lpstCustomAdv->SolicitUUID[index];
                        BTRCORELOG_INFO("Solicit UUID is %s\n", lpstCustomAdv->SolicitUUID[index]);
                        dbus_message_iter_append_basic(&lDBusMsgIterSubArray, type, &lppui8Props);
                    }
                    dbus_message_iter_close_container(&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
                    dbus_message_iter_close_container(&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
                dbus_message_iter_close_container(&lDBusMsgIterDictAdv, &lDBusMsgIterDictStr);
            }
        }

        {
            DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterArrStr, lDBusMsgIterDictVar, lDBusMsgIterArrVar, lDBusMsgIterVarDict, lDBusMsgIterSubArrStr;
            char*   key = "ManufacturerData";
            int     lManfId = lpstCustomAdv->ManfData.ManfID;
            int     type = DBUS_TYPE_BYTE;

            dbus_message_iter_open_container(&lDBusMsgIterDictAdv, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                dbus_message_iter_append_basic(&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &key);
                dbus_message_iter_open_container(&lDBusMsgIterDictStr,
                                                 DBUS_TYPE_VARIANT,
                                                 DBUS_TYPE_ARRAY_AS_STRING
                                                 DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                 DBUS_TYPE_UINT16_AS_STRING
                                                 DBUS_TYPE_VARIANT_AS_STRING
                                                 DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                                 &lDBusMsgIterDictVar);
                dbus_message_iter_open_container(&lDBusMsgIterDictVar,
                                                 DBUS_TYPE_ARRAY,
                                                 DBUS_TYPE_UINT16_AS_STRING
                                                 DBUS_TYPE_VARIANT_AS_STRING,
                                                 &lDBusMsgIterArrVar);
                dbus_message_iter_open_container(&lDBusMsgIterArrVar, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterVarDict);
                    dbus_message_iter_append_basic(&lDBusMsgIterVarDict, DBUS_TYPE_UINT16, &lManfId);
                    dbus_message_iter_open_container(&lDBusMsgIterVarDict,
                                                     DBUS_TYPE_VARIANT,
                                                     DBUS_TYPE_ARRAY_AS_STRING
                                                     DBUS_TYPE_BYTE_AS_STRING,
                                                     &lDBusMsgIterArrStr);
                    dbus_message_iter_open_container(&lDBusMsgIterArrStr, DBUS_TYPE_ARRAY, (char*)&type, &lDBusMsgIterSubArrStr);

                    for (int index = 0; index < lpstCustomAdv->ManfData.lenManfData; index++) {
                        dbus_message_iter_append_basic(&lDBusMsgIterSubArrStr, DBUS_TYPE_BYTE, &lpstCustomAdv->ManfData.data[index]);
                    }
                    dbus_message_iter_close_container(&lDBusMsgIterArrStr, &lDBusMsgIterSubArrStr);
                    dbus_message_iter_close_container(&lDBusMsgIterVarDict, &lDBusMsgIterArrStr);
                dbus_message_iter_close_container(&lDBusMsgIterArrVar, &lDBusMsgIterVarDict);
                dbus_message_iter_close_container(&lDBusMsgIterDictVar, &lDBusMsgIterArrVar);
                dbus_message_iter_close_container(&lDBusMsgIterDictStr, &lDBusMsgIterDictVar);
            dbus_message_iter_close_container(&lDBusMsgIterDictAdv, &lDBusMsgIterDictStr);
        }
        
        {
            DBusMessageIter lDBusMsgIterDictBoolPri;
            DBusMessageIter lDBusMsgIterValue;
            char*   key = "IncludeTxPower";
            int     type = DBUS_TYPE_BOOLEAN;
            dbus_bool_t lbTxPower = lpstCustomAdv->bTxPower;

            dbus_message_iter_open_container(&lDBusMsgIterDictAdv, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictBoolPri);
                dbus_message_iter_append_basic (&lDBusMsgIterDictBoolPri, DBUS_TYPE_STRING, &key);
                dbus_message_iter_open_container (&lDBusMsgIterDictBoolPri, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterValue);
                    dbus_message_iter_append_basic (&lDBusMsgIterValue, type, &lbTxPower);
                dbus_message_iter_close_container (&lDBusMsgIterDictBoolPri, &lDBusMsgIterValue);
            dbus_message_iter_close_container (&lDBusMsgIterDictAdv, &lDBusMsgIterDictBoolPri);
        }
    }

    dbus_message_iter_close_container (&lDBusMsgIterAdv, &lDBusMsgIterDictAdv);

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);

    return 0;
}



/* Interfaces */
void*
BtrCore_BTInitGetConnection (
    void
) {
    DBusError       lDBusErr;
    DBusConnection* lpDBusConn = NULL;
    stBtIfceHdl*    pstlhBtIfce= NULL;

    dbus_error_init(&lDBusErr);
    lpDBusConn = dbus_bus_get(DBUS_BUS_SYSTEM, &lDBusErr);

    if (lpDBusConn == NULL) {
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return NULL;
    }


    pstlhBtIfce = (stBtIfceHdl*)malloc(sizeof(stBtIfceHdl));
    if (!pstlhBtIfce)
        return NULL;


    pstlhBtIfce->pDBusConn                      = NULL;

    pstlhBtIfce->pcBTAgentPath                  = NULL;
    pstlhBtIfce->pcBTDAdapterPath               = NULL;
    pstlhBtIfce->pcBTDAdapterAddr               = NULL;
    pstlhBtIfce->pcBTAdapterPath                = NULL;
    pstlhBtIfce->pcDevTransportPath             = NULL;

    pstlhBtIfce->pcBTOutPassCode                = NULL;

    pstlhBtIfce->i32DoReject                    = 1;

    pstlhBtIfce->ui32cBConnAuthPassKey          = 0;
    pstlhBtIfce->ui32DevLost                    = 0;

    pstlhBtIfce->ui32IsAdapterDiscovering       = 0;

    memset(pstlhBtIfce->pcBTVersion,        '\0', sizeof(char) * BT_MAX_STR_LEN);
    memset(pstlhBtIfce->pcDeviceCurrState,  '\0', sizeof(char) * BT_MAX_STR_LEN);
    memset(pstlhBtIfce->pcLeDeviceCurrState,'\0', sizeof(char) * BT_MAX_STR_LEN);
    memset(pstlhBtIfce->pcLeDeviceAddress,  '\0', sizeof(char) * BT_MAX_STR_LEN);
    memset(pstlhBtIfce->pcMediaCurrState,   '\0', sizeof(char) * BT_MAX_STR_LEN);

    memset(&pstlhBtIfce->lstBTAdapterInfo, 0, sizeof(stBTAdapterInfo));
    memset(&pstlhBtIfce->lstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
    memset(&pstlhBtIfce->lstBTMediaInfo, 0, sizeof(stBTMediaInfo));

    memset(&pstlhBtIfce->stBTLeGatt, 0, sizeof(stBTLeGattInfo));
    memset(&pstlhBtIfce->stBTLeCustAdvt, 0, sizeof(stBTLeCustomAdv));
    memset(pstlhBtIfce->pcBTAdapterGattSrvEpPath, '\0', sizeof(char) * BT_MAX_DEV_PATH_LEN);

    strncpy(pstlhBtIfce->pcDeviceCurrState,   "disconnected", BT_MAX_STR_LEN - 1);
    strncpy(pstlhBtIfce->pcLeDeviceCurrState, "disconnected", BT_MAX_STR_LEN - 1);
    strncpy(pstlhBtIfce->pcLeDeviceAddress,   "none",         BT_MAX_STR_LEN - 1);
    strncpy(pstlhBtIfce->pcMediaCurrState,    "none",         BT_MAX_STR_LEN - 1); 

    pstlhBtIfce->pcBAdapterStatusUserData       = NULL;
    pstlhBtIfce->pcBDevStatusUserData           = NULL;
    pstlhBtIfce->pcBMediaStatusUserData         = NULL;
    pstlhBtIfce->pcBNegMediaUserData            = NULL;
    pstlhBtIfce->pcBTransPathMediaUserData      = NULL;
    pstlhBtIfce->pcBMediaPlayerPathUserData     = NULL;
    pstlhBtIfce->pcBConnIntimUserData           = NULL;
    pstlhBtIfce->pcBConnAuthUserData            = NULL;
    pstlhBtIfce->pcBLePathUserData              = NULL;

    pstlhBtIfce->fpcBAdapterStatusUpdate        = NULL;
    pstlhBtIfce->fpcBDevStatusUpdate            = NULL;
    pstlhBtIfce->fpcBMediaStatusUpdate          = NULL;
    pstlhBtIfce->fpcBNegotiateMedia             = NULL;
    pstlhBtIfce->fpcBTransportPathMedia         = NULL;
    pstlhBtIfce->fpcBTMediaPlayerPath           = NULL;
    pstlhBtIfce->fpcBConnectionIntimation       = NULL;
    pstlhBtIfce->fpcBConnectionAuthentication   = NULL;
    pstlhBtIfce->fpcBTLeGattPath                = NULL;


    BTRCORELOG_INFO ("DBus Debug DBus Connection Name %s\n", dbus_bus_get_unique_name (lpDBusConn));
    pstlhBtIfce->pDBusConn = lpDBusConn;


    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_OBJECT_MANAGER "',member='InterfacesAdded'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_OBJECT_MANAGER "',member='InterfacesRemoved'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_ADAPTER_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_DEVICE_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_CTRL_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_ITEM_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_LE_ADV_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_SERVICE_PATH"'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_CHAR_PATH "'", NULL);
    dbus_bus_add_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH "'", NULL);
 

    if (!dbus_connection_add_filter(pstlhBtIfce->pDBusConn, btrCore_BTDBusConnectionFilterCb, pstlhBtIfce, NULL)) {
        BTRCORELOG_ERROR ("Can't add signal filter - BtrCore_BTInitGetConnection\n");
        BtrCore_BTDeInitReleaseConnection((void*)pstlhBtIfce);
        return NULL;
    }

    return (void*)pstlhBtIfce;
}


int
BtrCore_BTDeInitReleaseConnection (
    void* apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl)
        return -1;

    dbus_connection_remove_filter(pstlhBtIfce->pDBusConn, btrCore_BTDBusConnectionFilterCb, pstlhBtIfce);

    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_CHAR_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_GATT_SERVICE_PATH"'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_LE_ADV_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_ITEM_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_CTRL_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_DEVICE_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_PROPERTIES "',member='PropertiesChanged'"",arg0='" BT_DBUS_BLUEZ_ADAPTER_PATH "'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_OBJECT_MANAGER "',member='InterfacesRemoved'", NULL);
    dbus_bus_remove_match(pstlhBtIfce->pDBusConn, "type='signal',sender='" BT_DBUS_BLUEZ_PATH "',interface='" DBUS_INTERFACE_OBJECT_MANAGER "',member='InterfacesAdded'", NULL);

    if (pstlhBtIfce->pcBTAgentPath) {
        free(pstlhBtIfce->pcBTAgentPath);
        pstlhBtIfce->pcBTAgentPath = NULL;
    }

    if (pstlhBtIfce->pcBTDAdapterAddr) {
        free(pstlhBtIfce->pcBTDAdapterAddr);
        pstlhBtIfce->pcBTDAdapterAddr = NULL;
    }

    if (pstlhBtIfce->pcBTDAdapterPath) {
        free(pstlhBtIfce->pcBTDAdapterPath);
        pstlhBtIfce->pcBTDAdapterPath = NULL;
    }

    if (pstlhBtIfce->pcBTAdapterPath) {
        free(pstlhBtIfce->pcBTAdapterPath);
        pstlhBtIfce->pcBTAdapterPath = NULL;
    }


    pstlhBtIfce->pcBLePathUserData              = NULL;
    pstlhBtIfce->pcBConnAuthUserData            = NULL;
    pstlhBtIfce->pcBConnIntimUserData           = NULL;
    pstlhBtIfce->pcBMediaPlayerPathUserData     = NULL;
    pstlhBtIfce->pcBTransPathMediaUserData      = NULL;
    pstlhBtIfce->pcBNegMediaUserData            = NULL;
    pstlhBtIfce->pcBMediaStatusUserData         = NULL;
    pstlhBtIfce->pcBDevStatusUserData           = NULL;
    pstlhBtIfce->pcBAdapterStatusUserData       = NULL;


    pstlhBtIfce->fpcBTLeGattPath                = NULL;
    pstlhBtIfce->fpcBConnectionAuthentication   = NULL;
    pstlhBtIfce->fpcBConnectionIntimation       = NULL;
    pstlhBtIfce->fpcBTMediaPlayerPath           = NULL;
    pstlhBtIfce->fpcBTransportPathMedia         = NULL;
    pstlhBtIfce->fpcBNegotiateMedia             = NULL;
    pstlhBtIfce->fpcBMediaStatusUpdate          = NULL;
    pstlhBtIfce->fpcBDevStatusUpdate            = NULL;
    pstlhBtIfce->fpcBAdapterStatusUpdate        = NULL;


    pstlhBtIfce->ui32IsAdapterDiscovering       = 0;
    pstlhBtIfce->ui32DevLost                    = 0;
    pstlhBtIfce->ui32cBConnAuthPassKey          = 0;
    pstlhBtIfce->i32DoReject                    = 0;


    pstlhBtIfce->pDBusConn = NULL;

    free(pstlhBtIfce);
    pstlhBtIfce = NULL;


    return 0;
}


char*
BtrCore_BTGetAgentPath (
    void* apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    char            lDefaultBTPath[BT_MAX_DEV_PATH_LEN] = {'\0'};

    if (!apstBtIfceHdl)
        return NULL;


    snprintf(lDefaultBTPath, sizeof(lDefaultBTPath), "/org/bluez/agent_%d", getpid());

    if (pstlhBtIfce->pcBTAgentPath) {
        free(pstlhBtIfce->pcBTAgentPath);
        pstlhBtIfce->pcBTAgentPath = NULL;
    }

    pstlhBtIfce->pcBTAgentPath = strdup(lDefaultBTPath);
    BTRCORELOG_INFO ("Agent Path: %s\n", pstlhBtIfce->pcBTAgentPath);
    return pstlhBtIfce->pcBTAgentPath;
}


int
BtrCore_BTReleaseAgentPath (
    void* apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl)
        return -1;


    if (pstlhBtIfce->pcBTAgentPath) {
        free(pstlhBtIfce->pcBTAgentPath);
        pstlhBtIfce->pcBTAgentPath = NULL;
    }

    return 0;
}


int
BtrCore_BTRegisterAgent (
    void*       apstBtIfceHdl,
    const char* apBtAdapter,
    const char* apBtAgentPath,
    const char* capabilities
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl)
        return -1;

    dbus_error_init(&lDBusErr);
    lDBusOp = dbus_connection_try_register_object_path(pstlhBtIfce->pDBusConn, apBtAgentPath, &gDBusAgentVTable, pstlhBtIfce, &lDBusErr);
    if (!lDBusOp) {
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        BTRCORELOG_ERROR ("Error registering object path for agent - %s\n", apBtAgentPath);

        if (strcmp(lDBusErr.name, DBUS_ERROR_OBJECT_PATH_IN_USE) != 0)
            return -1;
    }

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             "/org/bluez",
                                             BT_DBUS_BLUEZ_AGENT_MGR_PATH,
                                             "RegisterAgent");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Error allocating new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_STRING, &capabilities, DBUS_TYPE_INVALID);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Unable to register agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             "/org/bluez",
                                             BT_DBUS_BLUEZ_AGENT_MGR_PATH,
                                             "RequestDefaultAgent");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't unregister agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;//this was an error case
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTUnregisterAgent (
    void*       apstBtIfceHdl,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             "/org/bluez",
                                             BT_DBUS_BLUEZ_AGENT_MGR_PATH,
                                             "UnregisterAgent");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtAgentPath, DBUS_TYPE_INVALID);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Can't unregister agent\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;//this was an error case
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    if (!dbus_connection_unregister_object_path(pstlhBtIfce->pDBusConn, apBtAgentPath)) {
        BTRCORELOG_ERROR ("Error unregistering object path for agent\n");
        return -1;
    }

    /* UnregisterAgent in bluez5.54 internal it is setting Pairable false.
     * due to this pairing is failing, to avoid this implicitly setting
     * Pairable true here */
    if (!strncmp(pstlhBtIfce->pcBTVersion, BT_BLUEZ_VERSION_5_54, strlen(BT_BLUEZ_VERSION_5_54)) || !strncmp(pstlhBtIfce->pcBTVersion, BT_BLUEZ_VERSION_5_48, strlen(BT_BLUEZ_VERSION_5_48)) ) 
    {
        unBTOpIfceProp  lunBtOpAdapProp;
        int             isPairable = 1;
        lunBtOpAdapProp.enBtAdapterProp = enBTAdPropPairable;
        if (BtrCore_BTSetProp(apstBtIfceHdl, apBtAdapter, enBTAdapter, lunBtOpAdapProp, &isPairable)) {
	    BTRCORELOG_ERROR ("Set Adapter Property enBTAdPropPairable - FAILED\n");
	    return -1;
        }
    }

    return 0;
}


int
BtrCore_BTGetAdapterList (
    void*           apstBtIfceHdl,
    unsigned int*   apBtNumAdapters,
    char*           apcArrBtAdapterPath[BT_MAX_NUM_ADAPTERS]
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    int             c;
    int             rc = -1;
    int             a = 0;
    int             b = 0;
    int             d = 0;
    int             num = -1;
    int             index = 0;
    char            paths[BT_MAX_NUM_ADAPTERS][BT_MAX_DEV_PATH_LEN] = {'\0'};
    //char          **paths2 = NULL;

    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter rootIter;
    bool            adapterFound = FALSE;
    char*           adapter_path;
    char*           dbusObject2;
    char            objectPath[BT_MAX_DEV_PATH_LEN] = {'\0'};
    char            objectData[BT_MAX_DEV_PATH_LEN] = {'\0'};

    if (!apstBtIfceHdl)
        return -1;


    lpDBusReply = btrCore_BTSendMethodCall(pstlhBtIfce->pDBusConn, "/", DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects");
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("org.bluez.Manager.ListAdapters returned an error\n");
        return rc;
    }


    for (index = 0; index < BT_MAX_NUM_ADAPTERS; index++)
        memset (&paths[index], '\0',  BT_MAX_DEV_PATH_LEN);

    memset(objectPath, '\0', BT_MAX_DEV_PATH_LEN);
    memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);


    if (dbus_message_iter_init(lpDBusReply, &rootIter) &&               //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) { //get the type of message that iter points to

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array

        while (!adapterFound) {
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strncpy(objectPath, adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                    ++a;
                }

                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter) && (d < BT_MAX_NUM_ADAPTERS)) {
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter)) {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);

                                ////// getting all bluetooth adapters object paths //////

                                if (strcmp(dbusObject, BT_DBUS_BLUEZ_ADAPTER_PATH) == 0) {
                                    strncpy(&paths[d][0], adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                                    //strcpy(paths2+d,adapter_path);
                                    //paths2[d] = strdup(adapter_path);
                                    //BTRCORELOG_ERROR ("\n\n test");
                                    //(paths2+2) = strdup(adapter_path);
                                    ++d;
                                }
                            }

                            /////// NEW //////////
                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)) {

                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2)) {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of

                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2)) {
                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }

                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);
                                            strncpy(objectData, dbusObject3, (strlen(dbusObject3) < BT_MAX_DEV_PATH_LEN) ? strlen(dbusObject3) : BT_MAX_DEV_PATH_LEN - 1);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                        }
                                    }

                                    if (!dbus_message_iter_has_next(&innerArrayIter2)) {
                                        break; //check to see if end of 3rd array
                                    }
                                    else {
                                        dbus_message_iter_next(&innerArrayIter2);
                                    }
                                }
                            }
                        }

                        if (!dbus_message_iter_has_next(&innerArrayIter)) {
                            break; //check to see if end of 2nd array
                        }
                        else {
                            dbus_message_iter_next(&innerArrayIter);
                        }
                    }
                }

                if (!dbus_message_iter_has_next(&arrayElementIter)) {
                    break; //check to see if end of 1st array
                }
                else {
                    dbus_message_iter_next(&arrayElementIter);
                }
            } //while loop end --used to traverse array
        }
    }

    num = d;
    if (apBtNumAdapters && apcArrBtAdapterPath) {
        *apBtNumAdapters = num;

        for (c = 0; c < num; c++) {
            if (apcArrBtAdapterPath[c]) {
                if (*(apcArrBtAdapterPath[c]) == '\0') {
                    BTRCORELOG_DEBUG ("Adapter Path %d is: %s\n", c, &paths[c][0]);
                    strncpy(apcArrBtAdapterPath[c], &paths[c][0], (strlen(&paths[c][0]) < BT_MAX_DEV_PATH_LEN) ? strlen(&paths[c][0]) : BT_MAX_DEV_PATH_LEN - 1);
                    rc = 0;
                }
                else {
                    BTRCORELOG_DEBUG ("Adapter Path %d is: %s\n", c, &paths[c][0]);
                    BTRCORELOG_DEBUG ("Existing Adapter Path is: %s\n", apcArrBtAdapterPath[c]);
                    rc = 0;
                }
            }
        }
    }

    dbus_message_unref(lpDBusReply);

    return rc;
}


char*
BtrCore_BTGetAdapterPath (
    void*       apstBtIfceHdl,
    const char* apBtAdapter
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    char*           pui8DefAdapterPath = NULL;
    char*           pui8AdapterPath1 = "/org/bluez/hci0";
    char*           pui8AdapterPath2 = "/org/bluez/hci1";
    char*           pui8AdapterPath3 = "/org/bluez/hci2";
    char*           bt1 = "hci0";
    char*           bt2 = "hci1";
    char*           bt3 = "hci2";

    if (!apstBtIfceHdl)
        return NULL;


    if (!apBtAdapter) {
        if (!pstlhBtIfce->pcBTAdapterPath && ((pui8DefAdapterPath = btrCore_BTGetDefaultAdapterPath(pstlhBtIfce)) != NULL)) {
            BTRCORELOG_WARN ("Got Default Adapter Path is : %s - %p\n", pui8DefAdapterPath, pui8DefAdapterPath);
            pstlhBtIfce->pcBTAdapterPath = strndup(pui8DefAdapterPath, strlen(pui8DefAdapterPath));
        }
    }
    else if (strcmp(apBtAdapter, bt1) == 0) {
        if (!pstlhBtIfce->pcBTAdapterPath) {
            pstlhBtIfce->pcBTAdapterPath = strndup(pui8AdapterPath1, strlen(pui8AdapterPath1));
        }
    }
    else if (strcmp(apBtAdapter, bt2) == 0) {
        if (!pstlhBtIfce->pcBTAdapterPath) {
            pstlhBtIfce->pcBTAdapterPath = strndup(pui8AdapterPath2, strlen(pui8AdapterPath2));
        }
    }
    else if (strcmp(apBtAdapter, bt3) == 0) {
        if (!pstlhBtIfce->pcBTAdapterPath) {
            pstlhBtIfce->pcBTAdapterPath = strndup(pui8AdapterPath3, strlen(pui8AdapterPath3));
        }
    }
    else {
        if (!pstlhBtIfce->pcBTAdapterPath && ((pui8DefAdapterPath = btrCore_BTGetDefaultAdapterPath(pstlhBtIfce)) != NULL)) {
            pstlhBtIfce->pcBTAdapterPath = strndup(pui8DefAdapterPath, strlen(pui8DefAdapterPath));
        }
    }


    BTRCORELOG_WARN ("Adapter Path is : %s - %p\n", pstlhBtIfce->pcBTAdapterPath, pstlhBtIfce->pcBTAdapterPath);
    return pstlhBtIfce->pcBTAdapterPath;
}


int
BtrCore_BTReleaseAdapterPath (
    void*       apstBtIfceHdl,
    const char* apBtAdapter
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl)
        return -1;


    if (!apBtAdapter) {
        btrCore_BTReleaseDefaultAdapterPath(pstlhBtIfce);
    }

    if (pstlhBtIfce->pcBTAdapterPath) {

        BTRCORELOG_WARN ("Adapter Path is : %s - %p\n", pstlhBtIfce->pcBTAdapterPath, pstlhBtIfce->pcBTAdapterPath);
        if (pstlhBtIfce->pcBTAdapterPath != apBtAdapter) {
            BTRCORELOG_ERROR ("ERROR: Looks like Adapter path has been changed by User\n");
        }

        free(pstlhBtIfce->pcBTAdapterPath);
        pstlhBtIfce->pcBTAdapterPath = NULL;
    }

    BTRCORELOG_WARN ("Freed Adapter\n");

    return 0;
}


int
BtrCore_BTGetIfceNameVersion (
    void* apstBtIfceHdl,
    char* apBtOutIfceName,
    char* apBtOutVersion
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    FILE*           lfpVersion = NULL;
    char            lcpVersion[BT_MAX_STR_LEN] = {'\0'};

    if (!apstBtIfceHdl || !apBtOutIfceName || !apBtOutVersion)
        return -1;


    (void)pstlhBtIfce;

    /* strncpy() throws format truncation error on gcc 9.x when same length of src string
       is given in 3rd argument. Also, strncpy() will fill null terminating characters if
       3rd argument length is more than the length of src string
    */
    lfpVersion = popen("/usr/lib/bluez5/bluetooth/bluetoothd --version", "r");
    if ((lfpVersion == NULL)) {
        BTRCORELOG_ERROR ("Failed to run Version command\n");
        strncpy(lcpVersion, "5.XXX", strlen("5.XXX") + 1);
    }
    else {
        do {
            if (fgets(lcpVersion, sizeof(lcpVersion)-1, lfpVersion) == NULL) {
                BTRCORELOG_ERROR ("Failed to Valid Version\n");
                strncpy(lcpVersion, "5.XXX", sizeof(lcpVersion));
                lcpVersion[sizeof(lcpVersion) - 1] = '\0';
            }
        } while (strstr(lcpVersion, "breakpad") || strstr(lcpVersion, "Breakpad"));

        pclose(lfpVersion);
    }


    strncpy(apBtOutIfceName, "Bluez", strlen("Bluez") + 1);
    strncpy(apBtOutVersion, lcpVersion, strlen(lcpVersion) + 1);
    strncpy(pstlhBtIfce->pcBTVersion, lcpVersion, strlen(lcpVersion));

    BTRCORELOG_WARN ("Bluez Version - %s\n", apBtOutVersion);

    return 0;
}


int
BtrCore_BTGetProp (
    void*               apstBtIfceHdl,
    const char*         apcBtOpIfcePath,
    enBTOpIfceType      aenBtOpIfceType,
    unBTOpIfceProp      aunBtOpIfceProp,
    void*               apvVal
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    int                 rc = 0;

    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusPendingCall*    lpDBusPendC = NULL;
    DBusMessageIter     arg_i;
    DBusMessageIter     element_i;
    DBusMessageIter     variant_i;
    DBusError           lDBusErr;

    const char*         pParsedKey = NULL;
    const char*         pParsedValueString = NULL;
    int                 parsedValueNumber = 0;
    unsigned int        parsedValueUnsignedNumber = 0;
    unsigned short      parsedValueUnsignedShort = 0;

    const char*         lDBusKey = NULL;
    int                 lDBusType = DBUS_TYPE_INVALID;
    int                 i32StoreDefAdpAddr = 0;

    const char*         pInterface            = NULL;
    const char*         pAdapterInterface     = BT_DBUS_BLUEZ_ADAPTER_PATH;
    const char*         pDeviceInterface      = BT_DBUS_BLUEZ_DEVICE_PATH;
    const char*         pMediaTransInterface  = BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH;
    const char*         pGattServiceInterface = BT_DBUS_BLUEZ_GATT_SERVICE_PATH;
    const char*         pGattCharInterface    = BT_DBUS_BLUEZ_GATT_CHAR_PATH;
    const char*         pGattDescInterface    = BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH;

    if (!apstBtIfceHdl)
        return -1;


    if ((!apcBtOpIfcePath) || (!apvVal)) {
        BTRCORELOG_ERROR ("enBTRCoreInvalidArg - enBTRCoreInitFailure\n");
        return -1;
    }


    switch (aenBtOpIfceType) {
    case enBTAdapter:
        pInterface = pAdapterInterface;
        switch (aunBtOpIfceProp.enBtAdapterProp) {
        case enBTAdPropName:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "Alias";
            break;
        case enBTAdPropAddress:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "Address";
            if (!strncmp(apcBtOpIfcePath, pstlhBtIfce->pcBTDAdapterPath, strlen(pstlhBtIfce->pcBTDAdapterPath))) {
                i32StoreDefAdpAddr = 1;
            }
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
        case enBTAdPropPairable:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Pairable";
            break;
        case enBTAdPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid Adapter Property\n");
            return -1;
        }
        break;
    case enBTDevice:
        pInterface = pDeviceInterface;
        switch (aunBtOpIfceProp.enBtDeviceProp) {
        case enBTDevPropPaired:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Paired";
            break;
        case enBTDevPropConnected:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Connected";
            break;
        case enBTDevPropVendor:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Vendor";
            break;
        case enBTDevPropSrvRslvd:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "ServicesResolved";
            break;
        case enBTDevPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid Device Property\n");
            return -1;
        }
        break;
    case enBTMediaTransport:
        pInterface = pMediaTransInterface;
        switch (aunBtOpIfceProp.enBtMediaTransportProp) {
        case enBTMedTPropDelay:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Delay";
            break;
        case enBTMedTPropVol:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Volume";
            break;
        case enBTMedTPropState:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "State";
            break;
        case enBTMedTPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid MediaTransport Property\n");
            return -1;
        }
        break;
    case enBTGattService:
        pInterface = pGattServiceInterface;
        switch (aunBtOpIfceProp.enBtGattServiceProp) {
        case enBTGattSPropUUID:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "UUID";
            break;
        case enBTGattSPropPrimary:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Primary";
            break;
        case enBTGattSPropDevice:
            lDBusType = DBUS_TYPE_OBJECT_PATH;
            lDBusKey  = "Device";
            break;
        case enBTGattSPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid GattService Property\n");
            return -1;
        }
        break;
    case enBTGattCharacteristic:
        pInterface = pGattCharInterface;
        switch (aunBtOpIfceProp.enBtGattCharProp) {
        case enBTGattCPropUUID:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "UUID";
            break;
        case enBTGattCPropService:
            lDBusType = DBUS_TYPE_OBJECT_PATH;
            lDBusKey  = "Service";
            break;
        case enBTGattCPropValue:
            lDBusType = DBUS_TYPE_ARRAY;
            lDBusKey  = "Value";
            break;
        case enBTGattCPropNotifying:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Notifying";
            break;
        case enBTGattCPropFlags:
            lDBusType = DBUS_TYPE_ARRAY;
            lDBusKey  = "Flags";
            break;
        case enBTGattCPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid GattCharacteristic Property\n");
            return -1;
        }
        break;               
    case enBTGattDescriptor:
        pInterface = pGattDescInterface;
        switch (aunBtOpIfceProp.enBtGattDescProp) {
        case enBTGattDPropUUID:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "UUID";
            break;
        case enBTGattDPropCharacteristic:
            lDBusType = DBUS_TYPE_OBJECT_PATH;
            lDBusKey  = "Characteristic";
            break;
        case enBTGattDPropValue:
            lDBusType = DBUS_TYPE_ARRAY;
            lDBusKey  = "Value";
            break;
        case enBTGattDPropFlags:
            lDBusType = DBUS_TYPE_ARRAY;
            lDBusKey  = "Flags";
            break;
        case enBTGattDPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid GattDescriptor Property\n");
            return -1;
        }
        break;
    case enBTUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid Operational Interface\n");
        return -1;
    }


    if (!lDBusKey || (lDBusType == DBUS_TYPE_INVALID)) {
        BTRCORELOG_ERROR ("Invalid Interface Property\n");
        return -1;
    }
    

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcBtOpIfcePath,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "GetAll");

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message\n");
    }


    dbus_connection_flush(pstlhBtIfce->pDBusConn);


    if (!lpDBusPendC) {
        BTRCORELOG_ERROR ("%s.GetProperties returned an error\n", pInterface);
        rc = -1;
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        dbus_message_unref(lpDBusMsg);
        lpDBusMsg = NULL;
    }
    else {
        dbus_pending_call_block(lpDBusPendC);
        lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);


        if (!lpDBusReply) {
            BTRCORELOG_ERROR ("%s.GetProperties returned an error\n", pInterface);
            rc = -1;
            btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
            dbus_pending_call_unref(lpDBusPendC);
            dbus_message_unref(lpDBusMsg);
            lpDBusPendC = NULL;
            lpDBusMsg = NULL;
        }
        else {
            if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
                BTRCORELOG_ERROR ("GetProperties lpDBusReply has no arguments.\n");
                rc = -1;
            }
            else if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
                BTRCORELOG_ERROR ("GetProperties argument is not an array.\n");
                rc = -1;
            }
            else {
                dbus_message_iter_recurse(&arg_i, &element_i);
                while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
                    if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
                        DBusMessageIter dict_i;
                        dbus_message_iter_recurse(&element_i, &dict_i);
                        dbus_message_iter_get_basic(&dict_i, &pParsedKey);

                        if ((pParsedKey) && (strcmp (pParsedKey, lDBusKey) == 0)) {
                            dbus_message_iter_next(&dict_i);
                            dbus_message_iter_recurse(&dict_i, &variant_i);
                            if (lDBusType == DBUS_TYPE_STRING) {
                                dbus_message_iter_get_basic(&variant_i, &pParsedValueString);
                                //BTRCORELOG_ERROR ("Key is %s and the value in string is %s\n", pParsedKey, pParsedValueString);
                                strncpy (apvVal, pParsedValueString, strlen(pParsedValueString) < BT_MAX_UUID_STR_LEN ? strlen(pParsedValueString) : BT_MAX_UUID_STR_LEN - 1);

                                if (i32StoreDefAdpAddr && !pstlhBtIfce->pcBTDAdapterAddr) {
                                    pstlhBtIfce->pcBTDAdapterAddr = strndup(pParsedValueString, strlen(pParsedValueString) < BT_MAX_UUID_STR_LEN ? strlen(pParsedValueString) : BT_MAX_UUID_STR_LEN - 1);
                                }
                            }
                            else if (lDBusType == DBUS_TYPE_UINT16) {
                                unsigned short* ptr = (unsigned short*) apvVal;
                                dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedShort);
                                //BTRCORELOG_ERROR ("Key is %s and the value is %u\n", pParsedKey, parsedValueUnsignedNumber);
                                *ptr = parsedValueUnsignedShort;
                            }
                            else if (lDBusType == DBUS_TYPE_UINT32) {
                                unsigned int* ptr = (unsigned int*) apvVal;
                                dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedNumber);
                                //BTRCORELOG_ERROR ("Key is %s and the value is %u\n", pParsedKey, parsedValueUnsignedNumber);
                                *ptr = parsedValueUnsignedNumber;
                            }
                            else if (lDBusType == DBUS_TYPE_OBJECT_PATH) {
                                dbus_message_iter_get_basic(&variant_i, &pParsedValueString);
                                //BTRCORELOG_ERROR ("Key is %s and the value in string is %s\n", pParsedKey, pParsedValueString);
                                strncpy (apvVal, pParsedValueString, strlen(pParsedValueString) < BT_MAX_STR_LEN ? strlen(pParsedValueString) : BT_MAX_STR_LEN - 1);
                            }
                            else if (lDBusType == DBUS_TYPE_ARRAY) {
                                DBusMessageIter variantArray;
                                dbus_message_iter_recurse(&variant_i, &variantArray);
                                int lType = dbus_message_iter_get_arg_type(&variantArray);
                                int lIndex = 0;

                                if (lType == DBUS_TYPE_STRING) {
                                   char (*ptr)[BT_MAX_UUID_STR_LEN] = (char (*)[BT_MAX_UUID_STR_LEN])apvVal;

                                   while (dbus_message_iter_get_arg_type(&variantArray) != DBUS_TYPE_INVALID) {

                                       dbus_message_iter_get_basic(&variantArray, &pParsedValueString);
                                       strncpy (ptr[lIndex++], pParsedValueString, BT_MAX_UUID_STR_LEN-1);

                                       if (!dbus_message_iter_next(&variantArray)) {
                                          break; 
                                       }
                                   }
                                } else
                                if (lType == DBUS_TYPE_BYTE) {
                                   unsigned char chr = '\0';
                                   char hex[] = "0123456789abcdef";
                                   char *byteStream = (char*)apvVal;

                                   while (dbus_message_iter_get_arg_type(&variantArray) != DBUS_TYPE_INVALID) {

                                       dbus_message_iter_get_basic(&variantArray, &chr);
                                       byteStream[lIndex++] = hex[chr >> 4];
                                       byteStream[lIndex++] = hex[chr &  0x0F];

                                       if (!dbus_message_iter_next(&variantArray)) {
                                          break;
                                       }
                                   }
                                   byteStream[lIndex] = '\0';
                                }
                            }
                            else { /* As of now ints and bools are used. This function has to be extended for array if needed */
                                int* ptr = (int*) apvVal;
                                dbus_message_iter_get_basic(&variant_i, &parsedValueNumber);
                                //BTRCORELOG_ERROR ("Key is %s and the value is %d\n", pParsedKey, parsedValueNumber);
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

            btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);

            dbus_message_unref(lpDBusReply);
            dbus_pending_call_unref(lpDBusPendC);
            dbus_message_unref(lpDBusMsg);

            lpDBusReply = NULL;
            lpDBusPendC = NULL;
            lpDBusMsg = NULL;
        }
    }

    return rc;
}


int
BtrCore_BTSetProp (
    void*               apstBtIfceHdl,
    const char*         apcBtOpIfcePath,
    enBTOpIfceType      aenBtOpIfceType,
    unBTOpIfceProp      aunBtOpIfceProp,
    void*               apvVal
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusMessageIter     lDBusMsgIter;
    DBusMessageIter     lDBusMsgIterValue;
    DBusError           lDBusErr;

    const char*         lDBusTypeAsString;

    const char*         lDBusKey = NULL;
    int                 lDBusType = DBUS_TYPE_INVALID;

    const char*         pInterface          = NULL;
    const char*         pAdapterInterface   = BT_DBUS_BLUEZ_ADAPTER_PATH;
    const char*         pDeviceInterface    = BT_DBUS_BLUEZ_DEVICE_PATH;
    const char*         pMediaTransInterface= BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH;
    const char*         pGattCharInterface  = BT_DBUS_BLUEZ_GATT_CHAR_PATH;

    if (!apstBtIfceHdl)
        return -1;


    switch (aenBtOpIfceType) {
    case enBTAdapter:
        pInterface = pAdapterInterface;
        switch (aunBtOpIfceProp.enBtAdapterProp) {
        case enBTAdPropName:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "Alias";
            break;
        case enBTAdPropAddress:
            lDBusType = DBUS_TYPE_STRING;
            lDBusKey  = "Address";
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
        case enBTAdPropPairable:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Pairable";
            break;
        case enBTAdPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid Adapter Property\n");
            return -1;
        }
        break;
    case enBTDevice:
        pInterface = pDeviceInterface;
        switch (aunBtOpIfceProp.enBtDeviceProp) {
        case enBTDevPropPaired:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Paired";
            break;
        case enBTDevPropConnected:
            lDBusType = DBUS_TYPE_BOOLEAN;
            lDBusKey  = "Connected";
            break;
        case enBTDevPropVendor:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Vendor";
            break;
        case enBTDevPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid Device Property\n");
            return -1;
        }
        break;
    case enBTMediaTransport:
        pInterface = pMediaTransInterface;
        switch (aunBtOpIfceProp.enBtMediaTransportProp) {
        case enBTMedTPropDelay:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Delay";
            break;
        case enBTMedTPropVol:
            lDBusType = DBUS_TYPE_UINT16;
            lDBusKey  = "Volume";
            break;
        case enBTMedTPropState:
        case enBTMedTPropUnknown:
        default:
            BTRCORELOG_ERROR ("Invalid MediaTransport Property\n");
            return -1;
        }
        break;
    case enBTGattCharacteristic: /* SM: Only enBTGattCPropValue has been tested*/
        pInterface = pGattCharInterface;
        switch (aunBtOpIfceProp.enBtGattCharProp) {
            case enBTGattCPropUUID:
                lDBusType = DBUS_TYPE_STRING;
                lDBusKey = "UUID";
                break;
            case enBTGattCPropService:
                lDBusType = DBUS_TYPE_OBJECT_PATH;
                lDBusKey = "Service";
                break;
            case enBTGattCPropValue:
                lDBusType = DBUS_TYPE_ARRAY;
                lDBusKey = "Value";
                break;
            case enBTGattCPropNotifying:
                lDBusType = DBUS_TYPE_BOOLEAN;
                BTRCORELOG_INFO("Key is notifying\n");
                lDBusKey = "Notifying";
                break;
            case enBTGattCPropFlags:
                lDBusType = DBUS_TYPE_ARRAY;
                lDBusKey = "Flags";
                break;
            case enBTGattCPropUnknown:
            default:
                BTRCORELOG_ERROR("Invalid GattCharacteristic Property\n");
                return -1;
        }
        break;
    case enBTUnknown:
    default:
        BTRCORELOG_ERROR ("Invalid Operational Interface\n");
        return -1;
    }


    if (!lDBusKey || (lDBusType == DBUS_TYPE_INVALID)) {
       BTRCORELOG_ERROR ("Invalid Interface Property\n");
       return -1;
    }


    switch (lDBusType) {
    case DBUS_TYPE_BOOLEAN:
        lDBusTypeAsString = DBUS_TYPE_BOOLEAN_AS_STRING;
        break;
    case DBUS_TYPE_UINT32:
        lDBusTypeAsString = DBUS_TYPE_UINT32_AS_STRING;
        break;
    case DBUS_TYPE_UINT16:
        lDBusTypeAsString = DBUS_TYPE_UINT16_AS_STRING;
        break;
    case DBUS_TYPE_STRING:
        lDBusTypeAsString = DBUS_TYPE_STRING_AS_STRING;
        break;
    case DBUS_TYPE_ARRAY:
        lDBusTypeAsString = DBUS_TYPE_ARRAY_AS_STRING;
        break;
    default:
        BTRCORELOG_ERROR ("Invalid DBus Type\n");
        return -1;
    }
    
    if(aenBtOpIfceType == enBTGattCharacteristic) {
        lpDBusMsg = dbus_message_new_signal(apcBtOpIfcePath,
                                        DBUS_INTERFACE_PROPERTIES,
                                        "PropertiesChanged");
    }
    else {
        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 apcBtOpIfcePath,
                                                 DBUS_INTERFACE_PROPERTIES,
                                                 "Set");

        if (!lpDBusMsg) {
            BTRCORELOG_ERROR ("Can't allocate new method call\n");
            return -1;
        }
    }
    
    if (aenBtOpIfceType == enBTGattCharacteristic) {
        char* pVlaue = (char*)apvVal;
        BTRCORELOG_INFO("Notified Value is %s with length %lu\n", pVlaue, (unsigned long)strlen(pVlaue));
            
        DBusMessageIter lDBusMsgIter, signal_iter;
        DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterSubArray, lDBusMsgIterVariant;
        char*               Prop = "Value";
        int     type = DBUS_TYPE_BYTE;
            
        char array_type[5] = "a";
        strncat(array_type, (char*)&type, sizeof(array_type) - sizeof(type));
    
        dbus_message_iter_init_append(lpDBusMsg, &signal_iter);
            dbus_message_iter_append_basic(&signal_iter, DBUS_TYPE_STRING, &pGattCharInterface);
            dbus_message_iter_open_container(&signal_iter, DBUS_TYPE_ARRAY, "{sv}", &lDBusMsgIter);
            dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                dbus_message_iter_append_basic(&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &Prop);
                dbus_message_iter_open_container(&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                dbus_message_iter_open_container(&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                for (int i = 0; i < strlen(pVlaue); i++) {
                    dbus_message_iter_append_basic(&lDBusMsgIterSubArray, DBUS_TYPE_BYTE, &(pVlaue[i]));
                }
                dbus_message_iter_close_container(&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
                dbus_message_iter_close_container(&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
            dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterDictStr);
        dbus_message_iter_close_container(&signal_iter, &lDBusMsgIter);
    }
    else {
        dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
        dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &pInterface);
        dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &lDBusKey);
        dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
        dbus_message_iter_append_basic(&lDBusMsgIterValue, lDBusType, apvVal);
        dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);
        //dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pInterface, DBUS_TYPE_STRING, &lDBusKey, lDBusType, apvVal, DBUS_TYPE_INVALID);
    }
        
    const char* lsignature = dbus_message_get_signature(lpDBusMsg);
    int             li32MessageType;
    const char*     lpcSender;
    const char*     lpcDestination;
    BTRCORELOG_INFO("Signature is %s\n", lsignature);
        
    li32MessageType = dbus_message_get_type(lpDBusMsg);
    lpcSender       = dbus_message_get_sender(lpDBusMsg);
    lpcDestination  = dbus_message_get_destination(lpDBusMsg);
  
    BTRCORELOG_TRACE ("%s Sender=%s -> Dest=%s Path=%s; Interface=%s; Member=%s\n",
                    btrCore_DBusType2Name(li32MessageType),
                    lpcSender ? lpcSender : "Null",
                    lpcDestination ? lpcDestination : "Null",
                    dbus_message_get_path(lpDBusMsg),
                    dbus_message_get_interface(lpDBusMsg),
                    dbus_message_get_member(lpDBusMsg));
    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTStartDiscovery (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "StartDiscovery");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTStopDiscovery (
    void*       apstBtIfceHdl,
    const char* apBtAdapter,
    const char* apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "StopDiscovery");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);
    // for now, adding a 1s sleep to prevent stop discovery from being interrupted
    // by an LE connect request that immediately follows the stop discovery request
    // TODO: avoid sleep. handle correctly by listening for "Discovering" events from Adapter interface
    sleep (1);

    return 0;
}


int
BtrCore_BTStartLEDiscovery (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterDict;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "SetDiscoveryFilter");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }


    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);
   {
        DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
        char*   lpcKey      = "Transport";
        char*   lpcValue    = "le";
        int     i32DBusType = DBUS_TYPE_STRING;

        dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
            dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
            dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lpcValue);
            dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
    }
    {
        DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant, lDBusMsgIterSubArray;
        char*       lpcKey = "UUIDs";
        int         i32DBusType = DBUS_TYPE_STRING;
        const char* apcBtSrvUUID1 = BT_UUID_GATT_TILE_1;
        const char* apcBtSrvUUID2 = BT_UUID_GATT_TILE_2;
        const char* apcBtSrvUUID3 = BT_UUID_GATT_TILE_3;

        char array_type[5] = "a";
        strncat (array_type, (char*)&i32DBusType, sizeof(array_type) - sizeof(i32DBusType));

        const char *lppui8Props[] = { apcBtSrvUUID1, apcBtSrvUUID2, apcBtSrvUUID3, NULL };

        dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
            dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
            dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                dbus_message_iter_open_container (&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&i32DBusType, &lDBusMsgIterSubArray);
                    dbus_message_iter_append_basic (&lDBusMsgIterSubArray, i32DBusType, &lppui8Props);
                dbus_message_iter_close_container (&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
            dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
    }
#if 0
    //TODO: Enable in future when you can eliminate the UUID based filter 
    //      As in my opinion filtering based on RSSI and delta of RSSI change is a better approach as compared to 
    //      Filtering based on UUID as above
    {
        DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
        char*   lpcKey      = "RSSI";
        short   lpcValue    = -64;
        int     i32DBusType = DBUS_TYPE_INT16;

        dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
            dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
            dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lpcValue);
            dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
    }
#endif

    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);


    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return BtrCore_BTStartDiscovery(pstlhBtIfce, apBtAdapter, apBtAgentPath);
}


int
BtrCore_BTStopLEDiscovery (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;
    DBusMessageIter lDBusMsgIter, lDBusMsgIterDict;

    if (!apstBtIfceHdl)
        return -1;


    if (BtrCore_BTStopDiscovery(pstlhBtIfce, apBtAdapter, apBtAgentPath)) {
        BTRCORELOG_WARN ("Failed to Stop Discovery\n");
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "SetDiscoveryFilter");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }


    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);
    {
        // Empty
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);


    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTStartClassicDiscovery (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterDict;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "SetDiscoveryFilter");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }


    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);
   {
        DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
        char*   lpcKey      = "Transport";
        char*   lpcValue    = "bredr";
        int     i32DBusType = DBUS_TYPE_STRING;

        dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
            dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
            dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lpcValue);
            dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);


    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return BtrCore_BTStartDiscovery(pstlhBtIfce, apBtAdapter, apBtAgentPath);
}


int
BtrCore_BTStopClassicDiscovery (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;
    DBusMessageIter lDBusMsgIter, lDBusMsgIterDict;

    if (!apstBtIfceHdl)
        return -1;


    if (BtrCore_BTStopDiscovery(pstlhBtIfce, apBtAdapter, apBtAgentPath)) {
        BTRCORELOG_WARN ("Failed to Stop Discovery\n");
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_ADAPTER_PATH,
                                             "SetDiscoveryFilter");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }


    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);
    {
        // Empty
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);


    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}



int
BtrCore_BTGetPairedDeviceInfo (
    void*                   apstBtIfceHdl,
    const char*             apBtAdapter,
    stBTPairedDeviceInfo*   pPairedDeviceInfo
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusMessageIter rootIter;
    DBusError       lDBusErr;
    bool            adapterFound = FALSE;

    char*           pdeviceInterface = BT_DBUS_BLUEZ_DEVICE_PATH;
    char*           adapter_path;
    char*           dbusObject2;
    char            paths[BT_MAX_NUM_DEVICE][BT_MAX_DEV_PATH_LEN];
    char            objectPath[BT_MAX_DEV_PATH_LEN] = {'\0'};
    char            objectData[BT_MAX_DEV_PATH_LEN] = {'\0'};
    int             i = 0;
    int             num = 0;
    int             a = 0;
    int             b = 0;
    int             d = 0;


    if (!apstBtIfceHdl || !apBtAdapter || !pPairedDeviceInfo)
        return -1;


    dbus_error_init(&lDBusErr);
    lpDBusReply = btrCore_BTSendMethodCall(pstlhBtIfce->pDBusConn, "/", DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects");
    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("org.bluez.Manager.ListAdapters returned an error: '%s'\n", lDBusErr.message);
        dbus_error_free(&lDBusErr);
        return -1;
    }

    for ( i = 0; i < BT_MAX_NUM_DEVICE; i++) {
        memset(&paths[i][0], '\0', BT_MAX_DEV_PATH_LEN);
    }

    if (dbus_message_iter_init(lpDBusReply, &rootIter) &&               //point iterator to lpDBusReply message
        DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) { //get the type of message that iter points to

        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array

        while(!adapterFound) {
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {

                DBusMessageIter dictEntryIter;
                dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                    strncpy(objectPath, adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                    ++a;
                }

                dbus_message_iter_next(&dictEntryIter);
                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                    DBusMessageIter innerArrayIter;
                    dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                    while (dbus_message_iter_has_next(&innerArrayIter)) {
                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter)) {
                            DBusMessageIter innerDictEntryIter;
                            dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                char *dbusObject;
                                dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);
                            }


                            /////// NEW //////////
                            dbus_message_iter_next(&innerDictEntryIter);
                            if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                DBusMessageIter innerArrayIter2;
                                dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                while (dbus_message_iter_has_next(&innerArrayIter2)) {
                                    if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2)) {
                                        DBusMessageIter innerDictEntryIter2;
                                        dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of

                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2)) {
                                            dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                        }


                                        ////////////// NEW 2 ////////////
                                        dbus_message_iter_next(&innerDictEntryIter2);
                                        DBusMessageIter innerDictEntryIter3;
                                        char *dbusObject3;

                                        dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                        if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                            memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);
                                            strncpy(objectData, dbusObject3, (strlen(dbusObject3) < BT_MAX_DEV_PATH_LEN) ? strlen(dbusObject3) : BT_MAX_DEV_PATH_LEN - 1);
                                            ++b;
                                        }
                                        else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                            bool *device_prop = FALSE;
                                            dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);

                                            if (strcmp(dbusObject2, "Paired") == 0 && device_prop) {
                                                strncpy(&paths[d][0], adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                                                ++d;
                                            }
                                        }
                                    }

                                    if (!dbus_message_iter_has_next(&innerArrayIter2)) {
                                        break;  //check to see if end of 3rd array
                                    }
                                    else {
                                        dbus_message_iter_next(&innerArrayIter2);
                                    }
                                }
                            }
                        }

                        if (!dbus_message_iter_has_next(&innerArrayIter)) {
                            break; //check to see if end of 2nd array
                        }
                        else {
                            dbus_message_iter_next(&innerArrayIter);
                        }
                    }
                }

                if (!dbus_message_iter_has_next(&arrayElementIter)) {
                    break; //check to see if end of 1st array
                }
                else {
                    dbus_message_iter_next(&arrayElementIter);
                }
            } //while loop end --used to traverse array
        }
    }

    num = d;

    /* Update the number of devices */
    pPairedDeviceInfo->numberOfDevices = num;

    /* Update the paths of these devices */
    for ( i = 0; i < num; i++) {
        memset(&pPairedDeviceInfo->devicePath[i][0], '\0', BT_MAX_DEV_PATH_LEN);
        strncpy(&pPairedDeviceInfo->devicePath[i][0], &paths[i][0], (strlen(&paths[i][0]) < BT_MAX_DEV_PATH_LEN) ? strlen(&paths[i][0]) : BT_MAX_DEV_PATH_LEN - 1);
    }

    dbus_message_unref(lpDBusReply);
    lpDBusReply = NULL;


    for ( i = 0; i < num; i++) {
        DBusPendingCall*    lpDBusPendC = NULL;

        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 pPairedDeviceInfo->devicePath[i],
                                                 DBUS_INTERFACE_PROPERTIES,
                                                 "GetAll");
        dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pdeviceInterface, DBUS_TYPE_INVALID);

        dbus_error_init(&lDBusErr);

        if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
            BTRCORELOG_ERROR ("failed to send message");
            return -1;
        }

        dbus_connection_flush(pstlhBtIfce->pDBusConn);
        dbus_message_unref(lpDBusMsg);
        lpDBusMsg = NULL;

        if (lpDBusPendC != NULL) {
            dbus_pending_call_block(lpDBusPendC);
            lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);
            dbus_pending_call_unref(lpDBusPendC);

            if (lpDBusReply != NULL) {
                stBTDeviceInfo  apstBTDeviceInfo;

                memset (&apstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
                if (0 != btrCore_BTParseDevice(lpDBusReply, &apstBTDeviceInfo)) {
                    BTRCORELOG_ERROR ("Parsing the device %s failed..\n", pPairedDeviceInfo->devicePath[i]);
                    dbus_message_unref(lpDBusReply);
                    return -1;
                }
                else {
                    memcpy (&pPairedDeviceInfo->deviceInfo[i], &apstBTDeviceInfo, sizeof(stBTDeviceInfo));
                }

                dbus_message_unref(lpDBusReply);
            }
        }
    }


    BTRCORELOG_TRACE ("Exiting\n");

    return 0;
}


int
BtrCore_BTDiscoverDeviceServices (
    void*                           apstBtIfceHdl,
    const char*                     apcDevPath,
    stBTDeviceSupportedServiceList* pProfileList
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusError           lDBusErr;
    DBusMessageIter     args;
    DBusMessageIter     MsgIter;
    DBusPendingCall*    lpDBusPendC;
    int                 match = 0;
    const char*         apcSearchString = "UUIDs";
    const char*         pDeviceInterface= BT_DBUS_BLUEZ_DEVICE_PATH;

    if (!apstBtIfceHdl)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcDevPath,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "GetAll");

    dbus_message_iter_init_append(lpDBusMsg, &args);
    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &pDeviceInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendC, -1)) {
        BTRCORELOG_ERROR ("failed to send message");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);
    dbus_message_unref(lpDBusMsg);
    lpDBusMsg = NULL;

    dbus_pending_call_block(lpDBusPendC);
    lpDBusReply =  dbus_pending_call_steal_reply(lpDBusPendC);
    dbus_pending_call_unref(lpDBusPendC);


    dbus_message_iter_init(lpDBusReply, &MsgIter); //lpDBusMsg is pointer to dbus message received
    //dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge received
    /*if (!dbus_message_iter_init(lpDBusReply, &MsgIter))
    {
    BTRCORELOG_ERROR ("Message has no arguments!\n");
    }*/

    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&MsgIter)) {
        DBusMessageIter arrayElementIter;
        dbus_message_iter_recurse(&MsgIter, &arrayElementIter); //assign new iterator to first element of array
        while (dbus_message_iter_has_next(&arrayElementIter)) {
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&arrayElementIter)) {
                char *dbusObject2;
                dbus_message_iter_get_basic(&arrayElementIter, &dbusObject2);
                if (strcmp(apcSearchString, dbusObject2) == 0) {
                    match = 1;
                }
                else {
                    match = 0;
                }
            }

            if (!dbus_message_iter_has_next(&arrayElementIter)) {
                break; //check to see if end of 3rd array
            }
            else {
                dbus_message_iter_next(&arrayElementIter);
            }
        }
    }

    return match;
}


int
BtrCore_BTFindServiceSupported (
    void*           apstBtIfceHdl,
    const char*     apcDevPath,
    const char*     apcSearchString,
    char*           apcDataString
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;

    int match;
    const char* value;
    char* ret;

    if (!apstBtIfceHdl || !apcDevPath)
        return -1;


   //BTRCORELOG_ERROR ("%d\t: %s - apcDevPath is %s\n and service UUID is %s", __LINE__, __FUNCTION__, apcDevPath, apcSearchString);
    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apcDevPath,
                                             BT_DBUS_BLUEZ_DEVICE_PATH,
                                             "DiscoverServices");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    match = 0; //assume it does not match
    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_STRING, &apcSearchString, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("Failure attempting to Discover Services\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
       BTRCORELOG_ERROR ("DiscoverServices lpDBusReply has no information.");
       return -1;
    }

    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    // BTRCORELOG_ERROR ("type is %d\n", dbus_type);

    dbus_message_iter_recurse(&arg_i, &element_i);
    dbus_type = dbus_message_iter_get_arg_type(&element_i);
    //BTRCORELOG_ERROR ("checking the type, it is %d\n",dbus_type);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        dbus_type = dbus_message_iter_get_arg_type(&element_i);
        //BTRCORELOG_ERROR ("next element_i type is %d\n",dbus_type);

        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("checking the dict subtype, it is %d\n",dbus_type);

            dbus_message_iter_next(&dict_i);
            dbus_type = dbus_message_iter_get_arg_type(&dict_i);
            // BTRCORELOG_ERROR ("interating the dict subtype, it is %d\n",dbus_type);
            dbus_message_iter_get_basic(&dict_i, &value);

            // BTRCORELOG_ERROR ("Services: %s\n",value);
            if (apcDataString != NULL) {
                strcpy(apcDataString, value);
            }

            // lets strstr to see if "uuid value="<UUID>" is there
            ret =  strstr(value, apcSearchString);
            if (ret !=NULL) {
                match = 1;//assume it does match
                // BTRCORELOG_ERROR ("match\n");
            }
            else {
                //BTRCORELOG_ERROR ("NO match\n");
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
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    const char*     apBtAgentPath,
    const char*     apcDevPath,
    enBTAdapterOp   aenBTAdpOp
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    DBusMessageIter rootIter;
    bool            adapterFound = FALSE;
    char*           adapter_path = NULL;
    char            deviceObjectPath[BT_MAX_DEV_PATH_LEN] = {'\0'};
    char            deviceOpString[BT_MAX_STR_LEN/8] = {'\0'};
    char            objectPath[BT_MAX_DEV_PATH_LEN] = {'\0'};
    char            objectData[BT_MAX_DEV_PATH_LEN] = {'\0'};
    int             rc = 0;
    int             a = 0;
    int             b = 0;

    if (!apstBtIfceHdl || !apBtAdapter || !apBtAgentPath || !apcDevPath || (aenBTAdpOp == enBTAdpOpUnknown))
        return -1;


    switch (aenBTAdpOp) {
        case enBTAdpOpFindPairedDev:
        strcpy(deviceOpString, "FindDevice");
        break;
        case enBTAdpOpCreatePairedDev:
        case enBTAdpOpCreatePairedDevASync:
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
        lpDBusReply = btrCore_BTSendMethodCall(pstlhBtIfce->pDBusConn, "/", DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects");

        if (lpDBusReply && 
            dbus_message_iter_init(lpDBusReply, &rootIter) &&               //point iterator to lpDBusReply message
            DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) { //get the type of message that iter points to

            DBusMessageIter arrayElementIter;
            dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array

            while(!adapterFound) {
                if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {

                    DBusMessageIter dictEntryIter;
                    dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                        dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                        strncpy(objectPath, adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                        ++a;
                    }

                    dbus_message_iter_next(&dictEntryIter);
                    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                        DBusMessageIter innerArrayIter;
                        dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                        while (dbus_message_iter_has_next(&innerArrayIter)) {
                            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter)) {
                                DBusMessageIter innerDictEntryIter;
                                dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                                if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                    char *dbusObject;
                                    dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);
                                    ////// getting default adapter path //////
                                }


                                /////// NEW //////////
                                dbus_message_iter_next(&innerDictEntryIter);
                                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                    DBusMessageIter innerArrayIter2;
                                    dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                    while (dbus_message_iter_has_next(&innerArrayIter2)) {
                                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2)) {
                                            DBusMessageIter innerDictEntryIter2;
                                            dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2)) {
                                                char *dbusObject2;
                                                dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                            }


                                            ////////////// NEW 2 ////////////
                                            dbus_message_iter_next(&innerDictEntryIter2);
                                            DBusMessageIter innerDictEntryIter3;
                                            char *dbusObject3;

                                            dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                                memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);
                                                strncpy(objectData, dbusObject3, (strlen(dbusObject3) < BT_MAX_DEV_PATH_LEN) ? strlen(dbusObject3) : BT_MAX_DEV_PATH_LEN - 1);

                                                if (strcmp(apcDevPath, objectData) == 0) {
                                                    ++b;
                                                    adapterFound = TRUE;
                                                    break;
                                                }
                                            }
                                            else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                                bool *device_prop = FALSE;
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            }
                                        }

                                        if (!dbus_message_iter_has_next(&innerArrayIter2)) {
                                            break; //check to see if end of 3rd array
                                        }
                                        else {
                                            dbus_message_iter_next(&innerArrayIter2);
                                        }
                                    }
                                }
                            }

                            if (!dbus_message_iter_has_next(&innerArrayIter)) {
                                break; //check to see if end of 2nd array
                            }
                            else {
                                dbus_message_iter_next(&innerArrayIter);
                            }
                        }
                    }

                    if (!dbus_message_iter_has_next(&arrayElementIter)) {
                        break; //check to see if end of 1st array
                    }
                    else {
                        dbus_message_iter_next(&arrayElementIter);
                    }
                } //while loop end --used to traverse arra
            }

            dbus_message_unref(lpDBusReply);
        }
    }

    else if (aenBTAdpOp == enBTAdpOpRemovePairedDev) {
        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 apBtAdapter,
                                                 BT_DBUS_BLUEZ_ADAPTER_PATH,
                                                 deviceOpString);
        if (!lpDBusMsg) {
            BTRCORELOG_ERROR ("Can't allocate new method call\n");
            return -1;
        }

        dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apcDevPath, DBUS_TYPE_INVALID);

        dbus_error_init(&lDBusErr);
        lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
        dbus_message_unref(lpDBusMsg);

        if (!lpDBusReply) {
            BTRCORELOG_ERROR ("UnPairing failed...\n");
            btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
            return -1;
        }

        dbus_message_unref(lpDBusReply);
    }

    else if ((aenBTAdpOp == enBTAdpOpCreatePairedDev) || (aenBTAdpOp == enBTAdpOpCreatePairedDevASync)) {
        lpDBusReply = btrCore_BTSendMethodCall(pstlhBtIfce->pDBusConn, "/", DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects");

        if (lpDBusReply &&
            dbus_message_iter_init(lpDBusReply, &rootIter) &&               //point iterator to lpDBusReply message
            DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) { //get the type of message that iter points to

            DBusMessageIter arrayElementIter;
            dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array

            while (!adapterFound) {
                if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {

                    DBusMessageIter dictEntryIter;
                    dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)

                    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                        dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
                        strncpy(objectPath, adapter_path, (strlen(adapter_path) < BT_MAX_DEV_PATH_LEN) ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                        ++a;
                    }

                    dbus_message_iter_next(&dictEntryIter);
                    if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter)) {
                        DBusMessageIter innerArrayIter;
                        dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);

                        while (dbus_message_iter_has_next(&innerArrayIter)) {
                            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter)) {
                                DBusMessageIter innerDictEntryIter;
                                dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of

                                if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                    char *dbusObject;
                                    dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);
                                    ////// getting default adapter path //////
                                }

                                /////// NEW //////////
                                dbus_message_iter_next(&innerDictEntryIter);
                                if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&innerDictEntryIter)) {
                                    DBusMessageIter innerArrayIter2;
                                    dbus_message_iter_recurse(&innerDictEntryIter, &innerArrayIter2);

                                    while (dbus_message_iter_has_next(&innerArrayIter2)) {
                                        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter2)) {
                                            DBusMessageIter innerDictEntryIter2;
                                            dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of

                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2)) {
                                                char *dbusObject2;
                                                dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
                                            }

                                            ////////////// NEW 2 ////////////
                                            dbus_message_iter_next(&innerDictEntryIter2);
                                            DBusMessageIter innerDictEntryIter3;
                                            char *dbusObject3;

                                            dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
                                            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
                                                memset(objectData, '\0', BT_MAX_DEV_PATH_LEN);
                                                strncpy(objectData, dbusObject3, (strlen(dbusObject3) < BT_MAX_DEV_PATH_LEN) ? strlen(dbusObject3) : BT_MAX_DEV_PATH_LEN - 1);
                                                if (strcmp(apcDevPath, objectData) == 0) {
                                                    ++b;
                                                    strcpy(deviceObjectPath,adapter_path);
                                                    adapterFound = TRUE;
                                                    break;
                                                }
                                            }
                                            else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3)) {
                                                bool *device_prop = FALSE;
                                                dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
                                            }
                                        }

                                        if (!dbus_message_iter_has_next(&innerArrayIter2)) {
                                            break; //check to see if end of 3rd array
                                        }
                                        else {
                                            dbus_message_iter_next(&innerArrayIter2);
                                        }
                                    }
                                }
                            }

                            if (!dbus_message_iter_has_next(&innerArrayIter)) {
                                break; //check to see if end of 2nd array
                            }
                            else {
                                dbus_message_iter_next(&innerArrayIter);
                            }
                        }
                    }

                    if (!dbus_message_iter_has_next(&arrayElementIter)) {
                        break; //check to see if end of 1st array
                    }
                    else {
                        dbus_message_iter_next(&arrayElementIter);
                    }
                }   //while loop end --used to traverse arra
            }

            dbus_message_unref(lpDBusReply);
        }



        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 deviceObjectPath,
                                                 BT_DBUS_BLUEZ_DEVICE_PATH,
                                                 deviceOpString);
        if (!lpDBusMsg) {
            BTRCORELOG_ERROR ("Can't allocate new method call\n");
            return -1;
        }
        if (enBTAdpOpCreatePairedDev == aenBTAdpOp) {
            dbus_error_init(&lDBusErr);
            lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
            dbus_message_unref(lpDBusMsg);

            if (!lpDBusReply) {
                BTRCORELOG_ERROR ("Pairing failed...\n");
                btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
                return -1;
            }

            dbus_message_unref(lpDBusReply);
        }
        else
        {
        /* The Device Pairing should not block n wait for success as this action expected to trigger chain of events like PIN sharing.
         * So, let us use the device pairing alone to be unblocked and wait for response.
         */
            DBusPendingCall*    lpDBusPendC = NULL;
            if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendC, -1)) { //Send and expect lpDBusReply using pending call object
                BTRCORELOG_ERROR ("failed to send message!\n");
            }
            dbus_pending_call_set_notify(lpDBusPendC, btrCore_BTPendingCallCheckReply, NULL, NULL);

            dbus_connection_flush(pstlhBtIfce->pDBusConn);
            dbus_message_unref(lpDBusMsg);
        }
    }

    return 0;
}


int
BtrCore_BTIsDeviceConnectable (
    void*           apstBtIfceHdl,
    const char*     apcDevPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    FILE*           lfpL2Ping   = NULL;
    int             i32OpRet    = -1;
    char            lcpL2PingIp[BT_MAX_STR_LEN/2] = {'\0'};
    char            lcpL2PingOp[BT_MAX_STR_LEN] = {'\0'};

    if (!apstBtIfceHdl || !apcDevPath)
        return -1;

    (void)pstlhBtIfce;

    snprintf(lcpL2PingIp, BT_MAX_STR_LEN/2, "l2ping -i hci0 -c 3 -s 2 -d 2 %s", apcDevPath);
    BTRCORELOG_INFO ("lcpL2PingIp: %s\n", lcpL2PingIp);
#ifdef LIBSYSWRAPPER_BUILD
    lfpL2Ping = v_secure_popen("r","l2ping -i hci0 -c 3 -s 2 -d 2 %s", apcDevPath);
#else
    lfpL2Ping = popen(lcpL2PingIp, "r");
#endif
    if ((lfpL2Ping == NULL)) {
        BTRCORELOG_ERROR ("Failed to run BTIsDeviceConnectable command\n");
    }
    else {
        if (fgets(lcpL2PingOp, sizeof(lcpL2PingOp)-1, lfpL2Ping) == NULL) {
            BTRCORELOG_ERROR ("Failed to Output of l2ping\n");
        }
        else {
            BTRCORELOG_WARN ("Output of l2ping =  %s\n", lcpL2PingOp);
            if (!strstr(lcpL2PingOp, "Host is down")) {
                i32OpRet = 0;
            }
        }
#ifdef LIBSYSWRAPPER_BUILD
        v_secure_pclose(lfpL2Ping);
#else
        pclose(lfpL2Ping);
#endif
    }

    return i32OpRet;
}


int
BtrCore_BTConnectDevice (
    void*           apstBtIfceHdl,
    const char*     apDevPath,
    enBTDeviceType  aenBTDeviceType
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl || !apDevPath)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apDevPath,
                                             BT_DBUS_BLUEZ_DEVICE_PATH,
                                             "Connect");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTDisconnectDevice (
    void*           apstBtIfceHdl,
    const char*     apDevPath,
    enBTDeviceType  aenBTDeviceType
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl || !apDevPath)
        return -1;


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apDevPath,
                                             BT_DBUS_BLUEZ_DEVICE_PATH,
                                             "Disconnect");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTEnableEnhancedRetransmissionMode (
    void*           apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    FILE*           lfpBtErtm   = NULL;
    int             i32OpRet    = -1;
    char            lcpBtErtmIp[BT_MAX_STR_LEN/2] = {'\0'};
    char            lcpBtErtmOp[BT_MAX_STR_LEN] = {'\0'};

    if (!apstBtIfceHdl || !pstlhBtIfce)
        return -1;


    memset(lcpBtErtmIp, '\0', BT_MAX_STR_LEN/2);
    memset(lcpBtErtmOp, '\0', BT_MAX_STR_LEN);

    snprintf(lcpBtErtmIp, BT_MAX_STR_LEN/2, "echo 0 > /sys/module/bluetooth/parameters/disable_ertm");
    BTRCORELOG_INFO ("lcpBtErtmIp: %s\n", lcpBtErtmIp);

#ifdef LIBSYSWRAPPER_BUILD
    lfpBtErtm = v_secure_popen("r", lcpBtErtmIp);
#else
    lfpBtErtm = popen(lcpBtErtmIp, "r");
#endif
    if (lfpBtErtm == NULL) {
        BTRCORELOG_ERROR ("Failed to run BTEnableEnhancedRetransmissionMode command\n");
    }
    else {
        if (fgets(lcpBtErtmOp, sizeof(lcpBtErtmOp)-1, lfpBtErtm) == NULL) {
            BTRCORELOG_INFO ("Success  - Output of BtErtm\n");
            i32OpRet = 0;
        }
        else {
            BTRCORELOG_WARN ("Failed Output of BtErtm =  %s\n", lcpBtErtmOp);
            if (!strstr(lcpBtErtmOp, "Permission denied")) {
                BTRCORELOG_WARN ("Check path =  %s\n", lcpBtErtmIp);
            }
        }
#ifdef LIBSYSWRAPPER_BUILD
        v_secure_pclose(lfpBtErtm);
#else
        pclose(lfpBtErtm);
#endif
    }

    return i32OpRet;
}


int
BtrCore_BTDisableEnhancedRetransmissionMode (
    void*           apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    FILE*           lfpBtErtm   = NULL;
    int             i32OpRet    = -1;
    char            lcpBtErtmIp[BT_MAX_STR_LEN/2] = {'\0'};
    char            lcpBtErtmOp[BT_MAX_STR_LEN] = {'\0'};

    if (!apstBtIfceHdl || !pstlhBtIfce)
        return -1;


    memset(lcpBtErtmIp, '\0', BT_MAX_STR_LEN/2);
    memset(lcpBtErtmOp, '\0', BT_MAX_STR_LEN);

    snprintf(lcpBtErtmIp, BT_MAX_STR_LEN/2, "echo 1 > /sys/module/bluetooth/parameters/disable_ertm");
    BTRCORELOG_INFO ("lcpBtErtmIp: %s\n", lcpBtErtmIp);

#ifdef LIBSYSWRAPPER_BUILD
    lfpBtErtm = v_secure_popen("r", lcpBtErtmIp);
#else
    lfpBtErtm = popen(lcpBtErtmIp, "r");
#endif
    if ((lfpBtErtm == NULL)) {
        BTRCORELOG_ERROR ("Failed to run BTDisableEnhancedRetransmissionMode command\n");
    }
    else {
        if (fgets(lcpBtErtmOp, sizeof(lcpBtErtmOp)-1, lfpBtErtm) == NULL) {
            BTRCORELOG_INFO ("Success  - Output of BtErtm\n");
            i32OpRet = 0;
        }
        else {
            BTRCORELOG_WARN ("Failed Output of BtErtm =  %s\n", lcpBtErtmOp);
            if (!strstr(lcpBtErtmOp, "Permission denied")) {
                BTRCORELOG_WARN ("Check path =  %s\n", lcpBtErtmIp);
            }
        }
#ifdef LIBSYSWRAPPER_BUILD
        v_secure_pclose(lfpBtErtm);
#else
        pclose(lfpBtErtm);
#endif
    }

    return i32OpRet;
}


int
BtrCore_BTRegisterMedia (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    enBTDeviceType  aenBTDevType,
    enBTMediaType   aenBTMediaType,
    const char*     apBtUUID,
    void*           apBtMediaCapabilities,
    int             apBtMediaCapabilitiesSize,
    int             abBtMediaDelayReportEnable
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterArr;
    dbus_bool_t     lDBusOp;
    dbus_bool_t     lBtMediaDelayReport = FALSE;

    const char*     lpBtMediaEpObjPath;
    char            lBtMediaCodec;

    if (!apstBtIfceHdl)
        return -1;


    switch (aenBTMediaType) {
    case enBTMediaTypePCM:
        lBtMediaCodec = BT_MEDIA_CODEC_PCM;
        break;
    case enBTMediaTypeSBC:
        lBtMediaCodec = BT_MEDIA_CODEC_SBC;
        break;
    case enBTMediaTypeMP3:
        lBtMediaCodec = BT_MEDIA_CODEC_MPEG12;
        break;
    case enBTMediaTypeAAC:
        lBtMediaCodec = BT_MEDIA_CODEC_MPEG24;
        break;
    case enBTMediaTypeUnknown:
    default:
        lBtMediaCodec = BT_MEDIA_CODEC_SBC;
        break;
    }


    switch (aenBTDevType) {
    case enBTDevAudioSink:
        if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG12) {
            lpBtMediaEpObjPath = BT_MEDIA_MP3_A2DP_SOURCE_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG24) {
            lpBtMediaEpObjPath = BT_MEDIA_AAC_A2DP_SOURCE_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        }
        break;
    case enBTDevAudioSource:
        if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SINK_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG12) {
            lpBtMediaEpObjPath = BT_MEDIA_MP3_A2DP_SINK_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG24) {
            lpBtMediaEpObjPath = BT_MEDIA_AAC_A2DP_SINK_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SINK_ENDPOINT;
        }
        break;
    case enBTDevHFPHeadset:
        if (lBtMediaCodec == BT_MEDIA_CODEC_PCM) {
            lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_AG_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_HFP_AG_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_AG_ENDPOINT;
        }
        break;
    case enBTDevHFPAudioGateway:
        lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_HS_ENDPOINT;
        break;
    case enBTDevUnknown:
    default:
        lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        break;
    }

    if (abBtMediaDelayReportEnable)
        lBtMediaDelayReport = TRUE;

    dbus_error_init(&lDBusErr);
    lDBusOp = dbus_connection_try_register_object_path(pstlhBtIfce->pDBusConn, lpBtMediaEpObjPath, &gDBusMediaEndpointVTable, pstlhBtIfce, &lDBusErr);
    if (!lDBusOp) {
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        BTRCORELOG_ERROR ("Can't Register Media Object - %s\n", lpBtMediaEpObjPath);

        if (strcmp(lDBusErr.name, DBUS_ERROR_OBJECT_PATH_IN_USE) != 0)
            return -1;
    }

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_MEDIA_PATH,
                                             "RegisterEndpoint");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_OBJECT_PATH, &lpBtMediaEpObjPath);
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
                dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &lBtMediaCodec);
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
        strncat (array_type, (char*)&type, sizeof(array_type) - sizeof(type));

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
    lpDBusReply = dbus_connection_send_with_reply_and_block (pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTUnRegisterMedia (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter,
    enBTDeviceType  aenBTDevType,
    enBTMediaType   aenBTMediaType
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    const char*     lpBtMediaEpObjPath;
    int             lBtMediaCodec;

    if (!apstBtIfceHdl)
        return -1;


    switch (aenBTMediaType) {
    case enBTMediaTypePCM:
        lBtMediaCodec = BT_MEDIA_CODEC_PCM;
        break;
    case enBTMediaTypeSBC:
        lBtMediaCodec = BT_MEDIA_CODEC_SBC;
        break;
    case enBTMediaTypeMP3:
        lBtMediaCodec = BT_MEDIA_CODEC_MPEG12;
        break;
    case enBTMediaTypeAAC:
        lBtMediaCodec = BT_MEDIA_CODEC_MPEG24;
        break;
    case enBTMediaTypeUnknown:
    default:
        lBtMediaCodec = BT_MEDIA_CODEC_SBC;
        break;
    }


    switch (aenBTDevType) {
    case enBTDevAudioSink:
        if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG12) {
            lpBtMediaEpObjPath = BT_MEDIA_MP3_A2DP_SOURCE_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG24) {
            lpBtMediaEpObjPath = BT_MEDIA_AAC_A2DP_SOURCE_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        }
        break;
    case enBTDevAudioSource:
        if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SINK_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG12) {
            lpBtMediaEpObjPath = BT_MEDIA_MP3_A2DP_SINK_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_MPEG24) {
            lpBtMediaEpObjPath = BT_MEDIA_AAC_A2DP_SINK_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SINK_ENDPOINT;
        }
        break;
    case enBTDevHFPHeadset:
        if (lBtMediaCodec == BT_MEDIA_CODEC_PCM) {
            lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_AG_ENDPOINT;
        }
        else if (lBtMediaCodec == BT_MEDIA_CODEC_SBC) {
            lpBtMediaEpObjPath = BT_MEDIA_SBC_HFP_AG_ENDPOINT;
        }
        else {
            lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_AG_ENDPOINT;
        }
        break;
    case enBTDevHFPAudioGateway:
        lpBtMediaEpObjPath = BT_MEDIA_PCM_HFP_HS_ENDPOINT;
        break;
    case enBTDevUnknown:
    default:
        lpBtMediaEpObjPath = BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT;
        break;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_MEDIA_PATH,
                                             "UnregisterEndpoint");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &lpBtMediaEpObjPath, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    lDBusOp = dbus_connection_unregister_object_path(pstlhBtIfce->pDBusConn, lpBtMediaEpObjPath);
    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Can't Register Media Object\n");
        return -1;
    }


    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTAcquireDevDataPath (
    void*           apstBtIfceHdl,
    char*           apcDevTransportPath,
    int*            dataPathFd,
    int*            dataReadMTU,
    int*            dataWriteMTU
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl || !apcDevTransportPath)
        return -1;


    lpDBusMsg = dbus_message_new_method_call (BT_DBUS_BLUEZ_PATH,
                                              apcDevTransportPath,
                                              BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH,
                                              "Acquire");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
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
        BTRCORELOG_ERROR ("Can't get lpDBusReply arguments\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTReleaseDevDataPath (
    void*           apstBtIfceHdl,
    char*           apcDevTransportPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;

    if (!apstBtIfceHdl || !apcDevTransportPath)
        return -1;


    lpDBusMsg = dbus_message_new_method_call (BT_DBUS_BLUEZ_PATH,
                                              apcDevTransportPath,
                                              BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH,
                                              "Release");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}


int
BtrCore_BTSetDevDataAckTimeout (
    void*           apstBtIfceHdl,
    unsigned int    aui32AckTOutms
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    FILE*           lHciDataAck = NULL;
    int             i32OpRet    = -1;
    char            lcpHciDataWriteAckTOutIp[BT_MAX_STR_LEN/2] = {'\0'};
    char            lcpHciDataWriteAckTOutOp[BT_MAX_STR_LEN] = {'\0'};

    if (!apstBtIfceHdl)
        return -1;

    (void)pstlhBtIfce;

    snprintf(lcpHciDataWriteAckTOutIp, BT_MAX_STR_LEN/2, "hcitool -i hci0 cmd 0x03 0x0028 0x0C 0x00 0x%02x 0x00", aui32AckTOutms);
    BTRCORELOG_INFO ("lcpHciDataWriteAckTOutIp: %s\n", lcpHciDataWriteAckTOutIp);

    lHciDataAck = popen(lcpHciDataWriteAckTOutIp, "r");
    if ((lHciDataAck == NULL)) {
        BTRCORELOG_ERROR ("Failed to run lcpHciDataWriteAckTOutIp command\n");
    }
    else {
        if (fgets(lcpHciDataWriteAckTOutOp, sizeof(lcpHciDataWriteAckTOutOp)-1, lHciDataAck) == NULL) {
            BTRCORELOG_ERROR ("Failed to Output of lcpHciDataWriteAckTOutIp\n");
        }
        else {
            BTRCORELOG_WARN ("Output of lcpHciDataWriteAckTOutIp =  %s\n", lcpHciDataWriteAckTOutOp);
            if (strstr(lcpHciDataWriteAckTOutOp, "HCI Command: ogf 0x03, ocf 0x0028, plen 4")) {
                i32OpRet = 0;
            }
        }

        pclose(lHciDataAck);
    }

    return i32OpRet;
}


/////////////////////////////////////////////////////         AVRCP Functions         ////////////////////////////////////////////////////
/* Get Player Object Path on Remote BT Device*/
char*
BtrCore_BTGetMediaPlayerPath (
    void*          apstBtIfceHdl,
    const char*    apBtDevPath
) {
    stBtIfceHdl*   pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    char*          playerObjectPath = NULL;
    bool           isConnected      = FALSE;

    if (!apstBtIfceHdl || !apBtDevPath) {
        BTRCORELOG_ERROR ("Invalid args!!!");
        return NULL;
    }


    if (btrCore_BTGetMediaIfceProperty(pstlhBtIfce->pDBusConn, apBtDevPath, BT_DBUS_BLUEZ_MEDIA_CTRL_PATH, "Connected", (void*)&isConnected)) {
       BTRCORELOG_ERROR ("Failed to get %s property : Connected!!!", BT_DBUS_BLUEZ_MEDIA_CTRL_PATH);
       return NULL;
    }

    if (FALSE == isConnected) {
       BTRCORELOG_WARN ("%s is not connected", BT_DBUS_BLUEZ_MEDIA_CTRL_PATH);
       return NULL;
    }

    if (btrCore_BTGetMediaIfceProperty(pstlhBtIfce->pDBusConn, apBtDevPath, BT_DBUS_BLUEZ_MEDIA_CTRL_PATH, "Player", (void*)&playerObjectPath)) {
       BTRCORELOG_ERROR ("Failed to get %s property : Player!!!", BT_DBUS_BLUEZ_MEDIA_CTRL_PATH);
       return NULL;
    }

    return playerObjectPath;
}



/* Control Media on Remote BT Device*/
int
BtrCore_BTDevMediaControl (
    void*               apstBtIfceHdl,
    const char*         apMediaPlayerPath,
    enBTMediaControlCmd aenBTMediaOper
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    dbus_bool_t         lDBusOp;
    DBusMessage*        lpDBusMsg      = NULL;
    char                mediaOper[16]  = "\0";
    char*               mediaProp      = 0;
    char*               propValue      = 0;

    if (!apstBtIfceHdl || !apMediaPlayerPath) {
        BTRCORELOG_ERROR ("Invalid args!!!");
        return -1;
    }


    switch (aenBTMediaOper) {
    case enBTMediaCtrlPlay:
        strcpy(mediaOper, "Play");
        break;
    case enBTMediaCtrlPause:
        strcpy(mediaOper, "Pause");
        break;
    case enBTMediaCtrlStop:
        strcpy(mediaOper, "Stop");
        break;
    case enBTMediaCtrlNext:
        strcpy(mediaOper, "Next");
        break;
    case enBTMediaCtrlPrevious:
        strcpy(mediaOper, "Previous");
        break;
    case enBTMediaCtrlFastForward:
        strcpy(mediaOper, "FastForward");
        break;
    case enBTMediaCtrlRewind:
        strcpy(mediaOper, "Rewind");
        break;
    case enBTMediaCtrlVolumeUp:
        strcpy(mediaOper, "VolumeUp");
        break;
    case enBTMediaCtrlVolumeDown:
        strcpy(mediaOper, "VolumeDown");
        break;
    case enBTMediaCtrlEqlzrOff:
        mediaProp = "Equalizer";
        propValue = "off";
        break;
    case enBTMediaCtrlEqlzrOn:
        mediaProp = "Equalizer";
        propValue = "on";
        break;
    case enBTMediaCtrlShflOff:
        mediaProp = "Shuffle";
        propValue = "off";
        break;
    case enBTMediaCtrlShflAllTracks:
        mediaProp = "Shuffle";
        propValue = "alltracks";
        break;
    case enBTMediaCtrlShflGroup:
        mediaProp = "Shuffle";
        propValue = "group";
        break;
    case enBTMediaCtrlRptOff:
        mediaProp = "Repeat";
        propValue = "off";
        break;
    case enBTMediaCtrlRptSingleTrack:
        mediaProp = "Repeat";
        propValue = "singletrack";
        break;
    case enBTMediaCtrlRptAllTracks:
        mediaProp = "Repeat";
        propValue = "alltracks";
        break;
    case enBTMediaCtrlRptGroup:
        mediaProp = "Repeat";
        propValue = "group";
        break;
    default:
        break;
    }


    if (mediaOper[0]) {
        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 apMediaPlayerPath,
                                                 BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH,
                                                 mediaOper);
    }
    else
    if (mediaProp[0]) {
        lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                                 apMediaPlayerPath,
                                                 DBUS_INTERFACE_PROPERTIES,
                                                 "Set");
        {
            DBusMessageIter lDBusMsgIter;
            DBusMessageIter lDBusMsgIterValue;
            const char*     mediaPlayerIfce     = BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH;
            const char*     lDBusTypeAsString   = DBUS_TYPE_STRING_AS_STRING;

            dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
            dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaPlayerIfce);
            dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaProp);
            dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
            dbus_message_iter_append_basic(&lDBusMsgIterValue, DBUS_TYPE_STRING, &propValue);
            dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);
        }
    }
    else {
        BTRCORELOG_ERROR ("Unknown Media Control Command!\n");
        return -1;
    }

    if (lpDBusMsg == NULL) {
       BTRCORELOG_ERROR ("Can't allocate new method call\n");
       return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}

int
BtrCore_BTGetTransportState (
    void*           apstBtIfceHdl,
    const char*     apBtDataPath,
    void*           pState
) { 
    stBtIfceHdl*    pstlhBtIfce = NULL;

    if (!apstBtIfceHdl) {
        BTRCORELOG_ERROR ("Inavalid args - BtIfceHdl : %p\n", apstBtIfceHdl);
        return -1;
    }

    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    /* switch() */
    return btrCore_BTGetMediaIfceProperty(pstlhBtIfce->pDBusConn, apBtDataPath, BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH, "State", pState);
}

/* Get Media Player Property on Remote BT Device*/
int
BtrCore_BTGetMediaPlayerProperty (
    void*           apstBtIfceHdl,
    const char*     apBtMediaPlayerPath,
    const char*     mediaProperty,
    void*           mediaPropertyValue
) {
    stBtIfceHdl*    pstlhBtIfce = NULL;

    if (!apstBtIfceHdl) {
        BTRCORELOG_ERROR ("Inavalid args - BtIfceHdl : %p\n", apstBtIfceHdl);
        return -1;
    }

    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    /* switch() */
    return btrCore_BTGetMediaIfceProperty(pstlhBtIfce->pDBusConn, apBtMediaPlayerPath, BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH, mediaProperty, mediaPropertyValue);
}


int
BtrCore_BTGetMediaFolderNumberOfItems (
    void*           apstBtIfceHdl,
    const char*     apBtMediaPlayerPath,
    unsigned int*   aui32NumberOfItems
) {
    stBtIfceHdl*    pstlhBtIfce = NULL;

    if (!apstBtIfceHdl) {
        BTRCORELOG_ERROR ("Inavalid args - BtIfceHdl : %p\n", apstBtIfceHdl);
        return -1;
    }

    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    return btrCore_BTGetMediaIfceProperty (pstlhBtIfce->pDBusConn, apBtMediaPlayerPath, BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH, "NumberOfItems", (void*)aui32NumberOfItems);
}


/* Set Media Property on Remote BT Device (Equalizer, Repeat, Shuffle, Scan, Status)*/
int
BtrCore_BTSetMediaProperty (
    void*           apstBtIfceHdl,
    const char*     apBtAdapterPath,
    char*           mediaProperty,
    char*           pValue
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    DBusMessageIter lDBusMsgIter;
    DBusMessageIter lDBusMsgIterValue;
    const char*     mediaPlayerPath = BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH;
    const char*     lDBusTypeAsString = DBUS_TYPE_STRING_AS_STRING;

    char*           mediaPlayerObjectPath = NULL;

    if (!apstBtIfceHdl || !pValue || !apBtAdapterPath)
        return -1;


    mediaPlayerObjectPath = BtrCore_BTGetMediaPlayerPath (pstlhBtIfce, apBtAdapterPath);

    if (mediaPlayerObjectPath == NULL) {
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             mediaPlayerObjectPath,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "Set");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaPlayerPath);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaProperty);
    dbus_message_iter_open_container(&lDBusMsgIter, DBUS_TYPE_VARIANT, lDBusTypeAsString, &lDBusMsgIterValue);
    dbus_message_iter_append_basic(&lDBusMsgIterValue, DBUS_TYPE_STRING, pValue);
    dbus_message_iter_close_container(&lDBusMsgIter, &lDBusMsgIterValue);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}

/* Get Track information and place them in an array (Title, Artists, Album, number of tracks, tracknumber, duration, Genre)*/
// TODO : write a api that gets any properties irrespective of the objects' interfaces
int
BtrCore_BTGetTrackInformation (
    void*               apstBtIfceHdl,
    const char*         apBtmediaObjectPath,
    stBTMediaTrackInfo* apstBTMediaTrackInfo
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    char*               pcProperty  = "Track";
    DBusMessage*        lpDBusMsg   = NULL;
    DBusMessage*        lpDBusReply = NULL;
    DBusMessageIter     lDBusMsgIter;
    DBusError           lDBusErr;
    char*               mediaInterfacePath = BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH;


    if (!apstBtIfceHdl || !apBtmediaObjectPath) {
        BTRCORELOG_ERROR ("Media Object Path is NULL!!!");
        return -1;
    }

    if (strstr(apBtmediaObjectPath, "item")) {
        mediaInterfacePath = BT_DBUS_BLUEZ_MEDIA_ITEM_PATH;
        pcProperty = "Metadata";
    }

    BTRCORELOG_ERROR("[#####TESTING]Calling with : %s | %s | %s\n", mediaInterfacePath, pcProperty, apBtmediaObjectPath);
    //if (!strcmp (pcProperty, "Metadata")) { return 0; }

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtmediaObjectPath,
                                             DBUS_INTERFACE_PROPERTIES,
                                             "Get");

    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
    
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &mediaInterfacePath);
    dbus_message_iter_append_basic(&lDBusMsgIter, DBUS_TYPE_STRING, &pcProperty);


    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    if (lpDBusReply) {
        if (btrCore_BTParseMediaTrackMetadata(lpDBusReply, apstBTMediaTrackInfo)) {
            BTRCORELOG_ERROR ("Failed to parse MetaData !\n");
            dbus_message_unref (lpDBusReply);
            return -1;
        }
    }
    else {
        BTRCORELOG_ERROR ("lpDBusReply NULL\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_message_unref (lpDBusReply);
    return 0;
}

tBTMediaItemId
BtrCore_BTGetCommonParentMediaItemId (
    tBTMediaItemId      aBTcurrMediaItemId,
    tBTMediaItemId      aBTdestMediaItemId
) {
    tBTMediaItemId  currItemId = 0;
    tBTMediaItemId  destItemId = 0;
    tBTMediaItemId  comnItemId = 0;
    short           powerIdx   = 1;

    /* logic to reverse the digits. Ex: n=1234, val=4321 */
    while (destItemId = destItemId*10 + aBTdestMediaItemId/powerIdx%10, aBTdestMediaItemId/(powerIdx*=10));
    powerIdx = 1;
    /* logic to reverse the digits. Ex: n=1234, val=4321 */
    while (currItemId = currItemId*10 + aBTcurrMediaItemId/powerIdx%10, aBTcurrMediaItemId/(powerIdx*=10));
    powerIdx = 1;
    /* logic to find mediaID portion in common between 2 IDs. Ex: ID_1 - 1345, ID_2 - 132, common ID - 13 (parent ID) */
    while (comnItemId = comnItemId*10 + currItemId*10/powerIdx%10, powerIdx*=10, (currItemId*10/powerIdx || destItemId*10/powerIdx) && destItemId%powerIdx == currItemId%powerIdx);

    return comnItemId;
}

int
BtrCore_BTChangeMediaFolder (
    void*               apstBtIfceHdl,
    const char*         apBtmediaPlayerObjectPath,
    const char*         apBtdestMediaFolderPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;

    if (!apstBtIfceHdl || !apBtmediaPlayerObjectPath || !apBtdestMediaFolderPath) {
        BTRCORELOG_ERROR ("Invalid Args!!!\n");
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtmediaPlayerObjectPath,
                                             BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH,
                                             "ChangeFolder");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &apBtdestMediaFolderPath, DBUS_TYPE_INVALID);

    dbus_error_init(&lDBusErr);

    lpDBusReply = dbus_connection_send_with_reply_and_block (pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);

    dbus_message_unref(lpDBusMsg);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    if (lpDBusReply) {
        dbus_message_unref (lpDBusReply);
    }

    return 0;
}


int
BtrCore_BTSelectMediaFolderItems (
    void*               apstBtIfceHdl,
    const char*         apBtMediaPlayerObjectPath,
    // to change the diff filter args into a single filterList
    unsigned int        apBtMediaFolderStartIndex,
    unsigned int        apBtMediaFolderEndIndex,
    const char*         apBtMediaFilter,
    int                 apBtMediaFilterSize
) {
    stBtIfceHdl*     pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*     lpDBusMsg   = NULL;
    DBusMessage*     lpDBusReply = NULL;
    DBusError        lDBusErr;
    DBusMessageIter  lDBusMsgIter;
    DBusMessageIter  lDBusMsgIterArr;


    if (!apstBtIfceHdl || !apBtMediaPlayerObjectPath) {
        BTRCORELOG_ERROR ("Invalid Args!!!\n");
        return -1;
    }

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtMediaPlayerObjectPath,
                                             BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH,
                                             "ListItems");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_open_container (&lDBusMsgIter,
                                      DBUS_TYPE_ARRAY,
                                      DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                      DBUS_TYPE_STRING_AS_STRING
                                      DBUS_TYPE_VARIANT_AS_STRING
                                      DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                      &lDBusMsgIterArr);
    if (apBtMediaFolderEndIndex) {
        {
            DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
            char*   key = "Start";
            int     type = DBUS_TYPE_UINT32;

            dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
                dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
                dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                    dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtMediaFolderStartIndex);
                dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
            dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
        }
        {
            DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant;
            char*   key = "End";
            int     type = DBUS_TYPE_UINT32;

            dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
                dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
                dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, (char *)&type, &lDBusMsgIterVariant);
                    dbus_message_iter_append_basic (&lDBusMsgIterVariant, type, &apBtMediaFolderEndIndex);
                dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
            dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
        }
    }
/*  {
        DBusMessageIter lDBusMsgIterDict, lDBusMsgIterVariant, lDBusMsgIterSubArray;
        char*   key = "Attributes";
        int     type = DBUS_TYPE_STRING;

        char array_type[5] = "a";
        strncat (array_type, (char*)&type, sizeof(array_type) - sizeof(type));

        dbus_message_iter_open_container (&lDBusMsgIterArr, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDict);
            dbus_message_iter_append_basic (&lDBusMsgIterDict, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container (&lDBusMsgIterDict, DBUS_TYPE_VARIANT, array_type, &lDBusMsgIterVariant);
                dbus_message_iter_open_container (&lDBusMsgIterVariant, DBUS_TYPE_ARRAY, (char *)&type, &lDBusMsgIterSubArray);
                    dbus_message_iter_append_fixed_array (&lDBusMsgIterSubArray, type, &apBtMediaFilter, apBtMediaFilterSize);
                dbus_message_iter_close_container (&lDBusMsgIterVariant, &lDBusMsgIterSubArray);
            dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterVariant);
        dbus_message_iter_close_container (&lDBusMsgIterArr, &lDBusMsgIterDict);
    }
*/
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterArr);

    dbus_error_init(&lDBusErr);
    lpDBusReply = dbus_connection_send_with_reply_and_block (pstlhBtIfce->pDBusConn, lpDBusMsg, DBUS_TIMEOUT_INFINITE, &lDBusErr);
    dbus_message_unref(lpDBusMsg);

    if (!lpDBusReply) {
        BTRCORELOG_ERROR ("lpDBusReply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);
    {
#if 0
        DBusMessageIter lDBusReplyIter;
        DBusMessageIter dictMsgIter, subDictMsgIter;
        DBusMessageIter element, subElementMgsIter, subVarElementMsgIter;
        char*        pcKey      = 0;
        char*        pcValue    = 0;
        unsigned int ui32Value  = 0;
        int          dbus_type  = DBUS_TYPE_INVALID;

        dbus_message_iter_init(lpDBusReply, &lDBusReplyIter);                                // lpDBusMsg is pointer to dbus message received

        if ((dbus_message_iter_get_arg_type (&lDBusReplyIter)) == DBUS_TYPE_INVALID) {
            BTRCORELOG_ERROR ("lDBusReplyIter - DBUS_TYPE_INVALID\n");
            dbus_message_unref(lpDBusReply);
            return -1;
        }

        dbus_message_iter_recurse(&lDBusReplyIter, &dictMsgIter);                           // pointer to first element DICTIONARY of the dbus messge received

        while ((dbus_type = dbus_message_iter_get_arg_type(&dictMsgIter)) != DBUS_TYPE_INVALID) {
            if (DBUS_TYPE_DICT_ENTRY == dbus_type) {
                dbus_message_iter_recurse(&dictMsgIter, &element);                          // pointer to element Object of the dbus messge received

                if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&element)) {
                    dbus_message_iter_get_basic(&element, &pcKey);

                    dbus_message_iter_next(&element);                                       // pointer to element Array of the dbus messge received


                    dbus_message_iter_recurse(&element, &subDictMsgIter);                   // pointer to element Dictionary of the dbus messge received

                    btrCore_BTParseMediaTrackMetadata ();
                }
            }
            if (!dbus_message_iter_has_next(&dictMsgIter)) {
                break;
            }
            else {
                dbus_message_iter_next(&dictMsgIter);
            }
        }
#endif
    }
    dbus_message_unref(lpDBusReply);

    return 0;
}


int
BtrCore_BTPlayMediaTrackItem (
    void*               apstBtIfceHdl,
    const char*         apBtMediaItemObjectPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl || !apBtMediaItemObjectPath) {
        BTRCORELOG_ERROR ("Invalid Args!!!\n");
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtMediaItemObjectPath,
                                             BT_DBUS_BLUEZ_MEDIA_ITEM_PATH,
                                             "Play");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);

    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}

#if 0
int
BtrCore_BTAddTrackItemToPlayList (
    void*               apstBtIfceHdl,
    const char*         apBtMediaItemObjectPath
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;

    if (!apstBtIfceHdl || !apBtMediaItemObjectPath) {
        BTRCORELOG_ERROR ("Invalid Args!!!\n");
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtMediaItemObjectPath,
                                             BT_DBUS_BLUEZ_MEDIA_ITEM_PATH,
                                             "AddtoNowPlaying");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);

    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    return 0;
}
#endif

int
BtrCore_BTRegisterLeAdvertisement (
    void*               apstBtIfceHdl,
    const char*         apBtAdapter
) {
    stBtIfceHdl*        pstlhBtIfce = NULL;
    DBusMessage*        lpDBusMsg = NULL;
    DBusPendingCall*    lpDBusPendCAdv = NULL;
    DBusMessageIter     lDBusMsgIterAdv;
    DBusMessageIter     lDBusMsgIterDictAdv;
    const char*         lpBtLeGattAdvEpPath;

    if ((NULL == apstBtIfceHdl) || (NULL == apBtAdapter)) {
        return -1;
    }

    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    BTRCORELOG_INFO("Registering advertisement\n");
   
    memset(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath, '\0', BT_MAX_STR_LEN);
    strncpy(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath, apBtAdapter, strlen(apBtAdapter));
    strncat(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath, BT_LE_GATT_SERVER_ADVERTISEMENT, (sizeof(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath) - strlen(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath) -1));

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                            apBtAdapter,
                                            BT_DBUS_BLUEZ_LE_ADV_MGR_PATH,
                                            "RegisterAdvertisement");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR("Can't allocate new method call\n");
        return -1;
    }

    lpBtLeGattAdvEpPath = pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath;
    dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIterAdv);
    dbus_message_iter_append_basic(&lDBusMsgIterAdv, DBUS_TYPE_OBJECT_PATH, &lpBtLeGattAdvEpPath);

    dbus_message_iter_open_container(&lDBusMsgIterAdv,
                                        DBUS_TYPE_ARRAY,
                                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                        DBUS_TYPE_STRING_AS_STRING
                                        DBUS_TYPE_VARIANT_AS_STRING
                                        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                        &lDBusMsgIterDictAdv);

    dbus_message_iter_close_container(&lDBusMsgIterAdv, &lDBusMsgIterDictAdv);


    if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendCAdv, -1)) {
        //Send, expect lpDBusReply using pending call object
        BTRCORELOG_ERROR("failed to send message!\n");
    }

    dbus_pending_call_set_notify(lpDBusPendCAdv, btrCore_BTPendingCallCheckReply, NULL, NULL);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);
    dbus_message_unref(lpDBusMsg);

    BTRCORELOG_ERROR("Exiting!!!!\n");
    return 0;
}

int
BtrCore_BTRegisterLeGatt (
    void*               apstBtIfceHdl,
    const char*         apBtAdapter
) {
    stBtIfceHdl*        pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*        lpDBusMsg   = NULL;
    DBusPendingCall*    lpDBusPendC = NULL;

    DBusError           lDBusErr;
    DBusMessageIter     lDBusMsgIter;
    DBusMessageIter     lDBusMsgIterDict;
    dbus_bool_t         lDBusOp;

    const char*         lpBtLeGattSrvEpPath = BT_LE_GATT_SERVER_ENDPOINT;

    if (!apstBtIfceHdl || !apBtAdapter)
        return -1;

    if (!strncmp(apBtAdapter, pstlhBtIfce->pcBTDAdapterPath, strlen(pstlhBtIfce->pcBTDAdapterPath))) {

        char lCurAdapterAddress[BT_MAX_DEV_PATH_LEN] = "\0";

        if (!pstlhBtIfce->pcBTDAdapterAddr) {
            unBTOpIfceProp     lunBtOpAdapProp;
            lunBtOpAdapProp.enBtAdapterProp = enBTAdPropAddress;

            /* Get current device address */
            if (BtrCore_BTGetProp(pstlhBtIfce,
                                 (BtrCore_BTGetAdapterPath(pstlhBtIfce, NULL)),
                                 enBTAdapter,
                                 lunBtOpAdapProp,
                                 lCurAdapterAddress)) {
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            }
        }
        else {
            strncpy(lCurAdapterAddress, pstlhBtIfce->pcBTDAdapterAddr, strlen(pstlhBtIfce->pcBTDAdapterAddr));
        }

        char *current_pos = strchr(lCurAdapterAddress, ':');
        while (current_pos){
            *current_pos = '_';
            current_pos = strchr(current_pos, ':');
        }

        memset(pstlhBtIfce->pcBTAdapterGattSrvEpPath, '\0', sizeof(char) * BT_MAX_DEV_PATH_LEN);
        strncpy(pstlhBtIfce->pcBTAdapterGattSrvEpPath, pstlhBtIfce->pcBTDAdapterPath, strlen(pstlhBtIfce->pcBTDAdapterPath));
        strncat(pstlhBtIfce->pcBTAdapterGattSrvEpPath, "/dev_", (sizeof(pstlhBtIfce->pcBTAdapterGattSrvEpPath) - strlen(pstlhBtIfce->pcBTAdapterGattSrvEpPath) -1));
        strncat(pstlhBtIfce->pcBTAdapterGattSrvEpPath, lCurAdapterAddress, strlen(lCurAdapterAddress));

        lpBtLeGattSrvEpPath = pstlhBtIfce->pcBTAdapterGattSrvEpPath;
    }
    else {
        lpBtLeGattSrvEpPath = BT_LE_GATT_SERVER_ENDPOINT;
    }

    dbus_error_init(&lDBusErr);
    lDBusOp = dbus_connection_try_register_object_path(pstlhBtIfce->pDBusConn, lpBtLeGattSrvEpPath, &gDBusLeGattEndpointVTable, pstlhBtIfce, &lDBusErr);
    if (!lDBusOp) {
        btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
        BTRCORELOG_ERROR ("Can't Register Le Gatt Object - %s\n", lpBtLeGattSrvEpPath);

        if (strcmp(lDBusErr.name, DBUS_ERROR_OBJECT_PATH_IN_USE) != 0)
            return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_GATT_MGR_PATH,
                                             "RegisterApplication");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    dbus_message_iter_init_append (lpDBusMsg, &lDBusMsgIter);
    dbus_message_iter_append_basic (&lDBusMsgIter, DBUS_TYPE_OBJECT_PATH, &lpBtLeGattSrvEpPath);

    dbus_message_iter_open_container(&lDBusMsgIter,
                                     DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                     &lDBusMsgIterDict);
    {
        // Empty
    }
    dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);


    if (!dbus_connection_send_with_reply(pstlhBtIfce->pDBusConn, lpDBusMsg, &lpDBusPendC, -1)) { //Send and expect lpDBusReply using pending call object
        BTRCORELOG_ERROR ("failed to send message!\n");
    }


    dbus_pending_call_set_notify(lpDBusPendC, btrCore_BTPendingCallCheckReply, NULL, NULL);

    dbus_connection_flush(pstlhBtIfce->pDBusConn);
    dbus_message_unref(lpDBusMsg);

    return 0;
}


int
BtrCore_BTUnRegisterLeGatt (
    void*           apstBtIfceHdl,
    const char*     apBtAdapter
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    dbus_bool_t     lDBusOp;
    const char*     lpBtLeGattSrvEpPath;
    const char*     lpBtLeGattAdvEpPath;

    if (!apstBtIfceHdl || !apBtAdapter)
        return -1;

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_LE_ADV_MGR_PATH,
                                             "UnregisterAdvertisement");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lpBtLeGattAdvEpPath = pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath;
    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &lpBtLeGattAdvEpPath, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }


    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtAdapter,
                                             BT_DBUS_BLUEZ_GATT_MGR_PATH,
                                             "UnregisterApplication");
    if (!lpDBusMsg) {
        BTRCORELOG_ERROR ("Can't allocate new method call\n");
        return -1;
    }

    lpBtLeGattSrvEpPath = (pstlhBtIfce->pcBTAdapterGattSrvEpPath[0] == '\0') ? BT_LE_GATT_SERVER_ENDPOINT : pstlhBtIfce->pcBTAdapterGattSrvEpPath;
    dbus_message_append_args(lpDBusMsg, DBUS_TYPE_OBJECT_PATH, &lpBtLeGattSrvEpPath, DBUS_TYPE_INVALID);

    lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
    dbus_message_unref(lpDBusMsg);

    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Not enough memory for message send\n");
        return -1;
    }

    /* Get object paths */
    stBTLeGattInfo* lpstBTLeGattInfo = &pstlhBtIfce->stBTLeGatt;
    char            lCurAdapterAddress[BT_MAX_DEV_PATH_LEN] = "\0";


    if (!pstlhBtIfce->pcBTDAdapterAddr) {
        unBTOpIfceProp     lunBtOpAdapProp;
        lunBtOpAdapProp.enBtAdapterProp = enBTAdPropAddress;

        /* Get current device address */
        if (BtrCore_BTGetProp(pstlhBtIfce,
                             (BtrCore_BTGetAdapterPath(pstlhBtIfce, NULL)),
                             enBTAdapter,
                             lunBtOpAdapProp,
                             lCurAdapterAddress)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    else {
        strncpy(lCurAdapterAddress, pstlhBtIfce->pcBTDAdapterAddr,strlen(pstlhBtIfce->pcBTDAdapterAddr));
    }

    //pstlhBtIfce->fpcBTLeLocalGattPath(NULL, lCurAdapterAddress, &lpstBTLeGattSrv, &lNumGattServices, pstlhBtIfce->pcBLePathUserData);
    if (pstlhBtIfce->fpcBTLeGattPath) {
        pstlhBtIfce->fpcBTLeGattPath(enBTAdvertisement, enBTLeGattOpUnknown, NULL, lCurAdapterAddress, enBTDevStUnknown, (void*)lpstBTLeGattInfo, pstlhBtIfce->pcBLePathUserData);
    }

    for (int serviceIndex = 0; serviceIndex < lpstBTLeGattInfo->nNumGattServices; serviceIndex++) {
        stBTLeGattService *lpstBTLeGattService = &lpstBTLeGattInfo->astBTRGattService[serviceIndex];
        btrCore_BTReleaseLEGattObjPath(lpstBTLeGattService->servicePath, (void*)pstlhBtIfce);

        for (int index = 0; index < lpstBTLeGattService->ui16NumberOfGattChar; index++) {
            stBTLeGattChar *lpstBTLeGattChar = &lpstBTLeGattService->astBTRGattChar[index];
            btrCore_BTReleaseLEGattObjPath(lpstBTLeGattChar->charPath, (void*)pstlhBtIfce);

            for (int DescIndex = 0; DescIndex < lpstBTLeGattChar->ui16NumberOfGattDesc; DescIndex++) {
                stBTLeGattDesc *lpstBTLeGattDesc = &lpstBTLeGattChar->atBTRGattDesc[DescIndex];
                btrCore_BTReleaseLEGattObjPath(lpstBTLeGattDesc->descPath, (void*)pstlhBtIfce);
            }
        }
    }

    lDBusOp = dbus_connection_unregister_object_path(pstlhBtIfce->pDBusConn, lpBtLeGattSrvEpPath);
    if (!lDBusOp) {
        BTRCORELOG_ERROR ("Can't Unregister Object\n");
        return -1;
    }


    dbus_connection_flush(pstlhBtIfce->pDBusConn);

    btrCore_BTUnRegisterGattService((void*)pstlhBtIfce, NULL);

    memset(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath, '\0', BT_MAX_STR_LEN);

    return 0;
}


int
BtrCore_BTPerformLeGattOp (
    void*           apstBtIfceHdl,
    const char*     apBtGattPath,
    enBTOpIfceType  aenBTOpIfceType,
    enBTLeGattOp    aenBTLeGattOp,
    char*           apLeGatOparg1,
    char*           apLeGatOparg2,
    char*           rpLeOpRes
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;
    DBusMessage*    lpDBusMsg   = NULL;
    DBusMessage*    lpDBusReply = NULL;
    DBusError       lDBusErr;
    char            lcBtGattOpString[64] = "\0";
    char*           lpBtGattInterface    = NULL;
    int             rc = 0;

    if (!apstBtIfceHdl || !apBtGattPath) {
       BTRCORELOG_ERROR ("LE Gatt Path is NULL!!!");
       return -1;
    }


    if (aenBTOpIfceType == enBTGattCharacteristic) {
        lpBtGattInterface = BT_DBUS_BLUEZ_GATT_CHAR_PATH;
    }
    else if (aenBTOpIfceType == enBTGattDescriptor) {
        lpBtGattInterface = BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH;
    }

    switch (aenBTLeGattOp) {
    case enBTLeGattOpReadValue:
        strcpy(lcBtGattOpString, "ReadValue");
        break;
    case enBTLeGattOpWriteValue:
        strcpy(lcBtGattOpString, "WriteValue");
        break;
    case enBTLeGattOpStartNotify:
        strcpy(lcBtGattOpString, "StartNotify");
        break;
    case enBTLeGattOpStopNotify:
        strcpy(lcBtGattOpString, "StopNotify");
        break;
    case enBTLeGattOpUnknown:
    default:
        rc = -1;
        break;
    }

    if (rc == -1){
        BTRCORELOG_ERROR ("Invalid enBTLeGattOp Option %d !!!", aenBTLeGattOp);
        return rc;
    }

    lpDBusMsg = dbus_message_new_method_call(BT_DBUS_BLUEZ_PATH,
                                             apBtGattPath,
                                             lpBtGattInterface,
                                             lcBtGattOpString);
    if (!lpDBusMsg) {
       BTRCORELOG_ERROR ("Can't allocate new method call\n");
       return -1;
    }

    if (aenBTLeGattOp == enBTLeGattOpWriteValue && apLeGatOparg2) {

        BTRCORELOG_TRACE ("apLeGatOparg2 : %s \n", apLeGatOparg2);
        unsigned char lpcLeWriteByteData[BT_MAX_STR_LEN];
        int lWriteByteDataLen  = 0;
        unsigned short u16idx  = 0;
        unsigned char u8val    = 0;
        unsigned char *laptr   = (unsigned char*)apLeGatOparg2;

        memset (lpcLeWriteByteData, 0, BT_MAX_STR_LEN);

        while (*laptr) {
            // Below logic is to map literals in a string to Hex values. Ex: hexOf_a = 'a' - 87 | hexOf_B = 'B' - 55 | hexOf_0 = '0' - 48
            // TODO : May be we should check apLeGatOparg2 in upper layer, if it has only [a..f] | [A..F] | [0..9]
            u8val = *laptr - ((*laptr <= 'f' && *laptr >= 'a')? ('a'-10) : (*laptr <= 'F' && *laptr >= 'A')? ('A'-10) : '0');

            if (u16idx % 2) {
                lpcLeWriteByteData[u16idx++/2] |= u8val;
            }
            else {
                lpcLeWriteByteData[u16idx++/2]  = u8val << 4;
            }

            laptr++;
        }

        lpcLeWriteByteData[u16idx] = '\0';
        lWriteByteDataLen          = u16idx/2;

        for(int i = 0; i <= lWriteByteDataLen; i++ ) {
            BTRCORELOG_TRACE ("lpcLeWriteByteData[%d] : %02x\n", i, lpcLeWriteByteData[i]);
        }

        const unsigned char* lpcLeWriteByteDataC = ((unsigned char*)lpcLeWriteByteData);

        dbus_message_append_args (lpDBusMsg, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &lpcLeWriteByteDataC, lWriteByteDataLen, DBUS_TYPE_INVALID);
  
        {
            DBusMessageIter lDBusMsgIter, lDBusMsgIterDict;
            dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
            dbus_message_iter_open_container(&lDBusMsgIter,
                                            DBUS_TYPE_ARRAY,
                                            DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                            DBUS_TYPE_STRING_AS_STRING
                                            DBUS_TYPE_VARIANT_AS_STRING
                                            DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                            &lDBusMsgIterDict);
            {
                DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
                char*  lpcKey = "offset";
                unsigned short lus16Value = 0;
                int    i32DBusType = DBUS_TYPE_UINT16;

                dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                    dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
                    dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                        dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lus16Value);
                    dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
                dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
            }
#if 0
            {
              DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
              char*  lpcKey   = "device";
              char*  lpcValue = apLeGatOparg1;
              int    i32DBusType = DBUS_TYPE_OBJECT_PATH;

              dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
                  dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
                  dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                      dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lpcValue);
                  dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
              dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
           }
#endif
           dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);
        }
    }
    else if (aenBTLeGattOp == enBTLeGattOpReadValue) {

        DBusMessageIter lDBusMsgIter, lDBusMsgIterDict;
        dbus_message_iter_init_append(lpDBusMsg, &lDBusMsgIter);
        dbus_message_iter_open_container(&lDBusMsgIter,
                                        DBUS_TYPE_ARRAY,
                                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                        DBUS_TYPE_STRING_AS_STRING
                                        DBUS_TYPE_VARIANT_AS_STRING
                                        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
                                        &lDBusMsgIterDict);
        {
          DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
          char*  lpcKey = "offset";
          unsigned short lus16Value = 0;
          int    i32DBusType = DBUS_TYPE_UINT16;
  
          dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
              dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
              dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                  dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lus16Value);
              dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant); 
          dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);        
       }
       {
          DBusMessageIter lDBusMsgIterDictStr, lDBusMsgIterVariant;
          char*  lpcKey   = "device";
          char*  lpcValue = apLeGatOparg1;      // "/org/bluez/hci0/dev_D3_DE_82_9A_7F_1E";
          int    i32DBusType = DBUS_TYPE_OBJECT_PATH;

          dbus_message_iter_open_container(&lDBusMsgIterDict, DBUS_TYPE_DICT_ENTRY, NULL, &lDBusMsgIterDictStr);
              dbus_message_iter_append_basic (&lDBusMsgIterDictStr, DBUS_TYPE_STRING, &lpcKey);
              dbus_message_iter_open_container (&lDBusMsgIterDictStr, DBUS_TYPE_VARIANT, (char *)&i32DBusType, &lDBusMsgIterVariant);
                  dbus_message_iter_append_basic (&lDBusMsgIterVariant, i32DBusType, &lpcValue);
              dbus_message_iter_close_container (&lDBusMsgIterDictStr, &lDBusMsgIterVariant);
          dbus_message_iter_close_container (&lDBusMsgIterDict, &lDBusMsgIterDictStr);
       }
       dbus_message_iter_close_container (&lDBusMsgIter, &lDBusMsgIterDict);
    }

    if (aenBTLeGattOp == enBTLeGattOpReadValue) {

       dbus_error_init(&lDBusErr);
       lpDBusReply = dbus_connection_send_with_reply_and_block(pstlhBtIfce->pDBusConn, lpDBusMsg, -1, &lDBusErr);
       dbus_message_unref(lpDBusMsg);

       if (!lpDBusReply) {
          BTRCORELOG_ERROR ("Send Reply Block Method Failed!!!\n");
          btrCore_BTHandleDusError(&lDBusErr, __LINE__, __FUNCTION__);
          return -1;
       }

       DBusMessageIter arg_i, element_i;
       if (!dbus_message_iter_init(lpDBusReply, &arg_i)) {
          BTRCORELOG_ERROR ("lpDBusReply has no information.");
          return -1;
       }

       int dbus_type = DBUS_TYPE_INVALID;
       char byteValue[BT_MAX_STR_LEN] = "\0";
       char hexValue[] = "0123456789abcdef";
       unsigned short u16idx = 0;
       char ch = '\0';

       memset (byteValue, 0, sizeof(byteValue));

       dbus_message_iter_recurse(&arg_i, &element_i);

       while ((dbus_type = dbus_message_iter_get_arg_type(&element_i)) != DBUS_TYPE_INVALID) {

            if (dbus_type == DBUS_TYPE_BYTE) {
               dbus_message_iter_get_basic(&element_i, &ch);
               byteValue[u16idx++] = hexValue[ch >> 4];
               byteValue[u16idx++] = hexValue[ch &  0x0F];
            }
            if (!dbus_message_iter_next(&element_i)) {
               break;
            }
       }
       byteValue[u16idx] = '\0';
       strncpy (rpLeOpRes, byteValue, BT_MAX_STR_LEN-1);
       BTRCORELOG_DEBUG ("rpLeOpRes : %s\n", rpLeOpRes);
       dbus_message_unref(lpDBusReply);

    } else {
       dbus_bool_t  lDBusOp;

       lDBusOp = dbus_connection_send(pstlhBtIfce->pDBusConn, lpDBusMsg, NULL);
       dbus_message_unref(lpDBusMsg);

       if (!lDBusOp) {
          BTRCORELOG_ERROR ("Not enough memory for message send\n");
          return -1;
       }
    
       dbus_connection_flush(pstlhBtIfce->pDBusConn);
    }

    return 0;
}


int
BtrCore_BTSendReceiveMessages (
    void*           apstBtIfceHdl
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl)
        return -1;


    if(dbus_connection_read_write_dispatch(pstlhBtIfce->pDBusConn, 25) != TRUE) {
        return -1;
    }

    return 0;
}


// Outgoing callbacks Registration Interfaces
int
BtrCore_BTRegisterAdapterStatusUpdateCb (
    void*                                   apstBtIfceHdl,
    fPtr_BtrCore_BTAdapterStatusUpdateCb    afpcBAdapterStatusUpdate,
    void*                                   apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBAdapterStatusUpdate)
        return -1;


    pstlhBtIfce->fpcBAdapterStatusUpdate    = afpcBAdapterStatusUpdate;
    pstlhBtIfce->pcBAdapterStatusUserData   = apUserData;

    return 0;
}


int
BtrCore_BTRegisterDevStatusUpdateCb (
    void*                               apstBtIfceHdl,
    fPtr_BtrCore_BTDevStatusUpdateCb    afpcBDevStatusUpdate,
    void*                               apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBDevStatusUpdate)
        return -1;


    pstlhBtIfce->fpcBDevStatusUpdate    = afpcBDevStatusUpdate;
    pstlhBtIfce->pcBDevStatusUserData   = apUserData;

    return 0;
}


int
BtrCore_BTRegisterMediaStatusUpdateCb (
    void*                               apstBtIfceHdl,
    fPtr_BtrCore_BTMediaStatusUpdateCb  afpcBMediaStatusUpdate,
    void*                               apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBMediaStatusUpdate)
        return -1;


    pstlhBtIfce->fpcBMediaStatusUpdate    = afpcBMediaStatusUpdate;
    pstlhBtIfce->pcBMediaStatusUserData   = apUserData;

    return 0;
}

int
BtrCore_BTRegisterConnIntimationCb (
    void*                       apstBtIfceHdl,
    fPtr_BtrCore_BTConnIntimCb  afpcBConnIntim,
    void*                       apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBConnIntim)
        return -1;


    pstlhBtIfce->fpcBConnectionIntimation   = afpcBConnIntim;
    pstlhBtIfce->pcBConnIntimUserData       = apUserData;

    return 0;
}


int
BtrCore_BTRegisterConnAuthCb (
    void*                       apstBtIfceHdl,
    fPtr_BtrCore_BTConnAuthCb   afpcBConnAuth,
    void*                       apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBConnAuth)
        return -1;


    pstlhBtIfce->fpcBConnectionAuthentication   = afpcBConnAuth;
    pstlhBtIfce->pcBConnAuthUserData            = apUserData;

    return 0;
}


int
BtrCore_BTRegisterNegotiateMediaCb (
    void*                           apstBtIfceHdl,
    const char*                     apBtAdapter,
    fPtr_BtrCore_BTNegotiateMediaCb afpcBNegotiateMedia,
    void*                           apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !apBtAdapter || !afpcBNegotiateMedia)
        return -1;


    pstlhBtIfce->fpcBNegotiateMedia     = afpcBNegotiateMedia;
    pstlhBtIfce->pcBNegMediaUserData    = apUserData;

    return 0;
}


int
BtrCore_BTRegisterTransportPathMediaCb (
    void*                               apstBtIfceHdl,
    const char*                         apBtAdapter,
    fPtr_BtrCore_BTTransportPathMediaCb afpcBTransportPathMedia,
    void*                               apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !apBtAdapter || !afpcBTransportPathMedia)
        return -1;


    pstlhBtIfce->fpcBTransportPathMedia     = afpcBTransportPathMedia;
    pstlhBtIfce->pcBTransPathMediaUserData  = apUserData;

    return 0;
}


int
BtrCore_BTRegisterMediaPlayerPathCb (
    void*                               apstBtIfceHdl,
    const char*                         apBtAdapter,
    fPtr_BtrCore_BTMediaPlayerPathCb    afpcBTMediaPlayerPath,
    void*                               apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !apBtAdapter || !afpcBTMediaPlayerPath)
        return -1;


    pstlhBtIfce->fpcBTMediaPlayerPath       = afpcBTMediaPlayerPath;
    pstlhBtIfce->pcBMediaPlayerPathUserData = apUserData;

    return 0;
}


int
BtrCore_BTRegisterMediaBrowserUpdateCb (
    void*                               apstBtIfceHdl,
    fPtr_BtrCore_BTMediaBrowserPathCb   afpcBTMediaBrowserPath,
    void*                               apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !afpcBTMediaBrowserPath)
        return -1;


    pstlhBtIfce->fpcBTMediaBrowserPath       = afpcBTMediaBrowserPath;
    pstlhBtIfce->pcBMediaBrowserPathUserData = apUserData;

    return 0;
}


int
BtrCore_BTRegisterLEGattInfoCb (
    void*                           apstBtIfceHdl,
    const char*                     apBtAdapter,
    fPtr_BtrCore_BTLeGattPathCb     afpcBtLeGattPath,
    void*                           apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    if (!apstBtIfceHdl || !apBtAdapter || !afpcBtLeGattPath)
        return -1;


    pstlhBtIfce->fpcBTLeGattPath    = afpcBtLeGattPath;
    pstlhBtIfce->pcBLePathUserData  = apUserData;

    return 0;
}


int
BtrCore_BTRegisterLEAdvInfoCb (
    void*                                apstBtIfceHdl,
    const char*                          apBtAdapter,
    fPtr_BtrCore_BTLeAdvertisementCb     afpcBtLeAdvPath,
    void*                                apUserData
) {
    stBtIfceHdl*    pstlhBtIfce = NULL;

    if (!apstBtIfceHdl || !apBtAdapter || !afpcBtLeAdvPath) {
        return -1;
    }

    pstlhBtIfce = (stBtIfceHdl*)apstBtIfceHdl;

    pstlhBtIfce->fpcBTLeAdvPath = afpcBtLeAdvPath;
    pstlhBtIfce->pcBLeAdvUserData = apUserData;

    return 0;
}

//TODO: Add BtrCore_BTRegisterLEGattInfocB which can be used to register function pointer 
//which can be called back when the following interfaces gets added or removed or the properties
//changes on the following Interfaces
// BT_DBUS_BLUEZ_GATT_SERVICE_PATH   
// BT_DBUS_BLUEZ_GATT_CHAR_PATH      
// BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH


/* Incoming Callbacks */
static DBusHandlerResult
btrCore_BTDBusConnectionFilterCb (
    DBusConnection*  apDBusConn,
    DBusMessage*     apDBusMsg,
    void*            apvUserData
) {
    int              i32OpRet = -1;
    int              li32MessageType;
    stBTAdapterInfo* pstBTAdapterInfo   = NULL;
    stBTDeviceInfo*  pstBTDeviceInfo    = NULL;
    stBTMediaInfo*   pstBTMediaInfo     = NULL;
    const char*      lpcSender          = NULL;
    const char*      lpcDestination     = NULL;
    stBtIfceHdl*     pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    BTRCORELOG_DEBUG ("Connection Filter Activated....\n");

    if (!apDBusConn || !apDBusMsg || !apvUserData) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    pstBTAdapterInfo = &pstlhBtIfce->lstBTAdapterInfo;
    pstBTDeviceInfo  = &pstlhBtIfce->lstBTDeviceInfo;
    pstBTMediaInfo   = &pstlhBtIfce->lstBTMediaInfo;

    memset(pstBTAdapterInfo, 0, sizeof(stBTAdapterInfo));
    memset(pstBTDeviceInfo, 0, sizeof(stBTDeviceInfo));
    memset(pstBTMediaInfo, 0, sizeof(stBTMediaInfo));
    pstBTDeviceInfo->i32RSSI = INT_MIN;

    li32MessageType = dbus_message_get_type(apDBusMsg);
    lpcSender       = dbus_message_get_sender(apDBusMsg);
    lpcDestination  = dbus_message_get_destination(apDBusMsg);
  
    BTRCORELOG_INFO ("%s Sender=%s -> Dest=%s Path=%s; Interface=%s; Member=%s\n",
                    btrCore_DBusType2Name(li32MessageType),
                    lpcSender ? lpcSender : "Null",
                    lpcDestination ? lpcDestination : "Null",
                    dbus_message_get_path(apDBusMsg),
                    dbus_message_get_interface(apDBusMsg),
                    dbus_message_get_member(apDBusMsg));
    const char* lSignature = dbus_message_get_signature(apDBusMsg);
    BTRCORELOG_TRACE("Signature is %s\n", lSignature);

    if (li32MessageType == DBUS_MESSAGE_TYPE_ERROR) {
        const char* lpcError = dbus_message_get_error_name(apDBusMsg);
        BTRCORELOG_ERROR ("Error = %s\n", lpcError ? lpcError : NULL);
    }
    else if (dbus_message_is_signal(apDBusMsg, DBUS_INTERFACE_PROPERTIES, "PropertiesChanged")) {
        BTRCORELOG_DEBUG ("Property Changed!\n");

        DBusMessageIter lDBusMsgIter;
        const char*     lpcDBusIface = NULL;

        dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
        dbus_message_iter_get_basic(&lDBusMsgIter, &lpcDBusIface);
        dbus_message_iter_next(&lDBusMsgIter);

        if (dbus_message_iter_get_arg_type(&lDBusMsgIter) == DBUS_TYPE_ARRAY) {
            if (lpcDBusIface) {
                if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_ADAPTER_PATH)) {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_ADAPTER_PATH);

                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";

                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_ADAPTER_PATH, str);

                                /* Can listen on below events if required
                                Discovering
                                Name
                                Alias
                                Class
                                Powered
                                UUIDs
                                Discoverable
                                Pairable
                                Connectable
                                DiscoverableTimeout
                                PairableTimeout
                                */
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }

                    const char* adapter_path = dbus_message_get_path(apDBusMsg);
                    strncpy (pstBTAdapterInfo->pcPath, adapter_path, strlen(adapter_path) < BT_MAX_DEV_PATH_LEN ? strlen(adapter_path) : BT_MAX_DEV_PATH_LEN - 1);
                    i32OpRet = btrCore_BTParseAdapter(&lDBusMsgIter, pstBTAdapterInfo);

                    pstlhBtIfce->ui32IsAdapterDiscovering = pstBTAdapterInfo->bDiscovering;
                    if (pstBTAdapterInfo->bDiscovering) {
                        BTRCORELOG_INFO ("Adapter Started Discovering | %d\n", pstlhBtIfce->ui32IsAdapterDiscovering);
                    }
                    else {
                        BTRCORELOG_INFO ("Adapter Stopped Discovering | %d\n", pstlhBtIfce->ui32IsAdapterDiscovering);
                    }

                    if (!i32OpRet) {
                        if (pstlhBtIfce->fpcBAdapterStatusUpdate) {
                            if(pstlhBtIfce->fpcBAdapterStatusUpdate(enBTAdPropDiscoveryStatus, pstBTAdapterInfo, pstlhBtIfce->pcBAdapterStatusUserData)) {
                            }
                        }
                    }
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_DEVICE_PATH)) {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_DEVICE_PATH);
                    DBusMessageIter lDBusMsgIterDict;
                    const char* pui8DevPath = dbus_message_get_path(apDBusMsg);
                    int settingRSSItoZero = 0;
                    int bPairingEvent = 0;
                    int bPaired = 0;
                    int bSrvResolvedEvent = 0; //TODO: Bad way to do this. Live with it for now
                    int bSrvResolved = 0;
                    int bConnectEvent = 0; //TODO: Bad way to do this. Live with it for now
                    int bConnected = 0;
                    int bRssiEvent = 0; //TODO: Bad way to do this. Live with it for now
                    short i16RSSI = 0;

                    i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, pui8DevPath);

                    dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                    if (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_INVALID) {
                        settingRSSItoZero = 1;
                        BTRCORELOG_INFO ("Setting Dev %s RSSI to 0...\n", pstBTDeviceInfo->pcAddress);
                    }
                    else {
                        DBusMessageIter lDBusMsgParse;

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgParse);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgParse) == DBUS_TYPE_STRING) {
                                DBusMessageIter lDBusMsgPropertyValue;
                                const char *pNameOfProperty = NULL;

                                dbus_message_iter_get_basic(&lDBusMsgParse, &pNameOfProperty);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_DEVICE_PATH, pNameOfProperty);

                                if (strcmp (pNameOfProperty, "Paired") == 0) {
                                    dbus_message_iter_next(&lDBusMsgParse);
                                    dbus_message_iter_recurse(&lDBusMsgParse, &lDBusMsgPropertyValue);
                                    dbus_message_iter_get_basic(&lDBusMsgPropertyValue, &bPaired);
                                    bPairingEvent = 1;
                                    BTRCORELOG_DEBUG ("bPaired = %d\n", bPaired);
                                }
                                else if (strcmp (pNameOfProperty, "Connected") == 0) {
                                    dbus_message_iter_next(&lDBusMsgParse);
                                    dbus_message_iter_recurse(&lDBusMsgParse, &lDBusMsgPropertyValue);
                                    dbus_message_iter_get_basic(&lDBusMsgPropertyValue, &bConnected);
                                    bConnectEvent = 1;
                                    BTRCORELOG_DEBUG ("bConnected = %d\n", bConnected);
                                }
                                else if (strcmp (pNameOfProperty, "ServicesResolved") == 0) {
                                    dbus_message_iter_next(&lDBusMsgParse);
                                    dbus_message_iter_recurse(&lDBusMsgParse, &lDBusMsgPropertyValue);
                                    dbus_message_iter_get_basic(&lDBusMsgPropertyValue, &bSrvResolved);
                                    bSrvResolvedEvent = 1;
                                    BTRCORELOG_DEBUG ("bServicesResolved = %d\n", bSrvResolved);
                                }
                                else if (strcmp (pNameOfProperty, "RSSI") == 0) {
                                    dbus_message_iter_next(&lDBusMsgParse);
                                    dbus_message_iter_recurse(&lDBusMsgParse, &lDBusMsgPropertyValue);
                                    dbus_message_iter_get_basic(&lDBusMsgPropertyValue, &i16RSSI);
                                    pstBTDeviceInfo->i32RSSI = i16RSSI;
                                    bRssiEvent = 1;
                                    BTRCORELOG_DEBUG ("Event - bi32Rssi = %d\n", pstBTDeviceInfo->i32RSSI);
                                }


                                /* Can listen on below events if required
                                Name
                                Alias
                                Class
                                Address
                                Appearance
                                Icon
                                UUIDs
                                Trusted
                                Paired
                                Connected
                                ServicesResolved
                                ServiceData
                                ManufacturerData
                                RSSI
                                TxPower
                                Modalias
                                Blocked
                                LegacyPairing
                                */
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }

                    if (!i32OpRet) {
                        enBTDeviceState lenBtDevState = enBTDevStUnknown; 
                        enBTDeviceType  lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);

                        if (bPairingEvent) {
                            const char* value = NULL;
                            BTRCORELOG_INFO ("Parsing Property Changed event figured that its pairing change..\n");
                            if (bPaired) {
                                    value = "paired";
                            }
                            else {
                                    value = "unpaired";
                            }
                            strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                            strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                            strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                            lenBtDevState = enBTDevStPropChanged;
                            if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                }
                            }
                        }
                        else if (pstBTDeviceInfo->bPaired) {
                            if (settingRSSItoZero || bRssiEvent) {
                                if (pstlhBtIfce->ui32IsAdapterDiscovering || pstBTDeviceInfo->bConnected) {
                                    lenBtDevState = enBTDevStRSSIUpdate;
                                }
                            }
                            else {
                                if (pstBTDeviceInfo->bConnected) {
                                    const char* value = "connected";

                                    strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                                    strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                                    lenBtDevState = enBTDevStPropChanged;
                                }
                                else if (!pstBTDeviceInfo->bConnected) {
                                    const char* value = "disconnected";

                                    strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                                    strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                                    lenBtDevState = enBTDevStPropChanged;

                                    if (enBTDevAudioSink == lenBTDevType && pstlhBtIfce->ui32DevLost) {
                                        lenBtDevState = enBTDevStLost;
                                    }
                                }
                                pstlhBtIfce->ui32DevLost = 0;
                            }
 
                            if (enBTDevAudioSource != lenBTDevType || strcmp(pstlhBtIfce->pcDeviceCurrState, "connected")) {
                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                            }
                            else if ((enBTDevAudioSource == lenBTDevType) && bSrvResolvedEvent && !strcmp(pstBTDeviceInfo->pcDevicePrevState, "connected") && !strcmp(pstBTDeviceInfo->pcDeviceCurrState, "connected")) {
                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                            }
                        }
                        else if (!pstBTDeviceInfo->bPaired && !pstBTDeviceInfo->bConnected &&
                                 strncmp(pstlhBtIfce->pcLeDeviceAddress, pstBTDeviceInfo->pcAddress,
                                     ((BT_MAX_STR_LEN > strlen(pstBTDeviceInfo->pcAddress))?strlen(pstBTDeviceInfo->pcAddress):BT_MAX_STR_LEN))) {

                            if (pstlhBtIfce->ui32IsAdapterDiscovering && settingRSSItoZero) {
                                lenBtDevState = enBTDevStRSSIUpdate;
                            }
                            else {
                                lenBtDevState = enBTDevStFound;
                            }

                            if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                }
                            }
                        }
                        else if (lenBTDevType == enBTDevUnknown) { //TODO: Have to figure out a way to identify it as a LE device

                            if (bConnectEvent) {
                                if (pstBTDeviceInfo->bConnected) {
                                    const char* value = "connected";

                                    strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcLeDeviceCurrState, BT_MAX_STR_LEN - 1);
                                    strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcLeDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcLeDeviceAddress, pstBTDeviceInfo->pcAddress, BT_MAX_STR_LEN - 1);
                                }
                                else if (!pstBTDeviceInfo->bConnected) {
                                    const char* value = "disconnected";

                                    strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcLeDeviceCurrState, BT_MAX_STR_LEN - 1);
                                    strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcLeDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                    strncpy(pstlhBtIfce->pcLeDeviceAddress, "none", BT_MAX_STR_LEN - 1);
                                }

                                lenBTDevType  = enBTDevLE;
                                lenBtDevState = enBTDevStPropChanged;

                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                            }

                            if (bSrvResolvedEvent) {
                                if (pstlhBtIfce->fpcBTLeGattPath) {
                                    pstlhBtIfce->fpcBTLeGattPath(enBTDevice, enBTLeGattOpUnknown, pui8DevPath, pstBTDeviceInfo->pcAddress, enBTDevStPropChanged, NULL, pstlhBtIfce->pcBLePathUserData);
                                }
                            }
                            else if (!bConnectEvent) {
                                lenBTDevType  = enBTDevLE;
                                lenBtDevState = enBTDevStPropChanged;

                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                            }
                        }
                    }
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH)) {
                    const char*             apcMediaTransIface = dbus_message_get_path(apDBusMsg);
                    char                    apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                    unsigned int            ui32DeviceIfceLen = strstr(apcMediaTransIface, "/fd") - apcMediaTransIface;
                    enBTDeviceType          lenBTDevType = enBTDevUnknown;
                    char*                   apcDevAddr  = NULL;
                    enBTMediaTransportProp  lenBTMedTransProp = enBTMedTPropUnknown;
                    unsigned short          lVolume = 0;
                    char*                   pcState = 0;

                    if (!strncmp(pstlhBtIfce->pcBTVersion, BT_BLUEZ_VERSION_5_54, strlen(BT_BLUEZ_VERSION_5_54))) {
                        ui32DeviceIfceLen = strstr(apcMediaTransIface, "/sep") - apcMediaTransIface;
                    }

                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH);
                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1, lDBusMsgIterVal;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";

                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH, str);

                                /* Can listen on below events if required
                                State
                                Volume
                                Delay
                                */
                                if (!strncmp(str, "State", strlen("State"))) {
                                    lenBTMedTransProp = enBTMedTPropState;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcState);
                                }
                                else if (!strncmp(str, "Volume", strlen("Volume"))) {
                                    lenBTMedTransProp = enBTMedTPropVol;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &lVolume);
                                }
                                else if (!strncmp(str, "Delay", strlen("Delay"))) {
                                    lenBTMedTransProp = enBTMedTPropDelay;
                                }
                                else {
                                    lenBTMedTransProp = enBTMedTPropUnknown;
                                }
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }

                    /* We will eventually try not to call bluez api in its own context? */ 
                    i32OpRet = btrCore_BTGetMediaInfo(apDBusConn, pstBTMediaInfo, apcMediaTransIface);
 
                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                        strncpy(apcDeviceIfce, apcMediaTransIface, ui32DeviceIfceLen);
                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                        if (!i32OpRet) {
                            lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                            apcDevAddr    = pstBTDeviceInfo->pcAddress;
                        }
                    }

                    switch (lenBTMedTransProp) {
                    case enBTMedTPropState:
                        if ((!strcmp(pstlhBtIfce->pcMediaCurrState, "none")) && (!strcmp(pcState, "pending"))) {
                            strcpy(pstlhBtIfce->pcMediaCurrState, pcState);

                            if (!i32OpRet && pstBTDeviceInfo->bConnected) {
                                const char* value = "playing";
                                enBTDeviceState lenBtDevState = enBTDevStPropChanged; 

                                strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                                strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                                //if () {} t avmedia wiht device address info. from which id can be computed 
                            }
                        }
                        else if (lenBTDevType == enBTDevAudioSource) { //Lets handle AudioIn case for media events for now
                            char*  apcDevTransportPath = (char*)apcMediaTransIface;

                            // TODO: Obtain this data path reAcquire call from BTRCore - do as part of next commit
                            if (strcmp(pstlhBtIfce->pcMediaCurrState, "none") && !strcmp(pcState, "pending")) {
                                int    dataPathFd   = 0;
                                int    dataReadMTU  = 0;
                                int    dataWriteMTU = 0;

                                if (BtrCore_BTAcquireDevDataPath (pstlhBtIfce, apcDevTransportPath, &dataPathFd, &dataReadMTU, &dataWriteMTU)) {
                                    BTRCORELOG_ERROR ("Failed to ReAcquire transport path %s\n", apcMediaTransIface);
                                }
                                else {
                                    BTRCORELOG_INFO  ("Successfully ReAcquired transport path %s\n", apcMediaTransIface);
                                }
                            }

                            enBTMediaTransportState  lenBtMedTransportSt = enBTMedTransportStNone;
                            stBTMediaStatusUpdate    mediaStatusUpdate; 

                            if (!strcmp(pcState, "idle")) {
                                lenBtMedTransportSt = enBTMedTransportStIdle;
                                strncpy(pstlhBtIfce->pcMediaCurrState, "connected", BT_MAX_STR_LEN - 1);
                            }
                            else if (!strcmp(pcState, "pending")) {
                                lenBtMedTransportSt = enBTMedTransportStPending;
                                strncpy(pstlhBtIfce->pcMediaCurrState, pcState, strlen(pcState));
                            }
                            else if (!strcmp(pcState, "active")) {
                                lenBtMedTransportSt = enBTMedTransportStActive;
                                strncpy(pstlhBtIfce->pcMediaCurrState, "playing", BT_MAX_STR_LEN - 1);
                            }

                            mediaStatusUpdate.aenBtOpIfceType                        = enBTMediaTransport;
                            mediaStatusUpdate.aunBtOpIfceProp.enBtMediaTransportProp = lenBTMedTransProp;
                            mediaStatusUpdate.m_mediaTransportState                  = lenBtMedTransportSt;

                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                        }
                        else if (lenBTDevType == enBTDevAudioSink) {    // Lets handle AudioOut case for media events for a Paired device which connects at Pairing
                            if (!i32OpRet && pstBTDeviceInfo->bConnected && pstBTDeviceInfo->bPaired &&
                                (!strcmp(pstlhBtIfce->pcMediaCurrState, "none")) && (!strcmp(pcState, "idle"))) {
                                const char* value = "connected";
                                enBTDeviceState lenBtDevState = enBTDevStPropChanged; 

                                strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                                strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                                strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                    if (pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                    }
                                }
                            }
                        }
                        break;
                    case enBTMedTPropDelay:
                        BTRCORELOG_DEBUG ("MediaTransport Property - Delay\n");
                        break;
                    case enBTMedTPropVol:
                        {
                            stBTMediaStatusUpdate  mediaStatusUpdate;
                            BTRCORELOG_DEBUG ("MediaTransport Property - Volume\n");

                            mediaStatusUpdate.aenBtOpIfceType                        = enBTMediaTransport;
                            mediaStatusUpdate.aunBtOpIfceProp.enBtMediaTransportProp = lenBTMedTransProp;
                            mediaStatusUpdate.m_mediaTransportVolume                 = lVolume;

                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                        }
                        break;
                    case enBTMedTPropUnknown:
                    default:
                        BTRCORELOG_ERROR ("MediaTransport Property - Unknown\n");
                        break;
                    }
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_CTRL_PATH)) {
                    enBTDeviceType  lenBTDevType = enBTDevUnknown;
                    char*           apcDevAddr   = NULL;
                    stBTMediaStatusUpdate mediaStatusUpdate;

                    BTRCORELOG_ERROR ("Property Changed! : %s\n", BT_DBUS_BLUEZ_MEDIA_CTRL_PATH);

                    i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, dbus_message_get_path(apDBusMsg));
                    if (!i32OpRet) {
                        lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                        apcDevAddr    = pstBTDeviceInfo->pcAddress;
                    }

                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1, lDBusMsgIterVal;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";
                                mediaStatusUpdate.aenBtOpIfceType = enBTMediaControl;

                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_MEDIA_CTRL_PATH, str);

                                /* Can listen on below events if required
                                Connected
                                Player
                                */
                                if (!strncmp(str, "Connected", strlen("Connected"))) {
                                    unsigned char ui8var = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &ui8var);

                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedControlPropConnected;
                                    mediaStatusUpdate.m_mediaPlayerConnected              = ui8var;
                                }
                                else if (!strncmp(str, "Player", strlen("Player"))) {
                                    char* lpcPath = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &lpcPath);
      
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedControlPropPath;
                                    memset (mediaStatusUpdate.m_mediaPlayerPath, 0, BT_MAX_STR_LEN);
                                    strncpy(mediaStatusUpdate.m_mediaPlayerPath, lpcPath, BT_MAX_STR_LEN - 1);
                                }
                            }
                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }


                    if ((enBTDevAudioSource == lenBTDevType) && pstBTDeviceInfo->bConnected && mediaStatusUpdate.m_mediaPlayerConnected) {
                        const char* value = "connected";
                        enBTDeviceState lenBtDevState = enBTDevStPropChanged;

                        strncpy(pstBTDeviceInfo->pcDevicePrevState, pstlhBtIfce->pcDeviceCurrState, BT_MAX_STR_LEN - 1);
                        strncpy(pstBTDeviceInfo->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);
                        strncpy(pstlhBtIfce->pcDeviceCurrState, value, BT_MAX_STR_LEN - 1);

                        if (pstlhBtIfce->fpcBDevStatusUpdate) {
                            if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                            }
                        }
                    }

                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH)) {
                    const char*             apcMediaPlayerIface = dbus_message_get_path(apDBusMsg);
                    char                    apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                    unsigned int            ui32DeviceIfceLen = strstr(apcMediaPlayerIface, "/player") - apcMediaPlayerIface;
                    enBTDeviceType          lenBTDevType = enBTDevUnknown;
                    char*                   apcDevAddr   = NULL;
                    stBTMediaStatusUpdate   mediaStatusUpdate;

                    BTRCORELOG_ERROR ("Property Changed! : %s\n", BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH);

                    strncpy(apcDeviceIfce, apcMediaPlayerIface, ui32DeviceIfceLen);
                    i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                    if (!i32OpRet) {
                        lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                        apcDevAddr    = pstBTDeviceInfo->pcAddress;
                    }
                    memset (&mediaStatusUpdate, 0, sizeof(stBTMediaStatusUpdate));

                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1, lDBusMsgIterVal;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";
                                mediaStatusUpdate.aenBtOpIfceType = enBTMediaPlayer;

                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH, str);
                                /* Can listen on below events if required
                                Name
                                Type
                                Subtype
                                Status
                                Position
                                Track
                                Browsable
                                Searchable
                                Playlist
                                Equalizer
                                Repeat
                                Shuffle
                                Scan
                                */
                                if (!strncmp(str, "Name", strlen("Name"))) {
                                    char* pcName = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcName);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropName;
                                    strncpy (mediaStatusUpdate.m_mediaPlayerName, pcName, BT_MAX_STR_LEN - 1);
                                }
                                else if (!strncmp(str, "Type", strlen("Type"))) {
                                    char* pcType = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcType);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropType;

                                    if (!strncmp ("Audio", pcType, strlen("Audio"))) {
                                        mediaStatusUpdate.enMediaPlayerType = enBTMedPlayerTypAudio;
                                    }
                                    else if (!strncmp ("Video", pcType, strlen("Video"))) {
                                        mediaStatusUpdate.enMediaPlayerType = enBTMedPlayerTypVideo;
                                    }
                                    else if (!strncmp ("Audio Broadcasting", pcType, strlen("Audio Broadcasting"))) {
                                        mediaStatusUpdate.enMediaPlayerType = enBTMedPlayerTypAudioBroadcasting;
                                    }
                                    else if (!strncmp ("Video Broadcasting", pcType, strlen("Video Broadcasting"))) {
                                        mediaStatusUpdate.enMediaPlayerType = enBTMedPlayerTypVideoBroadcasting;
                                    }
                                }
                                else if (!strncmp(str, "Subtype", strlen("Subtype"))) {
                                    char* pcSubtype = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcSubtype);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropSubtype;

                                    if (!strncmp ("Audio Book", pcSubtype, strlen("Audio Book"))) {
                                        mediaStatusUpdate.enMediaPlayerSubtype = enBTMedPlayerSbTypAudioBook;
                                    }
                                    else if (!strncmp ("Podcast", pcSubtype, strlen("Podcast"))) {
                                        mediaStatusUpdate.enMediaPlayerSubtype = enBTMedPlayerSbTypPodcast;
                                    }
                                }
                                else if (!strncmp(str, "Status", strlen("Status"))) {
                                    char* pcStatus = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcStatus);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropStatus;

                                    if (!strncmp("playing", pcStatus, strlen("playing"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStPlaying;
                                    }
                                    else if (!strncmp("paused", pcStatus, strlen("paused"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStPaused;
                                    }
                                    else if (!strncmp("forward-seek", pcStatus, strlen("forward-seek"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStForwardSeek;
                                    }
                                    else if (!strncmp("reverse-seek", pcStatus, strlen("reverse-seek"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStReverseSeek;
                                    }
                                    else if (!strncmp("stopped", pcStatus, strlen("stopped"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStStopped;
                                    }
                                    else if (!strncmp("error", pcStatus, strlen("error"))) {
                                        mediaStatusUpdate.enMediaPlayerStatus = enBTMedPlayerStError;
                                    }
                                }
                                else if (!strncmp(str, "Position",   strlen("Position"))) {
                                    unsigned int ui32Position = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &ui32Position);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropPosition;
                                    mediaStatusUpdate.m_mediaPlayerPosition = ui32Position;
                                }
                                else if (!strncmp(str, "Track",      strlen("Track"))) {
                                    stBTMediaTrackInfo mediaTrackInfo;
                                    memset (&mediaTrackInfo, 0, sizeof(stBTMediaTrackInfo));

                                    if (!BtrCore_BTGetTrackInformation (pstlhBtIfce, apcMediaPlayerIface, &mediaTrackInfo)) {
                                        mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropTrack;
                                        memcpy(&mediaStatusUpdate.m_mediaTrackInfo, &mediaTrackInfo, sizeof(stBTMediaTrackInfo));
                                    }
                                }
                                else if (!strncmp(str, "Browsable",  strlen("Browsable"))) {
                                    bool bIsBrowsable = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &bIsBrowsable);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropBrowsable;
                                    mediaStatusUpdate.m_mediaPlayerBrowsable = bIsBrowsable;
                                }
                                else if (!strncmp(str, "Searchable", strlen("Searchable"))) {
                                    bool bIsSearchable = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &bIsSearchable);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropSearchable;
                                    mediaStatusUpdate.m_mediaPlayerBrowsable = bIsSearchable;
                                }

                                else if (!strncmp(str, "Playlist",  strlen("Playlist"))) {
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropPlaylist;
                                }

                                else if (!strncmp(str, "Equalizer", strlen("Equalizer"))) {
                                    char* pcEqualizer = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcEqualizer);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropEqualizer;

                                    if (!strncmp("on", pcEqualizer, strlen("on"))) {
                                        mediaStatusUpdate.enMediaPlayerEqualizer = enBTMedPlayerEqualizerOn;
                                    }
                                    else if (!strncmp("off", pcEqualizer, strlen("off"))) {
                                        mediaStatusUpdate.enMediaPlayerEqualizer = enBTMedPlayerEqualizerOff;
                                    }
                                }
                                else if (!strncmp(str, "Repeat",     strlen("Repeat"))) {
                                    char* pcRepeat = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcRepeat);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropRepeat;

                                    if (!strncmp("off", pcRepeat, strlen("off"))) {
                                        mediaStatusUpdate.enMediaPlayerRepeat = enBTMedPlayerRpOff;
                                    }
                                    else if (!strncmp("singletrack", pcRepeat, strlen("singletrack"))) {
                                        mediaStatusUpdate.enMediaPlayerRepeat = enBTMedPlayerRpSingleTrack;
                                    }
                                    else if (!strncmp("alltracks", pcRepeat, strlen("alltracks"))) {
                                        mediaStatusUpdate.enMediaPlayerRepeat = enBTMedPlayerRpAllTracks;
                                    }
                                    else if (!strncmp("group", pcRepeat, strlen("group"))) {
                                        mediaStatusUpdate.enMediaPlayerRepeat = enBTMedPlayerRpGroup;
                                    }
                                }
                                else if (!strncmp(str, "Shuffle", strlen("Shuffle"))) {
                                    char* pcShuffle = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcShuffle);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropShuffle;

                                    if (!strncmp("off", pcShuffle, strlen("off"))) {
                                        mediaStatusUpdate.enMediaPlayerShuffle = enBTMedPlayerShuffleOff;
                                    }
                                    else if (!strncmp("alltracks", pcShuffle, strlen("alltracks"))) {
                                        mediaStatusUpdate.enMediaPlayerShuffle = enBTMedPlayerShuffleAllTracks;
                                    }
                                    else if (!strncmp("group", pcShuffle, strlen("group"))) {
                                        mediaStatusUpdate.enMediaPlayerShuffle = enBTMedPlayerShuffleGroup;
                                    }
                                }
                                else if (!strncmp(str, "Scan", strlen("Scan"))) {
                                    char* pcScan = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcScan);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropScan;

                                    if (!strncmp("off", pcScan, strlen("off"))) {
                                        mediaStatusUpdate.enMediaPlayerScan = enBTMedPlayerScanOff;
                                    }
                                    else if (!strncmp("alltracks", pcScan, strlen("alltracks"))) {
                                        mediaStatusUpdate.enMediaPlayerScan = enBTMedPlayerScanAllTracks;
                                    }
                                    else if (!strncmp("group", pcScan, strlen("group"))) {
                                        mediaStatusUpdate.enMediaPlayerScan = enBTMedPlayerScanGroup;
                                    }
                                }
                            }

                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH)) {
                    const char*             apcMediaItemIface = dbus_message_get_path(apDBusMsg);
                    char                    apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                    unsigned int            ui32DeviceIfceLen = strstr(apcMediaItemIface, "/player") - apcMediaItemIface;
                    enBTDeviceType          lenBTDevType = enBTDevUnknown;
                    char*                   apcDevAddr   = NULL;
                    stBTMediaStatusUpdate   mediaStatusUpdate;

                    BTRCORELOG_ERROR ("Property Changed! : %s\n", BT_DBUS_BLUEZ_MEDIA_ITEM_PATH);

                    strncpy(apcDeviceIfce, apcMediaItemIface, ui32DeviceIfceLen);
                    i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                    if (!i32OpRet) {
                        lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                        apcDevAddr    = pstBTDeviceInfo->pcAddress;
                    }

                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";

                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_MEDIA_ITEM_PATH, str);

                                /* Can listen on below events if required
                                Playable
                                Metadata
                                */
                                if (!strncmp(str, "Playable", strlen("Playable"))) {
                                }
                                else if (!strncmp(str, "Metadata", strlen("Metadata"))) {
                                    char mediaPlayerIface[BT_MAX_STR_LEN];
                                    stBTMediaTrackInfo mediaTrackInfo;
                                    memset (mediaPlayerIface, 0, BT_MAX_STR_LEN);
                                    memset (&mediaTrackInfo, 0, sizeof(stBTMediaTrackInfo));

#if 1
                                    /* Eliminate querying for TrackInfo, rather read it from the incoming DBusMsg */
                                    if (strstr(apcMediaItemIface, "/NowPlaying")) {
                                        strncpy(mediaPlayerIface, apcMediaItemIface, (unsigned int)(strstr(apcMediaItemIface, "/NowPlaying") - apcMediaItemIface));
                                    }
                                    else if (strstr(apcMediaItemIface, "/Filesystem")) {
                                        strncpy(mediaPlayerIface, apcMediaItemIface, (unsigned int)(strstr(apcMediaItemIface, "/Filesystem") - apcMediaItemIface));
                                    }
#else
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);

                                    if (dbus_message_iter_get_arg_type(&lDBusMsgIterVal) == DBUS_TYPE_DICT_ENTRY) {
                                        BTRCORELOG_DEBUG ("DBUS_TYPE_DICT_ENTRY\n");
                                    }

                                    btrCore_BTParseMediaTrackMetadata(&lDBusMsgIterVal, &mediaTrackInfo);
#endif                               
                                    mediaStatusUpdate.aenBtOpIfceType                      = enBTMediaPlayer;
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp  = enBTMedPlayerPropTrack;

                                    if (!BtrCore_BTGetTrackInformation (pstlhBtIfce, mediaPlayerIface, &mediaTrackInfo)) {
                                        memcpy(&mediaStatusUpdate.m_mediaTrackInfo, &mediaTrackInfo, sizeof(stBTMediaTrackInfo));
                                    }
                                    else {
                                        BTRCORELOG_ERROR ("FAILED BtrCore_BTGetTrackInformation !!!!\n");
                                    }
                                }
                            }
                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }
                }
                else if (!strcmp(lpcDBusIface,  BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH)){
                    const char*             apcMediaItemIface = dbus_message_get_path(apDBusMsg);
                    char                    apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                    unsigned int            ui32DeviceIfceLen = strstr(apcMediaItemIface, "/player") - apcMediaItemIface;
                    enBTDeviceType          lenBTDevType = enBTDevUnknown;
                    char*                   apcDevAddr   = NULL;
                    stBTMediaStatusUpdate   mediaStatusUpdate;

                    BTRCORELOG_ERROR ("Property Changed! : %s\n", BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH);

                    strncpy(apcDeviceIfce, apcMediaItemIface, ui32DeviceIfceLen);
                    i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                    if (!i32OpRet) {
                        lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                        apcDevAddr    = pstBTDeviceInfo->pcAddress;
                    }

                    {
                        DBusMessageIter lDBusMsgIterDict, lDBusMsgIter1, lDBusMsgIterVal;
                        dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                        while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                            dbus_message_iter_recurse (&lDBusMsgIterDict, &lDBusMsgIter1);

                            if (dbus_message_iter_get_arg_type(&lDBusMsgIter1) == DBUS_TYPE_STRING) {
                                char *str="\0";

                                mediaStatusUpdate.aenBtOpIfceType   = enBTMediaFolder;
                                dbus_message_iter_get_basic(&lDBusMsgIter1, &str);
                                BTRCORELOG_DEBUG ("%s received event <%s>\n", BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH, str);

                                /* Can listen on below events if required
                                NumberOfItems
                                Name
                                */
                                if (!strncmp(str, "Name", strlen("Name"))) {
                                    char* pcName = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &pcName);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaFolderProp  = enBTMedFolderPropName;
                                    strncpy (mediaStatusUpdate.m_mediaFolderName, btrCore_BTGetMediaItemAbsoluteName (pcName), BT_MAX_STR_LEN -1);
                                }
                                else if (!strncmp(str, "NumberOfItems", strlen("NumberOfItems"))) {
                                    unsigned int ui32NumberOfItems = 0;
                                    dbus_message_iter_next(&lDBusMsgIter1);
                                    dbus_message_iter_recurse(&lDBusMsgIter1, &lDBusMsgIterVal);
                                    dbus_message_iter_get_basic(&lDBusMsgIterVal, &ui32NumberOfItems);
                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaFolderProp  = enBTMedFolderPropNumberOfItems;
                                    mediaStatusUpdate.m_mediaFolderNumberOfItems           = ui32NumberOfItems;
                                }
                            }

                            if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                }
                            }
                            dbus_message_iter_next(&lDBusMsgIterDict);
                        }
                    }
                }
                /* Added propert change for GATT */
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_GATT_SERVICE_PATH)) {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_GATT_SERVICE_PATH);
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_GATT_CHAR_PATH )) {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_GATT_CHAR_PATH);
                    const char* pCharIface = dbus_message_get_path(apDBusMsg);
                    unsigned int ui32DeviceIfceLen = strstr(pCharIface, "/service") - pCharIface;
                    char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};

                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                        strncpy(apcDeviceIfce, pCharIface, ui32DeviceIfceLen);

                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                        if (!i32OpRet) {
                            if (pstlhBtIfce->fpcBTLeGattPath) {
                                pstlhBtIfce->fpcBTLeGattPath(enBTGattCharacteristic, enBTLeGattOpUnknown, pCharIface, pstBTDeviceInfo->pcAddress, enBTDevStPropChanged, NULL, pstlhBtIfce->pcBLePathUserData);
                            }
                        }
                    }
                }
                else if (!strcmp(lpcDBusIface, BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH )) {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH);
                }
                else {
                    BTRCORELOG_INFO ("Property Changed! : %s\n", lpcDBusIface);
                }
            }
        }
    }
    else if (dbus_message_is_method_call(apDBusMsg, DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects")) {
        BTRCORELOG_INFO ("%s - GetManagedObjects\n", DBUS_INTERFACE_OBJECT_MANAGER);
    }
    else if (dbus_message_is_method_call(apDBusMsg, DBUS_INTERFACE_PROPERTIES, "GetAll")) {
        BTRCORELOG_INFO ("%s - GetAll\n", DBUS_INTERFACE_PROPERTIES);
        if (!strcmp(pstlhBtIfce->stBTLeCustAdvt.pui8AdvertPath, dbus_message_get_path(apDBusMsg))) {
            btrCore_BTRegisterLeAdvGetProp(apDBusConn, apDBusMsg, pstlhBtIfce);
            BTRCORELOG_INFO ("btrCore_BTRegisterLeAdvGetProp - GetAll\n");
            i32OpRet = 0;
        }
    }
    else if (dbus_message_is_signal(apDBusMsg, DBUS_INTERFACE_OBJECT_MANAGER, "InterfacesAdded")) {
        DBusMessageIter lDBusMsgIter;
        DBusMessageIter lDBusMsgIterDict;
        const char*     lpcDBusIface = NULL;

        dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
        if (dbus_message_iter_get_arg_type(&lDBusMsgIter) == DBUS_TYPE_OBJECT_PATH) {
            dbus_message_iter_get_basic(&lDBusMsgIter, &lpcDBusIface);
            dbus_message_iter_next(&lDBusMsgIter);

            if (dbus_message_iter_get_arg_type(&lDBusMsgIter) == DBUS_TYPE_ARRAY) {
                BTRCORELOG_INFO ("InterfacesAdded : Interface %s\n", lpcDBusIface ? lpcDBusIface : NULL);


                dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterDict);

                while (dbus_message_iter_get_arg_type(&lDBusMsgIterDict) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter lDBusMsgIterStrnArr;
                    const char*     lpcDBusIfaceInternal = NULL;

                    dbus_message_iter_recurse(&lDBusMsgIterDict, &lDBusMsgIterStrnArr);

                    if (dbus_message_iter_get_arg_type(&lDBusMsgIterStrnArr) == DBUS_TYPE_STRING) {
                        dbus_message_iter_get_basic(&lDBusMsgIterStrnArr, &lpcDBusIfaceInternal);

                        dbus_message_iter_next(&lDBusMsgIterStrnArr);
                        if (dbus_message_iter_get_arg_type(&lDBusMsgIterStrnArr) == DBUS_TYPE_ARRAY) {
                            if (lpcDBusIfaceInternal) {
                                if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_ADAPTER_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_ADAPTER_PATH);
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_DEVICE_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_DEVICE_PATH);

                                    if (lpcDBusIface) {
                                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, lpcDBusIface);
                                         if (!i32OpRet) {
                                            enBTDeviceState lenBtDevState = enBTDevStUnknown;
                                            enBTDeviceType  lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);

                                            if ((!pstBTDeviceInfo->bPaired && !pstBTDeviceInfo->bConnected) ||
                                                ((enBTDevUnknown == lenBTDevType) && (TRUE == pstBTDeviceInfo->bConnected)) ) {
                                                lenBtDevState = enBTDevStFound;

                                                if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                                    if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, lenBtDevState, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH);
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH);

                                    if (lpcDBusIface && pstlhBtIfce->fpcBTMediaPlayerPath) {
                                        if (pstlhBtIfce->fpcBTMediaPlayerPath(lpcDBusIface, pstlhBtIfce->pcBMediaPlayerPathUserData)) {
                                           BTRCORELOG_ERROR ("Media Player Path callBack Failed!!!\n");
                                        } 
                                    }   
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_MEDIA_ITEM_PATH);
                                    char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                                    unsigned int ui32DeviceIfceLen     = strstr(lpcDBusIface, "/player") - lpcDBusIface;
                                    enBTDeviceType  lenBTDevType       = enBTDevUnknown;
                                    char* apcDevAddr                   = 0;

                                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                                        strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);
                                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                                        if (!i32OpRet) {
                                            lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);
                                            apcDevAddr    = pstBTDeviceInfo->pcAddress;
                                        }
                                    }

                                    if (strstr(lpcDBusIface, "NowPlaying")) {
                                        stBTMediaBrowserUpdate mediaBrowserUpdate;
                                        BTRCORELOG_INFO ("MediaItem InterfacesAdded : %s\n", strstr(lpcDBusIface, "/NowPlaying"));

                                        if (strstr(lpcDBusIface, "item")) {

                                            stBTMediaStatusUpdate mediaStatusUpdate;
                                            stBTMediaTrackInfo mediaTrackInfo;
                                            char apcMediaIfce[BT_MAX_STR_LEN] = {'\0'};
                                            unsigned int ui32MediaIfceLen     = strstr(lpcDBusIface, "/NowPlaying") - lpcDBusIface;
                                            char* itemName = 0;
                                            char name[BT_MAX_STR_LEN] = "\0";

                                            if ((ui32MediaIfceLen > 0) && (ui32MediaIfceLen < (BT_MAX_STR_LEN - 1))) {
                                                strncpy(apcMediaIfce, lpcDBusIface, ui32MediaIfceLen);
                                            }

                                            memset (&mediaTrackInfo, 0, sizeof(stBTMediaTrackInfo));

                                            i32OpRet = btrCore_BTGetMediaIfceProperty (apDBusConn, lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH, "Name", (void*)&itemName);

                                            if (itemName) {
                                                strncpy(name, itemName, BT_MAX_STR_LEN -1);
                                            }

                                            if (/*!i32OpRet &&*/ !(i32OpRet = BtrCore_BTGetTrackInformation (pstlhBtIfce, apcMediaIfce, &mediaTrackInfo))) {
                                                /* Should look for a better reliable identification logic */
                                                if (!strncmp(mediaTrackInfo.pcTitle, name, (strlen(mediaTrackInfo.pcTitle) < strlen(name))?
                                                                                               strlen(name) : strlen(mediaTrackInfo.pcTitle))) {

                                                    mediaStatusUpdate.aenBtOpIfceType                     = enBTMediaPlayer;
                                                    mediaStatusUpdate.aunBtOpIfceProp.enBtMediaPlayerProp = enBTMedPlayerPropTrack;
                                                    memcpy(&mediaStatusUpdate.m_mediaTrackInfo, &mediaTrackInfo, sizeof(stBTMediaTrackInfo));
                                                    /* Media Status Cb */
                                                    if (pstlhBtIfce->fpcBMediaStatusUpdate) {
                                                        if(pstlhBtIfce->fpcBMediaStatusUpdate(lenBTDevType, &mediaStatusUpdate, apcDevAddr, pstlhBtIfce->pcBMediaStatusUserData)) {
                                                        }
                                                    }
                                                    memcpy (&mediaBrowserUpdate.mediaTrackInfo, &mediaTrackInfo, sizeof(stBTMediaTrackInfo));
                                                }
                                                /*else {
                                                    if (BtrCore_BTGetTrackInformation (pstlhBtIfce, lpcDBusIface, &mediaBrowserUpdate.mediaTrackInfo)) {
                                                        strncpy(mediaBrowserUpdate.mediaTrackInfo.pcTitle, itemName, BT_MAX_STR_LEN -1);
                                                    }
                                                }*/

                                                {
                                                    strncpy(mediaBrowserUpdate.pcMediaItemPath, lpcDBusIface, BT_MAX_STR_LEN -1);
                                                    mediaBrowserUpdate.eMediaItemType     = enBTMediaItemTypAudio;
                                                    mediaBrowserUpdate.ui32BTMediaItemId  = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                                                    if (name[0]) {
                                                        strncpy(mediaBrowserUpdate.mediaTrackInfo.pcTitle, name, BT_MAX_STR_LEN -1);
                                                    }
                                                    else {
                                                        memcpy (&mediaBrowserUpdate.mediaTrackInfo, &mediaTrackInfo, sizeof(stBTMediaTrackInfo));
                                                    }
                                                    strncpy(mediaBrowserUpdate.pcMediaItemName, mediaBrowserUpdate.mediaTrackInfo.pcTitle, BT_MAX_STR_LEN -1);
                                                    BTRCORELOG_DEBUG ("PlayList Audio Item : %s [ID: %llu]\n", mediaBrowserUpdate.mediaTrackInfo.pcTitle, mediaBrowserUpdate.ui32BTMediaItemId);
                                                }
                                            }
                                            else {
                                                BTRCORELOG_ERROR ("Failed to get MediaTrackInfo\n");
                                            }
                                        }
                                        else {
                                            strncpy(mediaBrowserUpdate.pcMediaItemName, "NowPlaying", BT_MAX_STR_LEN -1);
                                            strncpy(mediaBrowserUpdate.pcMediaItemPath, lpcDBusIface,  BT_MAX_STR_LEN -1);
                                            mediaBrowserUpdate.ui32BTMediaItemId = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                                            mediaBrowserUpdate.eMediaItemType    = enBTMediaItemTypFolder;
                                            BTRCORELOG_DEBUG ("PlayList root : %s [ID: %llu]\n", mediaBrowserUpdate.pcMediaItemName
                                                                                               , mediaBrowserUpdate.ui32BTMediaItemId);
                                        }

                                        /* Media Browser Cb */
                                        if (/*!i32OpRet &&*/ pstlhBtIfce->fpcBTMediaBrowserPath && 0xFFFFFFFFFFFFFFFF != mediaBrowserUpdate.ui32BTMediaItemId) {
                                            if (pstlhBtIfce->fpcBTMediaBrowserPath(&mediaBrowserUpdate, 1, apcDevAddr, pstlhBtIfce->pcBMediaBrowserPathUserData)) {
                                            }
                                        }
                                    }
                                    else if (strstr(lpcDBusIface, "Filesystem")) {
                                        stBTMediaBrowserUpdate mediaBrowserUpdate;
                                        char apcMediaPlayerIfce[BT_MAX_STR_LEN] = {'\0'};
                                        unsigned int ui32MediaIfceLen   = strstr(lpcDBusIface, "/Filesystem") - lpcDBusIface;

                                        BTRCORELOG_INFO ("MediaItem InterfacesAdded : %s\n", strstr(lpcDBusIface, "/Filesystem"));
                                        memset (&mediaBrowserUpdate, 0, sizeof(stBTMediaBrowserUpdate));

                                        if ((ui32MediaIfceLen > 0) && (ui32MediaIfceLen < (BT_MAX_STR_LEN - 1))) {
                                            strncpy(apcMediaPlayerIfce, lpcDBusIface, ui32MediaIfceLen);
                                        }

                                        if (strstr(lpcDBusIface, "item")) {
                                            char* itemType = 0;
                                            char* itemName = 0;

                                            btrCore_BTGetMediaIfceProperty (apDBusConn, lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH, "Name", (void*)&itemName);
                                            if (itemName) {
                                                strncpy(mediaBrowserUpdate.pcMediaItemName, btrCore_BTGetMediaItemAbsoluteName(itemName), BT_MAX_STR_LEN - 1);
                                            }
                                            btrCore_BTGetMediaIfceProperty (apDBusConn, lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH, "Type", (void*)&itemType);
                                            strncpy(mediaBrowserUpdate.pcMediaItemPath, lpcDBusIface, BT_MAX_STR_LEN - 1);

                                            itemName = mediaBrowserUpdate.pcMediaItemName;
                                            if (itemType != NULL) {

                                                if (!strncmp(itemType, "audio", strlen("audio"))) {

                                                    /*if (BtrCore_BTGetTrackInformation(apDBusConn, lpcDBusIface, &mediaBrowserUpdate.mediaTrackInfo)) {
                                                    BTRCORELOG_ERROR ("Failed to get Metadata Info for %s\n", lpcDBusIface);
                                                    i32OpRet = -1;
                                                    }*/
                                                    strncpy(mediaBrowserUpdate.mediaTrackInfo.pcTitle, itemName, BT_MAX_STR_LEN -1);
                                                    mediaBrowserUpdate.eMediaItemType   = enBTMediaItemTypAudio;
                                                    mediaBrowserUpdate.ui32BTMediaItemId  = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                                                    mediaBrowserUpdate.eMediaFolderType  = enBTMediaFldTypTrack;
                                                    BTRCORELOG_DEBUG ("MediaBrowser Audio Item : %s [ID: %llu]\n", itemName, mediaBrowserUpdate.ui32BTMediaItemId);
                                                }
                                                else if (!strncmp(itemType, "folder", strlen("folder"))) {
                                                    char* folderType          = 0;

                                                    btrCore_BTGetMediaIfceProperty (apDBusConn, lpcDBusIface, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH, "FolderType", (void*)&folderType);

                                                    mediaBrowserUpdate.eMediaItemType       = enBTMediaItemTypFolder;
                                                    mediaBrowserUpdate.ui32BTMediaItemId    = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                                                    mediaBrowserUpdate.eMediaFolderType     = btrCore_BTGetMediaFolderType (folderType);

                                                    BTRCORELOG_DEBUG ("MediaBrowser - Folder Item(%s) : %s [ID: %llu]\n", folderType, itemName, mediaBrowserUpdate.ui32BTMediaItemId);
                                                }
                                                else if (!strncmp(itemType, "video", strlen("video"))) {
                                                    BTRCORELOG_DEBUG ("Its a video Item : %s\n", itemName);
                                                    mediaBrowserUpdate.eMediaItemType = enBTMediaItemTypVideo;
                                                }
                                                else {
                                                    BTRCORELOG_ERROR ("Unknown item type : %s !\n", itemType);
                                                    mediaBrowserUpdate.eMediaItemType = enBTMediaItemTypUnknown;
                                                    i32OpRet = -1;
                                                }
                                            }
                                            else {
                                                BTRCORELOG_ERROR (" item type is NULL ");
                                                mediaBrowserUpdate.eMediaItemType = enBTMediaItemTypUnknown;
                                                i32OpRet = -1;
                                            }
                                        }
                                        else {
                                            //btrCore_BTGetMediaIfceProperty (apDBusConn, apcMediaPlayerIfce, BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH, "Name", (void*)&folderName);
                                            strncpy(mediaBrowserUpdate.pcMediaItemName, "Filesystem", BT_MAX_STR_LEN -1);
                                            strncpy(mediaBrowserUpdate.pcMediaItemPath, lpcDBusIface,  BT_MAX_STR_LEN -1);
                                            mediaBrowserUpdate.ui32BTMediaItemId = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                                            mediaBrowserUpdate.eMediaItemType    = enBTMediaItemTypFolder;
                                            BTRCORELOG_DEBUG ("MediaBrowser root : %s [ID: %llu]\n", mediaBrowserUpdate.pcMediaItemName, mediaBrowserUpdate.ui32BTMediaItemId);
                                        }

                                        if (!i32OpRet && pstlhBtIfce->fpcBTMediaBrowserPath) {
                                            pstlhBtIfce->fpcBTMediaBrowserPath(&mediaBrowserUpdate, 1, apcDevAddr, pstlhBtIfce->pcBMediaBrowserPathUserData);
                                        } 
                                    }
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH);
                                }
                                /* Add Interfaces for GATT */
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_SERVICE_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_GATT_SERVICE_PATH);
                                    char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                                    unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                                        strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                                        if (!i32OpRet) {
                                            if (pstlhBtIfce->fpcBTLeGattPath) {
                                                pstlhBtIfce->fpcBTLeGattPath(enBTGattService, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStFound, NULL, pstlhBtIfce->pcBLePathUserData);
                                            }
                                        }
                                    }
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_CHAR_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_GATT_CHAR_PATH);
                                    char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                                    unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                                        strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                                        if (!i32OpRet) {
                                            if (pstlhBtIfce->fpcBTLeGattPath) {
                                                pstlhBtIfce->fpcBTLeGattPath(enBTGattCharacteristic, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStFound, NULL, pstlhBtIfce->pcBLePathUserData);
                                            }
                                        }
                                    }
                                }
                                else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH)) {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH);
                                    char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                                    unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                                    if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                                        strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                                        i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                                        if (!i32OpRet) {
                                            if (pstlhBtIfce->fpcBTLeGattPath) {
                                                pstlhBtIfce->fpcBTLeGattPath(enBTGattDescriptor, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStFound, NULL, pstlhBtIfce->pcBLePathUserData);
                                            }
                                        }
                                    }
                                }
                                else {
                                    BTRCORELOG_INFO ("InterfacesAdded : %s\n", lpcDBusIfaceInternal);
                                }
                            }
                        }
                    }

                    dbus_message_iter_next(&lDBusMsgIterDict);
                }
            }
        }
    }
    else if (dbus_message_is_signal(apDBusMsg, DBUS_INTERFACE_OBJECT_MANAGER, "InterfacesRemoved")) {
        BTRCORELOG_WARN ("Device Lost!\n");

        DBusMessageIter lDBusMsgIterStr;
        DBusMessageIter lDBusMsgIter;
        const char*     lpcDBusIface = NULL;

        dbus_message_iter_init(apDBusMsg, &lDBusMsgIter);
        dbus_message_iter_get_basic(&lDBusMsgIter, &lpcDBusIface);
        dbus_message_iter_next(&lDBusMsgIter);

        if (dbus_message_iter_get_arg_type(&lDBusMsgIter) == DBUS_TYPE_ARRAY) {
            BTRCORELOG_INFO ("InterfacesRemoved : Interface %s\n", lpcDBusIface ? lpcDBusIface : NULL);

            dbus_message_iter_recurse(&lDBusMsgIter, &lDBusMsgIterStr);

            while (dbus_message_iter_get_arg_type(&lDBusMsgIterStr) == DBUS_TYPE_STRING) {
                const char* lpcDBusIfaceInternal = NULL;

                dbus_message_iter_get_basic(&lDBusMsgIterStr, &lpcDBusIfaceInternal);

                if (lpcDBusIfaceInternal) {
                    if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_ADAPTER_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_ADAPTER_PATH);
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_DEVICE_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_DEVICE_PATH);
                        
                        if (lpcDBusIface) {
                            enBTDeviceType  lenBTDevType = enBTDevUnknown;
                            /* On InterfacesRemoved signal of BT_DBUS_BLUEZ_DEVICE_PATH, query to get DevInfo is invalid 
                            i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, lpcDBusIface);
                            if (!i32OpRet) {
                                lenBTDevType  = btrCore_BTMapDevClasstoDevType(pstBTDeviceInfo->ui32Class);

                            }*/
                            if (pstlhBtIfce->fpcBDevStatusUpdate) {
                                btrCore_BTGetDevAddressFromDevPath(lpcDBusIface, pstBTDeviceInfo->pcAddress);
                                if(pstlhBtIfce->fpcBDevStatusUpdate(lenBTDevType, enBTDevStLost, pstBTDeviceInfo, pstlhBtIfce->pcBDevStatusUserData)) {
                                }
                            }
                        }
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_MEDIA_TRANSPORT_PATH);
                        // For Device Lost or Out Of Range cases                      
                        pstlhBtIfce->ui32DevLost = 1;                        
                       
                        //TODO: What if some other devices transport interface gets removed with delay ? 
                        strncpy(pstlhBtIfce->pcMediaCurrState, "none", BT_MAX_STR_LEN - 1); 
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_MEDIA_PLAYER_PATH);

                        if (lpcDBusIface && pstlhBtIfce->fpcBTMediaPlayerPath) {
                            //if (pstlhBtIfce->fpcBTMediaPlayerPath(lpcDBusIface, pstlhBtIfce->pcBMediaPlayerPathUserData)) {
                            //    BTRCORELOG_ERROR ("Media Player Path callBack Failed!!!\n");
                            //}
                        }  
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_ITEM_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_MEDIA_ITEM_PATH);
                        char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                        unsigned int ui32DeviceIfceLen     = strstr(lpcDBusIface, "/player") - lpcDBusIface;
                        char* apcDevAddr                   = 0;
                        stBTMediaBrowserUpdate mediaBrowserUpdate;

                        if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                            strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);
                            i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                            if (!i32OpRet) {
                                apcDevAddr    = pstBTDeviceInfo->pcAddress;
                            }
                           else {
                                btrCore_BTGetDevAddressFromDevPath(lpcDBusIface, pstBTDeviceInfo->pcAddress);
                                i32OpRet = 0;
                           }
                        }

                        if (strstr(lpcDBusIface, "/NowPlaying")) {
                            BTRCORELOG_INFO ("MediaItem InterfacesRemoved : %s\n", strstr(lpcDBusIface, "/NowPlaying"));
                        }
                        else if (strstr(lpcDBusIface, "/Filesystem")) {
                            BTRCORELOG_INFO ("MediaItem InterfacesRemoved : %s\n", strstr(lpcDBusIface, "/Filesystem"));
                        }

                        mediaBrowserUpdate.ui32BTMediaItemId  = btrCore_BTGetMediaItemIdFromMediaPath (lpcDBusIface);
                        strncpy(mediaBrowserUpdate.pcMediaItemPath, lpcDBusIface, BT_MAX_STR_LEN - 1);

                        if (!i32OpRet && pstlhBtIfce->fpcBTMediaBrowserPath) {
                            pstlhBtIfce->fpcBTMediaBrowserPath(&mediaBrowserUpdate, 0, apcDevAddr, pstlhBtIfce->pcBMediaBrowserPathUserData);
                        }
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_MEDIA_FOLDER_PATH);
                    }
                    /* Add Interfaces removed for GATT profile */ 
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_SERVICE_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_GATT_SERVICE_PATH);
                        char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                        unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                        if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                            strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                            i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                            if (!i32OpRet) {
                                if (pstlhBtIfce->fpcBTLeGattPath) {
                                    pstlhBtIfce->fpcBTLeGattPath(enBTGattService, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStLost, NULL, pstlhBtIfce->pcBLePathUserData);
                                }
                            }
                        }
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_CHAR_PATH  )) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_GATT_CHAR_PATH);
                        char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                        unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                        if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                            strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                            i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                            if (!i32OpRet) {
                                if (pstlhBtIfce->fpcBTLeGattPath) {
                                    pstlhBtIfce->fpcBTLeGattPath(enBTGattCharacteristic, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStLost, NULL, pstlhBtIfce->pcBLePathUserData);
                                }
                            }
                        }
                    }
                    else if (!strcmp(lpcDBusIfaceInternal, BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH)) {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH);
                        char apcDeviceIfce[BT_MAX_STR_LEN] = {'\0'};
                        unsigned int ui32DeviceIfceLen = strstr(lpcDBusIface, "/service") - lpcDBusIface; 

                        if ((ui32DeviceIfceLen > 0) && (ui32DeviceIfceLen < (BT_MAX_STR_LEN - 1))) {
                            strncpy(apcDeviceIfce, lpcDBusIface, ui32DeviceIfceLen);

                            i32OpRet = btrCore_BTGetDeviceInfo(apDBusConn, pstBTDeviceInfo, apcDeviceIfce);
                            if (!i32OpRet) {
                                if (pstlhBtIfce->fpcBTLeGattPath) {
                                    pstlhBtIfce->fpcBTLeGattPath(enBTGattDescriptor, enBTLeGattOpUnknown, lpcDBusIface, pstBTDeviceInfo->pcAddress, enBTDevStLost, NULL, pstlhBtIfce->pcBLePathUserData);
                                }
                            }
                        }
                    }
                    else {
                        BTRCORELOG_INFO ("InterfacesRemoved : %s\n", lpcDBusIfaceInternal);
                    }
                }

                dbus_message_iter_next(&lDBusMsgIterStr);
            }
        }
    }

    if (!i32OpRet)
        return DBUS_HANDLER_RESULT_HANDLED;
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
btrCore_BTMediaEndpointHandlerCb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply     = NULL;
    const char*     lpcPath         = NULL;
    enBTDeviceType  lenBTDeviceType = enBTDevUnknown;
    enBTMediaType   lenBTMediaType  = enBTMediaTypeUnknown;

    lpcPath = dbus_message_get_path(apDBusMsg);
    if (!lpcPath)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


    BTRCORELOG_INFO ("endpoint_handler: MediaEndpoint1 - %s\n", lpcPath);

    if (!strcmp(lpcPath, BT_MEDIA_SBC_A2DP_SOURCE_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSink;
        lenBTMediaType  = enBTMediaTypeSBC;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_SBC_A2DP_SINK_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSource;
        lenBTMediaType  = enBTMediaTypeSBC;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_PCM_HFP_AG_ENDPOINT)) {
        lenBTDeviceType = enBTDevHFPHeadset;
        lenBTMediaType  = enBTMediaTypePCM;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_AAC_A2DP_SOURCE_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSink;
        lenBTMediaType  = enBTMediaTypeAAC;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_AAC_A2DP_SINK_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSource;
        lenBTMediaType  = enBTMediaTypeAAC;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_MP3_A2DP_SOURCE_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSink;
        lenBTMediaType  = enBTMediaTypeMP3;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_MP3_A2DP_SINK_ENDPOINT)) {
        lenBTDeviceType = enBTDevAudioSource;
        lenBTMediaType  = enBTMediaTypeMP3;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_SBC_HFP_AG_ENDPOINT)) {
        lenBTDeviceType = enBTDevHFPHeadset;
        lenBTMediaType  = enBTMediaTypeSBC;
    }
    else if (!strcmp(lpcPath, BT_MEDIA_PCM_HFP_HS_ENDPOINT)) {
        lenBTDeviceType = enBTDevHFPAudioGateway;
        lenBTMediaType  = enBTMediaTypePCM;
    }


    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_MEDIA_ENDPOINT_PATH, "SelectConfiguration")) {
        BTRCORELOG_INFO ("endpoint_handler: MediaEndpoint1-SelectConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSelectConfiguration(apDBusMsg, lenBTDeviceType, lenBTMediaType, apvUserData);
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_MEDIA_ENDPOINT_PATH, "SetConfiguration"))  {
        BTRCORELOG_INFO ("endpoint_handler: MediaEndpoint1-SetConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointSetConfiguration(apDBusMsg, lenBTDeviceType, lenBTMediaType, apvUserData);
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_MEDIA_ENDPOINT_PATH, "ClearConfiguration")) {
        BTRCORELOG_INFO ("endpoint_handler: MediaEndpoint1-ClearConfiguration\n");
        lpDBusReply = btrCore_BTMediaEndpointClearConfiguration(apDBusMsg, lenBTDeviceType, lenBTMediaType, apvUserData);
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
btrCore_BTAgentMessageHandlerCb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {

    BTRCORELOG_INFO ("btrCore_BTAgentMessageHandlerCb\n");

    if (!apDBusConn || !apDBusMsg || !apvUserData)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "Release"))
        return btrCore_BTAgentRelease (apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "RequestPinCode"))
        return btrCore_BTAgentRequestPincode(apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "RequestPasskey"))
        return btrCore_BTAgentRequestPasskey(apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "RequestConfirmation"))
        return btrCore_BTAgentRequestConfirmation(apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "AuthorizeService"))
        return btrCore_BTAgentAuthorize(apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "Cancel"))
        return btrCore_BTAgentCancelMessage(apDBusConn, apDBusMsg, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "DisplayPinCode"))
        return btrCore_BTAgentDisplayPinCodePassKey(apDBusConn, apDBusMsg, TRUE, apvUserData);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "DisplayPasskey")) {
        BTRCORELOG_INFO ("btrCore_BTAgentMessageHandlerCb:: DisplayPasskey\n");
        return btrCore_BTAgentDisplayPinCodePassKey(apDBusConn, apDBusMsg, FALSE, apvUserData);
    }

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_AGENT_PATH, "RequestAuthorization")) {
        BTRCORELOG_INFO ("btrCore_BTAgentMessageHandlerCb:: RequestAuthorization\n");
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
btrCore_BTLeGattEndpointHandlerCb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath = NULL;

    int             li32MessageType;
    const char*     lpcSender;
    const char*     lpcDestination;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;


    if (!apDBusConn || !apDBusMsg || !apvUserData)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


    li32MessageType = dbus_message_get_type(apDBusMsg);
    lpcSender       = dbus_message_get_sender(apDBusMsg);
    lpcDestination  = dbus_message_get_destination(apDBusMsg);
  
    BTRCORELOG_INFO ("%s Sender=%s -> Dest=%s Path=%s; Interface=%s; Member=%s\n",
                    btrCore_DBusType2Name(li32MessageType),
                    lpcSender ? lpcSender : "Null",
                    lpcDestination ? lpcDestination : "Null",
                    dbus_message_get_path(apDBusMsg),
                    dbus_message_get_interface(apDBusMsg),
                    dbus_message_get_member(apDBusMsg));

    lpcPath = dbus_message_get_path(apDBusMsg);
    if (!lpcPath)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


    BTRCORELOG_INFO ("leGatt: /LeEndpoint/Gatt - %s\n", lpcPath);

    //TODO: Find the right way to store this and deallocate this memory when not needed anymore
    stBTLeGattInfo* lpstBTLeGattInfo = &pstlhBtIfce->stBTLeGatt;
    char            lCurAdapterAddress[BT_MAX_DEV_PATH_LEN] = "\0";

    if (!pstlhBtIfce->pcBTDAdapterAddr) {
        unBTOpIfceProp     lunBtOpAdapProp;

        lunBtOpAdapProp.enBtAdapterProp = enBTAdPropAddress;
        /* Get current device address */
        if (BtrCore_BTGetProp(pstlhBtIfce,
                              (BtrCore_BTGetAdapterPath(pstlhBtIfce, NULL)),
                              enBTAdapter,
                              lunBtOpAdapProp,
                              lCurAdapterAddress)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    else {
        strncpy(lCurAdapterAddress, pstlhBtIfce->pcBTDAdapterAddr, strlen(pstlhBtIfce->pcBTDAdapterAddr));
    }

    BTRCORELOG_TRACE("Current adapter address is : %s\n", lCurAdapterAddress);
    pstlhBtIfce->fpcBTLeGattPath(enBTAdvertisement, enBTLeGattOpUnknown, lpcPath, lCurAdapterAddress, enBTDevStUnknown, (void*)lpstBTLeGattInfo, pstlhBtIfce->pcBLePathUserData);
    if (dbus_message_is_method_call(apDBusMsg, DBUS_INTERFACE_OBJECT_MANAGER, "GetManagedObjects")) {

        BTRCORELOG_INFO ("leGatt: /LeEndpoint/Gatt - %s GetManagedObjects\n", lpcPath);
        BTRCORELOG_INFO("Number of gatt services%d\n", lpstBTLeGattInfo->nNumGattServices);
        if (!strcmp(lpcPath, BT_LE_GATT_SERVER_ENDPOINT)) {
            lpDBusReply = btrCore_BTRegisterGattService(apDBusConn, apDBusMsg, pstlhBtIfce, lpstBTLeGattInfo->astBTRGattService, lpstBTLeGattInfo->nNumGattServices);
            if (!lpDBusReply) {
                BTRCORELOG_ERROR ("Can't Register Le Gatt Object Paths\n");
            }
        }

        BTRCORELOG_INFO ("leGatt: /LeEndpoint/GattServer-GetManagedObjects\n");
    }
    else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    if (!lpDBusReply) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }


    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);


    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
btrCore_BTLeGattMessageHandlerCb (
    DBusConnection* apDBusConn,
    DBusMessage*    apDBusMsg,
    void*           apvUserData
) {
    DBusMessage*    lpDBusReply = NULL;
    const char*     lpcPath     = NULL;
    stBtIfceHdl*    pstlhBtIfce = (stBtIfceHdl*)apvUserData;

    lpcPath = dbus_message_get_path(apDBusMsg);
    if (!lpcPath)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    BTRCORELOG_INFO ("leGatt: /LeEndpoint/GattServer - %s\n", lpcPath);

    if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_CHAR_PATH, "ReadValue"))  {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Characteristic-ReadValue\n");

        lpDBusReply = btrCore_BTLEGattOps(apDBusMsg, pstlhBtIfce, enBTGattCharacteristic, enBTLeGattOpReadValue);
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_CHAR_PATH, "WriteValue"))  {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Characteristic-WriteValue\n");

        btrCore_BTLEGattOps(apDBusMsg, pstlhBtIfce, enBTGattCharacteristic, enBTLeGattOpWriteValue);
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_CHAR_PATH, "StartNotify"))  {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Characteristic-StartNotify\n");
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_CHAR_PATH, "StopNotify"))  {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Characteristic-StopNotify\n");
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH, "ReadValue")) {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Descriptor-ReadValue\n");

        lpDBusReply = btrCore_BTLEGattOps(apDBusMsg, pstlhBtIfce, enBTGattDescriptor, enBTLeGattOpReadValue);
    }
    else if (dbus_message_is_method_call(apDBusMsg, BT_DBUS_BLUEZ_GATT_DESCRIPTOR_PATH, "WriteValue")) {
        BTRCORELOG_INFO ("leGatt: LeEndpoint/GattServer-Descriptor-WriteValue\n");

        btrCore_BTLEGattOps(apDBusMsg, pstlhBtIfce, enBTGattDescriptor, enBTLeGattOpWriteValue);
    }
    else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (!lpDBusReply) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_connection_send(apDBusConn, lpDBusReply, NULL);
    dbus_connection_flush(apDBusConn);

    dbus_message_unref(lpDBusReply);

    return DBUS_HANDLER_RESULT_HANDLED;
}
