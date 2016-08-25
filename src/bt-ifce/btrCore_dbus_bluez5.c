/*
 * btrCore_dbus_bt_537.c
 * Implementation of DBus layer abstraction for BT functionality (BlueZ 5.37)
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/* External Library Headers */
#include <dbus/dbus.h>

/* Local Headers */
#include "btrCore_dbus_bt.h"

#define BD_NAME_LEN     248



/* Static Function Prototypes */
static int btrCore_BTHandleDusError(DBusError* aDBusErr, const char* aErrfunc, int aErrline);

static DBusHandlerResult btrCore_BTMediaEndpointHandler_cb (DBusConnection* apDBusConn, DBusMessage* apDBusMsg, void* userdata);

static char* btrCore_BTGetDefaultAdapterPath (void);
static int btrCore_BTReleaseDefaultAdapterPath (void);
static DBusMessage* btrCore_BTSendMethodCall (const char* objectpath, const char* interfacename, const char* methodname); //Send method call, Returns NULL on failure, else pointer to reply
static int btrCore_BTParseDevice (DBusMessage* apDBusMsg, stBTDeviceInfo* apstBTDeviceInfo);
static DBusMessage* btrCore_BTMediaEndpointSelectConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointSetConfiguration (DBusMessage* apDBusMsg);
static DBusMessage* btrCore_BTMediaEndpointClearConfiguration (DBusMessage* apDBusMsg);


/* Callbacks */
static fPtr_BtrCore_BTDevStatusUpdate_cB gfpcBDevStatusUpdate = NULL;
static fPtr_BtrCore_BTNegotiateMedia_cB gfpcBNegotiateMedia = NULL;
static fPtr_BtrCore_BTTransportPathMedia_cB gfpcBTransportPathMedia = NULL;

DBusConnection* conn = NULL;
char choice;

char object_path [10][512];
char object_data [30][512];
int i = 0;
int j = 0;
int k = 0;
int x = 0;
int powered = 0;


const char* property_name = "Name";
const char* property_alias = "Alias";
const char* property_address = "Address";
const char* property_paired = "Paired";
const char* property_connected = "Connected";
const char* defaultAdapterInterface = "org.bluez.Adapter1";
const char* deviceInterface = "org.bluez.Device1";
const char* defaultAdapterPath = "/org/bluez/hci0";


/* Static Global Variables Defs */
static DBusConnection*  gpDBusConn = NULL;
static char* gpcBTAgentPath = NULL;
static char* gpcBTDAdapterPath = NULL;
static char* gpcBTAdapterPath = NULL;
static void* gpcBUserData = NULL;
static char* gpcDevTransportPath = NULL;
static const DBusObjectPathVTable gDBusMediaEndpointVTable = {
    .message_function = btrCore_BTMediaEndpointHandler_cb,
};


//Helper functions

int getManagedObjects();
void devicePair();
void cancel_devicePair();
void removeDevice();
int connectPhoneAVRCP_Remote();
int getDeviceProperties (const char* adapterPath, const char* interface_name, const char* property);




/////////////////////////////////////   Main Function for Testing BlueZ 5.37   ///////////////////////////////////////////

