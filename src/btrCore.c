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



static char *gBTRAdapterPath = NULL;

static stBTRCoreScannedDevices  scanned_devices[BTRCORE_MAX_NUM_BT_DEVICES];//holds twenty scanned devices
static stBTRCoreScannedDevices  found_device;                        //a device for intermediate dbus processing
static stBTRCoreKnownDevice     known_devices[BTRCORE_MAX_NUM_BT_DEVICES];   //holds twenty known devices
static stBTRCoreDevStateCB      gstBTRCoreDevStateCbInfo;               //holds info for a callback

static void btrCore_InitDataSt (void);
static void btrCore_ClearScannedDevicesList (void);
static DBusMessage* sendMethodCall (DBusConnection* conn, const char* objectpath, const char* busname, const char* interfacename, const char* methodname);
static int remove_paired_device (DBusConnection* conn, const char* apBTRAdapterPath, const char* fullpath);


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

    /* Found Device */
    memset (found_device.bd_address, '\0', sizeof(BD_NAME));
    memset (found_device.device_name, '\0', sizeof(BD_NAME));
    found_device.RSSI = INT_MIN;
    found_device.found = FALSE;

    /* Known Devices */
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        memset (known_devices[i].bd_path, '\0', sizeof(BD_NAME));
        memset (known_devices[i].device_name, '\0', sizeof(BD_NAME));
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
    const char*     busname, 
    const char*     interfacename, 
    const char*     methodname
) {
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
set_property (
    DBusConnection* conn, 
    const char*     apBTRAdapterPath,
    const char*     key, 
    int             type, 
    void*           val
) {
	DBusMessage *message, *reply;
	DBusMessageIter array, value;
	DBusError error;
	const char *signature;

	message = dbus_message_new_method_call( "org.bluez", 
                                            apBTRAdapterPath,
                                            "org.bluez.Adapter",
                                            "SetProperty");

	if (!message)
		return -ENOMEM;

	switch (type) {
        case DBUS_TYPE_BOOLEAN:
            signature = DBUS_TYPE_BOOLEAN_AS_STRING;
            break;
        case DBUS_TYPE_UINT32:
            signature = DBUS_TYPE_UINT32_AS_STRING;
            break;
        case DBUS_TYPE_STRING:
            signature = DBUS_TYPE_STRING_AS_STRING;
            break;
        default:
            return -EILSEQ;
	}

	dbus_message_iter_init_append(message, &array);
	dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&array, DBUS_TYPE_VARIANT, signature, &value);
	dbus_message_iter_append_basic(&value, type, val);
	dbus_message_iter_close_container(&array, &value);
	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(conn,	message, -1, &error);
	dbus_message_unref(message);

	if (!reply) {
		if (dbus_error_is_set(&error) == TRUE) {
			fprintf(stderr, "%s\n", error.message);
			dbus_error_free(&error);
		} 
        else {
			fprintf(stderr, "Failed to set property\n");
        }

		return -EIO;
	}

	dbus_message_unref(reply);
//	BTRCore_LOG("Set property %s for %s\n", key, adapter);
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
                break;
            }
        }
    }
}


