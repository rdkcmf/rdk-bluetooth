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
/*
 * btrCore_avMedia.c
 * Implementation of Audio Video & Media finctionalities of Bluetooth
 */

/* System Headers */
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

#include <glib.h>

/* External Library Headers */
//TODO: Remove all direct references to bluetooth headers
#if defined(USE_BLUEZ5)
#include <bluetooth/bluetooth.h>
#endif
#include <bluetooth/uuid.h>
#include <bluetooth/audio/a2dp-codecs.h>
#if defined(USE_BLUEZ4)
#include <bluetooth/audio/ipc.h>
#endif

/* Interface lib Headers */
#include "btrCore_logger.h"

/* Local Headers */
#include "btrCore_avMedia.h"

#include "btrCore_bt_ifce.h"


#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


#if defined(USE_BLUEZ4)
#define BTR_A2DP_CHANNEL_MODE_MONO          BT_A2DP_CHANNEL_MODE_MONO
#define BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL  BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL
#define BTR_A2DP_CHANNEL_MODE_STEREO        BT_A2DP_CHANNEL_MODE_STEREO
#define BTR_A2DP_CHANNEL_MODE_JOINT_STEREO  BT_A2DP_CHANNEL_MODE_JOINT_STEREO

#define BTR_SBC_SAMPLING_FREQ_16000         BT_SBC_SAMPLING_FREQ_16000
#define BTR_SBC_SAMPLING_FREQ_32000         BT_SBC_SAMPLING_FREQ_32000
#define BTR_SBC_SAMPLING_FREQ_44100         BT_SBC_SAMPLING_FREQ_44100
#define BTR_SBC_SAMPLING_FREQ_48000         BT_SBC_SAMPLING_FREQ_48000

#define BTR_A2DP_ALLOCATION_SNR             BT_A2DP_ALLOCATION_SNR
#define BTR_A2DP_ALLOCATION_LOUDNESS        BT_A2DP_ALLOCATION_LOUDNESS

#define BTR_A2DP_SUBBANDS_4                 BT_A2DP_SUBBANDS_4
#define BTR_A2DP_SUBBANDS_8                 BT_A2DP_SUBBANDS_8

#define BTR_A2DP_BLOCK_LENGTH_4             BT_A2DP_BLOCK_LENGTH_4
#define BTR_A2DP_BLOCK_LENGTH_8             BT_A2DP_BLOCK_LENGTH_8
#define BTR_A2DP_BLOCK_LENGTH_12            BT_A2DP_BLOCK_LENGTH_12
#define BTR_A2DP_BLOCK_LENGTH_16            BT_A2DP_BLOCK_LENGTH_16
#elif defined(USE_BLUEZ5)
#define BTR_A2DP_CHANNEL_MODE_MONO          SBC_CHANNEL_MODE_MONO
#define BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL  SBC_CHANNEL_MODE_DUAL_CHANNEL
#define BTR_A2DP_CHANNEL_MODE_STEREO        SBC_CHANNEL_MODE_STEREO
#define BTR_A2DP_CHANNEL_MODE_JOINT_STEREO  SBC_CHANNEL_MODE_JOINT_STEREO

#define BTR_SBC_SAMPLING_FREQ_16000         SBC_SAMPLING_FREQ_16000
#define BTR_SBC_SAMPLING_FREQ_32000         SBC_SAMPLING_FREQ_32000
#define BTR_SBC_SAMPLING_FREQ_44100         SBC_SAMPLING_FREQ_44100
#define BTR_SBC_SAMPLING_FREQ_48000         SBC_SAMPLING_FREQ_48000

#define BTR_A2DP_ALLOCATION_SNR             SBC_ALLOCATION_SNR
#define BTR_A2DP_ALLOCATION_LOUDNESS        SBC_ALLOCATION_LOUDNESS

#define BTR_A2DP_SUBBANDS_4                 SBC_SUBBANDS_4
#define BTR_A2DP_SUBBANDS_8                 SBC_SUBBANDS_8

#define BTR_A2DP_BLOCK_LENGTH_4             SBC_BLOCK_LENGTH_4
#define BTR_A2DP_BLOCK_LENGTH_8             SBC_BLOCK_LENGTH_8
#define BTR_A2DP_BLOCK_LENGTH_12            SBC_BLOCK_LENGTH_12
#define BTR_A2DP_BLOCK_LENGTH_16            SBC_BLOCK_LENGTH_16
#endif

#define BTR_SBC_HIGH_BITRATE_BITPOOL	    51
#define BTR_SBC_MED_BITRATE_BITPOOL			33
#define BTR_SBC_LOW_BITRATE_BITPOOL			19

#define BTR_SBC_DEFAULT_BITRATE_BITPOOL		BTR_SBC_HIGH_BITRATE_BITPOOL

typedef enum _enBTRCoreAVMTransportPathState {
    enAVMTransportStConnected,
    enAVMTransportStTransition,
    enAVMTransportStDisconnected
} enBTRCoreAVMTransportPathState;


typedef struct _stBTRCoreAVMediaHdl {
    eBTRCoreAVMType                     eAVMediaType;
    a2dp_sbc_t*                         pstBTMediaConfig;
    int                                 iBTMediaDefSampFreqPref;
    char*                               pcAVMediaTransportPath;
    char*                               pcAVMediaPlayerPath;
    //FileSystem Path
    //NowPlaying Path
    //MediaItem  List []

    enBTRCoreAVMTransportPathState      eAVMTState;

    fPtr_BTRCore_AVMediaStatusUpdateCb  fpcBBTRCoreAVMediaStatusUpdate;

    void*                               pcBMediaStatusUserData;

    GThread*                            pMediaPollingThread;
    BOOLEAN                             mediaPollingThreadExit;
    GMutex                              mediaPollingThreadExitMutex;
    void*                               pvThreadData;
} stBTRCoreAVMediaHdl;



typedef struct _stBTRCoreAVMediaStatusUserData {
    void*        apvAVMUserData;
    const char*  apcAVMDevAddress;
} stBTRCoreAVMediaStatusUserData;

/* Static Function Prototypes */
static uint8_t btrCore_AVMedia_GetA2DPDefaultBitpool (uint8_t au8SamplingFreq, uint8_t au8AudioChannelsMode);

/* Local Op Threads Prototypes */
static void* btrCore_AVMedia_PlaybackPositionPolling (void* arg);

/* Incoming Callbacks Prototypes */
static int btrCore_AVMedia_NegotiateMediaCb (void* apBtMediaCapsInput, void** appBtMediaCapsOutput, void* apUserData);
static int btrCore_AVMedia_TransportPathCb (const char* apBtMediaTransportPath, void* apBtMediaCaps, void* apUserData);
static int btrCore_AVMedia_MediaPlayerPathCb (const char* apcBTMediaPlayerPath, void* apUserData);
static int btrCore_AVMedia_MediaStatusUpdateCb (enBTDeviceType aeBtDeviceType, stBTMediaStatusUpdate* apstBtMediaStUpdate, const char* apcBtDevAddr, void* apUserData);


