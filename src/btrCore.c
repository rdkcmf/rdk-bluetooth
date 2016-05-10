//btrCore.c

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for getpid
#include <pthread.h>    //for StopDiscovery test
#include <sched.h>      //for StopDiscovery test
#include <string.h>     //for strcnp
#include <errno.h>      //for error numbers

#include <dbus/dbus.h>

#include "btrCore.h"
#include "btrCore_avMedia.h"
#include "btrCore_dbus_bt.h"



static unsigned int gNumberOfScannedDevices = 0;
static unsigned int gNumberOfPairedDevices = 0;
static stBTRCoreScannedDevices  scanned_devices[BTRCORE_MAX_NUM_BT_DEVICES];//holds twenty scanned devices
static stBTRCoreScannedDevices  found_device;                        //a device for intermediate dbus processing
static stBTRCoreKnownDevice     known_devices[BTRCORE_MAX_NUM_BT_DEVICES];   //holds twenty known devices
static stBTRCoreDevStateCB      gstBTRCoreDevStateCbInfo;               //holds info for a callback

static void btrCore_InitDataSt (void);
static void btrCore_ClearScannedDevicesList (void);
static DBusMessage* sendMethodCall (DBusConnection* conn, const char* objectpath, const char* interfacename, const char* methodname);
static int remove_paired_device (DBusConnection* conn, const char* apBTRAdapterPath, const char* fullpath);
static enBTRCoreRet populateListOfPairedDevices(tBTRCoreHandle hBTRCore, const char* pAdapterPath);

/* Discovered Device notification */
BTRMgr_DeviceDiscoveryCallback cb_DeviceDiscovery = NULL;

typedef struct _stBTRCoreHdl {
    void*           connHandle;
    char*           agentPath;
    char*           curAdapterPath;
    unsigned int    numOfAdapters;
    pthread_t       dispatchThread;
    pthread_mutex_t dispatchMutex;
    BOOLEAN         dispatchThreadQuit;
} stBTRCoreHdl;

static void
btrCore_InitDataSt (
    void
) {
    int i;

    /* Scanned Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        memset (scanned_devices[i].bd_address, '\0', sizeof(BD_NAME));
        memset (scanned_devices[i].device_name, '\0', sizeof(BD_NAME));
        scanned_devices[i].RSSI = INT_MIN;
        scanned_devices[i].found = FALSE;
    }

    gNumberOfScannedDevices = 0;
    gNumberOfPairedDevices = 0;
    /* Found Device */
    memset (found_device.bd_address, '\0', sizeof(BD_NAME));
    memset (found_device.device_name, '\0', sizeof(BD_NAME));
    found_device.RSSI = INT_MIN;
    found_device.found = FALSE;

    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        memset (known_devices[i].bd_path, '\0', sizeof(BD_NAME));
        memset (known_devices[i].device_name, '\0', sizeof(BD_NAME));
        known_devices[i].RSSI = INT_MIN;
        known_devices[i].found = FALSE;
    }

    /* Callback Info */
    memset(gstBTRCoreDevStateCbInfo.cDeviceType, '\0', sizeof(gstBTRCoreDevStateCbInfo.cDeviceType));
    memset(gstBTRCoreDevStateCbInfo.cDevicePrevState, '\0', sizeof(gstBTRCoreDevStateCbInfo.cDevicePrevState));
    memset(gstBTRCoreDevStateCbInfo.cDeviceCurrState, '\0', sizeof(gstBTRCoreDevStateCbInfo.cDeviceCurrState));

    strncpy(gstBTRCoreDevStateCbInfo.cDeviceType, "Bluez", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(gstBTRCoreDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);
    strncpy(gstBTRCoreDevStateCbInfo.cDevicePrevState, "Initialized", BTRCORE_STRINGS_MAX_LEN - 1);

    /* Always safer to initialze Global variables, init if any left or added */
}


static void
btrCore_ClearScannedDevicesList (
    void
) {
    int i;

    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        memset (scanned_devices[i].device_name, '\0', sizeof(scanned_devices[i].device_name));
        memset (scanned_devices[i].bd_address,  '\0', sizeof(scanned_devices[i].bd_address));
        scanned_devices[i].RSSI = INT_MIN;
        scanned_devices[i].found = FALSE;
    }
    gNumberOfScannedDevices = 0;
}


static void 
btrCore_ShowSignalStrength (
    short strength
) {
    short pos_str;

    pos_str = 100 + strength;//strength is usually negative with number meaning more strength

    printf(" Signal Strength: %d dbmv  ",strength);

    if (pos_str > 70) {
        printf("++++\n");
    }

    if ((pos_str > 50) && (pos_str <= 70)) {
        printf("+++\n");
    }

    if ((pos_str > 37) && (pos_str <= 50)) {
        printf("++\n");
    }

    if (pos_str <= 37) {
        printf("+\n");
    } 
}


static DBusMessage* 
sendMethodCall (
    DBusConnection* conn,
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
    if (!dbus_connection_send_with_reply(conn, methodcall, &pending, -1)) { //Send and expect reply using pending call object
        printf("failed to send message!\n");
    }

    dbus_connection_flush(conn);
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
discover_services (
    DBusConnection* conn,
    const char*     fullpath,
    const char*     search_string,
    char*           data_string
) {
    DBusMessage *msg, *reply;
    DBusMessageIter arg_i, element_i;
    DBusMessageIter dict_i;
    int dbus_type;
    DBusError err;
    int match;
    const char* value;
    char* ret;
        
   //BTRCore_LOG("fullpath is %s\n and service UUID is %s", fullpath,search_string);
    msg = dbus_message_new_method_call( "org.bluez",
                                        fullpath,
                                        "org.bluez.Device",
                                        "DiscoverServices");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    match = 0; //assume it does not match
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &search_string, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

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
            if (data_string !=NULL) {
                strcpy(data_string,value);
            }

            // lets strstr to see if "uuid value="<UUID>" is there
            ret =  strstr(value,search_string);
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


static int 
remove_paired_device (
    DBusConnection* conn,
    const char*     apBTRAdapterPath,
    const char*     fullpath
) {
    dbus_bool_t success;
    DBusMessage *msg;
        
   // BTRCore_LOG("fullpath is %s\n",fullpath);
    msg = dbus_message_new_method_call( "org.bluez",
                                        apBTRAdapterPath,
                                        "org.bluez.Adapter",
                                        "RemoveDevice");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &fullpath, DBUS_TYPE_INVALID);
    success = dbus_connection_send(conn, msg, NULL);

    dbus_message_unref(msg);

    if (!success) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(conn);

    return 0;
}


static int
find_paired_device (
    DBusConnection* conn,
    const char*     apBTRAdapterPath,
    const char*     device
) {

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    msg = dbus_message_new_method_call( "org.bluez",
                                        apBTRAdapterPath,
                                        "org.bluez.Adapter",
                                        "FindDevice");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &device,DBUS_TYPE_INVALID);


    dbus_error_init(&err);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!reply) {
        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return -1;
    }

    return 0;
}


