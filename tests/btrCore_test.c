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

#include <sys/stat.h> //for mkfifo
#include <fcntl.h> //for open

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrCore.h"            //basic RDK BT functions
#include "btrCore_service.h"    //service UUIDs, use for service discovery


//for BT audio input testing
static GThread* fileWriteThread = NULL;
static int      writeSBC = 0;
unsigned int    BT_loop = 0;


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
    fprintf(stderr, "%d\t: %s - Thread starting: %s \n", __LINE__, __FUNCTION__, message);
    fd = open(myfifo, O_WRONLY );//not sure if I need nonblock or not the first time I tried it it failed. then I tried nonblock, no audio
    fprintf(stderr, "%d\t: %s - BT data flowing...\n", __LINE__, __FUNCTION__);

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
                    fprintf(stderr, "%d\t: %s - first sbc frame detected\n", __LINE__, __FUNCTION__);
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
                    fprintf(stderr, "%d\t: %s - channel mode 0x%x\n", __LINE__, __FUNCTION__, bit_channel_mode);
                    fprintf(stderr, "%d\t: %s - nrof_subbands 0x%x\n", __LINE__, __FUNCTION__, sub_bands);
                    fprintf(stderr, "%d\t: %s - nrof_blocks 0x%x\n", __LINE__, __FUNCTION__, blocks);
                    fprintf(stderr, "%d\t: %s - bitpool 0x%x\n", __LINE__, __FUNCTION__, bitpool );

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
                    //fprintf(stderr, "%d\t: %s - tmp = %d\n", __LINE__, __FUNCTION__tmp);
                    //fprintf(stderr, "%d\t: %s - computed length is %d\n", __LINE__, __FUNCTION__computed_len);
                    fprintf(stderr, "%d\t: %s - frame length is %d\n", __LINE__, __FUNCTION__, frame_length);
                    first_time_thru = 0;
                    //we now know how long the frames are. and what the two signature bytes after the sync are
                    //lets not bother with this frame, instead start searching now, based on what we know.
                }
            }

            //////END FIRST TIME CODE now that we know the details, lets parse
            if (read_return > 0) {
                if ( (k % 1000) == 0) {
                    fprintf(stderr, "%d\t: %s - reading %d - %d - %d\n", __LINE__, __FUNCTION__, read_return,k,BT_loop);
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

    fprintf(stderr, "%d\t: %s - Device Data Path = %d \n", __LINE__, __FUNCTION__,      pstAppData->iDataPath);
    fprintf(stderr, "%d\t: %s - Device Data Read MTU = %d \n", __LINE__, __FUNCTION__,  pstAppData->iDataReadMTU);
    fprintf(stderr, "%d\t: %s - Device Data Write MTU= %d \n", __LINE__, __FUNCTION__,  pstAppData->iDataWriteMTU);

    if (pstAppData->stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
		if (pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
            fprintf(stderr, "%d\t: %s - Device Media Info SFreq         = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
            fprintf(stderr, "%d\t: %s - Device Media Info AChan         = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcAllocMethod= %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcSubbands   = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcBlockLength= %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcMinBitpool = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcMaxBitpool = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcFrameLen   = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
            fprintf(stderr, "%d\t: %s - Device Media Info SbcBitrate    = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
		}
    }
}


static int
getChoice (
    void
) {
    int mychoice;
    fprintf(stderr, "\nEnter a choice...\n");
    scanf("%d", &mychoice);
        getchar();//suck up a newline?
    return mychoice;
}

static char*
getEncodedSBCFile (
    void
) {
    char sbcEncodedFile[1024];
    fprintf(stderr, "%d\t: %s - Enter SBC File location...\n", __LINE__, __FUNCTION__);
    scanf("%s", sbcEncodedFile);
        getchar();//suck up a newline?
    return strdup(sbcEncodedFile);
}

static char*
getLeUuidString (
    void
) {
    char leUuidString[64];
    fprintf(stderr, "%d\t: %s - Enter the UUID for Le device...\n", __LINE__, __FUNCTION__);
    scanf("%s",leUuidString );
        getchar();//suck up a newline?
    return strdup(leUuidString);
}

static int
getBitsToString (
    unsigned short  flagBits
) {
    unsigned char i = 0;
    for (; i<16; i++) {
        fprintf(stderr, "%d", (flagBits >> i) & 1);
    }

    return 0;
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

    fprintf(stderr, "%d\t: %s - fileLocation %s", __LINE__, __FUNCTION__, fileLocation);

    fseek(sbcFilePtr, 0, SEEK_END);
    bytesLeft = ftell(sbcFilePtr);
    fseek(sbcFilePtr, 0, SEEK_SET);

    fprintf(stderr, "%d\t: %s - File size: %d bytes\n", __LINE__, __FUNCTION__, (int)bytesLeft);

    encoded_buf = malloc (mtuSize);

    while (bytesLeft) {

        if (bytesLeft < mtuSize)
            bytesToSend = bytesLeft;

        timeout = poll (&pollout, 1, 1000); //delay 1s to allow others to update our state

        if (timeout == 0)
            continue;
        if (timeout < 0)
            fprintf (stderr, "%d\t: %s - Bluetooth Write Error : %d\n", __LINE__, __FUNCTION__, errno);

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


enBTRCoreRet
cb_connection_intimation (
    stBTRCoreConnCBInfo* apstConnCbInfo,
    int*                 api32ConnInIntimResp,
    void*                apvUserData
) {
    fprintf(stderr, "%d\t: %s - Choose 35 to verify pin-passcode or 36 to discard pin-passcode\n\n", __LINE__, __FUNCTION__);

    if (apstConnCbInfo->ui32devPassKey) {
        fprintf(stderr, "%d\t: %s - Incoming Connection passkey = %6d\n", __LINE__, __FUNCTION__, apstConnCbInfo->ui32devPassKey);
    }    

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    fprintf(stderr, "%d\t: %s - you picked %d\n", __LINE__, __FUNCTION__, acceptConnection);
    if (acceptConnection == 1) {
        fprintf(stderr, "%d\t: %s - Pin-Passcode accepted\n", __LINE__, __FUNCTION__);
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInIntimResp = 1;
    }
    else {
        fprintf(stderr, "%d\t: %s - Pin-Passcode denied\n", __LINE__, __FUNCTION__);
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInIntimResp = 0;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
cb_connection_authentication (
    stBTRCoreConnCBInfo* apstConnCbInfo,
    int*                 api32ConnInAuthResp,
    void*                apvUserData
) {
    fprintf(stderr, "%d\t: %s - Choose 35 to accept the connection or 36 to deny the connection\n\n", __LINE__, __FUNCTION__);

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    fprintf(stderr, "%d\t: %s - you picked %d\n", __LINE__, __FUNCTION__, acceptConnection);
    if (acceptConnection == 1) {
        fprintf(stderr, "%d\t: %s - connection accepted\n", __LINE__, __FUNCTION__);
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInAuthResp = 1;
    }
    else {
        fprintf(stderr, "%d\t: %s - connection denied\n", __LINE__, __FUNCTION__);
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInAuthResp = 0;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
cb_unsolicited_bluetooth_status (
    stBTRCoreDevStatusCBInfo*   p_StatusCB,
    void*                       apvUserData
) {
    //fprintf(stderr, "%d\t: %s - device status change: %d\n", __LINE__, __FUNCTION__p_StatusCB->eDeviceType);
    fprintf(stderr, "%d\t: %s - app level cb device status change: new state is %d\n", __LINE__, __FUNCTION__, p_StatusCB->eDeviceCurrState);
    if ((p_StatusCB->eDevicePrevState == enBTRCoreDevStConnected) && (p_StatusCB->eDeviceCurrState == enBTRCoreDevStPlaying)) {
        if (p_StatusCB->eDeviceType == enBTRCoreMobileAudioIn) {
            fprintf(stderr, "%d\t: %s - transition to playing, get the transport info...\n", __LINE__, __FUNCTION__);
            GetTransport((appDataStruct*)apvUserData);
        }
    }

    return enBTRCoreSuccess;
}

static void
printMenu (
    void
) {
    fprintf( stderr, "Bluetooth Test Menu\n\n");
    fprintf( stderr, "1. Get Current Adapter\n");
    fprintf( stderr, "2. Scan\n");
    fprintf( stderr, "3. Show found devices\n");
    fprintf( stderr, "4. Pair\n");
    fprintf( stderr, "5. UnPair/Forget a device\n");
    fprintf( stderr, "6. Show known devices\n");
    fprintf( stderr, "7. Connect to Headset/Speakers\n");
    fprintf( stderr, "8. Disconnect to Headset/Speakers\n");
    fprintf( stderr, "9. Connect as Headset/Speakerst\n");
    fprintf( stderr, "10. Disconnect as Headset/Speakerst\n");
    fprintf( stderr, "11. Show all Bluetooth Adapters\n");
    fprintf( stderr, "12. Enable Bluetooth Adapter\n");
    fprintf( stderr, "13. Disable Bluetooth Adapter\n");
    fprintf( stderr, "14. Set Discoverable Timeout\n");
    fprintf( stderr, "15. Set Discoverable \n");
    fprintf( stderr, "16. Set friendly name \n");
    fprintf( stderr, "17. Check for audio sink capability\n");
    fprintf( stderr, "18. Check for existance of a service\n");
    fprintf( stderr, "19. Find service details\n");
    fprintf( stderr, "20. Check if Device Paired\n");
    fprintf( stderr, "21. Get Connected Dev Data path\n");
    fprintf( stderr, "22. Release Connected Dev Data path\n");
    fprintf( stderr, "23. Send SBC data to BT Headset/Speakers\n");
    fprintf( stderr, "29. BT audio input test\n");
    fprintf( stderr, "30. install agent for accepting connections NoInputNoOutput\n");
    fprintf( stderr, "31. install agent for accepting connections DisplayYesNo\n");
    fprintf( stderr, "32. Uninstall agent - allows device-initiated pairing\n");
    fprintf( stderr, "33. Register connection-in intimation callback.\n");
    fprintf( stderr, "34. Register connection authentication callback to allow accepting or rejection of connections.\n");
    fprintf( stderr, "35. Accept a connection request\n");
    fprintf( stderr, "36. Deny a connection request\n");
    fprintf( stderr, "37. Check if Device is Connectable\n");
    fprintf( stderr, "38. Scan for LE Devices\n");
    fprintf( stderr, "39. Connect to LE Device\n");
    fprintf( stderr, "40. Disconnect to LE Device\n");
    fprintf( stderr, "41. Get Gatt Properties of connected LE device.\n");
    fprintf( stderr, "42. Perform Operation on connected LE device.\n");
    fprintf( stderr, "43. Connect to HID/Unknown\n");
    fprintf( stderr, "44. Disconnect to HID/Unknown\n");

    fprintf( stderr, "88. debug test\n");
    fprintf( stderr, "99. Exit\n");
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
        fprintf(stderr, "%d\t: %s - GetAdapter Returns Adapter number %d\n", __LINE__, __FUNCTION__, default_adapter);
    }
    else {
        fprintf(stderr, "%d\t: %s - No bluetooth adapter found!\n", __LINE__, __FUNCTION__);
        return -1;
    }

    stAppData.hBTRCore = lhBTRCore;
    //register callback for unsolicted events, such as powering off a bluetooth device
    BTRCore_RegisterStatusCb(lhBTRCore, cb_unsolicited_bluetooth_status, &stAppData);

    stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

    //display a menu of choices
    printMenu();
    //start Bluetooth input data writing thread - supports BT in audio test
    fileWriteThread = g_thread_new("DoSBCwrite", DoSBCwrite, (gpointer)&stAppData);
    do {
        fprintf(stderr, "Enter a choice...\n");
        scanf("%d", &choice);
        getchar();//suck up a newline?
        switch (choice) {
        case 1: 
            fprintf(stderr, "%d\t: %s - Adapter is %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
            break;
        case 2: 
            if (default_adapter != NO_ADAPTER) {
                fprintf(stderr, "%d\t: %s - Looking for devices on BT adapter %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
                fprintf(stderr, "%d\t: %s - Performing device scan for 15 seconds . Please wait...\n", __LINE__, __FUNCTION__);
                BTRCore_StartDiscovery(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, enBTRCoreUnknown, 15);
                fprintf(stderr, "%d\t: %s - scan complete\n", __LINE__, __FUNCTION__);
            }
            else {
                fprintf(stderr, "%d\t: %s - Error, no default_adapter set\n", __LINE__, __FUNCTION__);
            }
            break;
        case 3:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                fprintf(stderr, "%d\t: %s - Show Found Devices\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
            }
            break;
        case 4:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Pair...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();

                fprintf(stderr, "%d\t: %s -  adapter_path %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
                fprintf(stderr, "%d\t: %s -  agent_path %s\n", __LINE__, __FUNCTION__, agent_path);
                if ( BTRCore_PairDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    fprintf(stderr, "%d\t: %s - device pairing successful.\n", __LINE__, __FUNCTION__);
                else
                  fprintf(stderr, "%d\t: %s - device pairing FAILED.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 5:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - UnPair/Forget a device\n", __LINE__, __FUNCTION__);
                fprintf(stderr, "%d\t: %s - Pick a Device to Remove...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_UnPairDevice(lhBTRCore, devnum);
            }
            break;
        case 6:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Show Known Devices...using BTRCore_GetListOfPairedDevices\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList); //TODO pass in a different structure for each adapter
            }
            break;
        case 7:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Connect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                fprintf(stderr, "%d\t: %s - device connect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 8:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Disconnect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                fprintf(stderr, "%d\t: %s - device disconnect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 9:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Connect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                fprintf(stderr, "%d\t: %s - device connect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 10:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Disonnect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                fprintf(stderr, "%d\t: %s - device disconnect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 11:
            fprintf(stderr, "%d\t: %s - Getting all available adapters\n", __LINE__, __FUNCTION__);
            //START - adapter selection: if there is more than one adapter, offer choice of which adapter to use for pairing
            BTRCore_GetAdapters(lhBTRCore, &GetAdapters);
            if ( GetAdapters.number_of_adapters > 1) {
                fprintf(stderr, "%d\t: %s - There are %d Bluetooth adapters\n", __LINE__, __FUNCTION__, GetAdapters.number_of_adapters);
                fprintf(stderr, "%d\t: %s - current adatper is %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
                fprintf(stderr, "%d\t: %s - Which adapter would you like to use (0 = default)?\n", __LINE__, __FUNCTION__);
                myadapter = getChoice();

                BTRCore_SetAdapter(lhBTRCore, myadapter);
            }
            //END adapter selection
            break;
        case 12:
            lstBTRCoreAdapter.adapter_number = myadapter;
            fprintf(stderr, "%d\t: %s - Enabling adapter %d\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.adapter_number);
            BTRCore_EnableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 13:
            lstBTRCoreAdapter.adapter_number = myadapter;
            fprintf(stderr, "%d\t: %s - Disabling adapter %d\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.adapter_number);
            BTRCore_DisableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 14:
            fprintf(stderr, "%d\t: %s - Enter discoverable timeout in seconds.  Zero seconds = FOREVER \n", __LINE__, __FUNCTION__);
            lstBTRCoreAdapter.DiscoverableTimeout = getChoice();
            fprintf(stderr, "%d\t: %s - setting DiscoverableTimeout to %d\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.DiscoverableTimeout);
            BTRCore_SetAdapterDiscoverableTimeout(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, lstBTRCoreAdapter.DiscoverableTimeout);
            break;
        case 15:
            fprintf(stderr, "%d\t: %s - Set discoverable.  Zero = Not Discoverable, One = Discoverable \n", __LINE__, __FUNCTION__);
            lstBTRCoreAdapter.discoverable = getChoice();
            fprintf(stderr, "%d\t: %s - setting discoverable to %d\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.discoverable);
            BTRCore_SetAdapterDiscoverable(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, lstBTRCoreAdapter.discoverable);
            break;
        case 16:
            {
                char lcAdapterName[64] = {'\0'};
                fprintf(stderr, "%d\t: %s - Set friendly name (up to 64 characters): \n", __LINE__, __FUNCTION__);
                fgets(lcAdapterName, 63 , stdin);
                fprintf(stderr, "%d\t: %s - setting name to %s\n", __LINE__, __FUNCTION__, lcAdapterName);
                BTRCore_SetAdapterDeviceName(lhBTRCore, &lstBTRCoreAdapter, lcAdapterName);
            }
            break;
        case 17:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Check for Audio Sink capability\n", __LINE__, __FUNCTION__);
                fprintf(stderr, "%d\t: %s - Pick a Device to Check for Audio Sink...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                if (BTRCore_FindService(lhBTRCore, devnum, BTR_CORE_A2SNK,NULL,&bfound) == enBTRCoreSuccess) {
                    if (bfound) {
                        fprintf(stderr, "%d\t: %s - Service UUID BTRCore_A2SNK is found\n", __LINE__, __FUNCTION__);
                    }
                    else {
                        fprintf(stderr, "%d\t: %s - Service UUID BTRCore_A2SNK is NOT found\n", __LINE__, __FUNCTION__);
                    }
                }
                else {
                    fprintf(stderr, "%d\t: %s - Error on BTRCore_FindService\n", __LINE__, __FUNCTION__);
                }
            }
            break;
        case 18:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Find a Service\n", __LINE__, __FUNCTION__);
                fprintf(stderr, "%d\t: %s - Pick a Device to Check for Services...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                fprintf(stderr, "%d\t: %s - enter UUID of desired service... e.g. 0x110b for Audio Sink\n", __LINE__, __FUNCTION__);
                fgets(myService,sizeof(myService),stdin);
                for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                      {
                    if(myService[i] == '\n')
                       myService[i] = '\0';
                    }
                bfound=0;//assume not found
                if (BTRCore_FindService(lhBTRCore, devnum, myService,NULL,&bfound) == enBTRCoreSuccess) {
                    if (bfound) {
                        fprintf(stderr, "%d\t: %s - Service UUID %s is found\n", __LINE__, __FUNCTION__, myService);
                    }
                    else {
                        fprintf(stderr, "%d\t: %s - Service UUID %s is NOT found\n", __LINE__, __FUNCTION__, myService);
                    }
                }
                else {
                    fprintf(stderr, "%d\t: %s - Error on BTRCore_FindService\n", __LINE__, __FUNCTION__);
                }
            }
            break;
        case 19:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Find a Service and get details\n", __LINE__, __FUNCTION__);
                fprintf(stderr, "%d\t: %s - Pick a Device to Check for Services...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                fprintf(stderr, "%d\t: %s - enter UUID of desired service... e.g. 0x110b for Audio Sink\n", __LINE__, __FUNCTION__);
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
                        fprintf(stderr, "%d\t: %s - Service UUID %s is found\n", __LINE__, __FUNCTION__, myService);
                        fprintf(stderr, "%d\t: %s - Data is:\n %s \n", __LINE__, __FUNCTION__, myData);
                    }
                    else {
                        fprintf(stderr, "%d\t: %s - Service UUID %s is NOT found\n", __LINE__, __FUNCTION__, myService);
                    }
                }
                else {
                    fprintf(stderr, "%d\t: %s - Error on BTRCore_FindService\n", __LINE__, __FUNCTION__);
                }
            }
            break;
         case 20:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Find (see if it is already paired)...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();
                if ( BTRCore_FindDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    fprintf(stderr, "%d\t: %s - device FOUND successful.\n", __LINE__, __FUNCTION__);
                else
                  fprintf(stderr, "%d\t: %s - device was NOT found.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 21:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Get Data tranport parameters...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();

                BTRCore_GetDeviceMediaInfo(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.stBtrCoreDevMediaInfo);

                stAppData.iDataPath = 0;
                stAppData.iDataReadMTU = 0;
                stAppData.iDataWriteMTU = 0;

                BTRCore_AcquireDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.iDataPath, &stAppData.iDataReadMTU, &stAppData.iDataWriteMTU);

                fprintf(stderr, "%d\t: %s - Device Data Path = %d \n", __LINE__, __FUNCTION__, stAppData.iDataPath);
                fprintf(stderr, "%d\t: %s - Device Data Read MTU = %d \n", __LINE__, __FUNCTION__, stAppData.iDataReadMTU);
                fprintf(stderr, "%d\t: %s - Device Data Write MTU= %d \n", __LINE__, __FUNCTION__, stAppData.iDataWriteMTU);

                if (stAppData.stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
					if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                        fprintf(stderr, "%d\t: %s - Device Media Info SFreq         = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
                        fprintf(stderr, "%d\t: %s - Device Media Info AChan         = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcAllocMethod= %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcSubbands   = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcBlockLength= %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcMinBitpool = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcMaxBitpool = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcFrameLen   = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
                        fprintf(stderr, "%d\t: %s - Device Media Info SbcBitrate    = %d\n", __LINE__, __FUNCTION__, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
					}
                }
            }
            break;
        case 22:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to ReleaseData tranport...\n", __LINE__, __FUNCTION__);
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
            fprintf(stderr, "%d\t: %s - Enter Encoded SBC file location to send to BT Headset/Speakers...\n", __LINE__, __FUNCTION__);
            sbcEncodedFileName = getEncodedSBCFile();
            if (sbcEncodedFileName) {
                fprintf(stderr, "%d\t: %s -  We will send %s to BT FD %d \n", __LINE__, __FUNCTION__, sbcEncodedFileName, stAppData.iDataPath);
                sendSBCFileOverBT(sbcEncodedFileName, stAppData.iDataPath, stAppData.iDataWriteMTU);
                free(sbcEncodedFileName);
                sbcEncodedFileName = NULL;
            }
            else {
                fprintf(stderr, "%d\t: %s -  Invalid file location\n", __LINE__, __FUNCTION__);
            }
            break;
        case 29:
            fprintf(stderr, "%d\t: %s - rtp deplayload and play some music over BT\n", __LINE__, __FUNCTION__);
            fprintf(stderr, "%d\t: %s - about how many minutes to play?\n", __LINE__, __FUNCTION__);
            choice = getChoice();
            BT_loop = 6000 * choice;//6000 equates to roughly one minute
            writeSBC = 1;
            sleep(2);
            system("gst-launch-1.0 filesrc location=/tmp/myfifo   ! sbcparse ! sbcdec ! brcmpcmsink");
            break;
        case 30:
            fprintf(stderr, "%d\t: %s - install agent - NoInputNoOutput\n", __LINE__, __FUNCTION__);
            BTRCore_RegisterAgent(lhBTRCore, 0);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 31:
            fprintf(stderr, "%d\t: %s - install agent - DisplayYesNo\n", __LINE__, __FUNCTION__);
            BTRCore_RegisterAgent(lhBTRCore, 1);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 32:
            fprintf(stderr, "%d\t: %s - uninstall agent - DisplayYesNo\n", __LINE__, __FUNCTION__);
            BTRCore_UnregisterAgent(lhBTRCore);
            break;
        case 33:
            fprintf(stderr, "%d\t: %s - register connection-in Intimation CB\n", __LINE__, __FUNCTION__);
            BTRCore_RegisterConnectionIntimationCb(lhBTRCore, cb_connection_intimation, NULL);
            break;
        case 34:
            fprintf(stderr, "%d\t: %s - register authentication CB\n", __LINE__, __FUNCTION__);
            BTRCore_RegisterConnectionAuthenticationCb(lhBTRCore, cb_connection_authentication, NULL);
            break;
        case 35:
            fprintf(stderr, "%d\t: %s - accept the connection\n", __LINE__, __FUNCTION__);
            acceptConnection = 1;
            break;
        case 36:
            fprintf(stderr, "%d\t: %s - deny the connection\n", __LINE__, __FUNCTION__);
            acceptConnection = 2;//anything but 1 means do not connect
            break;
        case 37:
            fprintf(stderr, "%d\t: %s - Pick a Device to Check if Connectable...\n", __LINE__, __FUNCTION__);
            devnum = getChoice();
            BTRCore_IsDeviceConnectable(lhBTRCore, devnum);
            break;
        case 38: 
            if (default_adapter != NO_ADAPTER) {
                fprintf(stderr, "%d\t: %s - Looking for LE devices on BT adapter %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
                fprintf(stderr, "%d\t: %s - Performing LE scan for 30 seconds . Please wait...\n", __LINE__, __FUNCTION__);
                BTRCore_StartDiscovery(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, enBTRCoreLE, 30);
                fprintf(stderr, "%d\t: %s - scan complete\n", __LINE__, __FUNCTION__);
            }
            else {
                fprintf(stderr, "%d\t: %s - Error, no default_adapter set\n", __LINE__, __FUNCTION__);
            }
            break;
        case 39:
            {
                stBTRCoreScannedDevicesCount lBTRCoreScannedDevList;
                fprintf(stderr, "%d\t: %s - Pick a LE Device to Connect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lBTRCoreScannedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreLE);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                fprintf(stderr, "%d\t: %s - LE device connect process completed.\n", __LINE__, __FUNCTION__);
            }

            break;
        case 40:
           {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                fprintf(stderr, "%d\t: %s - Pick a LE Device to Disconnect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum,enBTRCoreLE);
                fprintf(stderr, "%d\t: %s - LE device disconnect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 41:
           {
                fprintf(stderr, "%d\t: %s - Pick a connected LE Device to Show Gatt Properties.\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                devnum = getChoice();
                char *leUuidString = getLeUuidString();

                if (leUuidString) {

                   fprintf(stderr, "%d\t: %s - select the property you want to query for.\n",  __LINE__, __FUNCTION__);
                   fprintf(stderr, "[0 - Uuid | 1 - Primary | 2 - Device | 3 - Service | 4 - Value |"
                                   " 5 - Notifying | 6 - Flags | 7 -Character]\n");
                   int propSelection = getChoice();
        
                   if (!propSelection) {
                      stBTRCoreUUIDList lstBTRCoreUUIDList;
                      unsigned char i = 0;
                      BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&lstBTRCoreUUIDList);
                      if (lstBTRCoreUUIDList.numberOfUUID) {
                          fprintf(stderr, "\n\nObtained 'Flag' indices are based on the below mapping..\n"
                                          "0  - read\n"
                                          "1  - write\n"
                                          "2  - encrypt-read\n"
                                          "3  - encrypt-write\n"
                                          "4  - encrypt-authenticated-read\n"
                                          "5  - encrypt-authenticated-write\n"
                                          "6  - secure-read (Server only)\n"
                                          "7  - secure-write (Server only)\n"
                                          "8  - notify\n"
                                          "9  - indicate\n"
                                          "10 - broadcast\n"
                                          "11 - write-without-response\n"
                                          "12 - authenticated-signed-writes\n"
                                          "13 - reliable-write\n"
                                          "14 - writable-auxiliaries\n");
                          fprintf(stderr, "\n\t%-40s Flag\n", "UUID List");
                          for (; i<lstBTRCoreUUIDList.numberOfUUID; i++) {
                              fprintf(stderr, "\n\t%-40s ", lstBTRCoreUUIDList.uuidList[i].uuid);
                              getBitsToString(lstBTRCoreUUIDList.uuidList[i].flags);
                          }
                      } else {
                          fprintf (stderr, "\n\n\tNo UUIDs Found...\n");
                      }
                   } else
                   if (propSelection == 1 || propSelection == 5) {
                      unsigned char val = 0;
                      BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                      fprintf(stderr, "\n\tResult : %d\n", val);
                   } else
                   if (propSelection == 4 || propSelection == 2 ||
                       propSelection == 3 || propSelection == 7 ){
                      char val[BTRCORE_MAX_STR_LEN] = "\0";
                      BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                      fprintf(stderr, "\n\tResult : %s\n", val);
                   } else
                   if (propSelection == 6) {
                      char val[15][64]; int i=0;
                      memset (val, 0, sizeof(val));
                      BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                      fprintf(stderr, "\n\tResult :\n");
                      for (; i < 15 && val[i][0]; i++) {
                          fprintf(stderr, "\t- %s\n", val[i]);
                      }
                   }  
                   free(leUuidString);
                   leUuidString = NULL;
                }
            }
            break;
        case 42:
           {
                fprintf(stderr, "%d\t: %s - Pick a connected LE Device to Perform Operation.\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                devnum = getChoice();
                char *leUuidString = getLeUuidString(); 

                fprintf(stderr, "%d\t: %s - Pick a option to perform method operation LE.\n", __LINE__, __FUNCTION__);
                fprintf(stderr, "\t[0 - ReadValue | 1 - WriteValue | 2 - StartNotify | 3 - StopNotify]\n");

                enBTRCoreLeOp aenBTRCoreLeOp = getChoice();

                if (leUuidString) {
                   char val[BTRCORE_MAX_STR_LEN] = "\0";
                   BTRCore_PerformLEOp (lhBTRCore, devnum, leUuidString, aenBTRCoreLeOp, (void*)&val);
                   free(leUuidString);
                   leUuidString = NULL; 
                   if (aenBTRCoreLeOp == 0) {
                      fprintf(stderr, "%d\t: %s - Obtained Value [%s]\n", __LINE__, __FUNCTION__, val  );
                   }
                }
            }
            break;
        case 43:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Connect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreUnknown);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                fprintf(stderr, "%d\t: %s - device connect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;
        case 44:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                fprintf(stderr, "%d\t: %s - Pick a Device to Disconnect...\n", __LINE__, __FUNCTION__);
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreUnknown);
                fprintf(stderr, "%d\t: %s - device disconnect process completed.\n", __LINE__, __FUNCTION__);
            }
            break;

        case 99: 
            fprintf(stderr, "%d\t: %s - Quitting program!\n", __LINE__, __FUNCTION__);
            BTRCore_DeInit(lhBTRCore);
            exit(0);
            break;
        default: 
            fprintf(stderr, "%d\t: %s - Available options are:\n", __LINE__, __FUNCTION__);
            printMenu();
            break;
        }
    } while (1);


    if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo)
        free(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);

    return 0;
}