int main (int argc, char **argv)
{
	BtrCore_BTInitGetConnection();
	
	/*DBusMessage* reply = sendMethodCall(TEST_OBJ_PATH, TEST_BUS_NAME, TEST_INTERFACE_NAME, TEST_METHOD_NAME);
	if(reply != NULL)    {
	
	DBusMessageIter MsgIter;
	dbus_message_iter_init(reply, &MsgIter);//msg is pointer to dbus message received
	
	if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&MsgIter)){
	char* str = NULL;
	dbus_message_iter_get_basic(&MsgIter, &str);
	printf("Received string: \n\n %s \n\n",str);
	}
	
	dbus_message_unref(reply);//unref reply
	}*/
	
	printf("0- Get Bluetooth Managed Objects/devices\n");
	printf("1- Start Bluetooth device discovery\n");
	printf("2- Stop Bluetooth device discovery\n");
	printf("3- Pair to phone\n");
	printf("4- Connect to  phone\n");
	printf("5- Connect to Phone with profile\n");
	printf("6- Disconnect Phone\n");
	printf("7- Cancel phone pairing\n");
	printf("8- Remove phone record\n");
	printf("9- Turn off bluetooth\n");
	printf("a- Turn on bluetooth\n");
	printf("b- Get Default Adapter Path\n");
	printf("c- Not sure!:\n");
	printf("\nEnter Choice: ");
	do
	{
		scanf("%c",&choice);
		switch(choice)
		{
			case '0':
			printf("Get list of managed objects\n\n");
			getManagedObjects();
			choice='x';
			break;
			
			case '1':
			printf("Started Bluetooth device discovery\n\n");
			BtrCore_BTStartDiscovery(gpDBusConn, "/org/bluez/hci0", NULL);
			choice='x';
			break;
			
			case '2':
			printf("Stopped Bluetooth device discovery\n\n");
			BtrCore_BTStopDiscovery(gpDBusConn, "/org/bluez/hci0", NULL);
			choice='x';
			break;
			
			case '3':
			printf("Pairing phone\n\n");
			devicePair();
			choice='x';
			break;
			
			case '4':
			printf("Connecting to phone\n\n");
			BtrCore_BTConnectDevice(gpDBusConn, "/org/bluez/hci0/dev_F8_CF_C5_C9_8E_0E", 0);
			choice='x';
			break;
			
			case '5':
			printf("Connecting to phone using profile\n\n");
			connectPhoneAVRCP_Remote();
			choice='x';
			break;
			
			case '6':
			printf("Disconnecting phone\n\n");
			BtrCore_BTDisconnectDevice(gpDBusConn, "/org/bluez/hci0/dev_F8_CF_C5_C9_8E_0E", 0);
			choice='x';
			break;
			
			case '7':
			printf("Canceling phone pairing\n\n");
			cancel_devicePair();
			choice='x';
			break;
			
			case '8':
			printf("Removing device record\n\n");
			removeDevice();
			choice='x';
			break;
			
			case '9':
			powered = 0;
			printf("Turned bluetooth off\n\n");
			BtrCore_BTSetAdapterProp(gpDBusConn, defaultAdapterPath, enBTAdPropPowered, &powered);
			choice='x';
			break;
			
			case 'a':
			powered = 1;
			printf("Turned bluetooth on\n\n");
			BtrCore_BTSetAdapterProp(gpDBusConn, defaultAdapterPath, enBTAdPropPowered, &powered);
			choice='x';
			break;
			
			case 'b':
			btrCore_BTGetDefaultAdapterPath();
			choice='x';
			break;
			
			case 'c':
			BtrCore_BTGetAgentPath(gpDBusConn);
			choice='x';
			break;
		}
		printf("\n");
	}while(choice!='x');
	
	return 0;
	
}



///////////////////////////////////////////    TEST and HELPER FUNCTIONS    /////////////////////////////////////////