static int 
create_paired_device (
    DBusConnection* conn, 
    const char*     apBTRAdapterPath,
    const char*     apBTAgentPath,
    const char*     capabilities,
    const char*     device
) {
   
    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    msg = dbus_message_new_method_call( "org.bluez", 
                                        apBTRAdapterPath,
                                        "org.bluez.Adapter",
                                        "CreatePairedDevice");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &device,
                             DBUS_TYPE_OBJECT_PATH, &apBTAgentPath,
                             DBUS_TYPE_STRING, &capabilities,
                             DBUS_TYPE_INVALID);

    dbus_error_init(&err);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

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


void
LoadScannedDevice (
    void
) {
    int i;
    int found;
    int last;

    found = FALSE;
    last = 0;

    //printf("LoadScannedDevice processing %s-%s\n",found_device.bd_address,found_device.device_name);
    for ( i = 0; i < 15; i++) {
        if (scanned_devices[i].found)
            last++; //keep track of last valid record in array

        if (strcmp(found_device.bd_address, scanned_devices[i].bd_address) == 0) {
            found = TRUE;
            found_device.found = TRUE; //mark this for callback need
            break;
        }
    }

    if (found == FALSE) { //device wasnt there, we got to add it
        for (i = 0; i < 15; i++) {
            if (!scanned_devices[i].found) {
                //printf("adding %s at location %d\n",found_device.bd_address,i);
                scanned_devices[i].found = TRUE; //mark the record as found
                strcpy(scanned_devices[i].bd_address,found_device.bd_address);
                strcpy(scanned_devices[i].device_name,found_device.device_name);
                scanned_devices[i].RSSI = found_device.RSSI;
                gNumberOfScannedDevices++;
                break;
            }
        }
    }
}


void
test_func (
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    if (p_Status_callback != NULL) {
        p_Status_callback();
    }
    else {
        printf("no callback installed\n");
    }
}


void*
DoDispatch (
    void* ptr
) {
    tBTRCoreHandle  hBTRCore = NULL;
    BOOLEAN         ldispatchThreadQuit = FALSE;
    enBTRCoreRet*   penDispThreadExitStatus = malloc(sizeof(enBTRCoreRet));

    hBTRCore = (stBTRCoreHdl*) ptr;
    printf("%s \n", "Dispatch Thread Started");


    if (!((stBTRCoreHdl*)hBTRCore) || !((stBTRCoreHdl*)hBTRCore)->connHandle) {
        fprintf(stderr, "Dispatch thread failure - BTRCore not initialized\n");
        *penDispThreadExitStatus = enBTRCoreNotInitialized;
        return (void*)penDispThreadExitStatus;
    }
    
    while (1) {
        pthread_mutex_lock (&((stBTRCoreHdl*)hBTRCore)->dispatchMutex);
        ldispatchThreadQuit = ((stBTRCoreHdl*)hBTRCore)->dispatchThreadQuit;
        pthread_mutex_unlock (&((stBTRCoreHdl*)hBTRCore)->dispatchMutex);

        if (ldispatchThreadQuit == TRUE)
            break;

#if 1
        usleep(25000); // 25ms
#else
        sched_yield(); // Would like to use some form of yield rather than sleep sometime in the future
#endif

        if (dbus_connection_read_write_dispatch(((stBTRCoreHdl*)hBTRCore)->connHandle, 25) != TRUE)
            break;
    }

    *penDispThreadExitStatus = enBTRCoreSuccess;
    return (void*)penDispThreadExitStatus;
}


int 
GetAdapters (
    DBusConnection* conn
) {
    DBusMessage *msg, *reply;
    DBusError err;
    char **paths = NULL;
    int i;
    int num = -1;

    msg = dbus_message_new_method_call( "org.bluez",
                                        "/",
                                        "org.bluez.Manager", 
                                        "ListAdapters");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return num;
    }

    dbus_error_init(&err);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!reply) {
        fprintf(stderr, "Can't get default adapter\n");

        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return num;
    }

    //mikek I think this would be similar to listdevices function
    if(reply != NULL) {
        if (!dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Manager.ListAdapters returned an error: '%s'\n", err.message);
        }

        for (i = 0; i < num; i++) {
            printf("adapter: %d is %s\n",i,paths[i]);
        }

        dbus_message_unref(reply);
    }

    return num;
}


static int 
parse_device (
    DBusMessage* msg
) {
    DBusMessageIter arg_i, element_i, variant_i;
    const char* key;
    const char* value;
    const char* bd_addr;
    short rssi;
    int dbus_type;

    //printf("\n\n\nBLUETOOTH DEVICE FOUND:\n");
    if (!dbus_message_iter_init(msg, &arg_i)) {
       printf("GetProperties reply has no arguments.");
    }

    if (!dbus_message_get_args( msg, NULL,
                                DBUS_TYPE_STRING, &bd_addr,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Invalid arguments for NameOwnerChanged signal");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // printf(" Address: %s\n",bd_addr);
    //TODO provide some indication, callback to app of devices being found in real time
    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
    //printf("type is %d\n",dbus_type);

    if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
        //printf("GetProperties argument is not a DBUS_TYPE_ARRAY... get next\n");
        dbus_message_iter_next(&arg_i);
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
        //printf("type is %d\n",dbus_type);

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            //printf("GetProperties argument is STILL not DBUS_TYPE_ARRAY... \n");
        }
        
    }

    dbus_message_iter_recurse(&arg_i, &element_i);

    while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter dict_i;

            dbus_message_iter_recurse(&element_i, &dict_i);
            dbus_message_iter_get_basic(&dict_i, &key);

            //printf("     %s\n",key);
            if (strcmp (key, "RSSI") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &rssi);
                //printf("RSSI is type %d\n",dbus_message_iter_get_arg_type(&variant_i));
                //printf("    rssi: %d\n",rssi);
                found_device.RSSI = rssi;
            }

            if (strcmp (key, "Name") == 0) {
                dbus_message_iter_next(&dict_i);
                dbus_message_iter_recurse(&dict_i, &variant_i);
                dbus_message_iter_get_basic(&variant_i, &value);
            
                //printf("    name: %s\n",value);

                //load the found device into our array
                found_device.found = FALSE; //Reset the flags
                strcpy(found_device.device_name,value);
                strcpy(found_device.bd_address,bd_addr);
                LoadScannedDevice(); //operates on found_device
            }
        }

        //load the new device into our list of scanned devices
        if (!dbus_message_iter_next(&element_i))
            break;
    }

    (void)dbus_type;

    return DBUS_HANDLER_RESULT_HANDLED;
}