/* Static Function Definition */
#if 0 		// if zerod for reference
static uint8_t 
btrCore_AVMedia_GetA2DPDefaultBitpool (
    uint8_t au8SamplingFreq, 
    uint8_t au8AudioChannelsMode
) {
    switch (au8SamplingFreq) {
    case BTR_SBC_SAMPLING_FREQ_16000:
    case BTR_SBC_SAMPLING_FREQ_32000:
        return 53;

    case BTR_SBC_SAMPLING_FREQ_44100:
        switch (au8AudioChannelsMode) {
        case BTR_A2DP_CHANNEL_MODE_MONO:
        case BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 31;

        case BTR_A2DP_CHANNEL_MODE_STEREO:
        case BTR_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return 53;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return 53;
        }
    case BTR_SBC_SAMPLING_FREQ_48000:
        switch (au8AudioChannelsMode) {
        case BTR_A2DP_CHANNEL_MODE_MONO:
        case BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 29;

        case BTR_A2DP_CHANNEL_MODE_STEREO:
        case BTR_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return 51;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return 51;
        }
    default:
        BTRCORELOG_ERROR ("Invalid Bluetooth SBC sampling freq %u\n", au8SamplingFreq);
        return 53;
    }
}
#else
static uint8_t 
btrCore_AVMedia_GetA2DPDefaultBitpool (
    uint8_t au8SamplingFreq, 
    uint8_t au8AudioChannelsMode
) {
    switch (au8SamplingFreq) {
    case BTR_SBC_SAMPLING_FREQ_16000:
    case BTR_SBC_SAMPLING_FREQ_32000:
        return BTR_SBC_DEFAULT_BITRATE_BITPOOL;

    case BTR_SBC_SAMPLING_FREQ_44100:
        switch (au8AudioChannelsMode) {
        case BTR_A2DP_CHANNEL_MODE_MONO:
        case BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 31;

        case BTR_A2DP_CHANNEL_MODE_STEREO:
        case BTR_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
        }
    case BTR_SBC_SAMPLING_FREQ_48000:
        switch (au8AudioChannelsMode) {
        case BTR_A2DP_CHANNEL_MODE_MONO:
        case BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 29;

        case BTR_A2DP_CHANNEL_MODE_STEREO:
        case BTR_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
        }
    default:
        BTRCORELOG_ERROR ("Invalid Bluetooth SBC sampling freq %u\n", au8SamplingFreq);
        return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
    }
}
#endif


/* Local Op Threads */
static void*
btrCore_AVMedia_PlaybackPositionPolling (
     void*    arg
) {

    if (NULL == arg) {
        BTRCORELOG_ERROR ("Exiting.. enBTRCoreInvalidArg!!!\n");
        return NULL;
    }

    BTRCORELOG_INFO ("Started AVMedia Position Polling thread successfully...");

    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            positionRet     = 0;
    enBTRCoreRet            statusRet       = 0;
    enBTRCoreRet            trackRet        = 0;
    char                    mediaTitle[BTRCORE_MAX_STR_LEN] = "\0";
    char                    lpcAVMediaPlayerPath[BTRCORE_MAX_STR_LEN] = "\0";
    unsigned char           isPlaying       = 0;
    unsigned char           isTrackChanged  = 0;
    void*                   apBtConn        = 0;

    stBTRCoreAVMediaStatusUpdate    mediaStatus;
    stBTRCoreAVMediaTrackInfo       mediaTrackInfo;
    unsigned int                    mediaPosition = 0;
    char*                           mediaState    = 0;

    stBTRCoreAVMediaStatusUserData* pstAVMediaStUserData = NULL;
    BOOLEAN  threadExit = FALSE;

    pstlhBTRCoreAVM      = (stBTRCoreAVMediaHdl*)arg;
    pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData;

    if (!pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate || !pstlhBTRCoreAVM->pvThreadData) {
        BTRCORELOG_ERROR("Exiting.. Invalid stBTRCoreAVMediaHdl Data!!! | pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate : %p | pstlhBTRCoreAVM->pvThreadData : %p", pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate, pstlhBTRCoreAVM->pvThreadData);
        return NULL;
    }

    pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData; 

    if (!pstAVMediaStUserData->apvAVMUserData || !pstAVMediaStUserData->apcAVMDevAddress) {
        BTRCORELOG_ERROR("Exiting.. Invalid stBTRCoreAVMediaStatusUserData Data!!! | pstAVMediaStUserData->apvAVMUserData : %p | pstAVMediaStUserData->apcAVMDevAddress : %s" , pstAVMediaStUserData->apvAVMUserData, pstAVMediaStUserData->apcAVMDevAddress);
        return NULL;
    }

    apBtConn  = pstAVMediaStUserData->apvAVMUserData;

    while (1) {

        g_mutex_lock(&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);
        threadExit = pstlhBTRCoreAVM->mediaPollingThreadExit;
        g_mutex_unlock(&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);

        if (threadExit)
            break;

        if (pstlhBTRCoreAVM->pcAVMediaTransportPath && pstlhBTRCoreAVM->pcAVMediaPlayerPath) {     /* a better way to synchronization has to be deviced */
            statusRet = BtrCore_BTGetMediaPlayerProperty(apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, "Status",   (void*)&mediaState);
        }
        else {
            mediaState = "ended";
        }

        if (statusRet || !mediaState) {
            BTRCORELOG_ERROR ("Failed to get MediaPlayer Property | statusRet : %d !!!\n", statusRet);
            continue;        /* arrive a exit state if req. */
        }

        if (!strcmp("playing", mediaState)) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        } 
        else if (!strcmp("paused", mediaState)) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPaused;
        }
        else if (!strcmp("forward-seek", mediaState)) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        }
        else if (!strcmp("reverse-seek", mediaState)) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        }
        else if (!strcmp("stopped", mediaState)) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStStopped;
            BTRCORELOG_WARN  ("Audio StreamIn Status : %s !!!", mediaState);
        }
        else if (!strcmp("ended", mediaState)) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaPlaybackEnded;
            BTRCORELOG_WARN  ("Audio StreamIn Status : %s !!!", mediaState);
        }
        else if (!strcmp("error", mediaState)) {
            BTRCORELOG_ERROR ("Audio StreamIn Status : %s !!!", mediaState);
        }


        if (isPlaying && pstlhBTRCoreAVM->pcAVMediaTransportPath && pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
            positionRet = BtrCore_BTGetMediaPlayerProperty(apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, "Position", (void*)&mediaPosition);
            trackRet    = BtrCore_BTGetTrackInformation(apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, (stBTMediaTrackInfo*)&mediaTrackInfo);

            if (positionRet || statusRet || trackRet) {
                BTRCORELOG_ERROR ("Failed to get MediaPlayer Property | positionRet : %d | trackRet : %d !!!\n", positionRet, trackRet);
                continue;        /* arrive a exit state if req. */
            }

            mediaStatus.m_mediaPositionInfo.ui32Position = mediaPosition;
            mediaStatus.m_mediaPositionInfo.ui32Duration = mediaTrackInfo.ui32Duration;

            /* can look for a better logic later */ 
            if (strcmp(lpcAVMediaPlayerPath, pstlhBTRCoreAVM->pcAVMediaPlayerPath)) {
                mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStChanged;
                memset(&mediaStatus.m_mediaTrackInfo, '\0', sizeof(stBTRCoreAVMediaTrackInfo));
                memcpy(&mediaStatus.m_mediaTrackInfo, &mediaTrackInfo, sizeof(stBTRCoreAVMediaTrackInfo));
                strncpy(lpcAVMediaPlayerPath, pstlhBTRCoreAVM->pcAVMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1);
                isTrackChanged = 1;
            }
            else if (strcmp(mediaTitle, mediaTrackInfo.pcTitle)) {
                if (!isTrackChanged) {
                    mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStChanged;
                    memset(&mediaStatus.m_mediaTrackInfo, '\0', sizeof(stBTRCoreAVMediaTrackInfo));
                    memcpy(&mediaStatus.m_mediaTrackInfo, &mediaTrackInfo, sizeof(stBTRCoreAVMediaTrackInfo));
                    isTrackChanged = 1;
                }
                else {
                    mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStStarted;
                    strncpy(mediaTitle, mediaTrackInfo.pcTitle, BTRCORE_MAX_STR_LEN - 1);
                    isTrackChanged = 0;
                }
            }

            /* post callback */
            pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate(&mediaStatus,
                                                            pstAVMediaStUserData->apcAVMDevAddress,
                                                            pstlhBTRCoreAVM->pcBMediaStatusUserData);
        }


        if (eBTRCoreAVMediaTrkStStarted == mediaStatus.eAVMediaState || eBTRCoreAVMediaTrkStPlaying == mediaStatus.eAVMediaState) {
            sleep(1);           /* polling playback position with 1 sec interval */
        }
        else {
            isPlaying = 0;
            usleep(100000);     /* sleeping 1/10th of a second to check playback status */
        }    
    }

    BTRCORELOG_INFO ("Exiting MediaPosition Polling Thread...");

    free ((void*)pstAVMediaStUserData->apcAVMDevAddress);
    free ((void*)pstAVMediaStUserData);

    pstlhBTRCoreAVM->pMediaPollingThread = NULL;

    return NULL;
}


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_AVMedia_Init (
    tBTRCoreAVMediaHdl* phBTRCoreAVM,
    void*               apBtConn,
    const char*         apBtAdapter
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     lBtAVMediaDelayReport   =  1;
    int                     lBtAVMediaASinkRegRet   = -1;
    int                     lBtAVMediaASrcRegRet    = -1;
    int                     lBtAVMediaNegotiateRet  = -1;
    int                     lBtAVMediaTransportPRet = -1;
    int                     lBTAVMediaPlayerPRet    = -1;
    int                     lBTAVMediaStatusRet     = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;

    if (!phBTRCoreAVM || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }


    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)malloc(sizeof(stBTRCoreAVMediaHdl));
    if (!pstlhBTRCoreAVM)
        return enBTRCoreInitFailure;

    memset(pstlhBTRCoreAVM, 0, sizeof(stBTRCoreAVMediaHdl));

    a2dp_sbc_t lstBtA2dpCapabilities;

    lstBtA2dpCapabilities.channel_mode       = BTR_A2DP_CHANNEL_MODE_MONO | BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL |
                                               BTR_A2DP_CHANNEL_MODE_STEREO | BTR_A2DP_CHANNEL_MODE_JOINT_STEREO;