int getManagedObjects()
{
	DBusMessage* reply;
	DBusMessageIter rootIter;
	dbus_bool_t lDbusOp;
	bool adapterFound = FALSE;
	char* adapter_path;
	char* device;
	
	printf("Sending Get All Managed Objects Message\n\n");
	reply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	
	if (dbus_message_iter_init(reply, &rootIter) && //point iterator to reply message
		DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) //get the type of message that iter points to
	{
		printf("::::::::::::: Getting arguments :::::::::::::\n\n");
		DBusMessageIter arrayElementIter;
		dbus_message_iter_recurse(&rootIter, &arrayElementIter); //assign new iterator to first element of array
		
		//printf("-->Descending into Array (recursing).\n");
		
		while(!adapterFound){
			if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter))
			{
				
				DBusMessageIter dictEntryIter;
				dbus_message_iter_recurse(&arrayElementIter,&dictEntryIter ); //assign new iterator to first element of (get all dict entries of 1st level (all object paths)
				
				if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&dictEntryIter))
				{
					//printf("    Type DBUS_TYPE_OBJECT_PATH.\n");
					dbus_message_iter_get_basic(&dictEntryIter, &adapter_path);
					printf("\n\nObject: %s\n\n",adapter_path);
					strcpy(object_path[i],adapter_path);
					++i;
					/*if(device && strstr(adapter_path,device))
					{
					adapterFound = TRUE;
					printf("    Adapter %s FOUND!\n",device);
					}*/
				}
				dbus_message_iter_next(&dictEntryIter);
				if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&dictEntryIter))
				{
					DBusMessageIter innerArrayIter;
					dbus_message_iter_recurse(&dictEntryIter, &innerArrayIter);
					
					while (dbus_message_iter_has_next(&innerArrayIter)){
						if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&innerArrayIter))
						{
							//printf("      Type Dict_Entry.\n");
							DBusMessageIter innerDictEntryIter;
							dbus_message_iter_recurse(&innerArrayIter,&innerDictEntryIter ); //assign new iterator to first element of
							
							if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter))
							{
								char *dbusObject;
								char *temp;
								dbus_message_iter_get_basic(&innerDictEntryIter, &dbusObject);
								
								////// getting default adapter path //////
								
								if (strcmp(dbusObject, "org.bluez.Adapter1") == 0)
								{
									gpcBTDAdapterPath = strdup(adapter_path);
								}
								
								printf("\tThis is interface: %s\n", dbusObject);
								
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
										//printf("      Type Dict_Entry.\n");
										DBusMessageIter innerDictEntryIter2;
										dbus_message_iter_recurse(&innerArrayIter2,&innerDictEntryIter2); //assign new iterator to first element of
										if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter2))
										{
											char *dbusObject2;
											dbus_message_iter_get_basic(&innerDictEntryIter2, &dbusObject2);
											printf("        \n\t%s: ",dbusObject2);
										}
										
										
										
										////////////// NEW 2 ////////////
										dbus_message_iter_next(&innerDictEntryIter2);
										DBusMessageIter innerDictEntryIter3;
										char *dbusObject3;
										
										dbus_message_iter_recurse(&innerDictEntryIter2,&innerDictEntryIter3);
										if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
										{
											dbus_message_iter_get_basic(&innerDictEntryIter3, &dbusObject3);
											printf("%s",dbusObject3);
											strcpy(object_data[j],dbusObject3);
											++j;
										}
										else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&innerDictEntryIter3))
										{
											bool *device_prop = FALSE;
											dbus_message_iter_get_basic(&innerDictEntryIter3, &device_prop);
											printf("%d",device_prop);
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
		
		x = i;
	}
	
	printf("\n\n::::::::::::::::::: OBJECT PATHS::::::::::::::::::::::::");
	for (i = 0; i <= x; ++i)
	{
		printf("\n%s", object_path[i]);
		
	}
	
	printf("\n\n::::::::::::::::::: OBJECT DATA::::::::::::::::::::::::");
	
	printf("\n\nDefault Adapter Name and MAC Address:  %s  %s",object_data[1], object_data[0]);
	printf("\nFirst Device Name and MAC address:  %s  %s", object_data[4], object_data[3]);
	printf("\n");
	
	const char* defaultAdapterPath = object_path[1];
	const char* devicePath1 = object_path[2];

	getDeviceProperties(defaultAdapterPath, defaultAdapterInterface, property_name);
	getDeviceProperties(defaultAdapterPath, defaultAdapterInterface, property_alias);
	getDeviceProperties(defaultAdapterPath, defaultAdapterInterface, property_address);
	getDeviceProperties(devicePath1, deviceInterface, property_name);
	getDeviceProperties(devicePath1, deviceInterface, property_alias);
	getDeviceProperties(devicePath1, deviceInterface, property_address);
	getDeviceProperties(devicePath1, deviceInterface, property_paired);
	getDeviceProperties(devicePath1, deviceInterface, property_connected);
	printf("\n\n\nDefault Adpater path is %s", gpcBTDAdapterPath);
	return 0;
}
//end if - outer arrray



void devicePair()
{
	DBusMessage* msg;
	dbus_bool_t lDbusOp;
	
	printf("Sending Pairing Request to phone....\n\n");
	
	msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0/dev_DC_EE_06_FD_C7_BA", "org.bluez.Device1", "Pair");
	
	if (!msg) {
		fprintf(stderr, "Can't allocate method call");
	}
	
	lDbusOp = dbus_connection_send(gpDBusConn, msg, NULL);
	if (!lDbusOp) {
		fprintf(stderr, "Not Enough memory for stop discovery message send\n");
		}
	
	dbus_message_unref(msg);
	dbus_connection_flush(gpDBusConn);
}

void cancel_devicePair()
{
	DBusMessage* msg;
	dbus_bool_t lDbusOp;
	
	printf("Canceling pairing of phone....\n\n");
	
	msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0/dev_DC_EE_06_FD_C7_BA", "org.bluez.Device1", "CancelPairing");
	
	lDbusOp = dbus_connection_send(gpDBusConn, msg, NULL);
	if (!lDbusOp) {
		fprintf(stderr, "Not Enough memory for stop discovery message send\n");
		}
	
	dbus_message_unref(msg);
	dbus_connection_flush(gpDBusConn);
}