static int 
parse_change (
    DBusMessage* msg
) {
    DBusMessageIter arg_i, variant_i;
    const char* value;
    const char* bd_addr;
    int dbus_type;

   // printf("\n\n\nBLUETOOTH DEVICE STATUS CHANGE:\n");
    if (!dbus_message_iter_init(msg, &arg_i)) {
       printf("GetProperties reply has no arguments.");
    }

    if (!dbus_message_get_args( msg, NULL,
                                DBUS_TYPE_STRING, &bd_addr,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Invalid arguments for NameOwnerChanged signal");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    //printf(" Name: %s\n",bd_addr);//"State" then the variant is a string
    if (strcmp(bd_addr,"State") == 0) {
        dbus_type = dbus_message_iter_get_arg_type(&arg_i);
       // printf("type is %d\n",dbus_type);

        if (dbus_type == DBUS_TYPE_STRING) {
            dbus_message_iter_next(&arg_i);
            dbus_message_iter_recurse(&arg_i, &variant_i);
            dbus_message_iter_get_basic(&variant_i, &value);      
            //  printf("    the new state is: %s\n",value);
            strncpy(gstBTRCoreDevStateCbInfo.cDeviceType, "Bluez", BTRCORE_STRINGS_MAX_LEN - 1);
            strncpy(gstBTRCoreDevStateCbInfo.cDevicePrevState, gstBTRCoreDevStateCbInfo.cDeviceCurrState, BTRCORE_STRINGS_MAX_LEN - 1);
            strncpy(gstBTRCoreDevStateCbInfo.cDeviceCurrState, value, BTRCORE_STRINGS_MAX_LEN - 1);

            if (p_Status_callback) {
                p_Status_callback(&gstBTRCoreDevStateCbInfo);
            }
        }
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult 
agent_filter (
    DBusConnection* conn,
    DBusMessage*    msg, 
    void*           data
) {
    const char *name, *old, *new;

    //printf("agent filter activated....\n");
    if (dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS,"DeviceCreated")) {
        printf("Device Created!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceFound")) {
        printf("Device Found!\n");
        parse_device(msg);

        /* call the registered cb */
        if (cb_DeviceDiscovery)
        {
            /* The device is not duplicate entry; So post it thro' callback */
            if (FALSE == found_device.found)
            {
                stBTRCoreScannedDevicesCount devicelist;
                memset (&devicelist, 0, sizeof(stBTRCoreScannedDevicesCount));

                memcpy (devicelist.devices, scanned_devices, sizeof (scanned_devices));
                devicelist.numberOfDevices = gNumberOfScannedDevices;
                cb_DeviceDiscovery (devicelist);
            }
        }
    }

    if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceDisappeared")) {
        printf("Device DeviceDisappeared!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceRemoved")) {
        printf("Device Removed!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Connected")) {
        printf("Device Connected - AudioSink!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Disconnected")) {
        printf("Device Disconnected - AudioSink!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.Headset","Connected")) {
        printf("Device Connected - Headset!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.Headset","Disconnected")) {
        printf("Device Disconnected - Headset!\n");
    }

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","PropertyChanged")) {
        printf("Device PropertyChanged!\n");
        parse_change(msg);
    }

    if (dbus_message_is_signal(msg, "org.bluez.Headset","PropertyChanged")) {
        printf("Device PropertyChanged!\n");
        parse_change(msg);
    }


    if (!dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args( msg, NULL,
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

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_Init (
    tBTRCoreHandle* phBTRCore
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL; 

    BTRCore_LOG(("BTRCore_Init\n"));
    p_Status_callback = NULL;//set callbacks to NULL, later an app can register callbacks


    if (!phBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
    }


    pstlhBTRCore = (stBTRCoreHdl*)malloc(sizeof(stBTRCoreHdl));
    if (!pstlhBTRCore) {
        fprintf(stderr, "%s:%d:%s - Insufficient memory - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInitFailure;
    }


    pstlhBTRCore->connHandle = BtrCore_BTInitGetConnection();
    if (!pstlhBTRCore->connHandle) {
        fprintf(stderr, "%s:%d:%s - Can't get on system bus - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    //init array of scanned , known & found devices
    btrCore_InitDataSt();


    pstlhBTRCore->agentPath = BtrCore_BTGetAgentPath();
    if (!pstlhBTRCore->agentPath) {
        fprintf(stderr, "%s:%d:%s - Can't get agent path - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }


    pstlhBTRCore->dispatchThreadQuit = FALSE;
    pthread_mutex_init(&pstlhBTRCore->dispatchMutex, NULL);
    if(pthread_create(&pstlhBTRCore->dispatchThread, NULL, DoDispatch, (void*)pstlhBTRCore)) {
        fprintf(stderr, "%s:%d:%s - Failed to create Dispatch Thread - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }


    if (!dbus_connection_add_filter(pstlhBTRCore->connHandle, agent_filter, NULL, NULL)) {
        fprintf(stderr, "%s:%d:%s - Can't add signal filter - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }


    dbus_bus_add_match(pstlhBTRCore->connHandle, "type='signal',interface='org.bluez.Adapter'", NULL); //mikek needed for device scan results


    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHandle, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        fprintf(stderr, "%s:%d:%s - Failed to get BT Adapter - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }


    printf("BTRCore_Init - adapter path %s\n", pstlhBTRCore->curAdapterPath);

    /* Initialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (enBTRCoreSuccess != BTRCore_AVMedia_Init(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath)) {
        fprintf(stderr, "%s:%d:%s - Failed to Init AV Media Subsystem - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        BTRCore_DeInit((tBTRCoreHandle)pstlhBTRCore);
        return enBTRCoreInitFailure;
    }

    *phBTRCore  = (tBTRCoreHandle)pstlhBTRCore;

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DeInit (
    tBTRCoreHandle  hBTRCore
) {
    void*           penDispThreadExitStatus = NULL;
    enBTRCoreRet    enDispThreadExitStatus = enBTRCoreFailure;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    fprintf(stderr, "hBTRCore   =   0x%8p\n", hBTRCore);

    /* Free any memory allotted for use in BTRCore */
    
    /* DeInitialize BTRCore SubSystems - AVMedia/Telemetry..etc. */

    if (enBTRCoreSuccess != BTRCore_AVMedia_DeInit(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath)) {
        fprintf(stderr, "Failed to DeInit AV Media Subsystem");
        enDispThreadExitStatus = enBTRCoreFailure;
    }


    dbus_bus_remove_match(pstlhBTRCore->connHandle, "type='signal',interface='org.bluez.Adapter'", NULL);

    pthread_mutex_lock(&pstlhBTRCore->dispatchMutex);
    pstlhBTRCore->dispatchThreadQuit = TRUE;
    pthread_mutex_unlock(&pstlhBTRCore->dispatchMutex);

    pthread_join(pstlhBTRCore->dispatchThread, &penDispThreadExitStatus);
    pthread_mutex_destroy(&pstlhBTRCore->dispatchMutex);
    

    fprintf(stderr, "BTRCore_DeInit - Exiting BTRCore - %d\n", *((enBTRCoreRet*)penDispThreadExitStatus));
    enDispThreadExitStatus = *((enBTRCoreRet*)penDispThreadExitStatus);
    free(penDispThreadExitStatus);



    if (pstlhBTRCore->curAdapterPath) {
        free(pstlhBTRCore->curAdapterPath);
        pstlhBTRCore->curAdapterPath = NULL;
    }

    if (pstlhBTRCore->agentPath) {
        free(pstlhBTRCore->agentPath);
        pstlhBTRCore->agentPath = NULL;
    }

    if (hBTRCore) {
        free(hBTRCore);
        hBTRCore = NULL;
    }

    return  enDispThreadExitStatus;
}


enBTRCoreRet
BTRCore_StartDiscovery (
    tBTRCoreHandle           hBTRCore,
    stBTRCoreStartDiscovery* pstStartDiscovery
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    btrCore_ClearScannedDevicesList();


    if (BtrCore_BTStartDiscovery(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    sleep(pstStartDiscovery->duration); //TODO: Better to setup a timer which calls BTStopDiscovery

    if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (pstlhBTRCore->curAdapterPath) {
        free(pstlhBTRCore->curAdapterPath);
        pstlhBTRCore->curAdapterPath = NULL;
    }

    pstlhBTRCore->curAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHandle, NULL); //mikek hard code to default adapter for now
    if (!pstlhBTRCore->curAdapterPath) {
        fprintf(stderr, "Failed to get BT Adapter");
        return enBTRCoreInvalidAdapter;
    }

    if (apstBTRCoreAdapter) {
        apstBTRCoreAdapter->adapter_number   = 0; //hard code to default adapter for now
        apstBTRCoreAdapter->pcAdapterPath    = pstlhBTRCore->curAdapterPath;
        apstBTRCoreAdapter->pcAdapterDevName = NULL;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapter (
    tBTRCoreHandle  hBTRCore,
    int             adapter_number
) {
    int pathlen;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pathlen = strlen(pstlhBTRCore->curAdapterPath);
    switch (adapter_number) {
        case 0:
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
            break;
        case 1:
            pstlhBTRCore->curAdapterPath[pathlen-1]='1';
            break;
        case 2:
            pstlhBTRCore->curAdapterPath[pathlen-1]='2';
            break;
        case 3:
            pstlhBTRCore->curAdapterPath[pathlen-1]='3';
            break;
        case 4:
            pstlhBTRCore->curAdapterPath[pathlen-1]='4';
            break;
        case 5:
            pstlhBTRCore->curAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
    }
    printf("Now current adatper is %s\n", pstlhBTRCore->curAdapterPath);

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_ListKnownDevices (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int pathlen; //temporary variable shoud be refactored away
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    pathlen = strlen(pstlhBTRCore->curAdapterPath);

    switch (apstBTRCoreAdapter->adapter_number) {
        case 0:
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
            break;
        case 1:
            pstlhBTRCore->curAdapterPath[pathlen-1]='1';
            break;
        case 2:
            pstlhBTRCore->curAdapterPath[pathlen-1]='2';
            break;
        case 3:
            pstlhBTRCore->curAdapterPath[pathlen-1]='3';
            break;
        case 4:
            pstlhBTRCore->curAdapterPath[pathlen-1]='4';
            break;
        case 5:
            pstlhBTRCore->curAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
    }

    printf("adapter path is %s\n", pstlhBTRCore->curAdapterPath);
    return populateListOfPairedDevices (hBTRCore, pstlhBTRCore->curAdapterPath);
}

enBTRCoreRet populateListOfPairedDevices(tBTRCoreHandle hBTRCore, const char* pAdapterPath)
{
    DBusError e;
    DBusMessageIter arg_i, element_i, variant_i;
    char **paths = NULL;
    const char * key;
    const char *value;
    int i;
    int num = -1;
    int paired;
    int connected;
    stBTRCoreHdl*   pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    dbus_error_init(&e);
    //path  busname interface  method

    DBusMessage* reply = sendMethodCall(pstlhBTRCore->connHandle, pAdapterPath, "org.bluez.Adapter", "ListDevices");
    if (reply != NULL) {
        if (!dbus_message_get_args(reply, &e, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Adapter.ListDevices returned an error: '%s'\n", e.message);
        }

        for ( i = 0; i < num; i++) {
            //printf("device: %d is %s\n",i,paths[i]);
            memset(known_devices[i].bd_path,'\0',sizeof(known_devices[i].bd_path));
            strcpy(known_devices[i].bd_path, paths[i]);
        }
        gNumberOfPairedDevices = num;
        dbus_message_unref(reply);
    }

    //mikek now lets see if we can get properties for each device we found...
    for ( i = 0; i < num; i++) {
        reply = sendMethodCall(pstlhBTRCore->connHandle, known_devices[i].bd_path, "org.bluez.Device", "GetProperties");

        if (!dbus_message_iter_init(reply, &arg_i)) {
            printf("GetProperties reply has no arguments.");
        }

        if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY) {
            printf("GetProperties argument is not an array.");
        }

        dbus_message_iter_recurse(&arg_i, &element_i);

        while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID) {

            if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter dict_i;

                dbus_message_iter_recurse(&element_i, &dict_i);
         
                dbus_message_iter_get_basic(&dict_i, &key);
                //printf("     %s\n",key);

                if (strcmp (key, "Name") == 0) {
                    dbus_message_iter_next(&dict_i);
                    dbus_message_iter_recurse(&dict_i, &variant_i);
                    dbus_message_iter_get_basic(&variant_i, &value);
                    printf("device: %d is %s  ", i, paths[i]);
                    printf("name: %s\n", value);

                    memset(known_devices[i].device_name, '\0', sizeof(known_devices[i].device_name));
                    strcpy(known_devices[i].device_name, value);
                }

                if (strcmp (key, "Paired") == 0) {
                    dbus_message_iter_next(&dict_i);
                    dbus_message_iter_recurse(&dict_i, &variant_i);
                    dbus_message_iter_get_basic(&variant_i, &paired);
                    printf(" paired: %d\n", paired);
                }

                if (strcmp (key, "Connected") == 0) {
                    dbus_message_iter_next(&dict_i);
                    dbus_message_iter_recurse(&dict_i, &variant_i);
                    dbus_message_iter_get_basic(&variant_i, &connected);
                    printf(" connected: %d\n", connected);
                }

                if (dbus_message_has_interface(reply, "org.bluez.Device")) {
                    printf(" got a device property!\n");
                }    
            }

            if (!dbus_message_iter_next(&element_i))
                break;
        }

        dbus_message_unref(reply);
    } //end for

  return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapters (
    tBTRCoreHandle          hBTRCore,
    stBTRCoreGetAdapters*   pstGetAdapters
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pstGetAdapters->number_of_adapters = GetAdapters(pstlhBTRCore->connHandle);

    return enBTRCoreSuccess;
}


/*BTRCore_ForgetDevice*/
enBTRCoreRet
BTRCore_ForgetDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    stBTRCoreKnownDevice* pstKnownDevice = &known_devices[aBTRCoreDevId]; 

    printf(" We will remove %s\n",known_devices[aBTRCoreDevId].bd_path);

    remove_paired_device(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstKnownDevice->bd_path);

    return enBTRCoreSuccess;
}


/*BTRCore_FindService, other inputs will include string and boolean pointer for returning*/
enBTRCoreRet
BTRCore_FindService (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId,
    const char*     UUID,
    char*           XMLdata,
    int*            found
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    //BTRCore_LOG(("BTRCore_FindService\n"));
    //printf("looking for %s\n", UUID);
    stBTRCoreKnownDevice*  pstKnownDevice = &known_devices[aBTRCoreDevId];

    printf("Checking for service %s on %s\n", UUID, known_devices[aBTRCoreDevId].bd_path);

    *found = discover_services(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, UUID, XMLdata);
    if (*found < 0) {
        return enBTRCoreFailure;
     }
     else {
        return enBTRCoreSuccess;
     }
}


enBTRCoreRet
BTRCore_ShowFoundDevices (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int i;
    int pathlen; //temporary variable shoud be refactored away
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    pathlen = strlen(pstlhBTRCore->curAdapterPath);

    switch (apstBTRCoreAdapter->adapter_number) {
        case 0:
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
            break;
        case 1:
            pstlhBTRCore->curAdapterPath[pathlen-1]='1';
            break;
        case 2:
            pstlhBTRCore->curAdapterPath[pathlen-1]='2';
            break;
        case 3:
            pstlhBTRCore->curAdapterPath[pathlen-1]='3';
            break;
        case 4:
            pstlhBTRCore->curAdapterPath[pathlen-1]='4';
            break;
        case 5:
            pstlhBTRCore->curAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            pstlhBTRCore->curAdapterPath[pathlen-1]='0';
    }

    printf("adapter path is %s\n", pstlhBTRCore->curAdapterPath);


    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        if (scanned_devices[i].found) {
            printf("Device %d. %s\n - %s  %d dbmV ",i,scanned_devices[i].device_name, scanned_devices[i].bd_address, scanned_devices[i].RSSI);
            btrCore_ShowSignalStrength(scanned_devices[i].RSSI);
            printf("\n\n");
        }
    }   

    return enBTRCoreSuccess;
}



enBTRCoreRet
BTRCore_PairDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    const char *capabilities = "NoInputNoOutput";   //I dont want to deal with pins and passcodes at this time
    stBTRCoreScannedDevices* pstScannedDevice = &scanned_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    printf(" We will pair %s\n",scanned_devices[aBTRCoreDevId].device_name);
    printf(" address %s\n",scanned_devices[aBTRCoreDevId].bd_address);

    if (create_paired_device(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstlhBTRCore->agentPath, capabilities, pstScannedDevice->bd_address) < 0) {
        BTRCore_LOG("pairing ERROR occurred\n");
        return enBTRCorePairingFailed;
    }

    return enBTRCoreSuccess;
}


/**See if a device has been previously paired***/
enBTRCoreRet
BTRCore_FindDevice (
    tBTRCoreHandle  hBTRCore,
    tBTRCoreDevId   aBTRCoreDevId
) {
    stBTRCoreScannedDevices* pstScannedDevice = &scanned_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    printf(" We will try to find %s\n",scanned_devices[aBTRCoreDevId].device_name);
    printf(" address %s\n",scanned_devices[aBTRCoreDevId].bd_address);

    if (find_paired_device(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, pstScannedDevice->bd_address) < 0) {
       // BTRCore_LOG("device not found\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


/*BTRCore_ConnectDevice*/
enBTRCoreRet
BTRCore_ConnectDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTDeviceType          lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice*   pstKnownDevice = &known_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    printf(" We will connect %s\n",known_devices[aBTRCoreDevId].bd_path);

    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
    // before making the connect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    if (BtrCore_BTConnectDevice(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, lenBTDeviceType)) {
        BTRCore_LOG("connection ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_DisconnectDevice (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTDeviceType          lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice*   pstKnownDevice = &known_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    printf(" We will disconnect %s\n",known_devices[aBTRCoreDevId].bd_path);

    // TODO: Implement a Device State Machine and Check whether the device is in a Disconnectable State
    // before making the Disconnect call
    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    if (BtrCore_BTDisconnectDevice(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, lenBTDeviceType)) {
        BTRCore_LOG("disconnection ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_AcquireDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType,
    int*                aiDataPath,
    int*                aidataReadMTU,
    int*                aidataWriteMTU
) {

    enBTDeviceType lenBTDeviceType = enBTDevUnknown;
    int liDataPath = 0;
    int lidataReadMTU = 0;
    int lidataWriteMTU = 0;

    stBTRCoreKnownDevice*   pstKnownDevice = &known_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (!aiDataPath || !aidataReadMTU || !aidataWriteMTU) {
        fprintf(stderr, "BTRCore_AcquireDeviceDataPath - Invalid Arguments \n");
        return enBTRCoreInvalidArg;
    }

    printf(" We will Acquire Data Path for %s\n",known_devices[aBTRCoreDevId].bd_path);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call

    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;
    if(enBTRCoreSuccess != BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->connHandle, pstKnownDevice->bd_path, &liDataPath, &lidataReadMTU, &lidataWriteMTU)) {
        BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    *aiDataPath     = liDataPath;
    *aidataReadMTU  = lidataReadMTU;
    *aidataWriteMTU = lidataWriteMTU;

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_ReleaseDeviceDataPath (
    tBTRCoreHandle      hBTRCore,
    tBTRCoreDevId       aBTRCoreDevId,
    enBTRCoreDeviceType aenBTRCoreDevType
) {
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;
    stBTRCoreKnownDevice*   pstKnownDevice = &known_devices[aBTRCoreDevId];
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    printf(" We will Release Data Path for %s\n",known_devices[aBTRCoreDevId].bd_path);

    // TODO: Implement a Device State Machine and Check whether the device is in a State  to acquire Device Data path
    // before making the call

    switch (aenBTRCoreDevType) {
    case enBTRCoreSpeakers:
    case enBTRCoreHeadSet:
        lenBTDeviceType = enBTDevAudioSink;
        break;
    case enBTRCoreMobileAudioIn:
    case enBTRCorePCAudioIn:
        lenBTDeviceType = enBTDevAudioSource;
        break;
    case enBTRCoreUnknown:
    default:
        lenBTDeviceType = enBTDevUnknown;
        break;
    }

    //TODO: Make a Device specific call baced on lenBTDeviceType
    (void)lenBTDeviceType;

    if(enBTRCoreSuccess != BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->connHandle, pstKnownDevice->bd_path)) {
        BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterStatusCallback (
    tBTRCoreHandle  hBTRCore,
    void*           cb
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    (void)pstlhBTRCore;

  p_Status_callback = cb;
  return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_EnableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int powered;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    powered = 1;
    BTRCore_LOG(("BTRCore_EnableAdapter\n"));

    apstBTRCoreAdapter->enable = TRUE;//does this even mean anything?


    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisableAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int powered;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    powered = 0;
    BTRCore_LOG(("BTRCore_DisableAdapter\n"));


    apstBTRCoreAdapter->enable = FALSE;


    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &powered)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverableTimeout (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    U32 timeout;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    timeout = apstBTRCoreAdapter->DiscoverableTimeout;


    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &timeout)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverable (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
    int discoverable;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    discoverable = apstBTRCoreAdapter->discoverable;


    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &discoverable)) {
        BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetAdapterDeviceName (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter,
	char*				apcAdapterDeviceName
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


	if (!apcAdapterDeviceName) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreInvalidArg;
	}

	if(apstBTRCoreAdapter->pcAdapterDevName) {
		free(apstBTRCoreAdapter->pcAdapterDevName);
		apstBTRCoreAdapter->pcAdapterDevName = NULL;
	}

    apstBTRCoreAdapter->pcAdapterDevName = strdup(apcAdapterDeviceName); //TODO: Free this memory

    if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropName, &(apstBTRCoreAdapter->pcAdapterDevName))) {
        BTRCore_LOG("Set Adapter Property enBTAdPropName - FAILED\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}

enBTRCoreRet BTRCore_GetListOfAdapters (tBTRCoreHandle hBTRCore, stBTRCoreListAdapters* pstListAdapters)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    DBusConnection* pConnHandle = NULL;

    if ((!hBTRCore) || (!pstListAdapters))
    {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            DBusError err;
            char **paths = NULL;
            int i;
            int num = -1;

            dbus_error_init(&err);
            DBusMessage *reply = sendMethodCall(pConnHandle, "/", "org.bluez.Manager", "ListAdapters");
            if (!reply)
            {
                printf("%s:%d - org.bluez.Manager.GetProperties returned an error: '%s'\n", __FUNCTION__, __LINE__, err.message);
                rc = enBTRCoreFailure;
			    dbus_error_free(&err);
            }
            else
            {
                if (dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID))
                {
                    pstListAdapters->number_of_adapters = num;
                    for (i = 0; i < num; i++)
                    {
                        printf("Adapter Path: %d is %s\n", i, paths[i]);
                        memset(pstListAdapters->adapter_path[i], '\0', sizeof(pstListAdapters->adapter_path[i]));
                        strncpy(pstListAdapters->adapter_path[i], paths[i], BD_NAME_LEN);
                    }
                    rc = enBTRCoreSuccess;
                }
                else
                {
                    printf("%s:%d - org.bluez.Manager.GetProperties parsing failed '%s'\n", __FUNCTION__, __LINE__, err.message);
                    rc = enBTRCoreFailure;
			        dbus_error_free(&err);
                }

                dbus_message_unref(reply);
            }
        }
    }

    return rc;
}

enBTRCoreRet BTRCore_SetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pAdapterName)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;

    if ((!hBTRCore) || (!pAdapterPath) ||(!pAdapterName))
    {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else
    {

        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropName, &pAdapterName)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropName - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else
        {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Name Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_SetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char powerStatus)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int power = powerStatus;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropPowered, &power)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropPowered - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else
        {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Powered Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_SetAdapterDiscoverableTimeout (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned short timeout)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    U32 givenTimeout = (U32) timeout;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverableTimeOut, &givenTimeout)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverableTimeOut - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else
        {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.DiscoverableTimeout Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_SetAdapterDiscoverable (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char discoverable)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    int isDiscoverable = (int) discoverable;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        if (BtrCore_BTSetAdapterProp(pstlhBTRCore->connHandle, pstlhBTRCore->curAdapterPath, enBTAdPropDiscoverable, &isDiscoverable)) {
            BTRCore_LOG("Set Adapter Property enBTAdPropDiscoverable - FAILED\n");
            rc = enBTRCoreFailure;
        }
        else
        {
            rc = enBTRCoreSuccess;
            printf("%s:%d - Set value for org.bluez.Adapter.Discoverable Success\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}

enBTRCoreRet get_property (DBusConnection* pConnection, const char* pDevicePath, const char *pInterface, const char* pKey, int type, void* pValue)
{
    enBTRCoreRet rc = enBTRCoreFailure;

    if ((!pConnection) || (!pDevicePath) || (!pInterface) || (!pKey) || (!pValue))
    {
        printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        DBusMessage *reply = NULL;
        DBusMessageIter arg_i, element_i, variant_i;
        DBusError err;
        const char *pParsedKey = NULL;
        const char *pParsedValueString = NULL;
        int parsedValueNumber = 0;
        U32 parsedValueUnsignedNumber = 0;


        /* */
        dbus_error_init(&err);
        reply = sendMethodCall(pConnection, pDevicePath, pInterface, "GetProperties");
        if (!reply)
        {
            printf("%s:%d - %s.GetProperties returned an error: '%s'\n", __FUNCTION__, __LINE__, pInterface, err.message);
            rc = enBTRCoreFailure;
            dbus_error_free(&err);
        }
        else
        {
            if (!dbus_message_iter_init(reply, &arg_i))
            {
                printf("GetProperties reply has no arguments.");
                rc = enBTRCoreFailure;
            }
            else if (dbus_message_iter_get_arg_type(&arg_i) != DBUS_TYPE_ARRAY)
            {
                printf("GetProperties argument is not an array.");
                rc = enBTRCoreFailure;
            }
            else
            {
                dbus_message_iter_recurse(&arg_i, &element_i);
                while (dbus_message_iter_get_arg_type(&element_i) != DBUS_TYPE_INVALID)
                {
                    if (dbus_message_iter_get_arg_type(&element_i) == DBUS_TYPE_DICT_ENTRY)
                    {
                        DBusMessageIter dict_i;
                        dbus_message_iter_recurse(&element_i, &dict_i);
                        dbus_message_iter_get_basic(&dict_i, &pParsedKey);

                        if ((pParsedKey) && (strcmp (pParsedKey, pKey) == 0))
                        {
                            dbus_message_iter_next(&dict_i);
                            dbus_message_iter_recurse(&dict_i, &variant_i);
                            if (type == DBUS_TYPE_STRING)
                            {
                                dbus_message_iter_get_basic(&variant_i, &pParsedValueString);
                                printf("Key is %s and the value in string is %s\n", pParsedKey, pParsedValueString);
                                strncpy (pValue, pParsedValueString, BD_NAME_LEN);
                            }
                            else if (type == DBUS_TYPE_UINT32)
                            {
                                unsigned int *ptr = (unsigned int*) pValue;
                                dbus_message_iter_get_basic(&variant_i, &parsedValueUnsignedNumber);
                                printf("Key is %s and the value is %u\n", pParsedKey, parsedValueUnsignedNumber);
                                *ptr = parsedValueUnsignedNumber;
                            }
                            else /* As of now ints and bools are used. This function has to be extended for array if needed */
                            {
                                int *ptr = (int*) pValue;
                                dbus_message_iter_get_basic(&variant_i, &parsedValueNumber);
                                printf("Key is %s and the value is %d\n", pParsedKey, parsedValueNumber);
                                *ptr = parsedValueNumber;
                            }
                            rc = enBTRCoreSuccess;
                            break;
                        }
                    }

                    if (!dbus_message_iter_next(&element_i))
                        break;
                }
            }

            if (dbus_error_is_set(&err))
            {
                printf("%s:%d - Some failure noticed and the err message is %s\n", __FUNCTION__, __LINE__, err.message);
                dbus_error_free(&err);
            }

            dbus_message_unref(reply);
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_GetAdapterName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, char* pAdapterName)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    DBusConnection* pConnHandle = NULL;
    char name[BD_NAME_LEN + 1] = "";

    memset (name, '\0', sizeof (name));

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterName))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }

        rc = get_property (pConnHandle, pAdapterPath, "org.bluez.Adapter", "Name", DBUS_TYPE_STRING, name);
        if (enBTRCoreSuccess == rc)
        {
            printf("%s:%d - Get value for org.bluez.Adapter.Name = %s\n", __FUNCTION__, __LINE__, name);
            strcpy(pAdapterName, name);
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.Name failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}

enBTRCoreRet BTRCore_GetAdapterPower (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pAdapterPower)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    DBusConnection* pConnHandle = NULL;
    int powerStatus = 0;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pAdapterPower))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }

        rc = get_property (pConnHandle, pAdapterPath, "org.bluez.Adapter", "Powered", DBUS_TYPE_BOOLEAN, &powerStatus);
        if (enBTRCoreSuccess == rc)
        {
            printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, powerStatus);
            *pAdapterPower = (unsigned char) powerStatus;
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.powered failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}

enBTRCoreRet BTRCore_GetAdapterDiscoverableStatus (tBTRCoreHandle hBTRCore, const char* pAdapterPath, unsigned char* pDiscoverable)
{
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    DBusConnection* pConnHandle = NULL;
    int discoverable = 0;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDiscoverable))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }

        rc = get_property (pConnHandle, pAdapterPath, "org.bluez.Adapter", "Discoverable", DBUS_TYPE_BOOLEAN, &discoverable);
        if (enBTRCoreSuccess == rc)
        {
            printf("%s:%d - Get value for org.bluez.Adapter.powered = %d\n", __FUNCTION__, __LINE__, discoverable);
            *pDiscoverable = (unsigned char) discoverable;
        }
        else
            printf("%s:%d - Get value for org.bluez.Adapter.powered failed\n", __FUNCTION__, __LINE__);
    }

  return rc;
}


enBTRCoreRet BTRCore_StartDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath)
{
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    char* pAgentPath = NULL;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;
        pAgentPath = pstlhBTRCore->agentPath;

        if ((!pConnHandle) || (!pAgentPath))
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            btrCore_ClearScannedDevicesList();
            if (0 == BtrCore_BTStartDiscovery(pConnHandle, pAdapterPath, pAgentPath))
                rc = enBTRCoreSuccess;
            else
                printf("%s:%d - Failed to Start\n", __FUNCTION__, __LINE__);
        }
    }

    return rc;
}

enBTRCoreRet BTRCore_StopDeviceDiscovery (tBTRCoreHandle hBTRCore, const char* pAdapterPath)
{
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    char* pAgentPath = NULL;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pAdapterPath)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;
        pAgentPath = pstlhBTRCore->agentPath;

        if ((!pConnHandle) || (!pAgentPath))
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            if (0 ==  BtrCore_BTStopDiscovery(pConnHandle, pAdapterPath, pAgentPath))
                rc = enBTRCoreSuccess;
            else
                printf("%s:%d - Failed to Stop\n", __FUNCTION__, __LINE__);
        }
    }
    return rc;
}

const char* getScannedDeviceAddress (const char* pDeviceName)
{
    int loop = 0;
    if (!pDeviceName)
        return NULL;

    if (gNumberOfScannedDevices)
    {
        for (loop = 0; loop < gNumberOfScannedDevices; loop++)
        {
            if (strcmp(pDeviceName, scanned_devices[loop].device_name) == 0)
             return scanned_devices[loop].bd_address;
        }
    }

    return NULL;
}

const char* getKnownDeviceAddress(const char* pDeviceName)
{
    int loop = 0;
    if (!pDeviceName)
        return NULL;

    if (gNumberOfPairedDevices)
    {
        for (loop = 0; loop < gNumberOfPairedDevices; loop++)
        {
            if (strcmp(pDeviceName, known_devices[loop].device_name) == 0)
             return known_devices[loop].bd_path;
        }
    }

    return NULL;
}

enBTRCoreRet BTRCore_PairDeviceByName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pDeviceName)
{
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    char* pAgentPath = NULL;
    /* We can enhance the BTRCore with passcode support later point in time */
    const char *pCapabilities = "NoInputNoOutput";

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDeviceName))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;
        pAgentPath = pstlhBTRCore->agentPath;

        if ((!pConnHandle) || (!pAgentPath))
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            const char *pDeviceAddress = getScannedDeviceAddress(pDeviceName);
            if (pDeviceAddress)
            {
                if (create_paired_device(pConnHandle, pAdapterPath, pAgentPath, pCapabilities, pDeviceAddress) < 0)
                {
                    printf("%s:%d - Failed to pair a device\n", __FUNCTION__, __LINE__);
                    rc = enBTRCorePairingFailed;
                }
                else
                {
                    rc = enBTRCoreSuccess;
                    printf("%s:%d - Pairing Success\n", __FUNCTION__, __LINE__);

                    /* Keep the list upto date */
                    populateListOfPairedDevices (hBTRCore, pAdapterPath);
                }
            }
            else
            {
                printf("%s:%d - Failed to find a instance in the scanned list\n", __FUNCTION__, __LINE__);
                rc = enBTRCorePairingFailed;
            }
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_GetListOfPairedDevices (tBTRCoreHandle hBTRCore, const char* pAdapterPath, stBTRCorePairedDevicesCount *pListOfDevices)
{
    enBTRCoreRet rc = enBTRCoreNotInitialized;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pListOfDevices))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else if (enBTRCoreSuccess ==  populateListOfPairedDevices (hBTRCore, pAdapterPath))
    {
        pListOfDevices->numberOfDevices = gNumberOfPairedDevices;
        memcpy (pListOfDevices->devices, known_devices, sizeof (known_devices));
        rc = enBTRCoreSuccess;
        printf("%s:%d - Copied all the known devices\n", __FUNCTION__, __LINE__);
    }
    return rc;
}