#if 0
    lstBtA2dpCapabilities.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                               BTR_SBC_SAMPLING_FREQ_44100 | BTR_SBC_SAMPLING_FREQ_48000;
#else
    //TODO: Enable 44100 for A2DP Source
    lstBtA2dpCapabilities.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                               BTR_SBC_SAMPLING_FREQ_48000;
#endif
    lstBtA2dpCapabilities.allocation_method  = BTR_A2DP_ALLOCATION_SNR | BTR_A2DP_ALLOCATION_LOUDNESS;
    lstBtA2dpCapabilities.subbands           = BTR_A2DP_SUBBANDS_4 | BTR_A2DP_SUBBANDS_8;
    lstBtA2dpCapabilities.block_length       = BTR_A2DP_BLOCK_LENGTH_4 | BTR_A2DP_BLOCK_LENGTH_8 |
                                               BTR_A2DP_BLOCK_LENGTH_12 | BTR_A2DP_BLOCK_LENGTH_16;
    lstBtA2dpCapabilities.min_bitpool        = MIN_BITPOOL;
    lstBtA2dpCapabilities.max_bitpool        = MAX_BITPOOL;

    pstlhBTRCoreAVM->pstBTMediaConfig = (a2dp_sbc_t*)malloc(sizeof(a2dp_sbc_t));
    if (!pstlhBTRCoreAVM->pstBTMediaConfig) {
        free(pstlhBTRCoreAVM);
        pstlhBTRCoreAVM = NULL;
        return enBTRCoreInitFailure;
    }

    pstlhBTRCoreAVM->eAVMediaType = eBTRCoreAVMTypeSBC;
    memcpy(pstlhBTRCoreAVM->pstBTMediaConfig, &lstBtA2dpCapabilities, sizeof(a2dp_sbc_t));
    pstlhBTRCoreAVM->iBTMediaDefSampFreqPref = BTR_SBC_SAMPLING_FREQ_48000;
    pstlhBTRCoreAVM->pcAVMediaTransportPath  = NULL;
    pstlhBTRCoreAVM->pcAVMediaPlayerPath     = NULL;
    pstlhBTRCoreAVM->pvThreadData            = NULL;
    pstlhBTRCoreAVM->pcBMediaStatusUserData  = NULL;
    pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate  = NULL;
    pstlhBTRCoreAVM->eAVMTState              = enAVMTransportStDisconnected;

    lBtAVMediaASinkRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    enBTDevAudioSink,
                                                    A2DP_SOURCE_UUID,
                                                    A2DP_CODEC_SBC,
                                                    (void*)&lstBtA2dpCapabilities,
                                                    sizeof(lstBtA2dpCapabilities),
                                                    lBtAVMediaDelayReport);

#if 1
    lstBtA2dpCapabilities.frequency |= BTR_SBC_SAMPLING_FREQ_44100;
