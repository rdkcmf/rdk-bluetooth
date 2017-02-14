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
/*BT.c file*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for close?
#include <errno.h>      //for errno handling
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h> //for mkfifo
#include <fcntl.h> //for open

/* Interface lib Headers */
#include "btrCore.h"            //basic RDK BT functions
#include "btrCore_service.h"    //service UUIDs, use for service discovery

//test func
void test_func(tBTRCoreHandle hBTRCore, stBTRCoreAdapter* apstBTRCoreAdapter);


//for BT audio input testing
static pthread_t fileWriteThread;
static int  iret1;
static int writeSBC = 0;
unsigned int BT_loop = 0;


typedef struct appDataStruct{
    tBTRCoreHandle          hBTRCore;
    stBTRCoreDevMediaInfo   stBtrCoreDevMediaInfo;
    int                     iDataPath;
    int                     iDataReadMTU;
    int                     iDataWriteMTU;
} appDataStruct;


#define NO_ADAPTER 1234

//for connection callback testing
static int acceptConnection = 0;
static int connectedDeviceIndex=0;

void *
DoSBCwrite (
    void* ptr
) {
    char *message = "SBC Write Thread Started";
    char * myfifo = "/tmp/myfifo";
    int fd;
    appDataStruct* pstAppData = (appDataStruct*)ptr;

    /* create the FIFO (named pipe) */
    mkfifo(myfifo, 0666);
    printf("Thread starting: %s \n", message);
    fd = open(myfifo, O_WRONLY );//not sure if I need nonblock or not the first time I tried it it failed. then I tried nonblock, no audio
    printf("BT data flowing...\n");

    do { 
        sleep (1);
    } while (writeSBC == 0);
           
   {
        int tmp;
        unsigned char sigByte1 = 0;
        unsigned char bitpool = 0;
        int blocks;
        int computed_len;
        int frame_length = 0;
        //unsigned char channel_mode;
        //unsigned char alloc_method;
        int sub_bands;
        
        unsigned char bit_blocks;
        unsigned char bit_channel_mode;
        //unsigned char bit_alloc_method;
        unsigned char bit_sub_bands;
        int k;
        int z;
        int y;//for first time processing
        unsigned char x;
        int first_time_thru;
        int sbcFrameState = 0;
        int bytes2write=0;
        unsigned char temp;
        int read_return;
        //FILE *w_ptr;
        //w_ptr=fopen("/tmp/myfifo", "wb");
        char buffy[1024];//hope MTU is not bigger than this, or we could get hurt...
        k=0;
        first_time_thru = 1;

        while (k < BT_loop) {
            //I guess the in is the out, so try 5 as our reading fd
            read_return = read(pstAppData->iDataPath, buffy, pstAppData->iDataReadMTU);
            /////////insert FIRST TIME CODE
            y=0;

            while (first_time_thru) {
                x = buffy[y];
                y++;

                if (x == 0x9C) {
                    printf("first sbc frame detected\n");
                    //get another byte
                    x = buffy[y];
                    y++;
                    sigByte1 = x;//we will use this later for parsing
                    bit_blocks = (x & 0x30) >> 4;
                    bit_channel_mode = (x & 0x0C) >> 2;
                    //   bit_alloc_method = (x & 0x02) >> 1;
                    bit_sub_bands = (x & 0x01);
                    sub_bands = (bit_sub_bands + 1) * 4;
                    blocks = (bit_blocks + 1) * 4;
                    //get another byte
                    x = buffy[y];
                    y++;
                    bitpool = x;
                    //get the length
                    printf("channel mode 0x%x\n",bit_channel_mode);
                    printf("nrof_subbands 0x%x\n",sub_bands);
                    printf("nrof_blocks 0x%x\n",blocks);
                    printf("bitpool 0x%x\n",bitpool );

                    switch (bit_channel_mode) {
                        case 0x00:
                        bit_blocks /= 2;
                        tmp = blocks * bitpool;
                        break;
                        case 0x01:
                        tmp = blocks * bitpool * 2;
                        break;
                        case 0x02:
                        tmp = blocks * bitpool;
                        break;
                        case 0x03:
                        tmp = blocks * bitpool + sub_bands;
                        break;
                        default:
                        return 0;
                    }

                    computed_len = sub_bands + ((tmp + 7) / 8);
                    frame_length = computed_len + 4;
                    //printf("tmp = %d\n",tmp);
                    //printf("computed length is %d\n",computed_len);
                    printf("frame length is %d\n",frame_length);
                    first_time_thru = 0;
                    //we now know how long the frames are. and what the two signature bytes after the sync are
                    //lets not bother with this frame, instead start searching now, based on what we know.
                }
            }

            //////END FIRST TIME CODE now that we know the details, lets parse
            if (read_return > 0) {
                if ( (k % 1000) == 0) {
                    printf("reading %d - %d - %d\n",read_return,k,BT_loop);
                }

                k++;
                //fwrite(buffy, 1 , read_return, fp);
                //remove the sbc data from rtp

                for(z=0;z<read_return;z++) {
                    x=buffy[z];
                    if ((sbcFrameState == 0) && (x == 0x9C)) {
                        sbcFrameState = 1;
                    }

                    if ((sbcFrameState == 1) && (x == sigByte1)) {
                        sbcFrameState = 2;
                    }

                    if ((sbcFrameState == 2) && (x == bitpool)) {
                        sbcFrameState = 3;

                        sbcFrameState = 4;
                        bytes2write=frame_length-2;///113 + 2 sig bytes
                        //write 0x9C plus header info
                        temp = 0x9C;
                        //   fwrite(&temp,1,1,w_ptr);
                        write(fd,&temp,1);
                        temp = sigByte1;
                        //     fwrite(&temp,1,1,w_ptr);
                        write(fd,&temp,1);
                    }

                    if (bytes2write) {
                        bytes2write--;
                        //write x
                        //fwrite(&x,1,1,w_ptr);
                        write(fd,&x,1);

                        if (bytes2write == 0)
                            sbcFrameState = 0;//reset the state machine
                    }
                }//end for
            }

            usleep(50);//chandresh did this, so lets try?
        }

        // fclose(w_ptr);
        close(fd);
        unlink(myfifo); 
    }

    return NULL;
}