enBTRCoreRet BTRCore_GetListOfScannedDevices (tBTRCoreHandle hBTRCore, const char* pAdapterPath, stBTRCoreScannedDevicesCount *pListOfScannedDevices)
{
    enBTRCoreRet rc = enBTRCoreInvalidArg;

    if ((!hBTRCore) || (!pAdapterPath) || (!pListOfScannedDevices))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        memset (pListOfScannedDevices, 0, sizeof(stBTRCoreScannedDevicesCount));
        memcpy (pListOfScannedDevices->devices, scanned_devices, sizeof (scanned_devices));
        pListOfScannedDevices->numberOfDevices = gNumberOfScannedDevices;
        printf("%s:%d - Copied all scanned devices\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreSuccess;
    }
    return rc;
}

enBTRCoreRet BTRCore_UnPairDeviceByName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pDeviceName)
{
    enBTRCoreRet rc = enBTRCoreNotInitialized;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    /* We can enhance the BTRCore with passcode support later point in time */

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDeviceName))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            if (0 == gNumberOfPairedDevices)
            {
                printf("%s:%d - Possibly the list is not populated\n", __FUNCTION__, __LINE__);
                /* Keep the list upto date */
                populateListOfPairedDevices (hBTRCore, pAdapterPath);
            }

            if (gNumberOfPairedDevices)
            {
                const char *pDeviceAddress = getKnownDeviceAddress(pDeviceName);
                if (pDeviceAddress)
                {
                    if (0 != remove_paired_device(pConnHandle, pAdapterPath, pDeviceAddress))
                    {
                        printf("%s:%d - Failed to unpair a device\n", __FUNCTION__, __LINE__);
                        rc = enBTRCorePairingFailed;
                    }
                    else
                    {
                        rc = enBTRCoreSuccess;
                        printf("%s:%d - UnPairing Success\n", __FUNCTION__, __LINE__);

                        /* Keep the list upto date */
                        populateListOfPairedDevices (hBTRCore, pAdapterPath);
                    }
                }
                else
                {
                    printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
                    rc = enBTRCorePairingFailed;
                }
            }
            else
            {
                printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
                rc = enBTRCoreFailure;
            }

        }
    }
    return rc;
}

