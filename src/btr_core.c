//bt_test_imp.c

#include <stdio.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for getpid
#include <pthread.h>    //for StopDiscovery test
#include <string.h>  //for strcnp
#include <errno.h> //for error numbers
#include <dbus/dbus.h>

#include "btr.h"


static char default_path[128];
char *adapter_path = NULL;
static char *agent_path = NULL;
static int  iret1;

static char *message2 = "Dispatch Thread Started";
static pthread_t dispatchThread;

const char *capabilities = "NoInputNoOutput";//I dont want to deal with pins and passcodes at this time

static char dbus_device[32];//device string in dbus format
DBusConnection *conn;

tScannedDevices scanned_devices[20];//holds twenty scanned devices
tScannedDevices found_device;//a device for intermediate dbus processing
tKnownDevices known_devices[20];//holds twenty known devices
tStatusCB callback_info;//holds info for a callback

static DBusMessage* sendMethodCall (const char* objectpath, const char* busname, const char* interfacename, const char* methodname);
static int remove_paired_device (DBusConnection* conn, const char* adapter_path, const char* fullpath);
static int btrDevConnectFullPath (DBusConnection* conn, const char* fullpath, eBtrDeviceType e_device_type);
static int btrDevDisconnectFullPath (DBusConnection* conn, const char* fullpath, eBtrDeviceType e_device_type);