#endif

    lBtAVMediaASrcRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                   apBtAdapter,
                                                   enBTDevAudioSource,
                                                   A2DP_SINK_UUID,
                                                   A2DP_CODEC_SBC,
                                                   (void*)&lstBtA2dpCapabilities,
                                                   sizeof(lstBtA2dpCapabilities),
                                                   lBtAVMediaDelayReport);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet)
       lBtAVMediaNegotiateRet = BtrCore_BTRegisterNegotiateMediaCb(apBtConn,
                                                                   apBtAdapter,
                                                                   &btrCore_AVMedia_NegotiateMediaCb,
                                                                   pstlhBTRCoreAVM);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet)
        lBtAVMediaTransportPRet = BtrCore_BTRegisterTransportPathMediaCb(apBtConn,
                                                                         apBtAdapter,
                                                                         &btrCore_AVMedia_TransportPathCb,
                                                                         pstlhBTRCoreAVM);
   
    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet && !lBtAVMediaTransportPRet)
        lBTAVMediaPlayerPRet = BtrCore_BTRegisterMediaPlayerPathCb(apBtConn,
                                                                   apBtAdapter,
                                                                   &btrCore_AVMedia_MediaPlayerPathCb,
                                                                   pstlhBTRCoreAVM);
                                       
    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet && !lBtAVMediaTransportPRet && !lBTAVMediaPlayerPRet)
        lBTAVMediaStatusRet = BtrCore_BTRegisterMediaStatusUpdateCb(apBtConn,
                                                                    &btrCore_AVMedia_MediaStatusUpdateCb,
                                                                    pstlhBTRCoreAVM);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet && !lBtAVMediaTransportPRet && !lBTAVMediaPlayerPRet && !lBTAVMediaStatusRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    if (lenBTRCoreRet != enBTRCoreSuccess) {
        free(pstlhBTRCoreAVM);
        pstlhBTRCoreAVM = NULL;
    }

    *phBTRCoreAVM  = (tBTRCoreAVMediaHdl)pstlhBTRCoreAVM;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_DeInit (
    tBTRCoreAVMediaHdl  hBTRCoreAVM,
    void*               apBtConn,
    const char*         apBtAdapter
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     lBtAVMediaASinkUnRegRet   = -1;
    int                     lBtAVMediaASrcUnRegRet    = -1;
    enBTRCoreRet            lenBTRCoreRet  = enBTRCoreFailure;

    if (!hBTRCoreAVM || !apBtConn || !apBtAdapter) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    lBtAVMediaASrcUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                       apBtAdapter,
                                                       enBTDevAudioSource);

    lBtAVMediaASinkUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                        apBtAdapter,
                                                        enBTDevAudioSink);

    if (pstlhBTRCoreAVM->pstBTMediaConfig) {
        free(pstlhBTRCoreAVM->pstBTMediaConfig);
        pstlhBTRCoreAVM->pstBTMediaConfig = NULL;
    }

    if (pstlhBTRCoreAVM->pcAVMediaTransportPath) {
        free(pstlhBTRCoreAVM->pcAVMediaTransportPath);
        pstlhBTRCoreAVM->pcAVMediaTransportPath = NULL;
    }

    if (pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        free(pstlhBTRCoreAVM->pcAVMediaPlayerPath);
        pstlhBTRCoreAVM->pcAVMediaPlayerPath = NULL;
    }

    pstlhBTRCoreAVM->pvThreadData            = NULL;
    pstlhBTRCoreAVM->pcBMediaStatusUserData  = NULL;
    pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate  = NULL;
    pstlhBTRCoreAVM->eAVMTState = enAVMTransportStDisconnected;

    if (!lBtAVMediaASrcUnRegRet && !lBtAVMediaASinkUnRegRet)
        lenBTRCoreRet = enBTRCoreSuccess;


    if (hBTRCoreAVM) {
        (void) pstlhBTRCoreAVM;
        free(hBTRCoreAVM);
        hBTRCoreAVM = NULL;
    }

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_GetCurMediaInfo (
    tBTRCoreAVMediaHdl      hBTRCoreAVM,
    void*                   apBtConn,
    const char*             apBtDevAddr,
    stBTRCoreAVMediaInfo*   apstBtrCoreAVMediaInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreFailure;

    if (!hBTRCoreAVM || !apBtConn || !apBtDevAddr || !apstBtrCoreAVMediaInfo) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (apstBtrCoreAVMediaInfo->pstBtrCoreAVMCodecInfo) {

        /* Max 4 sec timeout - Polled at 200ms second interval */
        unsigned int ui32sleepIdx = 20;
        do {
            sched_yield();
            usleep(200000);
        } while ((!pstlhBTRCoreAVM->pcAVMediaTransportPath) && (--ui32sleepIdx));


        //TODO: Get this from the Negotiated Media callback by storing in the handle
        apstBtrCoreAVMediaInfo->eBtrCoreAVMType = eBTRCoreAVMTypeSBC;

        if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypePCM) {
        }
        else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeSBC) {
            stBTRCoreAVMediaSbcInfo* pstBtrCoreAVMediaSbcInfo = (stBTRCoreAVMediaSbcInfo*)(apstBtrCoreAVMediaInfo->pstBtrCoreAVMCodecInfo);

            if (pstlhBTRCoreAVM->pstBTMediaConfig->frequency == BTR_SBC_SAMPLING_FREQ_16000) {
                pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 16000;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->frequency == BTR_SBC_SAMPLING_FREQ_32000) {
                pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 32000;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->frequency == BTR_SBC_SAMPLING_FREQ_44100) {
                pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 44100;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->frequency == BTR_SBC_SAMPLING_FREQ_48000) {
                pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 48000;
            }
            else {
                pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 0;
            }

            if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_MONO) {
                pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanMono;
                pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 1;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL) {
                pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanDualChannel;
                pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_STEREO) {
                pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanStereo;
                pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_JOINT_STEREO) {
                pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanJointStereo;
                pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
            }
            else {
                pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanUnknown;
            }

            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcAllocMethod  = pstlhBTRCoreAVM->pstBTMediaConfig->allocation_method;

            if (pstlhBTRCoreAVM->pstBTMediaConfig->subbands == BTR_A2DP_SUBBANDS_4) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 4;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->subbands == BTR_A2DP_SUBBANDS_8) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 8;
            }
            else {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 0;
            }

            if (pstlhBTRCoreAVM->pstBTMediaConfig->block_length == BTR_A2DP_BLOCK_LENGTH_4) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 4;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->block_length == BTR_A2DP_BLOCK_LENGTH_8) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 8;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->block_length == BTR_A2DP_BLOCK_LENGTH_12) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 12;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->block_length == BTR_A2DP_BLOCK_LENGTH_16) {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 16;
            }
            else {
                pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 0;
            }

            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMinBitpool   = pstlhBTRCoreAVM->pstBTMediaConfig->min_bitpool;
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool   = pstlhBTRCoreAVM->pstBTMediaConfig->max_bitpool;      

            if ((pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_MONO) ||
                (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL)) {
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 4 + ((4 * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan) / 8) +
                                                              ((pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool) / 8);
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_STEREO) {
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 4 + (4 * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan) / 8 +
                                                                (pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool) / 8;
            }
            else if (pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode == BTR_A2DP_CHANNEL_MODE_JOINT_STEREO) {
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 4 + ((4 * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan) / 8) +
                                                                (pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands + (pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool)) / 8;
            }
            else {
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 0;
            }

            if ((pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength != 0) && (pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands != 0))
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcBitrate  = ((8.0 * pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen * (float)pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq/1000) / pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands) / pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength;
            else
                pstBtrCoreAVMediaSbcInfo->ui16AVMSbcBitrate  = 0;

            BTRCORELOG_TRACE ("ui32AVMSFreq          = %d\n", pstBtrCoreAVMediaSbcInfo-> ui32AVMSFreq);
            BTRCORELOG_TRACE ("ui32AVMAChan          = %d\n", pstBtrCoreAVMediaSbcInfo-> ui32AVMAChan);
            BTRCORELOG_TRACE ("ui8AVMSbcAllocMethod  = %d\n", pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcAllocMethod);
            BTRCORELOG_TRACE ("ui8AVMSbcSubbands     = %d\n", pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcSubbands);
            BTRCORELOG_TRACE ("ui8AVMSbcBlockLength  = %d\n", pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcBlockLength);
            BTRCORELOG_TRACE ("ui8AVMSbcMinBitpool   = %d\n", pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcMinBitpool);
            BTRCORELOG_TRACE ("ui8AVMSbcMaxBitpool   = %d\n", pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcMaxBitpool);
            BTRCORELOG_TRACE ("ui16AVMSbcFrameLen    = %d\n", pstBtrCoreAVMediaSbcInfo-> ui16AVMSbcFrameLen);
            BTRCORELOG_DEBUG ("ui16AVMSbcBitrate     = %d\n", pstBtrCoreAVMediaSbcInfo-> ui16AVMSbcBitrate);
                             
        }
        else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeMPEG) {
        }
        else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeAAC) {
        }
    }

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_AcquireDataPath (
    tBTRCoreAVMediaHdl  hBTRCoreAVM,
    void*               apBtConn,
    const char*         apBtDevAddr,
    int*                apDataPath,
    int*                apDataReadMTU,
    int*                apDataWriteMTU
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     lBtAVMediaRet = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    unBTOpIfceProp          lunBtOpMedTProp;
    unsigned int            ui16Delay = 0xFFFFu;

    if (!hBTRCoreAVM || !apBtConn || !apBtDevAddr) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;
    (void) pstlhBTRCoreAVM;

    if (pstlhBTRCoreAVM->pcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }

    if (!(lBtAVMediaRet = BtrCore_BTAcquireDevDataPath (apBtConn, pstlhBTRCoreAVM->pcAVMediaTransportPath, apDataPath, apDataReadMTU, apDataWriteMTU)))
        lenBTRCoreRet = enBTRCoreSuccess;

    lunBtOpMedTProp.enBtMediaTransportProp = enBTMedTPropDelay;
    if ((lBtAVMediaRet = BtrCore_BTGetProp(apBtConn, pstlhBTRCoreAVM->pcAVMediaTransportPath, enBTMediaTransport, lunBtOpMedTProp, &ui16Delay)))
        lenBTRCoreRet = enBTRCoreFailure;

    BTRCORELOG_INFO ("BTRCore_AVMedia_AcquireDataPath: Delay value = %d\n", ui16Delay);

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_ReleaseDataPath (
    tBTRCoreAVMediaHdl  hBTRCoreAVM,
    void*               apBtConn,
    const char*         apBtDevAddr
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     lBtAVMediaRet = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;

    if (!hBTRCoreAVM || !apBtConn || !apBtDevAddr) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (pstlhBTRCoreAVM->pcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }

    if (!(lBtAVMediaRet = BtrCore_BTReleaseDevDataPath(apBtConn, pstlhBTRCoreAVM->pcAVMediaTransportPath)))
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_MediaControl (
       tBTRCoreAVMediaHdl   hBTRCoreAVM,
       void*                apBtConn,
       const char*          apBtDevAddr,
       enBTRCoreAVMediaCtrl aenBTRCoreAVMediaCtrl 
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM   = NULL;
    enBTRCoreRet            lenBTRCoreRet     = enBTRCoreSuccess;
    enBTMediaControl        aenBTMediaControl = 0;


    if (!hBTRCoreAVM || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
       //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
       //      Seems to be the root cause of the stack corruption as part of DELIA-25861
       if (!(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevAddr), BTRCORE_MAX_STR_LEN - 1))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    switch (aenBTRCoreAVMediaCtrl) {
    case enBTRCoreAVMediaCtrlPlay:
        aenBTMediaControl = enBTMediaCtrlPlay;
        break;
    case enBTRCoreAVMediaCtrlPause:
        aenBTMediaControl = enBTMediaCtrlPause;
        break;
    case enBTRCoreAVMediaCtrlStop:
        aenBTMediaControl = enBTMediaCtrlStop;
        break;
    case enBTRCoreAVMediaCtrlNext:
        aenBTMediaControl = enBTMediaCtrlNext;
        break;
    case enBTRCoreAVMediaCtrlPrevious:
        aenBTMediaControl = enBTMediaCtrlPrevious;
        break;
    case enBTRCoreAVMediaCtrlFastForward:
        aenBTMediaControl = enBTMediaCtrlFastForward;
        break;
    case enBTRCoreAVMediaCtrlRewind:
        aenBTMediaControl = enBTMediaCtrlRewind;
        break;
    case enBTRCoreAVMediaCtrlVolumeUp:
        aenBTMediaControl = enBTMediaCtrlVolumeUp;
        break;
    case enBTRCoreAVMediaCtrlVolumeDown:
        aenBTMediaControl = enBTMediaCtrlVolumeDown;
        break;
    default:
        break;
    }

    if (BtrCore_BTDevMediaControl (apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, aenBTMediaControl)) {
       BTRCORELOG_ERROR ("Failed to set the Media control option");
       lenBTRCoreRet = enBTRCoreFailure;
    }
      
    return lenBTRCoreRet;
}

//Combine TrackInfo, PositionInfo and basic info in GetMediaProperty handling with enums and switch?
enBTRCoreRet
BTRCore_AVMedia_GetTrackInfo (
       tBTRCoreAVMediaHdl         hBTRCoreAVM,
       void*                      apBtConn,
       const char*                apBtDevAddr,
       stBTRCoreAVMediaTrackInfo* apstBTAVMediaTrackInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
       //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
       //      Seems to be the root cause of the stack corruption as part of DELIA-25861
       if (!(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevAddr), BTRCORE_MAX_STR_LEN - 1))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    if (BtrCore_BTGetTrackInformation (apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, (stBTMediaTrackInfo*)apstBTAVMediaTrackInfo)) {
       BTRCORELOG_ERROR ("Failed to get Track information!!!");
       lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


//Combine TrackInfo, PositionInfo and basic info in GetMediaProperty handling with enums and switch?
enBTRCoreRet
BTRCore_AVMedia_GetPositionInfo (
       tBTRCoreAVMediaHdl            hBTRCoreAVM,
       void*                         apBtConn,
       const char*                   apBtDevAddr,
       stBTRCoreAVMediaPositionInfo* apstBTAVMediaPositionInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
       //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
       //      Seems to be the root cause of the stack corruption as part of DELIA-25861
       if (!(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevAddr), BTRCORE_MAX_STR_LEN - 1))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    stBTRCoreAVMediaTrackInfo   mediaTrackInfo;
    unsigned int  mediaPosition = 0;
    enBTRCoreRet  positionRet, trackRet;

    //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
    //      Seems to be the root cause of the stack corruption as part of DELIA-25861. This is call seems to be root cause of DELIA-25861
    positionRet = BtrCore_BTGetMediaPlayerProperty(apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, "Position", (void*)&mediaPosition);
    trackRet    = BtrCore_BTGetTrackInformation   (apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, (stBTMediaTrackInfo*)&mediaTrackInfo);

    if (positionRet || trackRet) {
       BTRCORELOG_ERROR ("Failed to get media info!!!");
       lenBTRCoreRet = enBTRCoreFailure;
    }
    else {
       apstBTAVMediaPositionInfo->ui32Duration = mediaTrackInfo.ui32Duration;
       apstBTAVMediaPositionInfo->ui32Position = mediaPosition;
    }

    return lenBTRCoreRet;
}


//Combine TrackInfo, PositionInfo and basic info in GetMediaProperty handling with enums and switch?
enBTRCoreRet
BTRCore_AVMedia_GetMediaProperty (
    tBTRCoreAVMediaHdl      hBTRCoreAVM,
    void*                   apBtConn,
    const char*             apBtDevAddr,
    const char*             mediaPropertyKey,
    void*                   mediaPropertyValue
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
       //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
       //      Seems to be the root cause of the stack corruption as part of DELIA-25861.
       if (!(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevAddr), BTRCORE_MAX_STR_LEN - 1))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    if (BtrCore_BTGetMediaPlayerProperty(apBtConn, pstlhBTRCoreAVM->pcAVMediaPlayerPath, mediaPropertyKey, mediaPropertyValue)) {
       BTRCORELOG_ERROR ("Failed to get Media Property : %s!!!",mediaPropertyKey);
       lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_StartMediaPositionPolling (
        tBTRCoreAVMediaHdl   hBTRCoreAVM,
        void*                apBtConn,
        const char*          apBtDevPath,
        const char*          apBtDevAddr
) {
    stBTRCoreAVMediaHdl* pstlhBTRCoreAVM = NULL;
    enBTRCoreRet   lenBTRCoreRet = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtDevAddr || !apBtConn)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM     =  (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
       //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
       //      Seems to be the root cause of the stack corruption as part of DELIA-25861.
       if (!(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(BtrCore_BTGetMediaPlayerPath (apBtConn, apBtDevPath), BTRCORE_MAX_STR_LEN - 1))) {
          BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
          return enBTRCoreFailure;
       }
    }

    if (!pstlhBTRCoreAVM->pMediaPollingThread) {
       stBTRCoreAVMediaStatusUserData* pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*) malloc (sizeof(stBTRCoreAVMediaStatusUserData));

       pstAVMediaStUserData->apvAVMUserData   = apBtConn;
       pstAVMediaStUserData->apcAVMDevAddress = strndup(apBtDevAddr, BTRCORE_MAX_STR_LEN - 1);

       pstlhBTRCoreAVM->pvThreadData = (void*)pstAVMediaStUserData;

       pstlhBTRCoreAVM->mediaPollingThreadExit = FALSE;
       g_mutex_init(&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);

       pstlhBTRCoreAVM->pMediaPollingThread = g_thread_new ("btrCore_AVMedia_PlaybackPositionPolling", btrCore_AVMedia_PlaybackPositionPolling, (void*)(pstlhBTRCoreAVM));

       if (!pstlhBTRCoreAVM->pMediaPollingThread) {
          BTRCORELOG_ERROR ("Failed to thread create btrCore_AVMedia_PlaybackPositionPolling");
          lenBTRCoreRet = enBTRCoreFailure;
       }
    } 
    else {
        BTRCORELOG_WARN ("btrCore_AVMedia_PlaybackPositionPolling thread is running already!!!");
    }
    
    return lenBTRCoreRet;    
}



enBTRCoreRet
BTRCore_AVMedia_ExitMediaPositionPolling (
    tBTRCoreAVMediaHdl      hBTRCoreAVM
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM )  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (pstlhBTRCoreAVM->pMediaPollingThread) {

        g_mutex_lock(&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);
        pstlhBTRCoreAVM->mediaPollingThreadExit = TRUE;   /* Exit playback position polling thread */
        g_mutex_unlock(&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);

        g_thread_join (pstlhBTRCoreAVM->pMediaPollingThread);
        g_mutex_clear (&pstlhBTRCoreAVM->mediaPollingThreadExitMutex);

        BTRCORELOG_INFO ("Successfully Exited Media Position Polling Thread");
    }
    else {
        BTRCORELOG_ERROR ("pstlhBTRCoreAVM->pMediaPollingThread doesn't exists!!!");
        lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


// Outgoing callbacks Registration Interfaces
enBTRCoreRet
BTRCore_AVMedia_RegisterMediaStatusUpdateCb (
    tBTRCoreAVMediaHdl                  hBTRCoreAVM,
    fPtr_BTRCore_AVMediaStatusUpdateCb  afpcBBTRCoreAVMediaStatusUpdate,
    void*                               apvBMediaStatusUserData
) {
    stBTRCoreAVMediaHdl*  pstlhBTRCoreAVM = NULL;

    if (!hBTRCoreAVM || !afpcBBTRCoreAVMediaStatusUpdate)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM  = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;
    
    pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate = afpcBBTRCoreAVMediaStatusUpdate;
    pstlhBTRCoreAVM->pcBMediaStatusUserData         = apvBMediaStatusUserData;

    return enBTRCoreSuccess;
}


/* Incoming Callbacks */
static int
btrCore_AVMedia_NegotiateMediaCb (
    void*   apBtMediaCapsInput,
    void**  appBtMediaCapsOutput,
    void*   apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    a2dp_sbc_t*             apBtMediaSBCCaps = NULL;
    a2dp_sbc_t              lstBTMediaSBCConfig;

    if (!apBtMediaCapsInput) {
        BTRCORELOG_ERROR ("Invalid input MT Media Capabilities\n");
        return -1;
    } 

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCapsInput;

    memset(&lstBTMediaSBCConfig, 0, sizeof(a2dp_sbc_t));
    lstBTMediaSBCConfig.frequency = pstlhBTRCoreAVM->iBTMediaDefSampFreqPref;

    if (apBtMediaSBCCaps->channel_mode & BTR_A2DP_CHANNEL_MODE_JOINT_STEREO) {
        lstBTMediaSBCConfig.channel_mode = BTR_A2DP_CHANNEL_MODE_JOINT_STEREO;
    }
    else if (apBtMediaSBCCaps->channel_mode & BTR_A2DP_CHANNEL_MODE_STEREO) {
        lstBTMediaSBCConfig.channel_mode = BTR_A2DP_CHANNEL_MODE_STEREO;
    }
    else if (apBtMediaSBCCaps->channel_mode & BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL) {
        lstBTMediaSBCConfig.channel_mode = BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL;
    }
    else if (apBtMediaSBCCaps->channel_mode & BTR_A2DP_CHANNEL_MODE_MONO) {
        lstBTMediaSBCConfig.channel_mode = BTR_A2DP_CHANNEL_MODE_MONO;
    } 
    else {
        BTRCORELOG_ERROR ("No supported channel modes\n");
        return -1;
    }

    if (apBtMediaSBCCaps->block_length & BTR_A2DP_BLOCK_LENGTH_16) {
        lstBTMediaSBCConfig.block_length = BTR_A2DP_BLOCK_LENGTH_16;
    }
    else if (apBtMediaSBCCaps->block_length & BTR_A2DP_BLOCK_LENGTH_12) {
        lstBTMediaSBCConfig.block_length = BTR_A2DP_BLOCK_LENGTH_12;
    }
    else if (apBtMediaSBCCaps->block_length & BTR_A2DP_BLOCK_LENGTH_8) {
        lstBTMediaSBCConfig.block_length = BTR_A2DP_BLOCK_LENGTH_8;
    }
    else if (apBtMediaSBCCaps->block_length & BTR_A2DP_BLOCK_LENGTH_4) {
        lstBTMediaSBCConfig.block_length = BTR_A2DP_BLOCK_LENGTH_4;
    }
    else {
        BTRCORELOG_ERROR ("No supported block lengths\n");
        return -1;
    }

    if (apBtMediaSBCCaps->subbands & BTR_A2DP_SUBBANDS_8) {
        lstBTMediaSBCConfig.subbands = BTR_A2DP_SUBBANDS_8;
    }
    else if (apBtMediaSBCCaps->subbands & BTR_A2DP_SUBBANDS_4) {
        lstBTMediaSBCConfig.subbands = BTR_A2DP_SUBBANDS_4;
    }
    else {
        BTRCORELOG_ERROR ("No supported subbands\n");
        return -1;
    }

    if (apBtMediaSBCCaps->allocation_method & BTR_A2DP_ALLOCATION_LOUDNESS) {
        lstBTMediaSBCConfig.allocation_method = BTR_A2DP_ALLOCATION_LOUDNESS;
    }
    else if (apBtMediaSBCCaps->allocation_method & BTR_A2DP_ALLOCATION_SNR) {
        lstBTMediaSBCConfig.allocation_method = BTR_A2DP_ALLOCATION_SNR;
    }

    lstBTMediaSBCConfig.min_bitpool = (uint8_t) MAX(MIN_BITPOOL, apBtMediaSBCCaps->min_bitpool);
    lstBTMediaSBCConfig.max_bitpool = (uint8_t) MIN(btrCore_AVMedia_GetA2DPDefaultBitpool(lstBTMediaSBCConfig.frequency, 
                                                                                          lstBTMediaSBCConfig.channel_mode),
                                                    apBtMediaSBCCaps->max_bitpool);

    BTRCORELOG_TRACE("Negotiated Configuration\n");
    BTRCORELOG_INFO ("channel_mode       = %d\n", lstBTMediaSBCConfig.channel_mode);
    BTRCORELOG_INFO ("frequency          = %d\n", lstBTMediaSBCConfig.frequency);
    BTRCORELOG_INFO ("allocation_method  = %d\n", lstBTMediaSBCConfig.allocation_method);
    BTRCORELOG_INFO ("subbands           = %d\n", lstBTMediaSBCConfig.subbands);
    BTRCORELOG_INFO ("block_length       = %d\n", lstBTMediaSBCConfig.block_length);
    BTRCORELOG_INFO ("min_bitpool        = %d\n", lstBTMediaSBCConfig.min_bitpool);
    BTRCORELOG_INFO ("max_bitpool        = %d\n", lstBTMediaSBCConfig.max_bitpool);

    if (pstlhBTRCoreAVM) {
        if (pstlhBTRCoreAVM->pstBTMediaConfig) {
            pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode        =  lstBTMediaSBCConfig.channel_mode;
            pstlhBTRCoreAVM->pstBTMediaConfig->frequency           =  lstBTMediaSBCConfig.frequency;
            pstlhBTRCoreAVM->pstBTMediaConfig->allocation_method   =  lstBTMediaSBCConfig.allocation_method;
            pstlhBTRCoreAVM->pstBTMediaConfig->subbands            =  lstBTMediaSBCConfig.subbands;
            pstlhBTRCoreAVM->pstBTMediaConfig->block_length        =  lstBTMediaSBCConfig.block_length;
            pstlhBTRCoreAVM->pstBTMediaConfig->min_bitpool         =  lstBTMediaSBCConfig.min_bitpool;
            pstlhBTRCoreAVM->pstBTMediaConfig->max_bitpool         =  lstBTMediaSBCConfig.max_bitpool;
        }

        *appBtMediaCapsOutput = (void*)(pstlhBTRCoreAVM->pstBTMediaConfig);
    }

    return *appBtMediaCapsOutput ? 0 : -1;
}


static int
btrCore_AVMedia_TransportPathCb (
    const char* apBtMediaTransportPath,
    void*       apBtMediaCaps,
    void*       apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     i32BtRet = -1;

    if (!apBtMediaTransportPath) {
        BTRCORELOG_ERROR ("Invalid transport path\n");
        return -1;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    if (apBtMediaCaps) {
        a2dp_sbc_t* apBtMediaSBCCaps = NULL;
        a2dp_sbc_t  lstBTMediaSBCConfig;

        apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCaps;

        lstBTMediaSBCConfig.channel_mode        =   apBtMediaSBCCaps->channel_mode;
        lstBTMediaSBCConfig.frequency           =   apBtMediaSBCCaps->frequency;
        lstBTMediaSBCConfig.allocation_method   =   apBtMediaSBCCaps->allocation_method;
        lstBTMediaSBCConfig.subbands            =   apBtMediaSBCCaps->subbands;
        lstBTMediaSBCConfig.block_length        =   apBtMediaSBCCaps->block_length;
        lstBTMediaSBCConfig.min_bitpool         =   apBtMediaSBCCaps->min_bitpool;
        lstBTMediaSBCConfig.max_bitpool         =   apBtMediaSBCCaps->max_bitpool;

        BTRCORELOG_TRACE("Set Configuration\n");
        BTRCORELOG_INFO ("channel_mode       = %d\n", lstBTMediaSBCConfig.channel_mode);
        BTRCORELOG_INFO ("frequency          = %d\n", lstBTMediaSBCConfig.frequency);
        BTRCORELOG_INFO ("allocation_method  = %d\n", lstBTMediaSBCConfig.allocation_method);
        BTRCORELOG_INFO ("subbands           = %d\n", lstBTMediaSBCConfig.subbands);
        BTRCORELOG_INFO ("block_length       = %d\n", lstBTMediaSBCConfig.block_length);
        BTRCORELOG_INFO ("min_bitpool        = %d\n", lstBTMediaSBCConfig.min_bitpool);
        BTRCORELOG_INFO ("max_bitpool        = %d\n", lstBTMediaSBCConfig.max_bitpool);

        //TODO: Best possible Generic solution for DELIA-23555 at this moment
        // Async nature of lower layer bt-ifce stack i.e. bluez session call enters an invalid 
        // state as the reference to the session with the device is completely unreffered when we
        // return too quickly from the callback which results in the crash at bluez is an open is still
        // pending at bluez from the device
        // Delaying the return to bluez results in the session being unreffed with delay and the 
        // incoming open from the device on avdtp to be processed as we have not returrned and there
        // is a valid ref in bluez
        sleep(1);

        if (pstlhBTRCoreAVM) {
            if (pstlhBTRCoreAVM->pstBTMediaConfig) {
                pstlhBTRCoreAVM->pstBTMediaConfig->channel_mode        =  lstBTMediaSBCConfig.channel_mode;
                pstlhBTRCoreAVM->pstBTMediaConfig->frequency           =  lstBTMediaSBCConfig.frequency;
                pstlhBTRCoreAVM->pstBTMediaConfig->allocation_method   =  lstBTMediaSBCConfig.allocation_method;
                pstlhBTRCoreAVM->pstBTMediaConfig->subbands            =  lstBTMediaSBCConfig.subbands;
                pstlhBTRCoreAVM->pstBTMediaConfig->block_length        =  lstBTMediaSBCConfig.block_length;
                pstlhBTRCoreAVM->pstBTMediaConfig->min_bitpool         =  lstBTMediaSBCConfig.min_bitpool;
                pstlhBTRCoreAVM->pstBTMediaConfig->max_bitpool         =  lstBTMediaSBCConfig.max_bitpool;
                i32BtRet = 0;
            }
        }
    }
    else {
        a2dp_sbc_t lstBtA2dpCapabilities;

        lstBtA2dpCapabilities.channel_mode       = BTR_A2DP_CHANNEL_MODE_MONO | BTR_A2DP_CHANNEL_MODE_DUAL_CHANNEL |
                                                   BTR_A2DP_CHANNEL_MODE_STEREO | BTR_A2DP_CHANNEL_MODE_JOINT_STEREO;
        lstBtA2dpCapabilities.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                                   BTR_SBC_SAMPLING_FREQ_44100 | BTR_SBC_SAMPLING_FREQ_48000;
        lstBtA2dpCapabilities.allocation_method  = BTR_A2DP_ALLOCATION_SNR | BTR_A2DP_ALLOCATION_LOUDNESS;
        lstBtA2dpCapabilities.subbands           = BTR_A2DP_SUBBANDS_4 | BTR_A2DP_SUBBANDS_8;
        lstBtA2dpCapabilities.block_length       = BTR_A2DP_BLOCK_LENGTH_4 | BTR_A2DP_BLOCK_LENGTH_8 |
                                                   BTR_A2DP_BLOCK_LENGTH_12 | BTR_A2DP_BLOCK_LENGTH_16;
        lstBtA2dpCapabilities.min_bitpool        = MIN_BITPOOL;
        lstBtA2dpCapabilities.max_bitpool        = MAX_BITPOOL;

        if (pstlhBTRCoreAVM) {
            if (pstlhBTRCoreAVM->pstBTMediaConfig) {
                BTRCORELOG_TRACE("Reset Media Configuration\n");
                memcpy(pstlhBTRCoreAVM->pstBTMediaConfig, &lstBtA2dpCapabilities, sizeof(a2dp_sbc_t));
            }
        }
    }


    if (pstlhBTRCoreAVM) {
        if (pstlhBTRCoreAVM->pcAVMediaTransportPath) {
            if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPath, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPath))) {
                BTRCORELOG_INFO ("Freeing %p:%s\n", pstlhBTRCoreAVM->pcAVMediaTransportPath, pstlhBTRCoreAVM->pcAVMediaTransportPath);
                i32BtRet = 0;
             }

            free(pstlhBTRCoreAVM->pcAVMediaTransportPath);
            pstlhBTRCoreAVM->pcAVMediaTransportPath = NULL;
        }
        else {
            pstlhBTRCoreAVM->pcAVMediaTransportPath = strndup(apBtMediaTransportPath, BTRCORE_MAX_STR_LEN - 1);
        }
    }


    return i32BtRet;
}