void removeDevice()
{
	
	const char* param = "/org/bluez/hci0/dev_F8_CF_C5_C9_8E_0E";
	DBusMessageIter args;
	DBusMessage* msg;
	dbus_bool_t lDbusOp;
	
	printf("Removing device record....\n\n");
	//dbus_message_set_path(msg, param);
	//dbus_message_append_args(msg, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID);
	msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0", "org.bluez.Adapter1", "RemoveDevice");
	
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &param)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}
	
	lDbusOp = dbus_connection_send(gpDBusConn, msg, NULL);
	if (!lDbusOp) {
		fprintf(stderr, "Not Enough memory for remove device message send\n");
		}
	
	dbus_message_unref(msg);
	dbus_connection_flush(gpDBusConn);
}

int connectPhoneAVRCP_Remote()
{
	const char* UUID = "0000110a-0000-1000-8000-00805f9b34fb";
	DBusMessage* msg;
	dbus_bool_t lDbusOp;
	DBusMessageIter args;
	
	
	printf("Connecting to phone....\n\n");
	
	msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0/dev_DC_EE_06_FD_C7_BA", "org.bluez.Device1", "ConnectProfile");
	
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &UUID)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}
	
	
	lDbusOp = dbus_connection_send(gpDBusConn, msg, NULL);
	if (!lDbusOp) {
		fprintf(stderr, "Not Enough memory connect message send\n");
	}
	
	dbus_message_unref(msg);
	dbus_connection_flush(gpDBusConn);
}



int getDeviceProperties (const char* adapterPath, const char* interface_name, const char* property)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter args;
	DBusMessageIter element;
	DBusPendingCall* pending;
	

	msg = dbus_message_new_method_call("org.bluez", adapterPath, "org.freedesktop.DBus.Properties", "Get");
	
	dbus_message_iter_init_append(msg, &args);
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &interface_name, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);
	
	dbus_error_init(&err);
	if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
	{
		printf("failed to send message");
	}
	
	dbus_connection_flush(gpDBusConn);
	dbus_message_unref(msg);
	
	dbus_pending_call_block(pending);
	reply =  dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	
	
	DBusMessageIter MsgIter;
	dbus_message_iter_init(reply, &MsgIter);//msg is pointer to dbus message received
	dbus_message_iter_recurse(&MsgIter,&element); //pointer to first element of the dbus messge received
	/*if (!dbus_message_iter_init(reply, &MsgIter))
	{
		fprintf(stderr, "Message has no arguments!\n");
	}*/
	
	if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&element))
	{
		const char* propertyOutput = NULL;
		dbus_message_iter_get_basic(&element, &propertyOutput);
		printf("\n%s: %s", property, propertyOutput);
		return 0;
	}
	else if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&element))
	{
		const bool* propertyBool = FALSE;
		dbus_message_iter_get_basic(&element, &propertyBool);
		printf("\n%s: %d", property, propertyBool);
		return 0;
	}

}






/////////////   Porting functions from original btrCore_dbus_bt.c from BlueZ 4.101 /////////////////////////////

/////////// Register Media Function for default device adapter (Xi5), act as audio sink and portable device is audio source ////////////