static DBusMessage* 
sendMethodCall (
    const char* objectpath, 
    const char* busname, 
    const char* interfacename, 
    const char* methodname
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

    dbus_pending_call_block(pending); //Now block on the pending call
    reply = dbus_pending_call_steal_reply(pending); //Get the reply message from the queue
    dbus_pending_call_unref(pending); //Free pending call handle

    if (dbus_message_get_type(reply) ==  DBUS_MESSAGE_TYPE_ERROR) {
        printf("Error : %s\n\n",dbus_message_get_error_name(reply));
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
        
   //BT_LOG("fullpath is %s\n and service UUID is %s",fullpath,search_string);
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
    // printf("type is %d\n",dbus_type);
    
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
        
   // BT_LOG("fullpath is %s\n",fullpath);
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
    DBusConnection* conn,
    const char*     fullpath,
    eBtrDeviceType  e_device_type
) {
    dbus_bool_t  success;
    DBusMessage* msg;
    char         dbusIfce[32] = {'\0'};
        
    //BT_LOG("fullpath is %s\n",fullpath);

    switch (e_device_type) {
    case eBtrAudioSink:
        strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    case eBtrHeadSet:
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

static int set_property(DBusConnection *conn, const char *adapter,
					const char *key, int type, void *val)
{
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
	dbus_message_iter_open_container(&array, DBUS_TYPE_VARIANT,
							signature, &value);
	dbus_message_iter_append_basic(&value, type, val);
	dbus_message_iter_close_container(&array, &value);
	dbus_error_init(&error);
	reply = dbus_connection_send_with_reply_and_block(conn,
							message, -1, &error);
	dbus_message_unref(message);
	if (!reply) {
		if (dbus_error_is_set(&error) == TRUE) {
			fprintf(stderr, "%s\n", error.message);
			dbus_error_free(&error);
		} else
			fprintf(stderr, "Failed to set property\n");
		return -EIO;
	}
	dbus_message_unref(reply);
//	BT_LOG("Set property %s for %s\n", key, adapter);
	return 0;
}


static int 
btrDevDisconnectFullPath (
    DBusConnection* conn,
    const char*     fullpath,
    eBtrDeviceType  e_device_type
) {
    dbus_bool_t  success;
    DBusMessage* msg;
    char         dbusIfce[32] = {'\0'};
        
    //BT_LOG("fullpath is %s\n",fullpath);

    switch (e_device_type) {
    case eBtrAudioSink:
        strncpy(dbusIfce, "org.bluez.AudioSink", strlen("org.bluez.AudioSink"));
        break;
    case eBtrHeadSet:
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
    DBusMessage*     reply;
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
    DBusMessage*     reply;
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
ClearScannedDeviceList (
    void
) {
    int i;
    for (i = 0; i < 15; i++) {
        scanned_devices[i].found=0;
        memset(scanned_devices[i].device_name,'\0',sizeof(scanned_devices[i].device_name));
        memset(scanned_devices[i].bd_address,'\0',sizeof(scanned_devices[i].bd_address));
    }
}

void
LoadScannedDevice (
    void
) {
    int i;
    int found;
    int last;

    found = 0;
    last = 0;

    //printf("LoadScannedDevice processing %s-%s\n",found_device.bd_address,found_device.device_name);
    for ( i = 0; i < 15; i++) {
        if (scanned_devices[i].found)
            last++; //keep track of last valid record in array

        if (strcmp(found_device.bd_address, scanned_devices[i].bd_address) == 0) {
            found = 1;
            break;
        }
    }

    if (found == 0) { //device wasnt there, we got to add it
        for (i = 0; i < 15; i++) {
            if (!scanned_devices[i].found) {
                //printf("adding %s at location %d\n",found_device.bd_address,i);
                strcpy(scanned_devices[i].bd_address,found_device.bd_address);
                strcpy(scanned_devices[i].device_name,found_device.device_name);
                scanned_devices[i].RSSI = found_device.RSSI;
                scanned_devices[i].found = 1; //mark the record as found
                break;
            }
        }
    }
}


void
test_func (tGetAdapter* p_get_adapter
) {

if (p_Status_callback != NULL)
  {
    p_Status_callback();
  }
  else
  {
    printf("no callback installed\n");
  }
}



void*
DoDispatch (
    void* ptr
) {
    char *message;
    message = (char *) ptr;
    printf("%s \n", message);
    
    //  dbus_connection_read_write_dispatch(conn, -1);
    while (1) {
        sleep(1);
        if (dbus_connection_read_write_dispatch(conn, 500) != TRUE)
            break;
    }

    return NULL;
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
    if (strcmp(bd_addr,"State") == 0)
    {
    dbus_type = dbus_message_iter_get_arg_type(&arg_i);
   // printf("type is %d\n",dbus_type);

    if (dbus_type == DBUS_TYPE_STRING) {
       dbus_message_iter_next(&arg_i);
       dbus_message_iter_recurse(&arg_i, &variant_i);
       dbus_message_iter_get_basic(&variant_i, &value);      
      //  printf("    the new state is: %s\n",value);
        strncpy(callback_info.device_type,"Bluez",60);
        strncpy(callback_info.device_state,value,60);
        if (p_Status_callback)
           {
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
    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","PropertyChanged"))
       {
         //printf("mikek Device PropertyChanged!\n");  
         parse_change(msg);
       }
    if (dbus_message_is_signal(msg, "org.bluez.Headset","PropertyChanged"))
       {
         //printf("mikek Device PropertyChanged!\n");  
         parse_change(msg);
       }
/*
    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Connected"))
        printf("Device Connected - AudioSink!\n");

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink","Disconnected"))
         {
           printf("Device Disconnected - AudioSink!\n");
         }

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

////////////////////////////////////
BT_error
BT_Init (
    void
) {
    BT_LOG(("BT_Init\n"));
    p_Status_callback = NULL;//set callbacks to NULL, later an app can register callbacks


    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    snprintf(default_path, sizeof(default_path), "/org/bluez/agent_%d", getpid());
    agent_path = strdup(default_path);

    adapter_path = get_adapter_path(conn, NULL); //mikek hard code to default adapter for now
    printf("BT_Init - adapter path %s\n",adapter_path);

    iret1 = pthread_create( &dispatchThread, NULL, DoDispatch, (void*) message2);

    if (!dbus_connection_add_filter(conn, agent_filter, NULL, NULL))
        fprintf(stderr, "Can't add signal filter");

    dbus_bus_add_match(conn, "type='signal',interface='org.bluez.Adapter'", NULL); //mikek needed for device scan results
    dbus_bus_add_match(conn, "type='signal',interface='org.bluez.AudioSink'", NULL); //mikek needed?
    dbus_bus_add_match(conn, "type='signal',interface='org.bluez.Headset'", NULL);

    return NO_ERROR;
}


BT_error
BT_StartDiscovery (
    tStartDiscovery* p_start_discovery
) {
    ClearScannedDeviceList();

    if (StartDiscovery(conn, adapter_path, agent_path, dbus_device) < 0) {
        dbus_connection_unref(conn);
        exit(1);
    }

    sleep(p_start_discovery->duration);
    

    if (StopDiscovery(conn, adapter_path, agent_path, dbus_device) < 0) {
        dbus_connection_unref(conn);
        exit(1);
    }

    

    return NO_ERROR;
}


BT_error
BT_GetAdapter (
    tGetAdapter* p_get_adapter
) {
    adapter_path = get_adapter_path(conn, NULL); //mikek hard code to default adapter for now

#if 0
    //*Bluez call hci_get_route with NULL to get instance of first available adapter
    if (p_get_adapter->first_available == TRUE) {
        p_get_adapter->adapter_number = hci_get_route(NULL);
        BT_LOG("BT_GetAdapter found adapter %d\n",p_get_adapter->adapter_number);

        if (p_get_adapter->adapter_number == 255) {
            return ERROR1;
        }
        else {
            return NO_ERROR;
        }
    }
    else {
        hci_open_dev((int)p_get_adapter->adapter_number);
        //TODO: check errors here
    }
#endif
    return NO_ERROR;
}


BT_error 
BT_ListKnownDevices (
    tGetAdapter* p_get_adapter
) {
    DBusError e;
    DBusMessageIter arg_i, element_i, variant_i;
    char **paths = NULL;
    const char * key;
    const char *value;
    int i;
    int num = -1;
    DBusConnection *conn;
    int paired;
    int connected;
    int pathlen; //temporary variable shoud be refactored away

    //const char *adapter_path;

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    //adapter_path = get_adapter_path(conn, p_get_adapter->adapter_number);
    pathlen = strlen(adapter_path);

    switch (p_get_adapter->adapter_number) {
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

    DBusMessage* reply = sendMethodCall(adapter_path, "org.bluez", "org.bluez.Adapter", "ListDevices");

    if (reply != NULL) {
        if (!dbus_message_get_args(reply, &e, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &paths, &num, DBUS_TYPE_INVALID)) {
            printf("org.bluez.Adapter.ListDevices returned an error: '%s'\n", e.message);
        }

        for ( i = 0; i < num; i++) {
            //printf("device: %d is %s\n",i,paths[i]);
            memset(known_devices[i].bd_path,'\0',sizeof(known_devices[i].bd_path));
            strcpy(known_devices[i].bd_path,paths[i]);
        }

        dbus_message_unref(reply);
    }

    //mikek now lets see if we can get properties for each device we found...
    for ( i = 0; i < num; i++) {
        reply = sendMethodCall(known_devices[i].bd_path, "org.bluez", "org.bluez.Device", "GetProperties");

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
                    printf("device: %d is %s  ",i,paths[i]);
                    printf("name: %s\n",value);
                }

                if (strcmp (key, "Paired") == 0) {
                    dbus_message_iter_next(&dict_i);
                    dbus_message_iter_recurse(&dict_i, &variant_i);
                    dbus_message_iter_get_basic(&variant_i, &paired);
                    printf(" paired: %d\n",paired);
                }

                if (strcmp (key, "Connected") == 0) {
                    dbus_message_iter_next(&dict_i);
                    dbus_message_iter_recurse(&dict_i, &variant_i);
                    dbus_message_iter_get_basic(&variant_i, &connected);
                    printf(" connected: %d\n",connected);
                }

                if (dbus_message_has_interface(reply, "org.bluez.Device")) {
                    printf(" got a device property!\n");
                }    
            }

            if (!dbus_message_iter_next(&element_i))
                break;
        }

        dbus_message_unref(reply);
    }//end for
  return NO_ERROR;
}


BT_error
BT_GetAdapters (
    tGetAdapters* p_get_adapters
) {
    DBusConnection *conn;

    //BT_LOG(("BT_GetAdapters\n"));

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    p_get_adapters->number_of_adapters = GetAdapters(conn);

    return NO_ERROR;
}


/*BT_ForgetDevice*/
BT_error
BT_ForgetDevice (
    tKnownDevices* p_known_device
) {
    DBusConnection *conn;

    //BT_LOG(("BT_ForgetDevice\n"));

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    remove_paired_device(conn,adapter_path, p_known_device->bd_path);

    return NO_ERROR;
}


/*BT_FindService, other inputs will include string and boolean pointer for returning*/
BT_error
BT_FindService (
    tKnownDevices* p_known_device,const char * UUID,char * XMLdata, int * found
) {
    DBusConnection *conn;

    //BT_LOG(("BT_FindService\n"));
    //printf("looking for %s\n",UUID);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }


    *found = discover_services(conn,p_known_device->bd_path,UUID,XMLdata);
    
    if (*found < 0)
     {
        return ERROR1;
     }
     else
     {
        return NO_ERROR;
     }
}
BT_error
BT_PairDevice (
    tScannedDevices* p_scanned_device
) {

    //BT_LOG(("BT_PairDevice\n"));

    if (create_paired_device(conn, adapter_path, agent_path, capabilities, p_scanned_device->bd_address) < 0) {
        BT_LOG("pairing ERROR occurred\n");
        return ERROR1;
    }

    return NO_ERROR;
}

/**See if a device has been previously paired***/
BT_error
BT_FindDevice (
    tScannedDevices* p_scanned_device
) {

    //BT_LOG(("BT_FindDevice\n"));

    if (find_paired_device(conn, adapter_path, p_scanned_device->bd_address) < 0) {
       // BT_LOG("device not found\n");
        return ERROR1;
    }

    return NO_ERROR;
}

/*BT_ConnectDevice*/
BT_error
BT_ConnectDevice (
    tKnownDevices* p_known_device,
    eBtrDeviceType e_device_type
) {
    DBusConnection *conn;

    //BT_LOG(("BT_ConnectDevice\n"));

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    if (btrDevConnectFullPath(conn, p_known_device->bd_path, e_device_type) < 0) {
        BT_LOG("connection ERROR occurred\n");
        return ERROR1;
    }

    return NO_ERROR;
}


BT_error 
BT_DisconnectDevice (
    tKnownDevices* p_known_device,
    eBtrDeviceType e_device_type
) {
    DBusConnection *conn;

   // BT_LOG(("BT_DisconnectDevice\n"));

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    if (btrDevDisconnectFullPath(conn, p_known_device->bd_path, e_device_type) < 0) {
        BT_LOG("disconnection ERROR occurred\n");
        return ERROR1;
    }

    return NO_ERROR;
}

BT_error
BT_RegisterStatusCallback(void * cb)
{
  p_Status_callback = cb;
  return NO_ERROR;
}


BT_error
BT_EnableAdapter (
    tGetAdapter* p_get_adapter
) {
    int powered;
    powered = 1;
    BT_LOG(("BT_EnableAdapter\n"));
    p_get_adapter->enable = TRUE;//does this even mean anything?

    set_property(conn, adapter_path, "Powered",DBUS_TYPE_BOOLEAN, &powered);
    return NO_ERROR;
}


BT_error
BT_DisableAdapter (
    tGetAdapter* p_get_adapter
) {
    int powered;
    powered = 0;
    BT_LOG(("BT_DisableAdapter\n"));
    p_get_adapter->enable = FALSE;
     set_property(conn, adapter_path, "Powered",DBUS_TYPE_BOOLEAN, &powered);
    return NO_ERROR;
}


BT_error BT_SetDiscoverableTimeout(tGetAdapter *p_get_adapter)
{
    U32 timeout;
    timeout = p_get_adapter->DiscoverableTimeout;
    set_property(conn, adapter_path, "DiscoverableTimeout",DBUS_TYPE_UINT32, &timeout);
    return NO_ERROR;
}


BT_error BT_SetDiscoverable(tGetAdapter *p_get_adapter)
{
    int discoverable;
    discoverable = p_get_adapter->discoverable;
    set_property(conn, adapter_path, "Discoverable",DBUS_TYPE_BOOLEAN, &discoverable);
    return NO_ERROR;
}


BT_error BT_SetDeviceName(tGetAdapter *p_get_adapter)
{
  char * myname;
  myname=p_get_adapter->device_name;
  set_property(conn, adapter_path, "Name",DBUS_TYPE_STRING, &myname);
  return NO_ERROR;
}



