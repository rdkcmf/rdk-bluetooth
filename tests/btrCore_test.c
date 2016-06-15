/*BT.c file*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for close?
#include <errno.h>      //for errno handling
#include <poll.h>

#include "btrCore.h"            //basic RDK BT functions
#include "btrCore_service.h"    //service UUIDs, use for service discovery

//test func
void test_func(stBTRCoreAdapter* apstBTRCoreAdapter);


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

static char*
getEncodedSBCFile (
    void
) {
    char sbcEncodedFile[1024];
    printf("Enter SBC File location...\n");
    scanf("%s", sbcEncodedFile);
        getchar();//suck up a newline?
    return strdup(sbcEncodedFile);
}


static void sendSBCFileOverBT (
    char* fileLocation,
    int fd,
    int mtuSize
) {
    FILE* sbcFilePtr = fopen(fileLocation, "rb");
    int    bytesLeft = 0;
    void   *encoded_buf = NULL;
    int bytesToSend = mtuSize;
    struct pollfd pollout = { fd, POLLOUT, 0 };
     int timeout;


    if (!sbcFilePtr)
        return;

    printf("fileLocation %s", fileLocation);

    fseek(sbcFilePtr, 0, SEEK_END);
    bytesLeft = ftell(sbcFilePtr);
    fseek(sbcFilePtr, 0, SEEK_SET);

    printf("File size: %d bytes\n", (int)bytesLeft);

    encoded_buf = malloc (mtuSize);

    while (bytesLeft) {

        if (bytesLeft < mtuSize)
            bytesToSend = bytesLeft;

        timeout = poll (&pollout, 1, 1000); //delay 1s to allow others to update our state

        if (timeout == 0)
            continue;
        if (timeout < 0)
            fprintf (stderr, "Bluetooth Write Error : %d\n", errno);

        // write bluetooth
        if (timeout > 0) {
            fread (encoded_buf, 1, bytesToSend, sbcFilePtr);
            write(fd, encoded_buf, bytesToSend);
            bytesLeft -= bytesToSend;
        }

        usleep(26000); //1ms delay //12.5 ms can hear words
    }

    free(encoded_buf);
    fclose(sbcFilePtr);
}


void
cb_unsolicited_bluetooth_status (
    stBTRCoreDevStateCB* p_StatusCB
) {
    printf("device status change: %s\n",p_StatusCB->cDeviceType);
    return;
}

static void
printMenu (
    void
) {
    printf("Bluetooth Test Menu\n\n");
    printf("1. Get Current Adapter\n");
    printf("2. Scan\n");
    printf("3. Show found devices\n");
    printf("4. Pair\n");
    printf("5. UnPair/Forget a device\n");
    printf("6. Show known devices\n");
    printf("7. Connect to Headset/Speakers\n");
    printf("8. Disconnect to Headset/Speakers\n");
    printf("9. Connect as Headset/Speakerst\n");
    printf("10. Disconnect as Headset/Speakerst\n");
    printf("11. Show all Bluetooth Adapters\n");
    printf("12. Enable Bluetooth Adapter\n");
    printf("13. Disable Bluetooth Adapter\n");
    printf("14. Set Discoverable Timeout\n");
    printf("15. Set Discoverable \n");
    printf("16. Set friendly name \n");
    printf("17. Check for audio sink capability\n");
    printf("18. Check for existance of a service\n");
    printf("19. Find service details\n");
    printf("20. Check if Device Paired\n");
    printf("21. Get Connected Dev Data path\n");
    printf("22. Release Connected Dev Data path\n");
    printf("23. Send SBC data to BT Headset/Speakers\n");
    printf("88. debug test\n");
    printf("99. Exit\n");
}


int
main (
    void
) {
    tBTRCoreHandle lhBTRCore = NULL;

    int choice;
    int devnum;
    int default_adapter = NO_ADAPTER;
	stBTRCoreGetAdapters    GetAdapters;
	stBTRCoreAdapter        lstBTRCoreAdapter;
	stBTRCoreStartDiscovery StartDiscovery;
	stBTRCoreAbortDiscovery AbortDiscovery;
	stBTRCoreFindService    FindService;
	stBTRCoreAdvertiseService AdvertiseService;

    char  default_path[128];
    char* agent_path = NULL;
    char myData[2048];
    int myadapter = 0;
    int bfound;
    int i;

    int liDataPath = 0;
    int lidataReadMTU = 0;
    int lidataWriteMTU = 0;
    char *sbcEncodedFileName = NULL;
    
    char myService[16];//for testing findService API

    snprintf(default_path, sizeof(default_path), "/org/bluez/agent_%d", getpid());

    if (!agent_path)
        agent_path = strdup(default_path);

    //call the BTRCore_init...eventually everything goes after this...
    BTRCore_Init(&lhBTRCore);

    //Init the adapter
    lstBTRCoreAdapter.bFirstAvailable = TRUE;
    if (enBTRCoreSuccess ==	BTRCore_GetAdapter(lhBTRCore, &lstBTRCoreAdapter)) {
        default_adapter = lstBTRCoreAdapter.adapter_number;
        BTRCore_LOG("GetAdapter Returns Adapter number %d\n",default_adapter);
    }
    else {
        BTRCore_LOG("No bluetooth adapter found!\n");
        return -1;
    }

    //register callback for unsolicted events, such as powering off a bluetooth device
    BTRCore_RegisterStatusCallback(lhBTRCore, cb_unsolicited_bluetooth_status);

    //display a menu of choices
    printMenu();

    do {
        printf("Enter a choice...\n");
        scanf("%d", &choice);
        getchar();//suck up a newline?
        switch (choice) {
        case 1: 
            printf("Adapter is %s\n", lstBTRCoreAdapter.pcAdapterPath);
            break;
        case 2: 
            if (default_adapter != NO_ADAPTER) {
                StartDiscovery.adapter_number = default_adapter;
                BTRCore_LOG("Looking for devices on BT adapter %d\n",StartDiscovery.adapter_number);
                StartDiscovery.duration = 13;
                BTRCore_LOG("duration %d\n",StartDiscovery.duration);
                StartDiscovery.max_devices = 10;
                BTRCore_LOG("max_devices %d\n",StartDiscovery.max_devices);
                StartDiscovery.lookup_names = TRUE;
                BTRCore_LOG("lookup_names %d\n",StartDiscovery.lookup_names);
                StartDiscovery.flags = 0;
                BTRCore_LOG("flags %d\n",StartDiscovery.flags);
                printf("Performing device scan. Please wait...\n");
                BTRCore_StartDiscovery(lhBTRCore, &StartDiscovery);
                printf("scan complete\n");
            }
            else {
                BTRCore_LOG("Error, no default_adapter set\n");
            }
            break;
        case 3:
            printf("Show Found Devices\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ShowFoundDevices(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 4:
            printf("Pick a Device to Pair...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ShowFoundDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();

            printf(" adapter_path %s\n", lstBTRCoreAdapter.pcAdapterPath);
            printf(" agent_path %s\n",agent_path);
            if ( BTRCore_PairDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                printf("device pairing successful.\n");
            else
              printf("device pairing FAILED.\n");
            break;
        case 5:
            printf("UnPair/Forget a device\n");
            printf("Pick a Device to Remove...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_ForgetDevice(lhBTRCore, devnum);
            break;
        case 6:
            printf("Show Known Devices...using BTRCore_ListKnownDevices\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter); //TODO pass in a different structure for each adapter
            break;
        case 7:
            printf("Pick a Device to Connect...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
            printf("device connect process completed.\n");
            break;
        case 8:
            printf("Pick a Device to Disconnect...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
            printf("device disconnect process completed.\n");
            break;
        case 9:
            printf("Pick a Device to Connect...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
            printf("device connect process completed.\n");
            break;
        case 10:
            printf("Pick a Device to Disonnect...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
            printf("device disconnect process completed.\n");
            break;
        case 11:
            printf("Getting all available adapters\n");
            //START - adapter selection: if there is more than one adapter, offer choice of which adapter to use for pairing
            BTRCore_GetAdapters(lhBTRCore, &GetAdapters);
            if ( GetAdapters.number_of_adapters > 1) {
                printf("There are %d Bluetooth adapters\n",GetAdapters.number_of_adapters);
                printf("current adatper is %s\n", lstBTRCoreAdapter.pcAdapterPath);
                printf("Which adapter would you like to use (0 = default)?\n");
                myadapter = getChoice();

                BTRCore_SetAdapter(lhBTRCore, myadapter);
            }
            //END adapter selection
            break;
        case 12:
            lstBTRCoreAdapter.adapter_number = myadapter;
            printf("Enabling adapter %d\n",lstBTRCoreAdapter.adapter_number);
            BTRCore_EnableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 13:
            lstBTRCoreAdapter.adapter_number = myadapter;
            printf("Disabling adapter %d\n",lstBTRCoreAdapter.adapter_number);
            BTRCore_DisableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 14:
            printf("Enter discoverable timeout in seconds.  Zero seconds = FOREVER \n");
            lstBTRCoreAdapter.DiscoverableTimeout = getChoice();
            printf("setting DiscoverableTimeout to %d\n",lstBTRCoreAdapter.DiscoverableTimeout);
            BTRCore_SetDiscoverableTimeout(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 15:
            printf("Set discoverable.  Zero = Not Discoverable, One = Discoverable \n");
            lstBTRCoreAdapter.discoverable = getChoice();
            printf("setting discoverable to %d\n",lstBTRCoreAdapter.discoverable);
            BTRCore_SetDiscoverable(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 16:
            {
                char lcAdapterName[64] = {'\0'};
                printf("Set friendly name (up to 64 characters): \n");
                fgets(lcAdapterName, 63 , stdin);
                printf("setting name to %s\n", lcAdapterName);
                BTRCore_SetAdapterDeviceName(lhBTRCore, &lstBTRCoreAdapter, lcAdapterName);
            }
            break;
        case 17:
            printf("Check for Audio Sink capability\n");
            printf("Pick a Device to Check for Audio Sink...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            if (BTRCore_FindService(lhBTRCore, devnum, BTR_CORE_A2SNK,NULL,&bfound) == enBTRCoreSuccess) {
                if (bfound) {
                    printf("Service UUID BTRCore_A2SNK is found\n");
                }
                else {
                    printf("Service UUID BTRCore_A2SNK is NOT found\n");
                }
            }
            else {
                printf("Error on BTRCore_FindService\n");
            }
            break;
        case 18:
            printf("Find a Service\n");
            printf("Pick a Device to Check for Services...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
            fgets(myService,sizeof(myService),stdin);
            for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                  {
                if(myService[i] == '\n')
                   myService[i] = '\0';
                }
            bfound=0;//assume not found
            if (BTRCore_FindService(lhBTRCore, devnum, myService,NULL,&bfound) == enBTRCoreSuccess) {
                if (bfound) {
                    printf("Service UUID %s is found\n",myService);
                }
                else {
                    printf("Service UUID %s is NOT found\n",myService);
                }
            }
            else {
                printf("Error on BTRCore_FindService\n");
            }
            break;
        case 19:
            printf("Find a Service and get details\n");
            printf("Pick a Device to Check for Services...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
            fgets(myService,sizeof(myService),stdin);
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
            if (BTRCore_FindService(lhBTRCore, devnum,myService,myData,&bfound)  == enBTRCoreSuccess) {
                if (bfound) {
                    printf("Service UUID %s is found\n",myService);
                    printf("Data is:\n %s \n",myData);
                }
                else {
                    printf("Service UUID %s is NOT found\n",myService);
                }
            }
            else {
                printf("Error on BTRCore_FindService\n");
            }
            break;
         case 20:
            printf("Pick a Device to Find (see if it is already paired)...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ShowFoundDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            if ( BTRCore_FindDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                printf("device FOUND successful.\n");
            else
              printf("device was NOT found.\n");
            break;
        case 21:
            printf("Pick a Device to Get Data tranport parameters...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_AcquireDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers, &liDataPath, &lidataReadMTU, &lidataWriteMTU);
            printf("Device Data Path = %d \n", liDataPath);
            printf("Device Data Read MTU = %d \n", lidataReadMTU);
            printf("Device Data Write MTU= %d \n", lidataWriteMTU);
            break;
        case 22:
            printf("Pick a Device to ReleaseData tranport...\n");
            lstBTRCoreAdapter.adapter_number = myadapter;
            BTRCore_ListKnownDevices(lhBTRCore, &lstBTRCoreAdapter);
            devnum = getChoice();
            BTRCore_ReleaseDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers);
            break;
        case 23:
            printf("Enter Encoded SBC file location to send to BT Headset/Speakers...\n");
            sbcEncodedFileName = getEncodedSBCFile ();
            if (sbcEncodedFileName) {
                printf(" We will send %s to BT FD %d \n", sbcEncodedFileName, liDataPath);
                sendSBCFileOverBT(sbcEncodedFileName, liDataPath, lidataWriteMTU);
                free(sbcEncodedFileName);
                sbcEncodedFileName = NULL;
            }
            else {
                printf(" Invalid file location\n");
            }
            break;
        case 88:
            test_func(&lstBTRCoreAdapter);
            break;
        case 99: 
            printf("Quitting program!\n");
            BTRCore_DeInit(lhBTRCore);
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



enBTRCoreRet
BTRCore_AbortDiscovery (
    tBTRCoreHandle  hBTRCore,
    stBTRCoreAbortDiscovery* pstAbortDiscovery
) {
    BTRCore_LOG(("BTRCore_AbortDiscovery\n"));
    return enBTRCoreSuccess;
}

/*BTRCore_ConfigureAdapter... set a particular attribute for the adapter*/
enBTRCoreRet 
BTRCore_ConfigureAdapter (
    tBTRCoreHandle      hBTRCore,
    stBTRCoreAdapter*   apstBTRCoreAdapter
) {
	BTRCore_LOG(("BTRCore_ConfigureAdapter\n"));
	return enBTRCoreSuccess;
}


/*BTRCore_DiscoverServices - finds a service from a given device*/
enBTRCoreRet 
BTRCore_DiscoverServices (
    tBTRCoreHandle  hBTRCore,
    stBTRCoreFindService* pstFindService
) {
    BTRCore_LOG(("BTRCore_DiscoverServices\n"));
#ifdef SIM_MODE
	BTRCore_LOG(("Looking for services with:\n"));
	BTRCore_LOG("Service Name: %s\n", pstFindService->filter_mode.service_name);
    BTRCore_LOG("UUID: %s\n", pstFindService->filter_mode.uuid);
    BTRCore_LOG("Service Name: %s\n", pstFindService->filter_mode.bd_address);
#endif
    return enBTRCoreSuccess;
}


enBTRCoreRet 
BTRCore_AdvertiseService (
    tBTRCoreHandle  hBTRCore,
    stBTRCoreAdvertiseService* pstAdvertiseService
) {
    BTRCore_LOG(("BTRCore_AdvertiseService\n"));
    return enBTRCoreSuccess;
}