void BTRCore_RegisterDiscoveryCallback (tBTRCoreHandle  hBTRCore, BTRMgr_DeviceDiscoveryCallback cb)
{
    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
    }

    if (!cb_DeviceDiscovery)
    {
        cb_DeviceDiscovery = cb;
        printf("%s:%d - Callback Registered Successfully\n", __FUNCTION__, __LINE__);
    }

    return;
}

enBTRCoreRet BTRCore_ConnectDeviceByName (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pDeviceName, enBTRCoreDeviceType aenBTRCoreDevType)
{
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDeviceName))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            if (0 == gNumberOfPairedDevices)
            {
                printf("%s:%d - Possibly the list is not populated; like booted and connecting\n", __FUNCTION__, __LINE__);
                /* Keep the list upto date */
                populateListOfPairedDevices (hBTRCore, pAdapterPath);
            }

            if (gNumberOfPairedDevices)
            {
                const char *pDeviceAddress = getKnownDeviceAddress(pDeviceName);
                if (pDeviceAddress)
                {
                    // TODO: Implement a Device State Machine and Check whether the device is in a Connectable State
                    // before making the connect call
                    switch (aenBTRCoreDevType)
                    {
                        case enBTRCoreSpeakers:
                        case enBTRCoreHeadSet:
                            lenBTDeviceType = enBTDevAudioSink;
                            break;
                        case enBTRCoreMobileAudioIn:
                        case enBTRCorePCAudioIn:
                            lenBTDeviceType = enBTDevAudioSource;
                            break;
                        case enBTRCoreUnknown:
                        default:
                            lenBTDeviceType = enBTDevUnknown;
                            break;
                    }
                    if (0 == BtrCore_BTConnectDevice(pConnHandle, pDeviceAddress, lenBTDeviceType))
                    {
                        rc = enBTRCoreSuccess;
                        printf("%s:%d - Connected to device Successfully. Lets start Play the audio\n", __FUNCTION__, __LINE__);
                        /* TODO */
                    }
                    else
                        printf("%s:%d - Connect to device failed\n", __FUNCTION__, __LINE__);
                }
                else
                {
                    printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
                }
            }
            else
            {
                printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
            }
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_DisconnectDeviceByName (tBTRCoreHandle hBTRCore, const char* pDeviceName, enBTRCoreDeviceType aenBTRCoreDevType)
{
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    DBusConnection* pConnHandle = NULL;
    enBTDeviceType lenBTDeviceType = enBTDevUnknown;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pDeviceName)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        /* Get the handle */
        pConnHandle = pstlhBTRCore->connHandle;

        if (!pConnHandle)
        {
            printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
            rc = enBTRCoreNotInitialized;
        }
        else
        {
            if (gNumberOfPairedDevices)
            {
                const char *pDeviceAddress = getKnownDeviceAddress(pDeviceName);
                if (pDeviceAddress)
                {

                    /* TODO */
                    /* Stop the audio playback before disconnect */

                    switch (aenBTRCoreDevType)
                    {
                        case enBTRCoreSpeakers:
                        case enBTRCoreHeadSet:
                            lenBTDeviceType = enBTDevAudioSink;
                            break;
                        case enBTRCoreMobileAudioIn:
                        case enBTRCorePCAudioIn:
                            lenBTDeviceType = enBTDevAudioSource;
                            break;
                        case enBTRCoreUnknown:
                        default:
                            lenBTDeviceType = enBTDevUnknown;
                            break;
                    }
                    if (0 == BtrCore_BTDisconnectDevice(pConnHandle, pDeviceAddress, lenBTDeviceType))
                    {
                        rc = enBTRCoreSuccess;
                        printf("%s:%d - DisConnected from device Successfully.\n", __FUNCTION__, __LINE__);
                    }
                    else
                        printf("%s:%d - DisConnect from device failed\n", __FUNCTION__, __LINE__);
                }
                else
                {
                    printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
                }
            }
            else
            {
                printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
            }
        }
    }
    return rc;
}

