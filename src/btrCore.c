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



static char *adapter_path = NULL;
static char *agent_path = NULL;


static pthread_t        dispatchThread;
static pthread_mutex_t  dispatchMutex;
static volatile BOOLEAN dispatchThreadQuit;


#if 0
static char dbus_device[32];                    //device string in dbus format
#endif 

static DBusConnection *gDBusConn = NULL;

stBTRCoreScannedDevices scanned_devices[BTRCORE_MAX_NUM_BT_DEVICES];//holds twenty scanned devices
static stBTRCoreScannedDevices found_device;                               //a device for intermediate dbus processing
stBTRCoreKnownDevice   known_devices[BTRCORE_MAX_NUM_BT_DEVICES];  //holds twenty known devices
static stBTRCoreDevStatusCB    callback_info;                              //holds info for a callback

static void btrCore_InitDataSt (void);
static void btrCore_ClearScannedDevicesList (void);
static DBusMessage* sendMethodCall (DBusConnection* conn, const char* objectpath, const char* busname, const char* interfacename, const char* methodname);
static int remove_paired_device (DBusConnection* conn, const char* adapter_path, const char* fullpath);
static int btrDevConnectFullPath (DBusConnection* conn, const char* fullpath, enBTRCoreDeviceType enDeviceType);
static int btrDevDisconnectFullPath (DBusConnection* conn, const char* fullpath, enBTRCoreDeviceType enDeviceType);


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
    memset(callback_info.device_type, '\0', sizeof(callback_info.device_type));
    memset(callback_info.device_state, '\0', sizeof(callback_info.device_state));

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
    if (!dbus_connection_send_with_reply(gDBusConn, methodcall, &pending, -1)) { //Send and expect reply using pending call object
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
    const char*     adapter_path, 
    const char*     fullpath
) {
    dbus_bool_t success;
    DBusMessage *msg;
        
   // BTRCore_LOG("fullpath is %s\n",fullpath);
    msg = dbus_message_new_method_call( "org.bluez",
                                        adapter_path,
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
btrDevConnectFullPath (
    DBusConnection*     conn,
    const char*         fullpath,
    enBTRCoreDeviceType enDeviceType
) {
    dbus_bool_t  success;
    DBusMessage* msg;
    char         dbusIfce[32] = {'\0'};
        
    //BTRCore_LOG("fullpath is %s\n",fullpath);

    switch (enDeviceType) {
        case enBTRCoreAudioSink:
            strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
            break;
        case enBTRCoreHeadSet:
            strncpy(dbusIfce, "org.bluez.Headset", strlen("org.bluez.Headset"));
            break;
        default:
            strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
            break;
    }

    msg = dbus_message_new_method_call( "org.bluez",
                                        fullpath,
                                        dbusIfce,
                                        "Connect");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

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
    const char*     adapter,
    const char*     key, 
    int             type, 
    void*           val
) {
	DBusMessage *message, *reply;
	DBusMessageIter array, value;
	DBusError error;
	const char *signature;

	message = dbus_message_new_method_call( "org.bluez", 
                                            adapter_path,
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
btrDevDisconnectFullPath (
    DBusConnection*     conn,
    const char*         fullpath,
    enBTRCoreDeviceType enDeviceType
) {
    dbus_bool_t  success;
    DBusMessage* msg;
    char         dbusIfce[32] = {'\0'};
        
    //BTRCore_LOG("fullpath is %s\n",fullpath);

    switch (enDeviceType) {
        case enBTRCoreAudioSink:
            strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
            break;
        case enBTRCoreHeadSet:
            strncpy(dbusIfce, "org.bluez.Headset", strlen("org.bluez.Headset"));
            break;
        default:
            strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
            break;
    }

    msg = dbus_message_new_method_call( "org.bluez",
                                        fullpath,
                                        dbusIfce,
                                        "Disconnect");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

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
    const char*     adapter_path,
    const char*     device
) {

    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    msg = dbus_message_new_method_call( "org.bluez",
                                        adapter_path,
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
    const char*     adapter_path,
    const char*     agent_path,
    const char*     capabilities,
    const char*     device
) {
   
    DBusMessage* msg;
    DBusMessage* reply;
    DBusError err;

    msg = dbus_message_new_method_call( "org.bluez", 
                                        adapter_path,
                                        "org.bluez.Adapter",
                                        "CreatePairedDevice");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &device,
                             DBUS_TYPE_OBJECT_PATH, &agent_path,
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
    char*           message;
    BOOLEAN         ldispatchThreadQuit = FALSE;
    enBTRCoreRet*   penDispThreadExitStatus = malloc(sizeof(enBTRCoreRet));

    message = (char*) ptr;
    printf("%s \n", message);



    if (!gDBusConn) {
        fprintf(stderr, "Dispatch thread failure - BTRCore not initialized\n");
        *penDispThreadExitStatus = enBTRCoreNotInitialized;
        return (void*)penDispThreadExitStatus;
    }
    
    while (1) {
        pthread_mutex_lock (&dispatchMutex);
        ldispatchThreadQuit = dispatchThreadQuit;
        pthread_mutex_unlock (&dispatchMutex);

        if (ldispatchThreadQuit == TRUE)
            break;

#if 1
        sleep(1);
#else
        sched_yield(); // Would like to use some form of yield rather than sleep sometime in the future
#endif
        if (dbus_connection_read_write_dispatch(gDBusConn, 500) != TRUE)
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


#if 0
static char*
get_default_adapter_path (
    DBusConnection* conn
) {
    DBusMessage *msg, *reply;
    DBusError err;
    const char *reply_path;
    char *path;

    msg = dbus_message_new_method_call( "org.bluez", 
                                        "/",
                                        "org.bluez.Manager", 
                                        "DefaultAdapter");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return NULL;
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

        return NULL;
    }

    if (!dbus_message_get_args( reply, &err,
                                DBUS_TYPE_OBJECT_PATH, &reply_path,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Can't get reply arguments\n");
        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return NULL;
    }

    path = strdup(reply_path);

    dbus_message_unref(reply);

    dbus_connection_flush(conn);

    return path;
}



static char*
get_adapter_path (
    DBusConnection* conn, 
    const char*     adapter
) {
    DBusMessage *msg, *reply;
    DBusError err;
    const char *reply_path;
    char *path;

    if (!adapter)
        return get_default_adapter_path(conn);

    msg = dbus_message_new_method_call( "org.bluez",
                                        "/",
                                        "org.bluez.Manager",
                                        "FindAdapter");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return NULL;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &adapter,
                    DBUS_TYPE_INVALID);

    dbus_error_init(&err);

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

    dbus_message_unref(msg);

    if (!reply) {
        fprintf(stderr, "Can't find adapter %s\n", adapter);
        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return NULL;
    }

    if (!dbus_message_get_args( reply, &err,
                                DBUS_TYPE_OBJECT_PATH, &reply_path,
                                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Can't get reply arguments\n");

        if (dbus_error_is_set(&err)) {
            fprintf(stderr, "%s\n", err.message);
            dbus_error_free(&err);
        }

        return NULL;
    }

    path = strdup(reply_path);

    dbus_message_unref(reply);

    dbus_connection_flush(conn);

    return path;
}


static int
StopDiscovery (
    DBusConnection* conn, 
    const char*     adapter_path,
    const char*     agent_path,
    const char*     device
) {
    dbus_bool_t success;
    DBusMessage *msg;

   // printf("calling StopDiscovery\n");

    msg = dbus_message_new_method_call( "org.bluez", 
                                        adapter_path,
                                        "org.bluez.Adapter",
                                        "StopDiscovery");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

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
StartDiscovery (
    DBusConnection* conn, 
    const char*     adapter_path,
    const char*     agent_path,
    const char*     device
) {
    dbus_bool_t success;
    DBusMessage *msg;

    //printf("calling StartDiscovery with %s\n",adapter_path);

    msg = dbus_message_new_method_call( "org.bluez",
                                        adapter_path,
                                        "org.bluez.Adapter",
                                        "StartDiscovery");

    if (!msg) {
        fprintf(stderr, "Can't allocate new method call\n");
        return -1;
    }

    /*dbus_message_append_args(msg, DBUS_TYPE_STRING, &device,
                    DBUS_TYPE_OBJECT_PATH, &agent_path,

                    DBUS_TYPE_INVALID);*/

    success = dbus_connection_send(conn, msg, NULL);

    dbus_message_unref(msg);

    if (!success) {
        fprintf(stderr, "Not enough memory for message send\n");
        return -1;
    }

    dbus_connection_flush(conn);

    return 0;
}
#endif

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

#if 1
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
#endif

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
            strncpy(callback_info.device_type,"Bluez",60);
            strncpy(callback_info.device_state,value,60);

            if (p_Status_callback) {
                p_Status_callback(&callback_info);
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
    if (dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS,"DeviceCreated"))
        printf("mikek Device Created!\n");

    if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceFound")) {
        //printf("mikek Device Found!\n");
        parse_device(msg);
    }


    /* if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceDisappeared"))
        printf("mikek Device DeviceDisappeared!\n");

    if (dbus_message_is_signal(msg, "org.bluez.Adapter","DeviceRemoved"))
        printf("mikek Device Removed!\n");
    */

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","PropertyChanged")) {
        //printf("mikek Device PropertyChanged!\n");  
        parse_change(msg);
    }

    if (dbus_message_is_signal(msg, "org.bluez.Headset","PropertyChanged")) {
        //printf("mikek Device PropertyChanged!\n");  
        parse_change(msg);
    }


    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Connected"))
        printf("Device Connected - AudioSink!\n");

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Disconnected")) {
        printf("Device Disconnected - AudioSink!\n");
    }

    if (dbus_message_is_method_call(msg, "org.bluez.MediaEndpoint","SelectConfiguration"))
        printf("MediaEndpoint - SelectConfiguration!\n");

    if (dbus_message_is_method_call(msg, "org.bluez.MediaEndpoint","SetConfiguration")) {
        printf("MediaEndpoint - SetConfiguration!\n");
    }

    if (dbus_message_is_method_call(msg, "org.bluez.MediaEndpoint","ClearConfiguration")) {
        printf("MediaEndpoint - ClearConfiguration!\n");
    }


    /*
    if (dbus_message_is_signal(msg, "org.bluez.Headset","Connected"))
        printf("Device Connected - Headset!\n");

    if (dbus_message_is_signal(msg, "org.bluez.Headset","Disconnected"))
        printf("Device Disconnected - Headset!\n");
    */

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
        //__io_terminated = 1;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_Init (
    void
) {
    char *message2 = "Dispatch Thread Started";
#if 0
    char default_path[128];
#endif

    BTRCore_LOG(("BTRCore_Init\n"));
    p_Status_callback = NULL;//set callbacks to NULL, later an app can register callbacks

#if 0
    gDBusConn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
#else
    gDBusConn = BtrCore_BTInitGetConnection();
#endif
    if (!gDBusConn) {
        fprintf(stderr, "Can't get on system bus");
        return enBTRCoreInitFailure;
    }

    //init array of scanned , known & found devices
    btrCore_InitDataSt();

#if 0
    snprintf(default_path, sizeof(default_path), "/org/bluez/agent_%d", getpid());
    agent_path = strdup(default_path);
#else
    agent_path = BtrCore_BTGetAgentPath();
#endif

#if 0
    adapter_path = get_adapter_path(gDBusConn, NULL); //mikek hard code to default adapter for now
#else
    adapter_path = BtrCore_BTGetAdapterPath(gDBusConn, NULL); //mikek hard code to default adapter for now
#endif
    if (!adapter_path) {
        fprintf(stderr, "Failed to get BT Adapter");
        return enBTRCoreInitFailure;
    }

    printf("BTRCore_Init - adapter path %s\n",adapter_path);

    /* Initialize BTRCore SubSystems - AVMedia/Telemetry..etc. */
    if (enBTRCoreSuccess != BTRCore_AVMedia_Init(gDBusConn, adapter_path)) {
        fprintf(stderr, "Failed to Init AV Media Subsystem");
        return enBTRCoreInitFailure;
    }


    dispatchThreadQuit = FALSE;
    pthread_mutex_init(&dispatchMutex, NULL);

    if(pthread_create(&dispatchThread, NULL, DoDispatch, (void*)message2)) {
        fprintf(stderr, "Failed to create Dispatch Thread");
        return enBTRCoreInitFailure;
    }

    if (!dbus_connection_add_filter(gDBusConn, agent_filter, NULL, NULL)) {
        fprintf(stderr, "Can't add signal filter");
        return enBTRCoreInitFailure;
    }

    dbus_bus_add_match(gDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL); //mikek needed for device scan results
    dbus_bus_add_match(gDBusConn, "type='signal',interface='org.bluez.AudioSink'", NULL); //mikek needed?
    dbus_bus_add_match(gDBusConn, "type='signal',interface='org.bluez.Headset'", NULL);

    dbus_bus_add_match(gDBusConn, "interface='org.bluez.MediaEndPoint'", NULL); //mikek needed? // Chandresh - Try this dbus_connection_register_object_path

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DeInit (
    void
) {
    void*           penDispThreadExitStatus = NULL;
    enBTRCoreRet    enDispThreadExitStatus = enBTRCoreFailure;

    pthread_mutex_lock(&dispatchMutex);
    dispatchThreadQuit = TRUE;
    pthread_mutex_unlock(&dispatchMutex);

    /* Free any memory allotted for use in BTRCore */

    pthread_join(dispatchThread, &penDispThreadExitStatus);
    pthread_mutex_destroy(&dispatchMutex);

    fprintf(stderr, "BTRCore_DeInit - Exiting BTRCore - %d\n", *((enBTRCoreRet*)penDispThreadExitStatus));
    enDispThreadExitStatus = *((enBTRCoreRet*)penDispThreadExitStatus);
    free(penDispThreadExitStatus);

    return  enDispThreadExitStatus;
}


enBTRCoreRet
BTRCore_StartDiscovery (
    stBTRCoreStartDiscovery* pstStartDiscovery
) {
    btrCore_ClearScannedDevicesList();

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_StartDiscovery - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

#if 0
    if (StartDiscovery(gDBusConn, adapter_path, agent_path, dbus_device) < 0) {
        dbus_connection_unref(gDBusConn); // Do we really need to unref the global connection just because Discovery failed
        return enBTRCoreDiscoveryFailure;
    }
#else
    if (BtrCore_BTStartDiscovery(gDBusConn, adapter_path, agent_path)) {
        return enBTRCoreDiscoveryFailure;
    }
#endif

    sleep(pstStartDiscovery->duration); //TODO: Better to setup a timer which calls BTStopDiscovery
    
#if 0
    if (StopDiscovery(gDBusConn, adapter_path, agent_path, dbus_device) < 0) {
        dbus_connection_unref(gDBusConn);
        return enBTRCoreDiscoveryFailure;
    }
#else
    if (BtrCore_BTStopDiscovery(gDBusConn, adapter_path, agent_path)) {
        return enBTRCoreDiscoveryFailure;
    }
#endif

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_GetAdapter (
    stBTRCoreGetAdapter* pstGetAdapter
) {
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_GetAdapter - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

#if 0
    adapter_path = get_adapter_path(gDBusConn, NULL); //mikek hard code to default adapter for now
#else
    adapter_path = BtrCore_BTGetAdapterPath(gDBusConn, NULL); //mikek hard code to default adapter for now
#endif

#if 0
    //*Bluez call hci_get_route with NULL to get instance of first available adapter
    if (pstGetAdapter->first_available == TRUE) {
        pstGetAdapter->adapter_number = hci_get_route(NULL);
        BTRCore_LOG("BTRCore_GetAdapter found adapter %d\n",pstGetAdapter->adapter_number);

        if (pstGetAdapter->adapter_number == 255) {
            return enBTRCoreFailure;
        }
        else {
            return enBTRCoreSuccess;
        }
    }
    else {
        hci_open_dev((int)pstGetAdapter->adapter_number);
        //TODO: check errors here
    }
#endif
    if (!adapter_path) {
        fprintf(stderr, "Failed to get BT Adapter");
        return enBTRCoreInvalidAdapter;
    }

    if (pstGetAdapter) {
        pstGetAdapter->adapter_number = 0; //hard code to default adapter for now
        pstGetAdapter->adapter_path = adapter_path;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_SetAdapter (
    int adapter_number
) {
    int pathlen;

    pathlen = strlen(adapter_path);
    switch (adapter_number) {
        case 0:
            adapter_path[pathlen-1]='0';
            break;
        case 1:
            adapter_path[pathlen-1]='1';
            break;
        case 2:
            adapter_path[pathlen-1]='2';
            break;
        case 3:
            adapter_path[pathlen-1]='3';
            break;
        case 4:
            adapter_path[pathlen-1]='4';
            break;
        case 5:
            adapter_path[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            adapter_path[pathlen-1]='0';
    }
    printf("Now current adatper is %s\n",adapter_path);

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_ListKnownDevices (
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

    //const char *adapter_path;

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_ListKnownDevices - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    //adapter_path = get_adapter_path(gDBusConn, pstGetAdapter->adapter_number);
    pathlen = strlen(adapter_path);

    switch (pstGetAdapter->adapter_number) {
        case 0:
            adapter_path[pathlen-1]='0';
            break;
        case 1:
            adapter_path[pathlen-1]='1';
            break;
        case 2:
            adapter_path[pathlen-1]='2';
            break;
        case 3:
            adapter_path[pathlen-1]='3';
            break;
        case 4:
            adapter_path[pathlen-1]='4';
            break;
        case 5:
            adapter_path[pathlen-1]='5';
            break;
        default:
            printf("max adapter value is 5, setting default\n");//6 adapters seems like plenty for now
            adapter_path[pathlen-1]='0';
    }

    printf("adapter path is %s\n",adapter_path);

    dbus_error_init(&e);
    //path  busname interface  method

    DBusMessage* reply = sendMethodCall(gDBusConn, adapter_path, "org.bluez", "org.bluez.Adapter", "ListDevices");

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
        reply = sendMethodCall(gDBusConn, known_devices[i].bd_path, "org.bluez", "org.bluez.Device", "GetProperties");

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
    stBTRCoreGetAdapters* pstGetAdapters
) {
    //BTRCore_LOG(("BTRCore_GetAdapters\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_GetAdapters - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }


    pstGetAdapters->number_of_adapters = GetAdapters(gDBusConn);

    return enBTRCoreSuccess;
}


/*BTRCore_ForgetDevice*/
enBTRCoreRet
BTRCore_ForgetDevice (
    stBTRCoreKnownDevice* pstKnownDevice
) {
    //BTRCore_LOG(("BTRCore_ForgetDevice\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_ForgetDevice - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    remove_paired_device(gDBusConn, adapter_path, pstKnownDevice->bd_path);

    return enBTRCoreSuccess;
}


/*BTRCore_FindService, other inputs will include string and boolean pointer for returning*/
enBTRCoreRet
BTRCore_FindService (
    stBTRCoreKnownDevice*  pstKnownDevice,
    const char*             UUID,
    char*                   XMLdata,
    int*                    found
) {
    //BTRCore_LOG(("BTRCore_FindService\n"));
    //printf("looking for %s\n", UUID);
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_FindService - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    *found = discover_services(gDBusConn, pstKnownDevice->bd_path, UUID, XMLdata);
    
    if (*found < 0) {
        return enBTRCoreFailure;
     }
     else {
        return enBTRCoreSuccess;
     }
}


enBTRCoreRet
BTRCore_PairDevice (
    stBTRCoreScannedDevices* pstScannedDevice
) {
    const char *capabilities = "NoInputNoOutput";   //I dont want to deal with pins and passcodes at this time

    //BTRCore_LOG(("BTRCore_PairDevice\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_PairDevice - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    if (create_paired_device(gDBusConn, adapter_path, agent_path, capabilities, pstScannedDevice->bd_address) < 0) {
        BTRCore_LOG("pairing ERROR occurred\n");
        return enBTRCorePairingFailed;
    }

    return enBTRCoreSuccess;
}


/**See if a device has been previously paired***/
enBTRCoreRet
BTRCore_FindDevice (
    stBTRCoreScannedDevices* pstScannedDevice
) {
    //BTRCore_LOG(("BTRCore_FindDevice\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_FindDevice - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    if (find_paired_device(gDBusConn, adapter_path, pstScannedDevice->bd_address) < 0) {
       // BTRCore_LOG("device not found\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


/*BTRCore_ConnectDevice*/
enBTRCoreRet
BTRCore_ConnectDevice (
    stBTRCoreKnownDevice* pstKnownDevice,
    enBTRCoreDeviceType enDeviceType
) {
    //BTRCore_LOG(("BTRCore_ConnectDevice\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_ConnectDevice - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    if (btrDevConnectFullPath(gDBusConn, pstKnownDevice->bd_path, enDeviceType) < 0) {
        BTRCore_LOG("connection ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_DisconnectDevice (
    stBTRCoreKnownDevice* pstKnownDevice,
    enBTRCoreDeviceType enDeviceType
) {
   // BTRCore_LOG(("BTRCore_DisconnectDevice\n"));
    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_DisconnectDevice - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    if (btrDevDisconnectFullPath(gDBusConn, pstKnownDevice->bd_path, enDeviceType) < 0) {
        BTRCore_LOG("disconnection ERROR occurred\n");
        return enBTRCoreFailure;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_RegisterStatusCallback (
    void* cb
) {
  p_Status_callback = cb;
  return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_EnableAdapter (
    stBTRCoreGetAdapter* pstGetAdapter
) {
    int powered;
    powered = 1;
    BTRCore_LOG(("BTRCore_EnableAdapter\n"));

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_EnableAdapter - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    pstGetAdapter->enable = TRUE;//does this even mean anything?

    set_property(gDBusConn, adapter_path, "Powered", DBUS_TYPE_BOOLEAN, &powered);
    return enBTRCoreSuccess;
}


enBTRCoreRet
BTRCore_DisableAdapter (
    stBTRCoreGetAdapter* pstGetAdapter
) {
    int powered;
    powered = 0;
    BTRCore_LOG(("BTRCore_DisableAdapter\n"));

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_DisableAdapter - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    pstGetAdapter->enable = FALSE;
    set_property(gDBusConn, adapter_path, "Powered", DBUS_TYPE_BOOLEAN, &powered);
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverableTimeout (
    stBTRCoreGetAdapter* pstGetAdapter
) {
    U32 timeout;

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_SetDiscoverableTimeout - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    timeout = pstGetAdapter->DiscoverableTimeout;
    set_property(gDBusConn, adapter_path, "DiscoverableTimeout", DBUS_TYPE_UINT32, &timeout);
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDiscoverable (
    stBTRCoreGetAdapter* pstGetAdapter
) {
    int discoverable;

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_SetDiscoverable - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

    discoverable = pstGetAdapter->discoverable;
    set_property(gDBusConn, adapter_path, "Discoverable", DBUS_TYPE_BOOLEAN, &discoverable);
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_SetDeviceName (
    stBTRCoreGetAdapter* pstGetAdapter
) {
  char * myname;

    if (!gDBusConn) {
        fprintf(stderr, "BTRCore_SetDeviceName - BTRCore not initialized\n");
        return enBTRCoreNotInitialized;
    }

  myname=pstGetAdapter->device_name;
  set_property(gDBusConn, adapter_path, "Name", DBUS_TYPE_STRING, &myname);
  return enBTRCoreSuccess;
}

