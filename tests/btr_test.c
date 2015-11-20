/*BT.c file*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h> //for malloc
#include <unistd.h> //for close?
#include <errno.h> //for errno handling

#include <dbus/dbus.h>

#include "btr.h"

//test func
void test_func(void);
//global connection variable for dbus?
DBusConnection *conn;

#if 0
typedef struct
{
   BD_NAME bd_address;
   BD_NAME device_name;
   int found;
} tScannedDevices;
tScannedDevices scanned_devices[20]; //holds twenty scanned devices
#endif

extern tScannedDevices scanned_devices[20]; //holds twenty scanned devices
extern char *adapter_path; //to keep track of currently selected
extern tKnownDevices known_devices[20]; //holds twenty known devices

#define NO_ADAPTER 1234

static int
getChoice (
    void
) {
    int mychoice;
    printf("Enter a choice...\n");
    scanf("%d", &mychoice);
    return mychoice;
}

static void 
InitFound (
    void
) {
    int i;
    for (i = 0; i < 15; i++) {
        scanned_devices[i].found=0;
    }
}

static void 
ShowFound (
    void
) {
    int i;
    for (i = 0; i < 15; i++) {
        if (scanned_devices[i].found)
        printf("Device %d. %s\n - %s\n\n",i,scanned_devices[i].device_name, scanned_devices[i].bd_address);
    }
}

int 
callback_test_function (
    void
) {
    printf("callback test\n");
    return 0;
}

static void
printMenu (
    void
) {
    printf("Bluetooth Test Menu\n\n");
    printf("1. Get Current Adapter\n");
    printf("2. Scan\n");
    printf("3. Pair\n");
    printf("4. Connect to Headset/Speakers\n");
    printf("5. Show found devices\n");
    printf("6. Show known devices\n");
    printf("7. Disconnect to Headset/Speakers\n");
    printf("8. Forget a device\n");
    printf("9. Show all Bluetooth Adapters\n");
    printf("10. Connect as Headset/Speakerst\n");
    printf("11. Disconnect as Headset/Speakerst\n");
    printf("88. debug test\n");
    printf("99. Exit\n");
}


int
main (
    void
) {
    int choice;
    int devnum;
    int default_adapter = NO_ADAPTER;
	tGetAdapters    GetAdapters;
	tGetAdapter     GetAdapter;
	tStartDiscovery StartDiscovery;
	tAbortDiscovery AbortDiscovery;
	tFindService    FindService;
	tAdvertiseService AdvertiseService;

	//Dbus stuff
    char  default_path[128];
    char* agent_path = NULL;

    int myadapter = 0;
    int pathlen;

    //init array of scanned devices
    InitFound();

    //Init Dbus
    snprintf(default_path, sizeof(default_path), "/org/bluez/agent_%d", getpid());

    if (!agent_path)
        agent_path = strdup(default_path);

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!conn) {
        fprintf(stderr, "Can't get on system bus");
        exit(1);
    }

    //Init the adapter
    GetAdapter.first_available = TRUE;
    if (NO_ERROR ==	BT_GetAdapter(&GetAdapter)) {
        default_adapter = GetAdapter.adapter_number;
        BT_LOG("GetAdapter Returns Adapter number %d\n",default_adapter);
    }
    else {
        BT_LOG("No bluetooth adapter found!\n");
    }

    //call the BT_init...eventually everything goes here...
    BT_Init();

    //display a menu of choices
    printMenu();

    do {
        printf("Enter a choice...\n");
        scanf("%d", &choice);

        switch (choice) {
        case 1: 
            printf("Adapter is %s\n",adapter_path);
            break;
        case 2: 
            if (default_adapter != NO_ADAPTER) {
                StartDiscovery.adapter_number = default_adapter;
                BT_LOG("Looking for devices on BT adapter %d\n",StartDiscovery.adapter_number);
                StartDiscovery.duration = 13;
                BT_LOG("duration %d\n",StartDiscovery.duration);
                StartDiscovery.max_devices = 10;
                BT_LOG("max_devices %d\n",StartDiscovery.max_devices);
                StartDiscovery.lookup_names = TRUE;
                BT_LOG("lookup_names %d\n",StartDiscovery.lookup_names);
                StartDiscovery.flags = 0;
                BT_LOG("flags %d\n",StartDiscovery.flags);
                BT_StartDiscovery(&StartDiscovery);
            }
            else {
                BT_LOG("Error, no default_adapter set\n");
            }
            break;
        case 3: 
            printf("Pick a Device to Pair...\n");
            ShowFound();
            devnum = getChoice();
            printf(" We will pair %s\n",scanned_devices[devnum].device_name);

            printf(" adapter_path %s\n",adapter_path);
            printf(" agent_path %s\n",agent_path);
            printf(" address %s\n",scanned_devices[devnum].bd_address);
            BT_PairDevice(&scanned_devices[devnum]);
            printf("device pairing completed.\n");
            break;
        case 4: 
            printf("Pick a Device to Connect...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf(" We will connect %s\n",known_devices[devnum].bd_path);

            BT_ConnectDevice(&known_devices[devnum], eBtrAudioSink);
            printf("device connect process completed.\n");
            break;
        case 5:
            printf("Show Found Devices\n");
            ShowFound();
            break;
        case 6:
            printf("Show Known Devices...using BT_ListKnownDevices\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter); //TODO pass in a different structure for each adapter
            break;
        case 7:
            printf("Pick a Device to Disonnect...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf(" We will disconnect %s\n",known_devices[devnum].bd_path);
            BT_DisconnectDevice(&known_devices[devnum], eBtrAudioSink);

            printf("device disconnect process completed.\n");
            break;
        case 8:
            printf("Forget a device\n");
            printf("Pick a Device to Remove...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf(" We will remove %s\n",known_devices[devnum].bd_path);
            BT_ForgetDevice(&known_devices[devnum]); 
            break;
        case 9:
            printf("Getting all available adapters\n");
            //START - adapter selection: if there is more than one adapter, offer choice of which adapter to use for pairing
            BT_GetAdapters(&GetAdapters);
            if ( GetAdapters.number_of_adapters > 1) {
                printf("There are %d Bluetooth adapters\n",GetAdapters.number_of_adapters);
                printf("current adatper is %s\n",adapter_path);
                printf("Which adapter would you like to use (0 = default)?\n");
                myadapter = getChoice();

                pathlen = strlen(adapter_path);
                switch (myadapter) {
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

                    printf("Now current adatper is %s\n",adapter_path);
                }
            }
            //END adapter selection
            break;
        case 10:
            printf("Pick a Device to Connect...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf(" We will connect %s\n",known_devices[devnum].bd_path);

            BT_ConnectDevice(&known_devices[devnum], eBtrHeadSet);
            printf("device connect process completed.\n");
            break;
        case 11:
            printf("Pick a Device to Disonnect...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf(" We will disconnect %s\n",known_devices[devnum].bd_path);
            BT_DisconnectDevice(&known_devices[devnum], eBtrHeadSet);

            printf("device disconnect process completed.\n");
            break;
        case 88:
            test_func();
            break;
        case 99: 
            printf("Quitting program!\n");
            exit(0);
            break;
        default: 
            printf("Available options are:\n");
            printMenu();
            break;
        }
    } while (1);

    return 0;
}


//TODO - stuff below is to be moved to shared library
BT_error
BT_EnableAdapter (
    tGetAdapter* p_get_adapter
) {
    BT_LOG(("BT_EnableAdapter\n"));
    p_get_adapter->enable = TRUE;
    return NO_ERROR;
}


BT_error
BT_DisableAdapter (
    tGetAdapter* p_get_adapter
) {
    BT_LOG(("BT_DisableAdapter\n"));
    p_get_adapter->enable = FALSE;
    return NO_ERROR;
}


BT_error
BT_AbortDiscovery (
    tAbortDiscovery* p_abort_discovery
) {
    BT_LOG(("BT_AbortDiscovery\n"));
    return NO_ERROR;
}

/*BT_ConfigureAdapter... set a particular attribute for the adapter*/
BT_error 
BT_ConfigureAdapter (
    tGetAdapter* p_get_adapter
) {
	BT_LOG(("BT_ConfigureAdapter\n"));
	return NO_ERROR;
}


/*BT_FindService - finds a service from a given device*/
BT_error 
BT_FindService (
    tFindService* p_find_service
) {
    BT_LOG(("BT_FindService\n"));
#ifdef SIM_MODE
	BT_LOG(("Looking for services with:\n"));
	BT_LOG("Service Name: %s\n",p_find_service->filter_mode.service_name);
    BT_LOG("UUID: %s\n",p_find_service->filter_mode.uuid);
    BT_LOG("Service Name: %s\n",p_find_service->filter_mode.bd_address);
#endif
    return NO_ERROR;
}


BT_error 
BT_AdvertiseService (
    tAdvertiseService* p_advertise_service
) {
    BT_LOG(("BT_AdvertiseService\n"));
    return NO_ERROR;
}