static void
GetTransport (
    appDataStruct* pstAppData
) {

	if (!pstAppData)
		return;

    BTRCore_GetDeviceMediaInfo ( pstAppData->hBTRCore, connectedDeviceIndex, enBTRCoreMobileAudioIn, &pstAppData->stBtrCoreDevMediaInfo);

    pstAppData->iDataPath = 0;
    pstAppData->iDataReadMTU = 0;
    pstAppData->iDataWriteMTU = 0;

    BTRCore_AcquireDeviceDataPath ( pstAppData->hBTRCore, 
                                    connectedDeviceIndex,
                                    enBTRCoreMobileAudioIn,
                                    &pstAppData->iDataPath,
                                    &pstAppData->iDataReadMTU,
                                    &pstAppData->iDataWriteMTU);

    printf("Device Data Path = %d \n",      pstAppData->iDataPath);
    printf("Device Data Read MTU = %d \n",  pstAppData->iDataReadMTU);
    printf("Device Data Write MTU= %d \n",  pstAppData->iDataWriteMTU);

    if (pstAppData->stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
		if (pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
            printf("Device Media Info SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
            printf("Device Media Info AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
            printf("Device Media Info SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
            printf("Device Media Info SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
            printf("Device Media Info SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
            printf("Device Media Info SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
            printf("Device Media Info SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
            printf("Device Media Info SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
            printf("Device Media Info SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
		}
    }
}


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


static void 
sendSBCFileOverBT (
    char*   fileLocation,
    int     fd,
    int     mtuSize
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


int
cb_connection_authentication (
    stBTRCoreConnCBInfo* apstConnCbInfo
) {
#if 0
    printf("\n\nConnection attempt by: %s\n",path);
#endif
    printf("Choose 35 to accept the connection/verify pin-passcode or 36 to deny the connection/discard pin-passcode\n\n");

    if (apstConnCbInfo->ui32devPassKey) {
        printf("Incoming Connection passkey = %6d\n", apstConnCbInfo->ui32devPassKey);
    }    

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    printf("you picked %d\n", acceptConnection);
    if (acceptConnection == 1) {
        printf("connection accepted\n");
        acceptConnection = 0;//reset variabhle for the next connection
        return 1;
    }
    else {
        printf("connection denied\n");
        acceptConnection = 0;//reset variabhle for the next connection
        return 0;
    }
}


void
cb_unsolicited_bluetooth_status (
    stBTRCoreDevStateCBInfo* p_StatusCB,
    void*                    apvUserData
) {
    //printf("device status change: %d\n",p_StatusCB->eDeviceType);
    printf("app level cb device status change: new state is %d\n",p_StatusCB->eDeviceCurrState);
    if ((p_StatusCB->eDevicePrevState == enBTRCore_DS_Connected) && (p_StatusCB->eDeviceCurrState == enBTRCore_DS_Playing)) {
        if (p_StatusCB->eDeviceType == enBTRCoreMobileAudioIn) {
            printf("transition to playing, get the transport info...\n");
            GetTransport((appDataStruct*)apvUserData);
        }
    }

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
    printf("29. BT audio input test\n");
    printf("30. install agent for accepting connections NoInputNoOutput\n");
    printf("31. install agent for accepting connections DisplayYesNo\n");
    printf("32. Uninstall agent - allows device-initiated pairing\n");
    printf("33. Register connection-in intimation callback.\n");
    printf("34. Register connection authentication callback to allow accepting or rejection of connections.\n");
    printf("35. Accept a connection request\n");
    printf("36. Deny a connection request\n");

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

    char  default_path[128];
    char* agent_path = NULL;
    char myData[2048];
    int myadapter = 0;
    int bfound;
    int i;

    char *sbcEncodedFileName = NULL;
    
    char myService[16];//for testing findService API

    appDataStruct stAppData;

    memset(&stAppData, 0, sizeof(stAppData));

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

    stAppData.hBTRCore = lhBTRCore;
    //register callback for unsolicted events, such as powering off a bluetooth device
    BTRCore_RegisterStatusCallback(lhBTRCore, cb_unsolicited_bluetooth_status, &stAppData);

    stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

    //display a menu of choices
    printMenu();
    //start Bluetooth input data writing thread - supports BT in audio test
    iret1 = pthread_create( &fileWriteThread, NULL, DoSBCwrite, (void*) &stAppData);
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
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Show Found Devices\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
            }
            break;
        case 4:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Pick a Device to Pair...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();

                printf(" adapter_path %s\n", lstBTRCoreAdapter.pcAdapterPath);
                printf(" agent_path %s\n",agent_path);
                if ( BTRCore_PairDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    printf("device pairing successful.\n");
                else
                  printf("device pairing FAILED.\n");
            }
            break;
        case 5:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("UnPair/Forget a device\n");
                printf("Pick a Device to Remove...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_UnPairDevice(lhBTRCore, devnum);
            }
            break;
        case 6:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Show Known Devices...using BTRCore_GetListOfPairedDevices\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList); //TODO pass in a different structure for each adapter
            }
            break;
        case 7:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Connect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                printf("device connect process completed.\n");
            }
            break;
        case 8:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Disconnect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                printf("device disconnect process completed.\n");
            }
            break;
        case 9:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Connect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                printf("device connect process completed.\n");
            }
            break;
        case 10:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Disonnect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                printf("device disconnect process completed.\n");
            }
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
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Check for Audio Sink capability\n");
                printf("Pick a Device to Check for Audio Sink...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
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
            }
            break;
        case 18:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Find a Service\n");
                printf("Pick a Device to Check for Services...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
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
            }
            break;
        case 19:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Find a Service and get details\n");
                printf("Pick a Device to Check for Services...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
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
            }
            break;
         case 20:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Pick a Device to Find (see if it is already paired)...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();
                if ( BTRCore_FindDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    printf("device FOUND successful.\n");
                else
                  printf("device was NOT found.\n");
            }
            break;
        case 21:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Get Data tranport parameters...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();

                BTRCore_GetDeviceMediaInfo(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.stBtrCoreDevMediaInfo);

                stAppData.iDataPath = 0;
                stAppData.iDataReadMTU = 0;
                stAppData.iDataWriteMTU = 0;

                BTRCore_AcquireDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.iDataPath, &stAppData.iDataReadMTU, &stAppData.iDataWriteMTU);

                printf("Device Data Path = %d \n", stAppData.iDataPath);
                printf("Device Data Read MTU = %d \n", stAppData.iDataReadMTU);
                printf("Device Data Write MTU= %d \n", stAppData.iDataWriteMTU);

                if (stAppData.stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
					if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                        printf("Device Media Info SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
                        printf("Device Media Info AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
                        printf("Device Media Info SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
                        printf("Device Media Info SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
                        printf("Device Media Info SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
                        printf("Device Media Info SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
                        printf("Device Media Info SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
                        printf("Device Media Info SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
                        printf("Device Media Info SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
					}
                }
            }
            break;
        case 22:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to ReleaseData tranport...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ReleaseDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers);

                stAppData.iDataPath      = 0;
                stAppData.iDataReadMTU   = 0;
                stAppData.iDataWriteMTU  = 0;
            }
            break;
        case 23:
            printf("Enter Encoded SBC file location to send to BT Headset/Speakers...\n");
            sbcEncodedFileName = getEncodedSBCFile();
            if (sbcEncodedFileName) {
                printf(" We will send %s to BT FD %d \n", sbcEncodedFileName, stAppData.iDataPath);
                sendSBCFileOverBT(sbcEncodedFileName, stAppData.iDataPath, stAppData.iDataWriteMTU);
                free(sbcEncodedFileName);
                sbcEncodedFileName = NULL;
            }
            else {
                printf(" Invalid file location\n");
            }
            break;
        case 29:
            printf("rtp deplayload and play some music over BT\n");
            printf("about how many minutes to play?\n");
            choice = getChoice();
            BT_loop = 6000 * choice;//6000 equates to roughly one minute
            writeSBC = 1;
            sleep(2);
            system("gst-launch-1.0 filesrc location=/tmp/myfifo   ! sbcparse ! sbcdec ! brcmpcmsink");
            break;
        case 30:
            printf("install agent - NoInputNoOutput\n");
            BTRCore_RegisterAgent(lhBTRCore, 0);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 31:
            printf("install agent - DisplayYesNo\n");
            BTRCore_RegisterAgent(lhBTRCore, 1);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 32:
            printf("uninstall agent - DisplayYesNo\n");
            BTRCore_UnregisterAgent(lhBTRCore);
            break;
        case 33:
            printf("register connection-in Intimation CB\n");
            BTRCore_RegisterConnectionIntimationCallback(lhBTRCore, cb_connection_authentication, NULL);
            break;
        case 34:
            printf("register authentication CB\n");
            BTRCore_RegisterConnectionAuthenticationCallback(lhBTRCore, cb_connection_authentication, NULL);
            break;
        case 35:
            printf("accept the connection\n");
            acceptConnection = 1;
            break;
        case 36:
            printf("deny the connection\n");
            acceptConnection = 2;//anything but 1 means do not connect
            break;
        case 88:
            test_func(lhBTRCore, &lstBTRCoreAdapter);
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


    if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo)
        free(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);

    return 0;
}