enBTRCoreRet BTRCore_GetDeviceDataPath (tBTRCoreHandle hBTRCore, const char* pAdapterPath, const char* pDeviceName, int* pDeviceFD, int* pDeviceReadMTU, int* pDeviceWriteMTU)
{
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;
    int liDataPath = 0;
    int lidataReadMTU = 0;
    int lidataWriteMTU = 0;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if ((!pAdapterPath) || (!pDeviceName) || (!pDeviceFD) || (!pDeviceReadMTU) || (!pDeviceWriteMTU))
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        if (0 == gNumberOfPairedDevices)
        {
            printf("%s:%d - Possibly the list is not populated; like booted and connecting\n", __FUNCTION__, __LINE__);
            /* Keep the list upto date */
            populateListOfPairedDevices (hBTRCore, pAdapterPath);
        }

        if (gNumberOfPairedDevices)
        {
            const char *pDeviceAddress = getKnownDeviceAddress(pDeviceName);
            if (pDeviceAddress)
            {
                if(enBTRCoreSuccess != BTRCore_AVMedia_AcquireDataPath(pstlhBTRCore->connHandle, pDeviceAddress, &liDataPath, &lidataReadMTU, &lidataWriteMTU))
                {
                    BTRCore_LOG("AVMedia_AcquireDataPath ERROR occurred\n");
                    rc = enBTRCoreFailure;
                }
                else
                {
                    *pDeviceFD = liDataPath;
                    *pDeviceReadMTU = lidataReadMTU;
                    *pDeviceWriteMTU = lidataWriteMTU;
                    rc = enBTRCoreSuccess;
                }
            }
            else
            {
                printf("%s:%d - Failed to find a instance in the paired devices list\n", __FUNCTION__, __LINE__);
            }
        }
        else
        {
            printf("%s:%d - There is no device paried for this adapter\n", __FUNCTION__, __LINE__);
        }
    }

    return rc;
}