static char*
btrCore_BTGetDefaultAdapterPath (
    void
) {
	DBusMessage* reply;
	DBusMessageIter rootIter;
	dbus_bool_t lDbusOp;
	bool adapterFound = FALSE;
	char* adapter_path;
	char* device;
	
	
	reply = btrCore_BTSendMethodCall("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	
	if (dbus_message_iter_init(reply, &rootIter) && //point iterator to reply message
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
					strcpy(object_path[i],adapter_path);
					++i;
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
								char *temp;
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
											strcpy(object_data[j],dbusObject3);
											++j;
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

    fprintf(stderr, "\nDBus Debug DBus Connection Name %s\n\n", dbus_bus_get_unique_name (lpDBusConn));
    gpDBusConn = lpDBusConn;


    //dbus_bus_add_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    gpcBUserData            = NULL;
    gfpcBDevStatusUpdate    = NULL;
    gfpcBNegotiateMedia     = NULL;
    gfpcBTransportPathMedia = NULL;

    return (void*)gpDBusConn;
}




int
BtrCore_BTDeInitReleaseConnection (
    void* apBtConn
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    gfpcBTransportPathMedia = NULL;
    gfpcBNegotiateMedia     = NULL;
    gfpcBDevStatusUpdate    = NULL;
    gpcBUserData            = NULL;

    //dbus_bus_remove_match(gpDBusConn, "type='signal',interface='org.bluez.Adapter'", NULL);

    //dbus_connection_remove_filter(gpDBusConn, btrCore_BTDBusAgentFilter_cb, NULL);

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
    printf("\n\nNot sure: %s", gpcBTAgentPath);
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





int BtrCore_BTStartDiscovery (
	void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath)
{
	DBusMessage*    lpDBusMsg;
	dbus_bool_t 	lDBusOp;
	
	if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;
        
	lpDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter, "org.bluez.Adapter1", "StartDiscovery"); //TODO: Remove static adapter path argument and replace it with variable
	
	if (lpDBusMsg == NULL) {
		printf("Cannot allocate Dbus message to Stop Bluetooth Device Discovery\n\n");
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




int BtrCore_BTStopDiscovery (
	void*       apBtConn,
    const char* apBtAdapter,
    const char* apBtAgentPath)
{
	DBusMessage*    lpDBusMsg;
	dbus_bool_t 	lDBusOp;
	
	if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;
        
	lpDBusMsg = dbus_message_new_method_call("org.bluez", apBtAdapter, "org.bluez.Adapter1", "StopDiscovery"); //TODO: Remove static adapter path argument and replace it with variable
	
	if (lpDBusMsg == NULL) {
		printf("Cannot allocate Dbus message to Stop Bluetooth Device Discovery\n\n");
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
BtrCore_BTGetProp (
    void*           apBtConn,
    const char*     pDevicePath,
    const char*     pInterface,
    const char*     pKey,
    void*           pValue
) {
    int             rc = 0;
    int             type;
    DBusMessage*	msg = NULL;
    DBusMessage*    reply = NULL;
    DBusMessageIter args;
    DBusMessageIter arg_i;
    DBusMessageIter element_i;
    DBusMessageIter variant_i;
    DBusError       err;
    DBusPendingCall* pending;
    const char*     pParsedKey = NULL;
    const char*     pParsedValueString = NULL;
    int             parsedValueNumber = 0;
    unsigned int    parsedValueUnsignedNumber = 0;


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
    else {
        type = DBUS_TYPE_INVALID;
        return -1;
    }
    
    msg = dbus_message_new_method_call("org.bluez", pDevicePath, "org.freedesktop.DBus.Properties", "GetAll");
	
	dbus_message_iter_init_append(msg, &args);
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &pInterface, DBUS_TYPE_INVALID);

    dbus_error_init(&err);
    if (!dbus_connection_send_with_reply(gpDBusConn, msg, &pending, -1))
	{
		printf("failed to send message");
	}
	
	
    dbus_connection_flush(gpDBusConn);
	dbus_message_unref(msg);
	
	dbus_pending_call_block(pending);
	reply =  dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	
	
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
    DBusMessageIter lDBusMsgIter2;
    DBusMessageIter lDBusMsgIterValue;
    DBusError       lDBusErr;
    int             lDBusType;
    const char*     lDBusTypeAsString;
    const char*     lDBusKey;

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
        fprintf(stderr, "Reply Null\n");
        btrCore_BTHandleDusError(&lDBusErr, __FUNCTION__, __LINE__);
        return -1;
    }

    dbus_message_unref(lpDBusReply);

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
    //DBusConnection* gpDBusConn;   // TODO: Remove when migrating to main code
    
    //gpDBusConn = conn; // TODO: Remove when migrating to main code

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
                                             "org.bluez.Media1",
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
    //DBusConnection* gpDBusConn;   // TODO: Remove when migrating to main code
    
    //gpDBusConn = conn; // TODO: Remove when migrating to main code
 
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;


    lpDBusMsg = dbus_message_new_method_call("org.bluez",
                                             apBtAdapter,
                                             "org.bluez.Media1",
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
    void*       apBtConn,
    fPtr_BtrCore_BTDevStatusUpdate_cB afpcBDevStatusUpdate,
    void*       apUserData
) {
    if (!gpDBusConn || (gpDBusConn != apBtConn))
        return -1;

    if (!afpcBDevStatusUpdate)
        return -1;

    gfpcBDevStatusUpdate = afpcBDevStatusUpdate;
    gpcBUserData = apUserData;

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