static int
btrCore_AVMedia_MediaPlayerPathCb (
    const char* apcBTMediaPlayerPath,
    void*       apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     i32BtRet = -1;

    if (!apcBTMediaPlayerPath) {
        BTRCORELOG_ERROR ("Invalid media path\n");
        return -1;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;
    //TODO: consider the player ended event case
    if (pstlhBTRCoreAVM) {
        if (pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
            if(!strncmp(pstlhBTRCoreAVM->pcAVMediaPlayerPath, apcBTMediaPlayerPath, strlen(pstlhBTRCoreAVM->pcAVMediaPlayerPath))) {
                BTRCORELOG_INFO ("Freeing 0x%p:%s\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, pstlhBTRCoreAVM->pcAVMediaPlayerPath);
         
                free(pstlhBTRCoreAVM->pcAVMediaPlayerPath);
                pstlhBTRCoreAVM->pcAVMediaPlayerPath = NULL;
            }
            else {
                BTRCORELOG_INFO ("Switching Media Player from  %s  to  %s\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, apcBTMediaPlayerPath);
                free(pstlhBTRCoreAVM->pcAVMediaPlayerPath);
                pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(apcBTMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1);
             }   
        }
        else {
            BTRCORELOG_INFO ("Storing Media Player : %s\n", apcBTMediaPlayerPath);
            pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(apcBTMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1);
        }

        i32BtRet = 0;
    }


    return i32BtRet;
}


/*implemented specific to StreamIn case, as it seems for StreamOut case that
  with idle, pending and active transport states can't predict the actual media stream state */
static int
btrCore_AVMedia_MediaStatusUpdateCb (
    enBTDeviceType          aeBtDeviceType,
    stBTMediaStatusUpdate*  apstBtMediaStUpdate,
    const char*             apcBtDevAddr,
    void*                   apUserData
) {
    if (!apcBtDevAddr || !apstBtMediaStUpdate || !apUserData) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!\n");
       return -1;
    }

    stBTRCoreAVMediaHdl* pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    if (!pstlhBTRCoreAVM->pcBMediaStatusUserData) {
       BTRCORELOG_ERROR ("pstlhBTRCoreAVM->pcBMediaStatusUserData is NULL!!!\n");
       return -1;
    }

    stBTRCoreAVMediaStatusUpdate  mediaStatus;
    BOOLEAN  postEvent = FALSE;

    switch (apstBtMediaStUpdate->aeBtMediaStatus) {
    case enBTMediaTransportUpdate:
        switch (apstBtMediaStUpdate->m_mediaTransportState) {
        case enBTMTransportStIdle:
            pstlhBTRCoreAVM->eAVMTState = enAVMTransportStDisconnected;
            break;
        case enBTMTransportStPending:
            pstlhBTRCoreAVM->eAVMTState = enAVMTransportStTransition;
            break;
        case enBTMTransportStActive:
            pstlhBTRCoreAVM->eAVMTState = enAVMTransportStConnected;
            break; 
        default:
            break;
        }
        break;
    case enBTMediaTrackUpdate:
        mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStChanged;
        memcpy(&mediaStatus.m_mediaTrackInfo, apstBtMediaStUpdate->m_mediaTrackInfo, sizeof(stBTRCoreAVMediaTrackInfo));
        postEvent = TRUE;
        break;
    case enBTMediaPlayerUpdate:
        break;
    case enBTMediaPlaylistUpdate:
        break;
    case enBTMediaBrowserUpdate:
        break;
    default:
        break;
    }

    /* post callback */
    if (pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate && postEvent) {
        if (pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate(&mediaStatus, apcBtDevAddr, pstlhBTRCoreAVM->pcBMediaStatusUserData) != enBTRCoreSuccess) {
            BTRCORELOG_ERROR ("fpcBBTRCoreAVMediaStatusUpdate - Failure !!!\n");
            return -1;
        }
    }
 
    return 0;
}
