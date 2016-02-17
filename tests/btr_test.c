/*BT.c file*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h> //for malloc
#include <unistd.h> //for close?
#include <errno.h> //for errno handling

#include <dbus/dbus.h>

#include "btr.h" //basic RDK BT functions
#include "btr_service.h" //service UUIDs, use for service discovery

//test func
void test_func(tGetAdapter* p_get_adapter);
//global connection variable for dbus?
DBusConnection *conn;


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
        getchar();//suck up a newline?
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
ShowSignalStrength (
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
static void 
ShowFound (
    void
) {
    int i;
    for (i = 0; i < 15; i++) {
        if (scanned_devices[i].found)
          {
        printf("Device %d. %s\n - %s  %d dbmV ",i,scanned_devices[i].device_name, scanned_devices[i].bd_address, scanned_devices[i].RSSI);
        ShowSignalStrength(scanned_devices[i].RSSI);
        printf("\n\n");
       }
    }
}

int cb_unsolicited_bluetooth_status (tStatusCB* p_StatusCB)
{
    printf("device status change: %s\n",p_StatusCB->device_state);
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
    printf("12. Enable Bluetooth Adapter\n");
    printf("13. Disable Bluetooth Adapter\n");
    printf("14. Set Discoverable Timeout\n");
    printf("15. Set Discoverable \n");
    printf("16. Set friendly name \n");
    printf("17. Check for audio sink capability\n");
    printf("18. Check for existance of a service\n");
    printf("19. Find service details\n");
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
    char myData[2048];
    int myadapter = 0;
    int bfound;
    int i, pathlen;
    
    char myService[16];//for testing findService API
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

    //register callback for unsolicted events, such as powering off a bluetooth device
    BT_RegisterStatusCallback(cb_unsolicited_bluetooth_status);

    //display a menu of choices
    printMenu();

    do {
        printf("Enter a choice...\n");
        scanf("%d", &choice);
        getchar();//suck up a newline?
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
                printf("Performing device scan. Please wait...\n");
                BT_StartDiscovery(&StartDiscovery);
                printf("scan complete\n");
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
            if ( BT_PairDevice(&scanned_devices[devnum]) == NO_ERROR)
                printf("device pairing successful.\n");
            else
              printf("device pairing FAILED.\n");
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
            printf("Pick a Device to Disconnect...\n");
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
        case 12:
            GetAdapter.adapter_number = myadapter;
            printf("Enabling adapter %d\n",GetAdapter.adapter_number);
            BT_EnableAdapter(&GetAdapter);
            break;
        case 13:
            GetAdapter.adapter_number = myadapter;
            printf("Disabling adapter %d\n",GetAdapter.adapter_number);
            BT_DisableAdapter(&GetAdapter);
            break;
        case 14:
            printf("Enter discoverable timeout in seconds.  Zero seconds = FOREVER \n");
            GetAdapter.DiscoverableTimeout = getChoice();
            printf("setting DiscoverableTimeout to %d\n",GetAdapter.DiscoverableTimeout);
            BT_SetDiscoverableTimeout(&GetAdapter);
            break;
        case 15:
            printf("Set discoverable.  Zero = Not Discoverable, One = Discoverable \n");
            GetAdapter.discoverable = getChoice();
            printf("setting discoverable to %d\n",GetAdapter.discoverable);
            BT_SetDiscoverable(&GetAdapter);
            break;
        case 16:
            printf("Set friendly name (up to 64 characters): \n");
            fgets(GetAdapter.device_name,sizeof(GetAdapter.device_name),stdin);
            printf("setting name to %s\n",GetAdapter.device_name);
            BT_SetDeviceName(&GetAdapter);
            break;
        case 17:
            printf("Check for Audio Sink capability\n");
            printf("Pick a Device to Check for Audio Sink...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf("Checking for an audio sink on %s\n", known_devices[devnum].bd_path); 
            if (BT_FindService(&known_devices[devnum],BT_A2SNK,NULL,&bfound) == NO_ERROR)
              {
               if (bfound)
                {
		  printf("Service UUID BT_A2SNK is found\n");
                }
               else
                {
                  printf("Service UUID BT_A2SNK is NOT found\n");
                }
               }
              else
              {
                 printf("Error on BT_FindService\n");
              }
            break;
        case 18:
            printf("Find a Service\n");
            printf("Pick a Device to Check for Services...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
            fgets(myService,sizeof(myService),stdin);
            printf("Checking for service %s on %s\n",myService, known_devices[devnum].bd_path);
            for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                  {
                if(myService[i] == '\n')
                   myService[i] = '\0';
                }
            bfound=0;//assume not found
            if (BT_FindService(&known_devices[devnum],myService,NULL,&bfound) == NO_ERROR)
            {
             if (bfound)
                {
		  printf("Service UUID %s is found\n",myService);
                }
               else
                {
                  printf("Service UUID %s is NOT found\n",myService);
                }
             }
             else
             {
                printf("Error on BT_FindService\n");
             }
            break;
        case 19:
            printf("Find a Service and get details\n");
            printf("Pick a Device to Check for Services...\n");
            GetAdapter.adapter_number = myadapter;
            BT_ListKnownDevices(&GetAdapter);
            devnum = getChoice();
            printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
            fgets(myService,sizeof(myService),stdin);
            printf("Checking for service %s on %s\n",myService, known_devices[devnum].bd_path);
            for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                  {
                if(myService[i] == '\n')
                   myService[i] = '\0';
                }
            bfound=0;//assume not found
            /*CAUTION! This usage is intended for development purposes.
           myData needs to be allocated large enough to hold the returned device data
           for development purposes it may be helpful for an app to gain access to this data,
           so this usage  can provide that capability.
         In most cases, simply knowing if the service exists may suffice, in which case you can use
         the simplified option where the data pointer is NULL, and no data is copied*/
            if (BT_FindService(&known_devices[devnum],myService,myData,&bfound)  == NO_ERROR)
              {
              if (bfound)
                {
		  printf("Service UUID %s is found\n",myService);
                  printf("Data is:\n %s \n",myData);
                }
               else
                {
                  printf("Service UUID %s is NOT found\n",myService);
                }
               }
              else
             {
                printf("Error on BT_FindService\n");
             }
            break;
         case 20:
            printf("Pick a Device to Find (see if it is already paired)...\n");
            ShowFound();
            devnum = getChoice();
            printf(" We will try to find %s\n",scanned_devices[devnum].device_name);
            printf(" address %s\n",scanned_devices[devnum].bd_address);
            if ( BT_FindDevice(&scanned_devices[devnum]) == NO_ERROR)
                printf("device FOUND successful.\n");
            else
              printf("device was NOT found.\n");
            break;
        case 88:
            test_func(&GetAdapter); 
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


    (void)AbortDiscovery;
    (void)FindService;
    (void)AdvertiseService;

    return 0;
}


//TODO - stuff below is to be moved to shared library



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


/*BT_DiscoverServices - finds a service from a given device*/
BT_error 
BT_DiscoverServices (
    tFindService* p_find_service
) {
    BT_LOG(("BT_DiscoverServices\n"));
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