void
test_func (
    stBTRCoreGetAdapter* pstGetAdapter
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

    gBTRAdapterPath = pstlhBTRCore->curAdapterPath;

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


    if (BtrCore_BTStartDiscovery(pstlhBTRCore->connHandle, gBTRAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    sleep(pstStartDiscovery->duration); //TODO: Better to setup a timer which calls BTStopDiscovery

    if (BtrCore_BTStopDiscovery(pstlhBTRCore->connHandle, gBTRAdapterPath, pstlhBTRCore->agentPath)) {
        return enBTRCoreDiscoveryFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapter (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
) {
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    if (gBTRAdapterPath) {
        free(gBTRAdapterPath);
        gBTRAdapterPath = NULL;
    }

    gBTRAdapterPath = BtrCore_BTGetAdapterPath(pstlhBTRCore->connHandle, NULL); //mikek hard code to default adapter for now
    if (!gBTRAdapterPath) {
        fprintf(stderr, "Failed to get BT Adapter");
        return enBTRCoreInvalidAdapter;
    }

    if (pstGetAdapter) {
        pstGetAdapter->adapter_number   = 0; //hard code to default adapter for now
        pstGetAdapter->pcAdapterPath    = gBTRAdapterPath;
        pstGetAdapter->pcAdapterDevName = NULL;
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
            gBTRAdapterPath[pathlen-1]='0';
            break;
        case 1:
            gBTRAdapterPath[pathlen-1]='1';
            break;
        case 2:
            gBTRAdapterPath[pathlen-1]='2';
            break;
        case 3:
            gBTRAdapterPath[pathlen-1]='3';
            break;
        case 4:
            gBTRAdapterPath[pathlen-1]='4';
            break;
        case 5:
            gBTRAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            gBTRAdapterPath[pathlen-1]='0';
    }
    printf("Now current adatper is %s\n",gBTRAdapterPath);

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_ListKnownDevices (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
) {
    DBusError e;
    DBusMessageIter arg_i, element_i, variant_i;
    char **paths = NULL;
    const char * key;
    const char *value;
    int i;
    int num = -1;
    int paired;
    int connected;
    int pathlen; //temporary variable shoud be refactored away

    //const char *pcAdapterPath;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    //gBTRAdapterPath = get_adapter_path(gBTRConnHandle, pstGetAdapter->adapter_number);
    pathlen = strlen(gBTRAdapterPath);

    switch (pstGetAdapter->adapter_number) {
        case 0:
            gBTRAdapterPath[pathlen-1]='0';
            break;
        case 1:
            gBTRAdapterPath[pathlen-1]='1';
            break;
        case 2:
            gBTRAdapterPath[pathlen-1]='2';
            break;
        case 3:
            gBTRAdapterPath[pathlen-1]='3';
            break;
        case 4:
            gBTRAdapterPath[pathlen-1]='4';
            break;
        case 5:
            gBTRAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            gBTRAdapterPath[pathlen-1]='0';
    }

    printf("adapter path is %s\n",gBTRAdapterPath);

    dbus_error_init(&e);
    //path  busname interface  method

    DBusMessage* reply = sendMethodCall(pstlhBTRCore->connHandle, gBTRAdapterPath, "org.bluez", "org.bluez.Adapter", "ListDevices");
    if (reply != NULL) {
        if (!dbus_message_get_args(reply, &e, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Adapter.ListDevices returned an error: '%s'\n", e.message);
        }

        for ( i = 0; i < num; i++) {
            //printf("device: %d is %s\n",i,paths[i]);
            memset(known_devices[i].bd_path,'\0',sizeof(known_devices[i].bd_path));
            strcpy(known_devices[i].bd_path, paths[i]);
        }

        dbus_message_unref(reply);
    }

    //mikek now lets see if we can get properties for each device we found...
    for ( i = 0; i < num; i++) {
        reply = sendMethodCall(pstlhBTRCore->connHandle, known_devices[i].bd_path, "org.bluez", "org.bluez.Device", "GetProperties");

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

    remove_paired_device(pstlhBTRCore->connHandle, gBTRAdapterPath, pstKnownDevice->bd_path);

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
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
) {
    int i;
    int pathlen; //temporary variable shoud be refactored away
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    //gBTRAdapterPath = get_adapter_path(gBTRConnHandle, pstGetAdapter->adapter_number);
    pathlen = strlen(pstlhBTRCore->curAdapterPath);

    switch (pstGetAdapter->adapter_number) {
        case 0:
            gBTRAdapterPath[pathlen-1]='0';
            break;
        case 1:
            gBTRAdapterPath[pathlen-1]='1';
            break;
        case 2:
            gBTRAdapterPath[pathlen-1]='2';
            break;
        case 3:
            gBTRAdapterPath[pathlen-1]='3';
            break;
        case 4:
            gBTRAdapterPath[pathlen-1]='4';
            break;
        case 5:
            gBTRAdapterPath[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            gBTRAdapterPath[pathlen-1]='0';
    }

    printf("adapter path is %s\n",gBTRAdapterPath);


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

    if (create_paired_device(pstlhBTRCore->connHandle, gBTRAdapterPath, pstlhBTRCore->agentPath, capabilities, pstScannedDevice->bd_address) < 0) {
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

    if (find_paired_device(pstlhBTRCore->connHandle, gBTRAdapterPath, pstScannedDevice->bd_address) < 0) {
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
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
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

    pstGetAdapter->enable = TRUE;//does this even mean anything?

    set_property(pstlhBTRCore->connHandle, gBTRAdapterPath, "Powered", DBUS_TYPE_BOOLEAN, &powered);
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisableAdapter (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
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


    pstGetAdapter->enable = FALSE;
    set_property(pstlhBTRCore->connHandle, gBTRAdapterPath, "Powered", DBUS_TYPE_BOOLEAN, &powered);
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverableTimeout (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
) {
    U32 timeout;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;

    timeout = pstGetAdapter->DiscoverableTimeout;

    set_property(pstlhBTRCore->connHandle, gBTRAdapterPath, "DiscoverableTimeout", DBUS_TYPE_UINT32, &timeout);

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverable (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter
) {
    int discoverable;
    stBTRCoreHdl*   pstlhBTRCore = NULL;

    if (!hBTRCore) {
        fprintf(stderr, "%s:%d:%s - enBTRCoreInvalidArg - enBTRCoreInitFailure\n", __FILE__, __LINE__, __FUNCTION__);
        return enBTRCoreNotInitialized;
    }

    pstlhBTRCore = (stBTRCoreHdl*)hBTRCore;


    discoverable = pstGetAdapter->discoverable;

    set_property(pstlhBTRCore->connHandle, gBTRAdapterPath, "Discoverable", DBUS_TYPE_BOOLEAN, &discoverable);

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetAdapterDeviceName (
    tBTRCoreHandle       hBTRCore,
    stBTRCoreGetAdapter* pstGetAdapter,
	char*				 apcAdapterDeviceName
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

	if(pstGetAdapter->pcAdapterDevName) {
		free(pstGetAdapter->pcAdapterDevName);
		pstGetAdapter->pcAdapterDevName = NULL;
	}

  pstGetAdapter->pcAdapterDevName = strdup(apcAdapterDeviceName); //TODO: Free this memory

  set_property(pstlhBTRCore->connHandle, gBTRAdapterPath, "Name", DBUS_TYPE_STRING, &(pstGetAdapter->pcAdapterDevName));

  return enBTRCoreSuccess;
}