enBTRCoreRet BTRCore_FreeDeviceDataPath (tBTRCoreHandle hBTRCore, const char* pDeviceName)
{
    enBTRCoreRet rc = enBTRCoreFailure;
    stBTRCoreHdl*  pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    if (!hBTRCore)
    {
        printf("%s:%d - enBTRCoreInitFailure\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreNotInitialized;
    }
    else if (!pDeviceName)
    {
        printf("%s:%d - enBTRCoreInvalidArg\n", __FUNCTION__, __LINE__);
        rc = enBTRCoreInvalidArg;
    }
    else
    {
        if (gNumberOfPairedDevices)
        {
            const char *pDeviceAddress = getKnownDeviceAddress(pDeviceName);
            if (pDeviceAddress)
            {
                if(enBTRCoreSuccess != BTRCore_AVMedia_ReleaseDataPath(pstlhBTRCore->connHandle, pDeviceAddress))
                {
                    BTRCore_LOG("AVMedia_ReleaseDataPath ERROR occurred\n");
                }
                else
                {
                    BTRCore_LOG("AVMedia_ReleaseDataPath Success\n");
                    rc = enBTRCoreSuccess;
                }
            }
            else
                BTRCore_LOG("Given device is Not found\n");
        }
        else
            BTRCore_LOG("No device is found\n");
    }

    return rc;
}

/* End of File */
