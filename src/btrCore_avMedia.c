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
#include <bluetooth/audio/a2dp-codecs.h>
#if defined(USE_BLUEZ4)
#include <bluetooth/audio/ipc.h>
#endif

/* Interface lib Headers */
#include "btrCore_logger.h"

/* Local Headers */
#include "btrCore_avMedia.h"

#include "btrCore_bt_ifce.h"


//#define AAC_SUPPORTED


#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


#if defined(USE_BLUEZ4)

/* SBC Definitions */
#define BTR_SBC_CHANNEL_MODE_MONO           BT_A2DP_CHANNEL_MODE_MONO
#define BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL   BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL
#define BTR_SBC_CHANNEL_MODE_STEREO         BT_A2DP_CHANNEL_MODE_STEREO
#define BTR_SBC_CHANNEL_MODE_JOINT_STEREO   BT_A2DP_CHANNEL_MODE_JOINT_STEREO

#define BTR_SBC_SAMPLING_FREQ_16000         BT_SBC_SAMPLING_FREQ_16000
#define BTR_SBC_SAMPLING_FREQ_32000         BT_SBC_SAMPLING_FREQ_32000
#define BTR_SBC_SAMPLING_FREQ_44100         BT_SBC_SAMPLING_FREQ_44100
#define BTR_SBC_SAMPLING_FREQ_48000         BT_SBC_SAMPLING_FREQ_48000

#define BTR_SBC_ALLOCATION_SNR              BT_A2DP_ALLOCATION_SNR
#define BTR_SBC_ALLOCATION_LOUDNESS         BT_A2DP_ALLOCATION_LOUDNESS

#define BTR_SBC_SUBBANDS_4                  BT_A2DP_SUBBANDS_4
#define BTR_SBC_SUBBANDS_8                  BT_A2DP_SUBBANDS_8

#define BTR_SBC_BLOCK_LENGTH_4              BT_A2DP_BLOCK_LENGTH_4
#define BTR_SBC_BLOCK_LENGTH_8              BT_A2DP_BLOCK_LENGTH_8
#define BTR_SBC_BLOCK_LENGTH_12             BT_A2DP_BLOCK_LENGTH_12

#define BTR_SBC_BLOCK_LENGTH_16             BT_A2DP_BLOCK_LENGTH_16

#elif defined(USE_BLUEZ5)

/* SBC Definitions */
#define BTR_SBC_CHANNEL_MODE_MONO           SBC_CHANNEL_MODE_MONO
#define BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL   SBC_CHANNEL_MODE_DUAL_CHANNEL
#define BTR_SBC_CHANNEL_MODE_STEREO         SBC_CHANNEL_MODE_STEREO
#define BTR_SBC_CHANNEL_MODE_JOINT_STEREO   SBC_CHANNEL_MODE_JOINT_STEREO

#define BTR_SBC_SAMPLING_FREQ_16000         SBC_SAMPLING_FREQ_16000
#define BTR_SBC_SAMPLING_FREQ_32000         SBC_SAMPLING_FREQ_32000
#define BTR_SBC_SAMPLING_FREQ_44100         SBC_SAMPLING_FREQ_44100
#define BTR_SBC_SAMPLING_FREQ_48000         SBC_SAMPLING_FREQ_48000

#define BTR_SBC_ALLOCATION_SNR              SBC_ALLOCATION_SNR
#define BTR_SBC_ALLOCATION_LOUDNESS         SBC_ALLOCATION_LOUDNESS

#define BTR_SBC_SUBBANDS_4                  SBC_SUBBANDS_4
#define BTR_SBC_SUBBANDS_8                  SBC_SUBBANDS_8

#define BTR_SBC_BLOCK_LENGTH_4              SBC_BLOCK_LENGTH_4
#define BTR_SBC_BLOCK_LENGTH_8              SBC_BLOCK_LENGTH_8
#define BTR_SBC_BLOCK_LENGTH_12             SBC_BLOCK_LENGTH_12
#define BTR_SBC_BLOCK_LENGTH_16             SBC_BLOCK_LENGTH_16


#if defined(AAC_SUPPORTED)

/* MPEG Definitions */
#define BTR_MPEG_CHANNEL_MODE_MONO          MPEG_CHANNEL_MODE_MONO
#define BTR_MPEG_CHANNEL_MODE_DUAL_CHANNEL  MPEG_CHANNEL_MODE_DUAL_CHANNEL
#define BTR_MPEG_CHANNEL_MODE_STEREO        MPEG_CHANNEL_MODE_STEREO
#define BTR_MPEG_CHANNEL_MODE_JOINT_STEREO  MPEG_CHANNEL_MODE_JOINT_STEREO

#define BTR_MPEG_LAYER_MP1                  MPEG_LAYER_MP1
#define BTR_MPEG_LAYER_MP2                  MPEG_LAYER_MP2
#define BTR_MPEG_LAYER_MP3                  MPEG_LAYER_MP3

#define BTR_MPEG_SAMPLING_FREQ_16000        MPEG_SAMPLING_FREQ_16000
#define BTR_MPEG_SAMPLING_FREQ_22050        MPEG_SAMPLING_FREQ_22050
#define BTR_MPEG_SAMPLING_FREQ_24000        MPEG_SAMPLING_FREQ_24000
#define BTR_MPEG_SAMPLING_FREQ_32000        MPEG_SAMPLING_FREQ_32000
#define BTR_MPEG_SAMPLING_FREQ_44100        MPEG_SAMPLING_FREQ_44100
#define BTR_MPEG_SAMPLING_FREQ_48000        MPEG_SAMPLING_FREQ_48000

#define BTR_MPEG_BIT_RATE_VBR               MPEG_BIT_RATE_VBR
#define BTR_MPEG_BIT_RATE_320000            MPEG_BIT_RATE_320000
#define BTR_MPEG_BIT_RATE_256000            MPEG_BIT_RATE_256000
#define BTR_MPEG_BIT_RATE_224000            MPEG_BIT_RATE_224000
#define BTR_MPEG_BIT_RATE_192000            MPEG_BIT_RATE_192000
#define BTR_MPEG_BIT_RATE_160000            MPEG_BIT_RATE_160000
#define BTR_MPEG_BIT_RATE_128000            MPEG_BIT_RATE_128000
#define BTR_MPEG_BIT_RATE_112000            MPEG_BIT_RATE_112000
#define BTR_MPEG_BIT_RATE_96000             MPEG_BIT_RATE_96000
#define BTR_MPEG_BIT_RATE_80000             MPEG_BIT_RATE_80000
#define BTR_MPEG_BIT_RATE_64000             MPEG_BIT_RATE_64000
#define BTR_MPEG_BIT_RATE_56000             MPEG_BIT_RATE_56000
#define BTR_MPEG_BIT_RATE_48000             MPEG_BIT_RATE_48000
#define BTR_MPEG_BIT_RATE_40000             MPEG_BIT_RATE_40000
#define BTR_MPEG_BIT_RATE_32000             MPEG_BIT_RATE_32000
#define BTR_MPEG_BIT_RATE_FREE              MPEG_BIT_RATE_FREE

/* AAC Definitions */
#define BTR_AAC_OT_MPEG2_AAC_LC             AAC_OBJECT_TYPE_MPEG2_AAC_LC
#define BTR_AAC_OT_MPEG4_AAC_LC             AAC_OBJECT_TYPE_MPEG4_AAC_LC
#define BTR_AAC_OT_MPEG4_AAC_LTP            AAC_OBJECT_TYPE_MPEG4_AAC_LTP
#define BTR_AAC_OT_MPEG4_AAC_SCA            AAC_OBJECT_TYPE_MPEG4_AAC_SCA

#define BTR_AAC_SAMPLING_FREQ_8000          AAC_SAMPLING_FREQ_8000
#define BTR_AAC_SAMPLING_FREQ_11025         AAC_SAMPLING_FREQ_11025
#define BTR_AAC_SAMPLING_FREQ_12000         AAC_SAMPLING_FREQ_12000
#define BTR_AAC_SAMPLING_FREQ_16000         AAC_SAMPLING_FREQ_16000
#define BTR_AAC_SAMPLING_FREQ_22050         AAC_SAMPLING_FREQ_22050
#define BTR_AAC_SAMPLING_FREQ_24000         AAC_SAMPLING_FREQ_24000
#define BTR_AAC_SAMPLING_FREQ_32000         AAC_SAMPLING_FREQ_32000
#define BTR_AAC_SAMPLING_FREQ_44100         AAC_SAMPLING_FREQ_44100
#define BTR_AAC_SAMPLING_FREQ_48000         AAC_SAMPLING_FREQ_48000
#define BTR_AAC_SAMPLING_FREQ_64000         AAC_SAMPLING_FREQ_64000
#define BTR_AAC_SAMPLING_FREQ_88200         AAC_SAMPLING_FREQ_88200
#define BTR_AAC_SAMPLING_FREQ_96000         AAC_SAMPLING_FREQ_96000

#define BTR_AAC_CHANNELS_1                  AAC_CHANNELS_1
#define BTR_AAC_CHANNELS_2                  AAC_CHANNELS_2

#define BTR_AAC_SET_BITRATE                 AAC_SET_BITRATE
#define BTR_AAC_SET_FREQ                    AAC_SET_FREQUENCY
#define BTR_AAC_GET_BITRATE                 AAC_GET_BITRATE
#define BTR_AAC_GET_FREQ                    AAC_GET_FREQUENCY

#endif

#endif

#define BTR_SBC_HIGH_BITRATE_BITPOOL	    51
#define BTR_SBC_MED_BITRATE_BITPOOL			33
#define BTR_SBC_LOW_BITRATE_BITPOOL			19

#define BTR_SBC_DEFAULT_BITRATE_BITPOOL		BTR_SBC_HIGH_BITRATE_BITPOOL

typedef enum _enBTRCoreAVMTransportPathState {
    enAVMTransportStConnected,
    enAVMTransportStToBeConnected,
    enAVMTransportStDisconnected
} enBTRCoreAVMTransportPathState;

typedef enum _eBTRCoreAVMediaPlayerType {
    eBTRCoreAVMPTypAudio,
    eBTRCoreAVMPTypVideo,
    eBTRCoreAVMPTypAudioBroadcasting,
    eBTRCoreAVMPTypVideoBroadcasting,
    eBTRCoreAVMPTypUnknown
} eBTRCoreAVMediaPlayerType;

typedef enum _eBTRCoreAVMediaPlayerSubtype {
    eBTRCoreAVMPSbTypAudioBook,
    eBTRCoreAVMPSbTypPodcast,
    eBTRCoreAVMPSbTypUnknown
} eBTRCoreAVMediaPlayerSubtype;

typedef enum _eBTRCoreAVMediaPlayerScan {
    eBTRCoreAVMPScanOff,
    eBTRCoreAVMPScanAllTracks,
    eBTRCoreAVMPScanGroup,
    eBTRCoreAVMPScanUnknown
} eBTRCoreAVMediaPlayerScan;

typedef struct _stBTRCoreAVMediaPlayer {
    char                            m_mediaPlayerName[BTRCORE_MAX_STR_LEN];
    eBTRCoreAVMediaPlayerType       eAVMediaPlayerType;
    eBTRCoreAVMediaPlayerSubtype    eAVMediaPlayerSubtype;
    eBTRCoreAVMediaPlayerScan       eAVMediaPlayerScan;
    enBTRCoreAVMediaCtrl            eAVMediaPlayerShuffle;
    enBTRCoreAVMediaCtrl            eAVMediaPlayerRepeat;
    unsigned int                    m_mediaPlayerPosition;
    unsigned char                   m_mediaPlayerEqualizer;
    unsigned char                   m_mediaPlayerBrowsable;
    unsigned char                   m_mediaPlayerSearchable;
    unsigned char                   m_mediaTrackChanged;
    eBTRCoreAVMediaStatusUpdate     eAVMediaStatusUpdate;       /* change to eAVMediaTrackStatus later */
    stBTRCoreAVMediaTrackInfo       m_mediaTrackInfo;
    //char*           m_mediaPlayerPlaylist;
} stBTRCoreAVMediaPlayer;

typedef struct _stBTRCoreAVMediaFolder {
    char*           m_mediaParentFolder;
    char            m_mediaFolderName[BTRCORE_MAX_STR_LEN];
    unsigned int    m_mediaFolderNumberOfItems;
    unsigned int    m_mediaFolderFilterStartItemNumber;
    unsigned int    m_mediaFolderFilterEndItemNumber;
    //m_mediaFolderFilterWithAttributes[];
} stBTRCoreAVMediaFolder;
    


typedef struct _stBTRCoreAVMediaHdl {
    void*                               btIfceHdl;
    int                                 iBTMediaDefSampFreqPref;
    eBTRCoreAVMType                     eAVMediaTypeOut;
    eBTRCoreAVMType                     eAVMediaTypeIn;
    void*                               pstBTMediaConfigOut;
    void*                               pstBTMediaConfigIn;
    char*                               pcAVMediaTransportPathOut;
    char*                               pcAVMediaTransportPathIn;

    unsigned short                      ui16BTMediaTransportVolume;
    enBTRCoreAVMTransportPathState      eAVMTState;

    char*                               pcAVMediaPlayerPath;
    unsigned char                       bAVMediaPlayerConnected;

    stBTRCoreAVMediaPlayer              pstAVMediaPlayer;
    //FileSystem Path
    //NowPlaying Path
    //MediaItem  List []


    fPtr_BTRCore_AVMediaStatusUpdateCb  fpcBBTRCoreAVMediaStatusUpdate;

    void*                               pcBMediaStatusUserData;

    GThread*                            pMediaPollingThread;
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
static int btrCore_AVMedia_NegotiateMediaCb (void* apBtMediaCapsInput, void** appBtMediaCapsOutput, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apUserData);
static int btrCore_AVMedia_TransportPathCb (const char* apBtMediaTransportPath, const char* apBtMediaUuid, void* apBtMediaCaps, enBTDeviceType aenBTDeviceType, enBTMediaType aenBTMediaType, void* apUserData);
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
        case BTR_SBC_CHANNEL_MODE_MONO:
        case BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL:
            return 31;

        case BTR_SBC_CHANNEL_MODE_STEREO:
        case BTR_SBC_CHANNEL_MODE_JOINT_STEREO:
            return 53;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return 53;
        }
    case BTR_SBC_SAMPLING_FREQ_48000:
        switch (au8AudioChannelsMode) {
        case BTR_SBC_CHANNEL_MODE_MONO:
        case BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL:
            return 29;

        case BTR_SBC_CHANNEL_MODE_STEREO:
        case BTR_SBC_CHANNEL_MODE_JOINT_STEREO:
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
        case BTR_SBC_CHANNEL_MODE_MONO:
        case BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL:
            return 31;

        case BTR_SBC_CHANNEL_MODE_STEREO:
        case BTR_SBC_CHANNEL_MODE_JOINT_STEREO:
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;

        default:
            BTRCORELOG_ERROR ("Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
        }
    case BTR_SBC_SAMPLING_FREQ_48000:
        switch (au8AudioChannelsMode) {
        case BTR_SBC_CHANNEL_MODE_MONO:
        case BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL:
            return 29;

        case BTR_SBC_CHANNEL_MODE_STEREO:
        case BTR_SBC_CHANNEL_MODE_JOINT_STEREO:
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
    stBTRCoreAVMediaHdl*            pstlhBTRCoreAVM         = NULL;
    stBTRCoreAVMediaPlayer*         lpstAVMediaPlayer       = 0;
    stBTRCoreAVMediaStatusUserData* pstAVMediaStUserData    = NULL;
    stBTRCoreAVMediaStatusUpdate    mediaStatus;

    unsigned char   isPlaying       = 0;
    unsigned int    mediaPosition   = 0;
    BOOLEAN         threadExit      = FALSE;
    BOOLEAN         postEvent       = TRUE;

    if (NULL == arg) {
        BTRCORELOG_ERROR ("Exiting.. enBTRCoreInvalidArg!!!\n");
        return NULL;
    }

    BTRCORELOG_INFO ("Started AVMedia Position Polling thread successfully...\n");


    pstlhBTRCoreAVM      = (stBTRCoreAVMediaHdl*)arg;
    pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData;

    if (!pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate || !pstlhBTRCoreAVM->pvThreadData) {
        BTRCORELOG_ERROR("Exiting.. Invalid stBTRCoreAVMediaHdl Data!!! | fpcBBTRCoreAVMediaStatusUpdate : %p | pvThreadData : %p\n"
                        , pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate, pstlhBTRCoreAVM->pvThreadData);
        return NULL;
    }

    pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData; 

    if (!pstAVMediaStUserData->apvAVMUserData || !pstAVMediaStUserData->apcAVMDevAddress) {
        BTRCORELOG_ERROR("Exiting.. Invalid stBTRCoreAVMediaStatusUserData Data!!! | apvAVMUserData : %p | apcAVMDevAddress : %s\n"
                        , pstAVMediaStUserData->apvAVMUserData, pstAVMediaStUserData->apcAVMDevAddress);
        return NULL;
    }

    lpstAVMediaPlayer   = &pstlhBTRCoreAVM->pstAVMediaPlayer;


    while (1) {

        if (threadExit) {
            break;
        }

        if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
            lpstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaPlaybackEnded;
        }

        if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStPlaying) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStForwardSeek) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStReverseSeek) {
            isPlaying = 1;
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPlaying;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStPaused) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPaused;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStStopped) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStStopped;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaPlaybackEnded) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaPlaybackEnded;
            BTRCORELOG_WARN  ("Audio StreamIn Status : PlaybackEnded !!!\n");
            isPlaying  = 1;
            threadExit = TRUE;
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaPlaybackError) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaPlaybackError;
            BTRCORELOG_ERROR ("[%s] Audio StreamIn Status : Error !!!\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath);
        }
        else if (lpstAVMediaPlayer->eAVMediaStatusUpdate != eBTRCoreAVMediaTrkStStarted) {
            mediaStatus.eAVMediaState = eBTRCoreAVMediaPlaybackError;
            BTRCORELOG_ERROR ("[%s] Unknown Audio StreamIn Status : %d !!!\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, lpstAVMediaPlayer->eAVMediaStatusUpdate);
        }



        if (isPlaying) {

            if (mediaStatus.eAVMediaState != eBTRCoreAVMediaPlaybackEnded && mediaStatus.eAVMediaState != eBTRCoreAVMediaPlaybackError) {

                if (!BtrCore_BTGetMediaPlayerProperty(pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, "Position", (void*)&mediaPosition)) {
                    lpstAVMediaPlayer->m_mediaPlayerPosition = mediaPosition;
                }
                else {
                    BTRCORELOG_ERROR ("Failed to get MediaPlayer Position Property  !!!\n");
                }

                if (pstlhBTRCoreAVM->eAVMTState == enAVMTransportStConnected) {
                    if (lpstAVMediaPlayer->m_mediaTrackChanged) {
                        mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStStarted;
                        lpstAVMediaPlayer->m_mediaTrackChanged = 0;
                    }
                }
                else {
                    if (lpstAVMediaPlayer->eAVMediaStatusUpdate == eBTRCoreAVMediaTrkStPlaying) {
                        mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStPaused;
                    }
                }
            }
            else {
                postEvent = TRUE;
            }


            mediaStatus.m_mediaPositionInfo.ui32Position = lpstAVMediaPlayer->m_mediaPlayerPosition;
            mediaStatus.m_mediaPositionInfo.ui32Duration = lpstAVMediaPlayer->m_mediaTrackInfo.ui32Duration;

            /* post callback */
            if (postEvent) {
                pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate(&mediaStatus,
                                                                pstAVMediaStUserData->apcAVMDevAddress,
                                                                pstlhBTRCoreAVM->pcBMediaStatusUserData);
            }

            if (pstlhBTRCoreAVM->eAVMTState != enAVMTransportStConnected) {
                postEvent = FALSE;
            }
            else {
                postEvent = TRUE;
            }
        }


        if (eBTRCoreAVMediaTrkStStarted == mediaStatus.eAVMediaState || eBTRCoreAVMediaTrkStPlaying == mediaStatus.eAVMediaState) {
            sleep(1);           /* polling playback position with 1 sec interval */
        }
        else {
            isPlaying = 0;
            usleep(100000);     /* sleeping 1/10th of a second to check playback status */
        }    
    }

    BTRCORELOG_INFO ("Exiting MediaPosition Polling Thread...\n");

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

    pstlhBTRCoreAVM->pstBTMediaConfigOut     = malloc(MAX(MAX(sizeof(a2dp_aac_t), sizeof(a2dp_mpeg_t)), sizeof(a2dp_sbc_t)));
    pstlhBTRCoreAVM->pstBTMediaConfigIn      = malloc(MAX(MAX(sizeof(a2dp_aac_t), sizeof(a2dp_mpeg_t)), sizeof(a2dp_sbc_t)));

    if (!pstlhBTRCoreAVM->pstBTMediaConfigOut || !pstlhBTRCoreAVM->pstBTMediaConfigIn) {

        if (pstlhBTRCoreAVM->pstBTMediaConfigIn)
            free(pstlhBTRCoreAVM->pstBTMediaConfigIn);

        if (pstlhBTRCoreAVM->pstBTMediaConfigOut)
            free(pstlhBTRCoreAVM->pstBTMediaConfigOut);

        free(pstlhBTRCoreAVM);
        pstlhBTRCoreAVM = NULL;
        return enBTRCoreInitFailure;
    }

    pstlhBTRCoreAVM->btIfceHdl                      = apBtConn;
    pstlhBTRCoreAVM->iBTMediaDefSampFreqPref        = BTR_SBC_SAMPLING_FREQ_48000;
    pstlhBTRCoreAVM->eAVMediaTypeOut                = eBTRCoreAVMTypeUnknown;
    pstlhBTRCoreAVM->eAVMediaTypeIn                 = eBTRCoreAVMTypeUnknown;
    pstlhBTRCoreAVM->pcAVMediaTransportPathOut      = NULL;
    pstlhBTRCoreAVM->pcAVMediaTransportPathIn       = NULL;
    pstlhBTRCoreAVM->pcAVMediaPlayerPath            = NULL;
    pstlhBTRCoreAVM->pvThreadData                   = NULL;
    pstlhBTRCoreAVM->pcBMediaStatusUserData         = NULL;
    pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate = NULL;
    pstlhBTRCoreAVM->eAVMTState                     = enAVMTransportStDisconnected;


    a2dp_sbc_t lstBtA2dpSbcCaps;
    lstBtA2dpSbcCaps.channel_mode       = BTR_SBC_CHANNEL_MODE_MONO | BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL |
                                          BTR_SBC_CHANNEL_MODE_STEREO | BTR_SBC_CHANNEL_MODE_JOINT_STEREO;
#if 0
    lstBtA2dpSbcCaps.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                          BTR_SBC_SAMPLING_FREQ_44100 | BTR_SBC_SAMPLING_FREQ_48000;
#else
    //TODO: Enable 44100 for A2DP Source
    lstBtA2dpSbcCaps.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                          BTR_SBC_SAMPLING_FREQ_48000;
#endif
    lstBtA2dpSbcCaps.allocation_method  = BTR_SBC_ALLOCATION_SNR | BTR_SBC_ALLOCATION_LOUDNESS;
    lstBtA2dpSbcCaps.subbands           = BTR_SBC_SUBBANDS_4 | BTR_SBC_SUBBANDS_8;
    lstBtA2dpSbcCaps.block_length       = BTR_SBC_BLOCK_LENGTH_4 | BTR_SBC_BLOCK_LENGTH_8 |
                                          BTR_SBC_BLOCK_LENGTH_12 | BTR_SBC_BLOCK_LENGTH_16;
    lstBtA2dpSbcCaps.min_bitpool        = MIN_BITPOOL;
    lstBtA2dpSbcCaps.max_bitpool        = MAX_BITPOOL;


    memcpy(pstlhBTRCoreAVM->pstBTMediaConfigOut, &lstBtA2dpSbcCaps, sizeof(a2dp_sbc_t));
    lBtAVMediaASinkRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    enBTDevAudioSink,
                                                    enBTMediaTypeSBC,
                                                    BT_UUID_A2DP_SOURCE,
                                                    (void*)&lstBtA2dpSbcCaps,
                                                    sizeof(lstBtA2dpSbcCaps),
                                                    lBtAVMediaDelayReport);

#if 1
    lstBtA2dpSbcCaps.frequency |= BTR_SBC_SAMPLING_FREQ_44100;
#endif

    memcpy(pstlhBTRCoreAVM->pstBTMediaConfigIn,  &lstBtA2dpSbcCaps, sizeof(a2dp_sbc_t));
    lBtAVMediaASrcRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                   apBtAdapter,
                                                   enBTDevAudioSource,
                                                   enBTMediaTypeSBC,
                                                   BT_UUID_A2DP_SINK,
                                                   (void*)&lstBtA2dpSbcCaps,
                                                   sizeof(lstBtA2dpSbcCaps),
                                                   lBtAVMediaDelayReport);

#if defined(AAC_SUPPORTED)
    a2dp_aac_t lstBtA2dpAacCaps;
    lstBtA2dpAacCaps.object_type        = BTR_AAC_OT_MPEG2_AAC_LC | BTR_AAC_OT_MPEG4_AAC_LC |
                                          BTR_AAC_OT_MPEG4_AAC_LTP | BTR_AAC_OT_MPEG4_AAC_SCA;
    lstBtA2dpAacCaps.channels           = BTR_AAC_CHANNELS_1 | BTR_AAC_CHANNELS_2;
    lstBtA2dpAacCaps.rfa                = 0x3;
    lstBtA2dpAacCaps.vbr                = 1;

    BTR_AAC_SET_FREQ (lstBtA2dpAacCaps,(BTR_AAC_SAMPLING_FREQ_8000  | BTR_AAC_SAMPLING_FREQ_11025 | BTR_AAC_SAMPLING_FREQ_12000 |
                                        BTR_AAC_SAMPLING_FREQ_16000 | BTR_AAC_SAMPLING_FREQ_22050 | BTR_AAC_SAMPLING_FREQ_24000 |
                                        BTR_AAC_SAMPLING_FREQ_32000 | BTR_AAC_SAMPLING_FREQ_44100 | BTR_AAC_SAMPLING_FREQ_48000 |
                                        BTR_AAC_SAMPLING_FREQ_64000 | BTR_AAC_SAMPLING_FREQ_88200 | BTR_AAC_SAMPLING_FREQ_96000));

    BTR_AAC_SET_BITRATE (lstBtA2dpAacCaps,(BTR_MPEG_BIT_RATE_VBR    | BTR_MPEG_BIT_RATE_320000 | BTR_MPEG_BIT_RATE_256000 |
                                           BTR_MPEG_BIT_RATE_224000 | BTR_MPEG_BIT_RATE_192000 | BTR_MPEG_BIT_RATE_160000 |
                                           BTR_MPEG_BIT_RATE_128000 | BTR_MPEG_BIT_RATE_112000 | BTR_MPEG_BIT_RATE_96000  |
                                           BTR_MPEG_BIT_RATE_80000  | BTR_MPEG_BIT_RATE_64000  | BTR_MPEG_BIT_RATE_56000  |
                                           BTR_MPEG_BIT_RATE_48000  | BTR_MPEG_BIT_RATE_40000  | BTR_MPEG_BIT_RATE_32000));

#if 0
    lBtAVMediaASinkRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    enBTDevAudioSink,
                                                    enBTMediaTypeAAC,
                                                    BT_UUID_A2DP_SOURCE,
                                                    (void*)&lstBtA2dpAacCaps,
                                                    sizeof(lstBtA2dpAacCaps),
                                                    lBtAVMediaDelayReport);
#endif

    lBtAVMediaASrcRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                   apBtAdapter,
                                                   enBTDevAudioSource,
                                                   enBTMediaTypeAAC,
                                                   BT_UUID_A2DP_SINK,
                                                   (void*)&lstBtA2dpAacCaps,
                                                   sizeof(lstBtA2dpAacCaps),
                                                   lBtAVMediaDelayReport);
#endif

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

    if (pstlhBTRCoreAVM->btIfceHdl != apBtConn) {
        BTRCORELOG_WARN ("Incorrect Argument - btIfceHdl : Continue\n");
    }


#if defined(AAC_SUPPORTED)
    lBtAVMediaASrcUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                       apBtAdapter,
                                                       enBTDevAudioSource,
                                                       enBTMediaTypeAAC);

#if 0
    lBtAVMediaASinkUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                        apBtAdapter,
                                                        enBTDevAudioSink,
                                                        enBTMediaTypeAAC);
#endif
#endif

    lBtAVMediaASrcUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                       apBtAdapter,
                                                       enBTDevAudioSource,
                                                       enBTMediaTypeSBC);

    lBtAVMediaASinkUnRegRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                                        apBtAdapter,
                                                        enBTDevAudioSink,
                                                        enBTMediaTypeSBC);

    if (pstlhBTRCoreAVM->pstBTMediaConfigIn) {
        free(pstlhBTRCoreAVM->pstBTMediaConfigIn);
        pstlhBTRCoreAVM->pstBTMediaConfigIn = NULL;
    }

    if (pstlhBTRCoreAVM->pstBTMediaConfigOut) {
        free(pstlhBTRCoreAVM->pstBTMediaConfigOut);
        pstlhBTRCoreAVM->pstBTMediaConfigOut = NULL;
    }

    if (pstlhBTRCoreAVM->pcAVMediaTransportPathOut) {
        free(pstlhBTRCoreAVM->pcAVMediaTransportPathOut);
        pstlhBTRCoreAVM->pcAVMediaTransportPathOut = NULL;
    }

    if (pstlhBTRCoreAVM->pcAVMediaTransportPathIn) {
        free(pstlhBTRCoreAVM->pcAVMediaTransportPathIn);
        pstlhBTRCoreAVM->pcAVMediaTransportPathIn = NULL;
    }

    if (pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        free(pstlhBTRCoreAVM->pcAVMediaPlayerPath);
        pstlhBTRCoreAVM->pcAVMediaPlayerPath = NULL;
    }

    pstlhBTRCoreAVM->btIfceHdl               = NULL;
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
    const char*             apBtDevAddr,
    stBTRCoreAVMediaInfo*   apstBtrCoreAVMediaInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM     = NULL;
    enBTRCoreRet            lenBTRCoreRet       = enBTRCoreFailure;
    void*                   lpstBTMediaConfig   = NULL;

    if (!hBTRCoreAVM || !apBtDevAddr || !apstBtrCoreAVMediaInfo) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;


    if (apstBtrCoreAVMediaInfo->eBtrCoreAVMFlow == eBTRCoreAVMediaFlowOut) {
        /* Max 4 sec timeout - Polled at 200ms second interval */
        unsigned int ui32sleepIdx = 20;
        do {
            sched_yield();
            usleep(200000);
        } while ((!pstlhBTRCoreAVM->pcAVMediaTransportPathOut) && (--ui32sleepIdx));

        lpstBTMediaConfig                       = pstlhBTRCoreAVM->pstBTMediaConfigOut;
        apstBtrCoreAVMediaInfo->eBtrCoreAVMType = pstlhBTRCoreAVM->eAVMediaTypeOut;
    }
    else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMFlow == eBTRCoreAVMediaFlowIn) {
        /* Max 4 sec timeout - Polled at 200ms second interval */
        unsigned int ui32sleepIdx = 20;
        do {
            sched_yield();
            usleep(200000);
        } while ((!pstlhBTRCoreAVM->pcAVMediaTransportPathIn) && (--ui32sleepIdx));

        lpstBTMediaConfig                       = pstlhBTRCoreAVM->pstBTMediaConfigIn;
        apstBtrCoreAVMediaInfo->eBtrCoreAVMType = pstlhBTRCoreAVM->eAVMediaTypeIn;
    }
    else {
        //TODO: Handle Other flows and default
        lpstBTMediaConfig                       = pstlhBTRCoreAVM->pstBTMediaConfigOut;
        apstBtrCoreAVMediaInfo->eBtrCoreAVMType = pstlhBTRCoreAVM->eAVMediaTypeOut;
    }


    if (!apstBtrCoreAVMediaInfo->pstBtrCoreAVMCodecInfo || !lpstBTMediaConfig) {
        return enBTRCoreFailure;
    }


    if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypePCM) {
    }
    else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeSBC) {
        a2dp_sbc_t*              lpstBTMediaSbcConfig     = (a2dp_sbc_t*)lpstBTMediaConfig;
        stBTRCoreAVMediaSbcInfo* pstBtrCoreAVMediaSbcInfo = (stBTRCoreAVMediaSbcInfo*)(apstBtrCoreAVMediaInfo->pstBtrCoreAVMCodecInfo);

        if (lpstBTMediaSbcConfig->frequency == BTR_SBC_SAMPLING_FREQ_16000) {
            pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 16000;
        }
        else if (lpstBTMediaSbcConfig->frequency == BTR_SBC_SAMPLING_FREQ_32000) {
            pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 32000;
        }
        else if (lpstBTMediaSbcConfig->frequency == BTR_SBC_SAMPLING_FREQ_44100) {
            pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 44100;
        }
        else if (lpstBTMediaSbcConfig->frequency == BTR_SBC_SAMPLING_FREQ_48000) {
            pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 48000;
        }
        else {
            pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq = 0;
        }

        if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_MONO) {
            pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanMono;
            pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 1;
        }
        else if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL) {
            pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanDualChannel;
            pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
        }
        else if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_STEREO) {
            pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanStereo;
            pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
        }
        else if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_JOINT_STEREO) {
            pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanJointStereo;
            pstBtrCoreAVMediaSbcInfo->ui32AVMAChan = 2;
        }
        else {
            pstBtrCoreAVMediaSbcInfo->eAVMAChan = eBTRCoreAVMAChanUnknown;
        }

        pstBtrCoreAVMediaSbcInfo->ui8AVMSbcAllocMethod  = lpstBTMediaSbcConfig->allocation_method;

        if (lpstBTMediaSbcConfig->subbands == BTR_SBC_SUBBANDS_4) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 4;
        }
        else if (lpstBTMediaSbcConfig->subbands == BTR_SBC_SUBBANDS_8) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 8;
        }
        else {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands = 0;
        }

        if (lpstBTMediaSbcConfig->block_length == BTR_SBC_BLOCK_LENGTH_4) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 4;
        }
        else if (lpstBTMediaSbcConfig->block_length == BTR_SBC_BLOCK_LENGTH_8) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 8;
        }
        else if (lpstBTMediaSbcConfig->block_length == BTR_SBC_BLOCK_LENGTH_12) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 12;
        }
        else if (lpstBTMediaSbcConfig->block_length == BTR_SBC_BLOCK_LENGTH_16) {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 16;
        }
        else {
            pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength  = 0;
        }

        pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMinBitpool   = lpstBTMediaSbcConfig->min_bitpool;
        pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool   = lpstBTMediaSbcConfig->max_bitpool;      

        if ((lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_MONO) ||
            (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL)) {
            pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 4 + ((4 * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan) / 8) +
                                                          ((pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool) / 8);
        }
        else if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_STEREO) {
            pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen = 4 + (4 * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands * pstBtrCoreAVMediaSbcInfo->ui32AVMAChan) / 8 +
                                                            (pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength * pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool) / 8;
        }
        else if (lpstBTMediaSbcConfig->channel_mode == BTR_SBC_CHANNEL_MODE_JOINT_STEREO) {
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

        BTRCORELOG_TRACE ("ui32AVMSFreq          = %d\n", pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq);
        BTRCORELOG_TRACE ("ui32AVMAChan          = %d\n", pstBtrCoreAVMediaSbcInfo->ui32AVMAChan);
        BTRCORELOG_TRACE ("ui8AVMSbcAllocMethod  = %d\n", pstBtrCoreAVMediaSbcInfo->ui8AVMSbcAllocMethod);
        BTRCORELOG_TRACE ("ui8AVMSbcSubbands     = %d\n", pstBtrCoreAVMediaSbcInfo->ui8AVMSbcSubbands);
        BTRCORELOG_TRACE ("ui8AVMSbcBlockLength  = %d\n", pstBtrCoreAVMediaSbcInfo->ui8AVMSbcBlockLength);
        BTRCORELOG_TRACE ("ui8AVMSbcMinBitpool   = %d\n", pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMinBitpool);
        BTRCORELOG_TRACE ("ui8AVMSbcMaxBitpool   = %d\n", pstBtrCoreAVMediaSbcInfo->ui8AVMSbcMaxBitpool);
        BTRCORELOG_TRACE ("ui16AVMSbcFrameLen    = %d\n", pstBtrCoreAVMediaSbcInfo->ui16AVMSbcFrameLen);
        BTRCORELOG_DEBUG ("ui16AVMSbcBitrate     = %d\n", pstBtrCoreAVMediaSbcInfo->ui16AVMSbcBitrate);

        if (pstBtrCoreAVMediaSbcInfo->ui32AVMSFreq && pstBtrCoreAVMediaSbcInfo->ui16AVMSbcBitrate)
            lenBTRCoreRet = enBTRCoreSuccess;
                         
    }
    else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeMPEG) {
    }
    else if (apstBtrCoreAVMediaInfo->eBtrCoreAVMType == eBTRCoreAVMTypeAAC) {
#if defined(AAC_SUPPORTED)
        unsigned short              ui16AacFreq   = 0;
        unsigned short              ui16AacBitrate= 0;
        a2dp_aac_t*                 lpstBTMediaAacConfig     = (a2dp_aac_t*)lpstBTMediaConfig;
        stBTRCoreAVMediaMpegInfo*   pstBtrCoreAVMediaAacInfo = (stBTRCoreAVMediaMpegInfo*)(apstBtrCoreAVMediaInfo->pstBtrCoreAVMCodecInfo);

        ui16AacFreq = BTR_AAC_GET_FREQ(*lpstBTMediaAacConfig);
        if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_8000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 8000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_11025) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 11025;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_12000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 12000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_16000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 16000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_22050) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 22050;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_24000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 24000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_32000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 32000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_44100) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 44100;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_48000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 48000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_64000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 64000;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_88200) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 88200;
        }
        else if (ui16AacFreq & BTR_AAC_SAMPLING_FREQ_96000) {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 96000;
        }
        else {
            pstBtrCoreAVMediaAacInfo->ui32AVMSFreq = 0;
        }

        ui16AacBitrate = BTR_AAC_GET_BITRATE(*lpstBTMediaAacConfig);
        if (ui16AacBitrate & BTR_MPEG_BIT_RATE_VBR) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 1;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_320000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 320;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_256000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 256;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_224000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 224;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_192000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 192;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_160000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 160;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_128000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 128;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_112000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 112;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_96000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 96;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_80000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 80;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_64000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 64;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_56000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 56;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_48000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 48;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_40000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 40;
        }
        else if (ui16AacBitrate & BTR_MPEG_BIT_RATE_32000) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 32;
        }
        else {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = 0;
        }

        
        if (lpstBTMediaAacConfig->vbr) {
            pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate = lpstBTMediaAacConfig->vbr;
        }

        pstBtrCoreAVMediaAacInfo->ui8AVMMpegRfa = lpstBTMediaAacConfig->rfa;

        if (lpstBTMediaAacConfig->channels & BTR_AAC_CHANNELS_1) {
            pstBtrCoreAVMediaAacInfo->eAVMAChan = eBTRCoreAVMAChanMono;
            pstBtrCoreAVMediaAacInfo->ui32AVMAChan = 1;
        }
        else if (lpstBTMediaAacConfig->channels & BTR_AAC_CHANNELS_2) {
            pstBtrCoreAVMediaAacInfo->eAVMAChan = eBTRCoreAVMAChanStereo;
            pstBtrCoreAVMediaAacInfo->ui32AVMAChan = 2;
        }
        else {
            pstBtrCoreAVMediaAacInfo->eAVMAChan = eBTRCoreAVMAChanUnknown;
            pstBtrCoreAVMediaAacInfo->ui32AVMAChan = 0;
        }

        if (lpstBTMediaAacConfig->object_type == BTR_AAC_OT_MPEG2_AAC_LC) {
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion = 2;
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegType = 4;
        }
        else if (lpstBTMediaAacConfig->object_type == BTR_AAC_OT_MPEG4_AAC_LC) {
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion = 4;
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegType = 4;
        }
        else if (lpstBTMediaAacConfig->object_type == BTR_AAC_OT_MPEG4_AAC_LTP) {
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion = 4;
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegType = 4;
        }
        else if (lpstBTMediaAacConfig->object_type == BTR_AAC_OT_MPEG4_AAC_SCA) {
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion = 4;
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegType = 4;
        }
        else {
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion = 0;
            pstBtrCoreAVMediaAacInfo->ui8AVMMpegType = 0;
        }

    
        BTRCORELOG_TRACE ("eAVMAChan            = %d\n", pstBtrCoreAVMediaAacInfo->eAVMAChan);
        BTRCORELOG_TRACE ("ui32AVMAChan         = %d\n", pstBtrCoreAVMediaAacInfo->ui32AVMAChan);
        BTRCORELOG_DEBUG ("ui32AVMSFreq         = %d\n", pstBtrCoreAVMediaAacInfo->ui32AVMSFreq);
        BTRCORELOG_TRACE ("ui8AVMMpegVersion    = %d\n", pstBtrCoreAVMediaAacInfo->ui8AVMMpegVersion);
        BTRCORELOG_TRACE ("ui8AVMMpegLayer      = %d\n", pstBtrCoreAVMediaAacInfo->ui8AVMMpegLayer);
        BTRCORELOG_TRACE ("ui8AVMMpegType       = %d\n", pstBtrCoreAVMediaAacInfo->ui8AVMMpegType);
        BTRCORELOG_TRACE ("ui8AVMMpegMpf        = %d\n", pstBtrCoreAVMediaAacInfo->ui8AVMMpegMpf);
        BTRCORELOG_TRACE ("ui8AVMMpegRfa        = %d\n", pstBtrCoreAVMediaAacInfo->ui8AVMMpegRfa);
        BTRCORELOG_TRACE ("ui16AVMMpegFrameLen  = %d\n", pstBtrCoreAVMediaAacInfo->ui16AVMMpegFrameLen);
        BTRCORELOG_DEBUG ("ui16AVMMpegBitrate   = %d\n", pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate);

        if (pstBtrCoreAVMediaAacInfo->ui32AVMSFreq && pstBtrCoreAVMediaAacInfo->ui16AVMMpegBitrate)
            lenBTRCoreRet = enBTRCoreSuccess;

#endif
    }


    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_AcquireDataPath (
    tBTRCoreAVMediaHdl  hBTRCoreAVM,
    const char*         apBtDevAddr,
    int*                apDataPath,
    int*                apDataReadMTU,
    int*                apDataWriteMTU
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    char*                   lpcAVMediaTransportPath = NULL;
    int                     lBtAVMediaRet = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;
    unBTOpIfceProp          lunBtOpMedTProp;
    unsigned int            ui16Delay = 0xFFFFu;

    if (!hBTRCoreAVM || !apBtDevAddr) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;


    if (pstlhBTRCoreAVM->pcAVMediaTransportPathOut && strstr(pstlhBTRCoreAVM->pcAVMediaTransportPathOut, apBtDevAddr)) {
        lpcAVMediaTransportPath = pstlhBTRCoreAVM->pcAVMediaTransportPathOut;
    }
    else if (pstlhBTRCoreAVM->pcAVMediaTransportPathIn && strstr(pstlhBTRCoreAVM->pcAVMediaTransportPathIn, apBtDevAddr)) {
        lpcAVMediaTransportPath = pstlhBTRCoreAVM->pcAVMediaTransportPathIn;
    }


    if (lpcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }

    if (!(lBtAVMediaRet = BtrCore_BTAcquireDevDataPath(pstlhBTRCoreAVM->btIfceHdl, lpcAVMediaTransportPath, apDataPath, apDataReadMTU, apDataWriteMTU)))
        lenBTRCoreRet = enBTRCoreSuccess;

    lunBtOpMedTProp.enBtMediaTransportProp = enBTMedTPropDelay;
    if ((lBtAVMediaRet = BtrCore_BTGetProp(pstlhBTRCoreAVM->btIfceHdl, lpcAVMediaTransportPath, enBTMediaTransport, lunBtOpMedTProp, &ui16Delay)))
        lenBTRCoreRet = enBTRCoreFailure;

    BTRCORELOG_INFO ("BTRCore_AVMedia_AcquireDataPath: Delay value = %d\n", ui16Delay);

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_ReleaseDataPath (
    tBTRCoreAVMediaHdl      hBTRCoreAVM,
    const char*             apBtDevAddr
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    char*                   lpcAVMediaTransportPath = NULL;
    int                     lBtAVMediaRet = -1;
    enBTRCoreRet            lenBTRCoreRet = enBTRCoreFailure;

    if (!hBTRCoreAVM || !apBtDevAddr) {
        return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;


    if (pstlhBTRCoreAVM->pcAVMediaTransportPathOut && strstr(pstlhBTRCoreAVM->pcAVMediaTransportPathOut, apBtDevAddr)) {
        lpcAVMediaTransportPath = pstlhBTRCoreAVM->pcAVMediaTransportPathOut;
    }
    else if (pstlhBTRCoreAVM->pcAVMediaTransportPathIn && strstr(pstlhBTRCoreAVM->pcAVMediaTransportPathIn, apBtDevAddr)) {
        lpcAVMediaTransportPath = pstlhBTRCoreAVM->pcAVMediaTransportPathIn;
    }


    if (lpcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }


    if (!(lBtAVMediaRet = BtrCore_BTReleaseDevDataPath(pstlhBTRCoreAVM->btIfceHdl, lpcAVMediaTransportPath)))
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_MediaControl (
    tBTRCoreAVMediaHdl      hBTRCoreAVM,
    const char*             apBtDevAddr,
    enBTRCoreAVMediaCtrl    aenBTRCoreAVMediaCtrl 
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM   = NULL;
    enBTRCoreRet            lenBTRCoreRet     = enBTRCoreSuccess;
    enBTMediaControlCmd     aenBTMediaControl = 0;


    if (!hBTRCoreAVM)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
        //      Seems to be the root cause of the stack corruption as part of DELIA-25861
        char*   lpcAVMediaPlayerPath = BtrCore_BTGetMediaPlayerPath(pstlhBTRCoreAVM->btIfceHdl, apBtDevAddr);
        if (!lpcAVMediaPlayerPath || !(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(lpcAVMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1))) {
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

    if (BtrCore_BTDevMediaControl(pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, aenBTMediaControl)) {
       BTRCORELOG_ERROR ("Failed to set the Media control option");
       lenBTRCoreRet = enBTRCoreFailure;
    }
      
    return lenBTRCoreRet;
}

//Combine TrackInfo, PositionInfo and basic info in GetMediaProperty handling with enums and switch?
enBTRCoreRet
BTRCore_AVMedia_GetTrackInfo (
    tBTRCoreAVMediaHdl         hBTRCoreAVM,
    const char*                apBtDevAddr,
    stBTRCoreAVMediaTrackInfo* apstBTAVMediaTrackInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtDevAddr || !apstBTAVMediaTrackInfo)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
        //      Seems to be the root cause of the stack corruption as part of DELIA-25861
        char*   lpcAVMediaPlayerPath = BtrCore_BTGetMediaPlayerPath (pstlhBTRCoreAVM->btIfceHdl, apBtDevAddr);
        if (!lpcAVMediaPlayerPath || !(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(lpcAVMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1))) {
            BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
            return enBTRCoreFailure;
        }
    }

    if (BtrCore_BTGetTrackInformation (pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, (stBTMediaTrackInfo*)apstBTAVMediaTrackInfo)) {
       BTRCORELOG_WARN ("Failed to get Track information!!! from Bluez !");
       lenBTRCoreRet = enBTRCoreFailure;
    }

    return lenBTRCoreRet;
}


//Combine TrackInfo, PositionInfo and basic info in GetMediaProperty handling with enums and switch?
enBTRCoreRet
BTRCore_AVMedia_GetPositionInfo (
    tBTRCoreAVMediaHdl            hBTRCoreAVM,
    const char*                   apBtDevAddr,
    stBTRCoreAVMediaPositionInfo* apstBTAVMediaPositionInfo
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtDevAddr || !apstBTAVMediaPositionInfo)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
        //      Seems to be the root cause of the stack corruption as part of DELIA-25861
        char*   lpcAVMediaPlayerPath = BtrCore_BTGetMediaPlayerPath(pstlhBTRCoreAVM->btIfceHdl, apBtDevAddr);
        if (!lpcAVMediaPlayerPath || !(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(lpcAVMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1))) {
            BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
            return enBTRCoreFailure;
        }
    }

    stBTRCoreAVMediaTrackInfo   mediaTrackInfo;
    unsigned int  mediaPosition = 0;
    enBTRCoreRet  positionRet, trackRet;

    //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
    //      Seems to be the root cause of the stack corruption as part of DELIA-25861. This is call seems to be root cause of DELIA-25861
    positionRet = BtrCore_BTGetMediaPlayerProperty(pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, "Position", (void*)&mediaPosition);
    trackRet    = BtrCore_BTGetTrackInformation(pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, (stBTMediaTrackInfo*)&mediaTrackInfo);

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
    const char*             apBtDevAddr,
    const char*             mediaPropertyKey,
    void*                   mediaPropertyValue
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    enBTRCoreRet            lenBTRCoreRet   = enBTRCoreSuccess;

    if (!hBTRCoreAVM || !apBtDevAddr)  {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg\n");
       return enBTRCoreInvalidArg;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)hBTRCoreAVM;

    if (!pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
        //TODO: The pcAVMediaPlayerPath changes during transition between Players on Smartphone (Local->Youtube->Local)
        //      Seems to be the root cause of the stack corruption as part of DELIA-25861
        char*   lpcAVMediaPlayerPath = BtrCore_BTGetMediaPlayerPath(pstlhBTRCoreAVM->btIfceHdl, apBtDevAddr);
        if (!lpcAVMediaPlayerPath || !(pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(lpcAVMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1))) {
            BTRCORELOG_ERROR ("Failed to get Media Player Object!!!");
            return enBTRCoreFailure;
        }
    }

    if (BtrCore_BTGetMediaPlayerProperty(pstlhBTRCoreAVM->btIfceHdl, pstlhBTRCoreAVM->pcAVMediaPlayerPath, mediaPropertyKey, mediaPropertyValue)) {
       BTRCORELOG_ERROR ("Failed to get Media Property : %s!!!",mediaPropertyKey);
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
    void*           apBtMediaCapsInput,
    void**          appBtMediaCapsOutput,
    enBTDeviceType  aenBTDeviceType,
    enBTMediaType   aenBTMediaType,
    void*           apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;

    if (!apBtMediaCapsInput) {
        BTRCORELOG_ERROR ("Invalid input MT Media Capabilities\n");
        return -1;
    } 

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;


    if (aenBTMediaType == enBTMediaTypeSBC) {
        a2dp_sbc_t* apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCapsInput;
        a2dp_sbc_t  lstBTMediaSbcConfig;

        memset(&lstBTMediaSbcConfig, 0, sizeof(a2dp_sbc_t));

#if 0
        lstBTMediaSbcConfig.frequency = pstlhBTRCoreAVM->iBTMediaDefSampFreqPref;
#else
        if (apBtMediaSBCCaps->frequency & BTR_SBC_SAMPLING_FREQ_48000) {
            lstBTMediaSbcConfig.frequency = BTR_SBC_SAMPLING_FREQ_48000;
        }
        else if (apBtMediaSBCCaps->frequency & BTR_SBC_SAMPLING_FREQ_44100) {
            lstBTMediaSbcConfig.frequency = BTR_SBC_SAMPLING_FREQ_44100;
        }
        else if (apBtMediaSBCCaps->frequency & BTR_SBC_SAMPLING_FREQ_32000) {
            lstBTMediaSbcConfig.frequency = BTR_SBC_SAMPLING_FREQ_32000;
        }
        else if (apBtMediaSBCCaps->frequency & BTR_SBC_SAMPLING_FREQ_16000) {
            lstBTMediaSbcConfig.frequency = BTR_SBC_SAMPLING_FREQ_16000;
        }
        else {
            BTRCORELOG_ERROR ("No supported frequency\n");
            return -1;
        }
#endif

        if (apBtMediaSBCCaps->channel_mode & BTR_SBC_CHANNEL_MODE_JOINT_STEREO) {
            lstBTMediaSbcConfig.channel_mode = BTR_SBC_CHANNEL_MODE_JOINT_STEREO;
        }
        else if (apBtMediaSBCCaps->channel_mode & BTR_SBC_CHANNEL_MODE_STEREO) {
            lstBTMediaSbcConfig.channel_mode = BTR_SBC_CHANNEL_MODE_STEREO;
        }
        else if (apBtMediaSBCCaps->channel_mode & BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL) {
            lstBTMediaSbcConfig.channel_mode = BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL;
        }
        else if (apBtMediaSBCCaps->channel_mode & BTR_SBC_CHANNEL_MODE_MONO) {
            lstBTMediaSbcConfig.channel_mode = BTR_SBC_CHANNEL_MODE_MONO;
        } 
        else {
            BTRCORELOG_ERROR ("No supported channel modes\n");
            return -1;
        }

        if (apBtMediaSBCCaps->block_length & BTR_SBC_BLOCK_LENGTH_16) {
            lstBTMediaSbcConfig.block_length = BTR_SBC_BLOCK_LENGTH_16;
        }
        else if (apBtMediaSBCCaps->block_length & BTR_SBC_BLOCK_LENGTH_12) {
            lstBTMediaSbcConfig.block_length = BTR_SBC_BLOCK_LENGTH_12;
        }
        else if (apBtMediaSBCCaps->block_length & BTR_SBC_BLOCK_LENGTH_8) {
            lstBTMediaSbcConfig.block_length = BTR_SBC_BLOCK_LENGTH_8;
        }
        else if (apBtMediaSBCCaps->block_length & BTR_SBC_BLOCK_LENGTH_4) {
            lstBTMediaSbcConfig.block_length = BTR_SBC_BLOCK_LENGTH_4;
        }
        else {
            BTRCORELOG_ERROR ("No supported block lengths\n");
            return -1;
        }

        if (apBtMediaSBCCaps->subbands & BTR_SBC_SUBBANDS_8) {
            lstBTMediaSbcConfig.subbands = BTR_SBC_SUBBANDS_8;
        }
        else if (apBtMediaSBCCaps->subbands & BTR_SBC_SUBBANDS_4) {
            lstBTMediaSbcConfig.subbands = BTR_SBC_SUBBANDS_4;
        }
        else {
            BTRCORELOG_ERROR ("No supported subbands\n");
            return -1;
        }

        if (apBtMediaSBCCaps->allocation_method & BTR_SBC_ALLOCATION_LOUDNESS) {
            lstBTMediaSbcConfig.allocation_method = BTR_SBC_ALLOCATION_LOUDNESS;
        }
        else if (apBtMediaSBCCaps->allocation_method & BTR_SBC_ALLOCATION_SNR) {
            lstBTMediaSbcConfig.allocation_method = BTR_SBC_ALLOCATION_SNR;
        }

        lstBTMediaSbcConfig.min_bitpool = (uint8_t) MAX(MIN_BITPOOL, apBtMediaSBCCaps->min_bitpool);
        lstBTMediaSbcConfig.max_bitpool = (uint8_t) MIN(btrCore_AVMedia_GetA2DPDefaultBitpool(lstBTMediaSbcConfig.frequency, 
                                                                                              lstBTMediaSbcConfig.channel_mode),
                                                        apBtMediaSBCCaps->max_bitpool);

        BTRCORELOG_TRACE("Negotiated Configuration\n");
        BTRCORELOG_INFO ("channel_mode       = %d\n", lstBTMediaSbcConfig.channel_mode);
        BTRCORELOG_INFO ("frequency          = %d\n", lstBTMediaSbcConfig.frequency);
        BTRCORELOG_INFO ("allocation_method  = %d\n", lstBTMediaSbcConfig.allocation_method);
        BTRCORELOG_INFO ("subbands           = %d\n", lstBTMediaSbcConfig.subbands);
        BTRCORELOG_INFO ("block_length       = %d\n", lstBTMediaSbcConfig.block_length);
        BTRCORELOG_INFO ("min_bitpool        = %d\n", lstBTMediaSbcConfig.min_bitpool);
        BTRCORELOG_INFO ("max_bitpool        = %d\n", lstBTMediaSbcConfig.max_bitpool);

        if (pstlhBTRCoreAVM) {
            a2dp_sbc_t* lpstBTAVMMediaSbcConfigOut  = (a2dp_sbc_t*)pstlhBTRCoreAVM->pstBTMediaConfigOut;
            a2dp_sbc_t* lpstBTAVMMediaSbcConfigIn   = (a2dp_sbc_t*)pstlhBTRCoreAVM->pstBTMediaConfigIn;

            if (lpstBTAVMMediaSbcConfigOut &&
                (aenBTDeviceType == enBTDevAudioSink) &&
                (pstlhBTRCoreAVM->eAVMediaTypeOut != eBTRCoreAVMTypeSBC)) {
                pstlhBTRCoreAVM->eAVMediaTypeOut                =  eBTRCoreAVMTypeSBC;
                lpstBTAVMMediaSbcConfigOut->channel_mode        =  lstBTMediaSbcConfig.channel_mode;
                lpstBTAVMMediaSbcConfigOut->frequency           =  lstBTMediaSbcConfig.frequency;
                lpstBTAVMMediaSbcConfigOut->allocation_method   =  lstBTMediaSbcConfig.allocation_method;
                lpstBTAVMMediaSbcConfigOut->subbands            =  lstBTMediaSbcConfig.subbands;
                lpstBTAVMMediaSbcConfigOut->block_length        =  lstBTMediaSbcConfig.block_length;
                lpstBTAVMMediaSbcConfigOut->min_bitpool         =  lstBTMediaSbcConfig.min_bitpool;
                lpstBTAVMMediaSbcConfigOut->max_bitpool         =  lstBTMediaSbcConfig.max_bitpool;

                *appBtMediaCapsOutput  = (void*)lpstBTAVMMediaSbcConfigOut;
            }

            if (lpstBTAVMMediaSbcConfigIn &&
                (aenBTDeviceType == enBTDevAudioSource) &&
                (pstlhBTRCoreAVM->eAVMediaTypeIn != eBTRCoreAVMTypeSBC)) {
                pstlhBTRCoreAVM->eAVMediaTypeIn                =  eBTRCoreAVMTypeSBC;
                lpstBTAVMMediaSbcConfigIn->channel_mode        =  lstBTMediaSbcConfig.channel_mode;
                lpstBTAVMMediaSbcConfigIn->frequency           =  lstBTMediaSbcConfig.frequency;
                lpstBTAVMMediaSbcConfigIn->allocation_method   =  lstBTMediaSbcConfig.allocation_method;
                lpstBTAVMMediaSbcConfigIn->subbands            =  lstBTMediaSbcConfig.subbands;
                lpstBTAVMMediaSbcConfigIn->block_length        =  lstBTMediaSbcConfig.block_length;
                lpstBTAVMMediaSbcConfigIn->min_bitpool         =  lstBTMediaSbcConfig.min_bitpool;
                lpstBTAVMMediaSbcConfigIn->max_bitpool         =  lstBTMediaSbcConfig.max_bitpool;

                *appBtMediaCapsOutput  = (void*)lpstBTAVMMediaSbcConfigIn;
            }
        }
    }
    else if (aenBTMediaType == enBTMediaTypePCM) {

    }
    else if (aenBTMediaType == enBTMediaTypeAAC) {
#if defined(AAC_SUPPORTED)
        a2dp_aac_t*     apBtMediaAACCaps = (a2dp_aac_t*)apBtMediaCapsInput;
        a2dp_aac_t      lstBTMediaAacConfig;
        unsigned short  ui16AacInFreq   = 0;
        unsigned short  ui16AacOutFreq  = 0;
        unsigned short  ui16AacInBitrate= 0;
        unsigned short  ui16AacOutBitrate= 0;

        memset(&lstBTMediaAacConfig, 0, sizeof(a2dp_aac_t));

        ui16AacInFreq = BTR_AAC_GET_FREQ(*apBtMediaAACCaps);
        if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_8000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_8000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_11025) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_11025;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_12000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_12000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_16000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_16000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_22050) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_22050;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_24000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_24000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_32000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_32000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_44100) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_44100;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_48000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_48000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_64000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_64000;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_88200) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_88200;
        }
        else if (ui16AacInFreq & BTR_AAC_SAMPLING_FREQ_96000) {
            ui16AacOutFreq = BTR_AAC_SAMPLING_FREQ_96000;
        }
        else {
            BTRCORELOG_ERROR ("No supported frequency\n");
            return -1;
        }

        BTR_AAC_SET_FREQ(lstBTMediaAacConfig, ui16AacOutFreq);

        ui16AacInBitrate = BTR_AAC_GET_BITRATE(*apBtMediaAACCaps);
        if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_VBR) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_VBR; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_320000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_320000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_256000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_256000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_224000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_224000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_192000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_192000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_160000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_160000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_128000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_128000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_112000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_112000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_96000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_96000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_80000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_80000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_64000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_64000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_56000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_56000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_48000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_48000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_40000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_40000; 
        }
        else if (ui16AacInBitrate & BTR_MPEG_BIT_RATE_32000) {
            ui16AacOutBitrate = BTR_MPEG_BIT_RATE_32000; 
        }
        else {
            BTRCORELOG_ERROR ("No supported bitrate\n");
            return -1;
        }

        BTR_AAC_SET_BITRATE(lstBTMediaAacConfig, ui16AacOutBitrate);


        if (apBtMediaAACCaps->object_type & BTR_AAC_OT_MPEG2_AAC_LC) {
            lstBTMediaAacConfig.object_type = BTR_AAC_OT_MPEG2_AAC_LC;
        }
        else if (apBtMediaAACCaps->object_type & BTR_AAC_OT_MPEG4_AAC_LC) {
            lstBTMediaAacConfig.object_type = BTR_AAC_OT_MPEG4_AAC_LC;
        }
        else if (apBtMediaAACCaps->object_type & BTR_AAC_OT_MPEG4_AAC_LTP) {
            lstBTMediaAacConfig.object_type = BTR_AAC_OT_MPEG4_AAC_LTP;
        }
        else if (apBtMediaAACCaps->object_type & BTR_AAC_OT_MPEG4_AAC_SCA) {
            lstBTMediaAacConfig.object_type = BTR_AAC_OT_MPEG4_AAC_SCA;
        }
        else {
            BTRCORELOG_ERROR ("No supported aac object type\n");
            return -1;
        }


        if (apBtMediaAACCaps->channels & BTR_AAC_CHANNELS_1) {
            lstBTMediaAacConfig.channels = BTR_AAC_CHANNELS_1;
        }
        else if (apBtMediaAACCaps->channels & BTR_AAC_CHANNELS_2) {
            lstBTMediaAacConfig.channels = BTR_AAC_CHANNELS_2;
        }
        else {
            BTRCORELOG_ERROR ("No supported aac channel\n");
            return -1;
        }

        lstBTMediaAacConfig.vbr = apBtMediaAACCaps->vbr;
        lstBTMediaAacConfig.rfa = apBtMediaAACCaps->rfa;


        BTRCORELOG_TRACE("Negotiated Configuration\n");
        BTRCORELOG_INFO ("object_type  = %d\n", lstBTMediaAacConfig.object_type);
        BTRCORELOG_INFO ("frequency1   = %d\n", lstBTMediaAacConfig.frequency1);
        BTRCORELOG_INFO ("rfa          = %d\n", lstBTMediaAacConfig.rfa);
        BTRCORELOG_INFO ("channels     = %d\n", lstBTMediaAacConfig.channels);
        BTRCORELOG_INFO ("frequency2   = %d\n", lstBTMediaAacConfig.frequency2);
        BTRCORELOG_INFO ("bitrate1     = %d\n", lstBTMediaAacConfig.bitrate1);
        BTRCORELOG_INFO ("vbr          = %d\n", lstBTMediaAacConfig.vbr);
        BTRCORELOG_INFO ("bitrate2     = %d\n", lstBTMediaAacConfig.bitrate2);
        BTRCORELOG_INFO ("bitrate3     = %d\n", lstBTMediaAacConfig.bitrate3);

        if (pstlhBTRCoreAVM) {
            a2dp_aac_t* lpstBTAVMMediaAacConfigOut  = (a2dp_aac_t*)pstlhBTRCoreAVM->pstBTMediaConfigOut;
            a2dp_aac_t* lpstBTAVMMediaAacConfigIn   = (a2dp_aac_t*)pstlhBTRCoreAVM->pstBTMediaConfigIn;

            if (lpstBTAVMMediaAacConfigOut &&
                (aenBTDeviceType == enBTDevAudioSink) &&
                (pstlhBTRCoreAVM->eAVMediaTypeOut != eBTRCoreAVMTypeAAC)) {
                pstlhBTRCoreAVM->eAVMediaTypeOut          =  eBTRCoreAVMTypeAAC;
                lpstBTAVMMediaAacConfigOut->object_type   =  lstBTMediaAacConfig.object_type;
                lpstBTAVMMediaAacConfigOut->frequency1    =  lstBTMediaAacConfig.frequency1;
                lpstBTAVMMediaAacConfigOut->rfa           =  lstBTMediaAacConfig.rfa;
                lpstBTAVMMediaAacConfigOut->channels      =  lstBTMediaAacConfig.channels;
                lpstBTAVMMediaAacConfigOut->frequency2    =  lstBTMediaAacConfig.frequency2;
                lpstBTAVMMediaAacConfigOut->bitrate1      =  lstBTMediaAacConfig.bitrate1;
                lpstBTAVMMediaAacConfigOut->vbr           =  lstBTMediaAacConfig.vbr;
                lpstBTAVMMediaAacConfigOut->bitrate2      =  lstBTMediaAacConfig.bitrate2;
                lpstBTAVMMediaAacConfigOut->bitrate3      =  lstBTMediaAacConfig.bitrate3;

                *appBtMediaCapsOutput  = (void*)lpstBTAVMMediaAacConfigOut;
            }

            if (lpstBTAVMMediaAacConfigIn &&
                (aenBTDeviceType == enBTDevAudioSource) &&
                (pstlhBTRCoreAVM->eAVMediaTypeIn != eBTRCoreAVMTypeAAC)) {
                pstlhBTRCoreAVM->eAVMediaTypeIn          =  eBTRCoreAVMTypeAAC;
                lpstBTAVMMediaAacConfigIn->object_type   =  lstBTMediaAacConfig.object_type;
                lpstBTAVMMediaAacConfigIn->frequency1    =  lstBTMediaAacConfig.frequency1;
                lpstBTAVMMediaAacConfigIn->rfa           =  lstBTMediaAacConfig.rfa;
                lpstBTAVMMediaAacConfigIn->channels      =  lstBTMediaAacConfig.channels;
                lpstBTAVMMediaAacConfigIn->frequency2    =  lstBTMediaAacConfig.frequency2;
                lpstBTAVMMediaAacConfigIn->bitrate1      =  lstBTMediaAacConfig.bitrate1;
                lpstBTAVMMediaAacConfigIn->vbr           =  lstBTMediaAacConfig.vbr;
                lpstBTAVMMediaAacConfigIn->bitrate2      =  lstBTMediaAacConfig.bitrate2;
                lpstBTAVMMediaAacConfigIn->bitrate3      =  lstBTMediaAacConfig.bitrate3;

                *appBtMediaCapsOutput  = (void*)lpstBTAVMMediaAacConfigIn;
            }
        }
#endif
    }
    else if (aenBTMediaType == enBTMediaTypeMP3) {

    }

    return *appBtMediaCapsOutput ? 0 : -1;
}


static int
btrCore_AVMedia_TransportPathCb (
    const char*     apBtMediaTransportPath,
    const char*     apBtMediaUuid,
    void*           apBtMediaCaps,
    enBTDeviceType  aenBTDeviceType,
    enBTMediaType   aenBTMediaType,
    void*           apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     i32BtRet = -1;

    if (!apBtMediaTransportPath) {
        BTRCORELOG_ERROR ("Invalid transport path\n");
        return -1;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    if (apBtMediaCaps && apBtMediaUuid) {
        if (aenBTMediaType == enBTMediaTypeSBC) {
            a2dp_sbc_t* apBtMediaSBCCaps = NULL;
            a2dp_sbc_t  lstBTMediaSbcConfig;

            apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCaps;

            lstBTMediaSbcConfig.channel_mode        =   apBtMediaSBCCaps->channel_mode;
            lstBTMediaSbcConfig.frequency           =   apBtMediaSBCCaps->frequency;
            lstBTMediaSbcConfig.allocation_method   =   apBtMediaSBCCaps->allocation_method;
            lstBTMediaSbcConfig.subbands            =   apBtMediaSBCCaps->subbands;
            lstBTMediaSbcConfig.block_length        =   apBtMediaSBCCaps->block_length;

            if (apBtMediaSBCCaps->min_bitpool == MIN_BITPOOL)
                lstBTMediaSbcConfig.min_bitpool = (uint8_t) MAX(MIN_BITPOOL, apBtMediaSBCCaps->min_bitpool);
            else
                lstBTMediaSbcConfig.min_bitpool = apBtMediaSBCCaps->min_bitpool;

            if (apBtMediaSBCCaps->max_bitpool == MAX_BITPOOL)
                lstBTMediaSbcConfig.max_bitpool = (uint8_t) MIN(btrCore_AVMedia_GetA2DPDefaultBitpool(lstBTMediaSbcConfig.frequency, 
                                                                                                      lstBTMediaSbcConfig.channel_mode),
                                                                                                      apBtMediaSBCCaps->max_bitpool);
            else
                lstBTMediaSbcConfig.max_bitpool = apBtMediaSBCCaps->max_bitpool;

            BTRCORELOG_TRACE("Set Configuration\n");
            BTRCORELOG_INFO ("channel_mode       = %d\n", lstBTMediaSbcConfig.channel_mode);
            BTRCORELOG_INFO ("frequency          = %d\n", lstBTMediaSbcConfig.frequency);
            BTRCORELOG_INFO ("allocation_method  = %d\n", lstBTMediaSbcConfig.allocation_method);
            BTRCORELOG_INFO ("subbands           = %d\n", lstBTMediaSbcConfig.subbands);
            BTRCORELOG_INFO ("block_length       = %d\n", lstBTMediaSbcConfig.block_length);
            BTRCORELOG_INFO ("min_bitpool        = %d\n", lstBTMediaSbcConfig.min_bitpool);
            BTRCORELOG_INFO ("max_bitpool        = %d\n", lstBTMediaSbcConfig.max_bitpool);

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
                a2dp_sbc_t*     lpstBTAVMMediaSbcConfig = NULL;

                if (!strncmp(apBtMediaUuid, BT_UUID_A2DP_SOURCE, strlen(BT_UUID_A2DP_SOURCE))) {
                    lpstBTAVMMediaSbcConfig         = pstlhBTRCoreAVM->pstBTMediaConfigOut;
                    pstlhBTRCoreAVM->eAVMediaTypeOut= eBTRCoreAVMTypeSBC;
                }
                else if (!strncmp(apBtMediaUuid, BT_UUID_A2DP_SINK, strlen(BT_UUID_A2DP_SINK))) {
                    lpstBTAVMMediaSbcConfig         = pstlhBTRCoreAVM->pstBTMediaConfigIn;
                    pstlhBTRCoreAVM->eAVMediaTypeIn = eBTRCoreAVMTypeSBC;
                }
                else {
                    lpstBTAVMMediaSbcConfig         = pstlhBTRCoreAVM->pstBTMediaConfigOut;
                    pstlhBTRCoreAVM->eAVMediaTypeOut= eBTRCoreAVMTypeSBC;
                }

                if (lpstBTAVMMediaSbcConfig) {
                    lpstBTAVMMediaSbcConfig->channel_mode        =  lstBTMediaSbcConfig.channel_mode;
                    lpstBTAVMMediaSbcConfig->frequency           =  lstBTMediaSbcConfig.frequency;
                    lpstBTAVMMediaSbcConfig->allocation_method   =  lstBTMediaSbcConfig.allocation_method;
                    lpstBTAVMMediaSbcConfig->subbands            =  lstBTMediaSbcConfig.subbands;
                    lpstBTAVMMediaSbcConfig->block_length        =  lstBTMediaSbcConfig.block_length;
                    lpstBTAVMMediaSbcConfig->min_bitpool         =  lstBTMediaSbcConfig.min_bitpool;
                    lpstBTAVMMediaSbcConfig->max_bitpool         =  lstBTMediaSbcConfig.max_bitpool;
                    i32BtRet = 0;
                }
            }
        }
        else if (aenBTMediaType == enBTMediaTypePCM) {

        }
        else if (aenBTMediaType == enBTMediaTypeAAC) {
#if defined(AAC_SUPPORTED)
            a2dp_aac_t* apBtMediaAACCaps = NULL;
            a2dp_aac_t  lstBTMediaAacConfig;

            apBtMediaAACCaps = (a2dp_aac_t*)apBtMediaCaps;

            lstBTMediaAacConfig.object_type =   apBtMediaAACCaps->object_type;
            lstBTMediaAacConfig.frequency1  =   apBtMediaAACCaps->frequency1;
            lstBTMediaAacConfig.rfa         =   apBtMediaAACCaps->rfa;
            lstBTMediaAacConfig.channels    =   apBtMediaAACCaps->channels;
            lstBTMediaAacConfig.frequency2  =   apBtMediaAACCaps->frequency2;
            lstBTMediaAacConfig.bitrate1    =   apBtMediaAACCaps->bitrate1;
            lstBTMediaAacConfig.vbr         =   apBtMediaAACCaps->vbr;
            lstBTMediaAacConfig.bitrate2    =   apBtMediaAACCaps->bitrate2;
            lstBTMediaAacConfig.bitrate3    =   apBtMediaAACCaps->bitrate3;

            BTRCORELOG_TRACE("Set Configuration\n");
            BTRCORELOG_INFO ("object_type   = %d\n",  lstBTMediaAacConfig.object_type);
            BTRCORELOG_INFO ("frequency1    = %d\n",  lstBTMediaAacConfig.frequency1);
            BTRCORELOG_INFO ("rfa           = %d\n",  lstBTMediaAacConfig.rfa);
            BTRCORELOG_INFO ("channels      = %d\n",  lstBTMediaAacConfig.channels);
            BTRCORELOG_INFO ("frequency2    = %d\n",  lstBTMediaAacConfig.frequency2);
            BTRCORELOG_INFO ("bitrate1      = %d\n",  lstBTMediaAacConfig.bitrate1);
            BTRCORELOG_INFO ("vbr           = %d\n",  lstBTMediaAacConfig.vbr);
            BTRCORELOG_INFO ("bitrate2      = %d\n",  lstBTMediaAacConfig.bitrate2);
            BTRCORELOG_INFO ("bitrate3      = %d\n",  lstBTMediaAacConfig.bitrate3);

            //TODO: Best possible Generic solution for DELIA-23555 at this moment
            sleep(1);

            if (pstlhBTRCoreAVM) {
                a2dp_aac_t*     lpstBTAVMMediaAacConfig = NULL;

                if (!strncmp(apBtMediaUuid, BT_UUID_A2DP_SOURCE, strlen(BT_UUID_A2DP_SOURCE))) {
                    lpstBTAVMMediaAacConfig         = pstlhBTRCoreAVM->pstBTMediaConfigOut;
                    pstlhBTRCoreAVM->eAVMediaTypeOut= eBTRCoreAVMTypeAAC;
                }
                else if (!strncmp(apBtMediaUuid, BT_UUID_A2DP_SINK, strlen(BT_UUID_A2DP_SINK))) {
                    lpstBTAVMMediaAacConfig         = pstlhBTRCoreAVM->pstBTMediaConfigIn;
                    pstlhBTRCoreAVM->eAVMediaTypeIn = eBTRCoreAVMTypeAAC;
                }
                else {
                    lpstBTAVMMediaAacConfig         = pstlhBTRCoreAVM->pstBTMediaConfigOut;
                    pstlhBTRCoreAVM->eAVMediaTypeOut= eBTRCoreAVMTypeAAC;
                }

                if (lpstBTAVMMediaAacConfig) {
                    lpstBTAVMMediaAacConfig->object_type  =  lstBTMediaAacConfig.object_type;
                    lpstBTAVMMediaAacConfig->frequency1   =  lstBTMediaAacConfig.frequency1;
                    lpstBTAVMMediaAacConfig->rfa          =  lstBTMediaAacConfig.rfa;
                    lpstBTAVMMediaAacConfig->channels     =  lstBTMediaAacConfig.channels;
                    lpstBTAVMMediaAacConfig->frequency2   =  lstBTMediaAacConfig.frequency2;
                    lpstBTAVMMediaAacConfig->bitrate1     =  lstBTMediaAacConfig.bitrate1;
                    lpstBTAVMMediaAacConfig->vbr          =  lstBTMediaAacConfig.vbr;
                    lpstBTAVMMediaAacConfig->bitrate2     =  lstBTMediaAacConfig.bitrate2;
                    lpstBTAVMMediaAacConfig->bitrate3     =  lstBTMediaAacConfig.bitrate3;
                    i32BtRet = 0;
                }
            }
#endif
        }
        else if (aenBTMediaType == enBTMediaTypeMP3) {

        }
    }
    else {
        a2dp_sbc_t lstBtA2dpSbcCaps;
        lstBtA2dpSbcCaps.channel_mode       = BTR_SBC_CHANNEL_MODE_MONO | BTR_SBC_CHANNEL_MODE_DUAL_CHANNEL |
                                              BTR_SBC_CHANNEL_MODE_STEREO | BTR_SBC_CHANNEL_MODE_JOINT_STEREO;
        lstBtA2dpSbcCaps.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                              BTR_SBC_SAMPLING_FREQ_44100 | BTR_SBC_SAMPLING_FREQ_48000;
        lstBtA2dpSbcCaps.allocation_method  = BTR_SBC_ALLOCATION_SNR | BTR_SBC_ALLOCATION_LOUDNESS;
        lstBtA2dpSbcCaps.subbands           = BTR_SBC_SUBBANDS_4 | BTR_SBC_SUBBANDS_8;
        lstBtA2dpSbcCaps.block_length       = BTR_SBC_BLOCK_LENGTH_4 | BTR_SBC_BLOCK_LENGTH_8 |
                                              BTR_SBC_BLOCK_LENGTH_12 | BTR_SBC_BLOCK_LENGTH_16;
        lstBtA2dpSbcCaps.min_bitpool        = MIN_BITPOOL;
        lstBtA2dpSbcCaps.max_bitpool        = MAX_BITPOOL;

        if (pstlhBTRCoreAVM) {
            void*     lpstBTAVMMediaConfig   = NULL;

            if (pstlhBTRCoreAVM->pcAVMediaTransportPathOut) {
                if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPathOut, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPathOut))) {
                    BTRCORELOG_INFO ("Freeing %p:%s\n", pstlhBTRCoreAVM->pcAVMediaTransportPathOut, pstlhBTRCoreAVM->pcAVMediaTransportPathOut);

                    free(pstlhBTRCoreAVM->pcAVMediaTransportPathOut);
                    pstlhBTRCoreAVM->pcAVMediaTransportPathOut = NULL;
                    pstlhBTRCoreAVM->eAVMediaTypeOut           = eBTRCoreAVMTypeUnknown;

                    lpstBTAVMMediaConfig = pstlhBTRCoreAVM->pstBTMediaConfigOut;
                    i32BtRet = 0;
                }
            }

            if (pstlhBTRCoreAVM->pcAVMediaTransportPathIn) {
                if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPathIn, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPathIn))) {
                    BTRCORELOG_INFO ("Freeing %p:%s\n", pstlhBTRCoreAVM->pcAVMediaTransportPathIn, pstlhBTRCoreAVM->pcAVMediaTransportPathIn);
                    
                    free(pstlhBTRCoreAVM->pcAVMediaTransportPathIn);
                    pstlhBTRCoreAVM->pcAVMediaTransportPathIn = NULL;
                    pstlhBTRCoreAVM->eAVMediaTypeIn           = eBTRCoreAVMTypeUnknown;

                    lpstBTAVMMediaConfig = pstlhBTRCoreAVM->pstBTMediaConfigIn;
                    i32BtRet = 0;
                }
            }

            if (lpstBTAVMMediaConfig) {
                BTRCORELOG_TRACE("Reset Media Configuration\n");
                memcpy(lpstBTAVMMediaConfig, &lstBtA2dpSbcCaps, sizeof(a2dp_sbc_t));
            }
        }
    }


    if (pstlhBTRCoreAVM) {
        if (apBtMediaUuid && !strncmp(apBtMediaUuid, BT_UUID_A2DP_SOURCE, strlen(BT_UUID_A2DP_SOURCE))) {
            if (pstlhBTRCoreAVM->pcAVMediaTransportPathOut) {
                if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPathOut, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPathOut))) {
                    BTRCORELOG_ERROR ("Freeing %p:%s - Unexpected\n", pstlhBTRCoreAVM->pcAVMediaTransportPathOut, pstlhBTRCoreAVM->pcAVMediaTransportPathOut);
                    i32BtRet = 0;
                 }

                free(pstlhBTRCoreAVM->pcAVMediaTransportPathOut);
                pstlhBTRCoreAVM->pcAVMediaTransportPathOut = NULL;
            }
            else {
                pstlhBTRCoreAVM->pcAVMediaTransportPathOut = strndup(apBtMediaTransportPath, BTRCORE_MAX_STR_LEN - 1);
            }
        }
        else if (apBtMediaUuid && !strncmp(apBtMediaUuid, BT_UUID_A2DP_SINK, strlen(BT_UUID_A2DP_SINK))) {
            if (pstlhBTRCoreAVM->pcAVMediaTransportPathIn) {
                if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPathIn, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPathIn))) {
                    BTRCORELOG_ERROR ("Freeing %p:%s - Unexpected\n", pstlhBTRCoreAVM->pcAVMediaTransportPathIn, pstlhBTRCoreAVM->pcAVMediaTransportPathIn);
                    i32BtRet = 0;
                 }

                free(pstlhBTRCoreAVM->pcAVMediaTransportPathIn);
                pstlhBTRCoreAVM->pcAVMediaTransportPathIn = NULL;
            }
            else {
                pstlhBTRCoreAVM->pcAVMediaTransportPathIn = strndup(apBtMediaTransportPath, BTRCORE_MAX_STR_LEN - 1);
            }
        }
    }


    return i32BtRet;
}


static int
btrCore_AVMedia_MediaPlayerPathCb (
    const char*     apcBTMediaPlayerPath,
    void*           apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    int                     i32BtRet = -1;

    if (!apcBTMediaPlayerPath) {
        BTRCORELOG_ERROR ("Invalid media path\n");
        return i32BtRet;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    if (pstlhBTRCoreAVM) {
        if (pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
            char* ptr = pstlhBTRCoreAVM->pcAVMediaPlayerPath;

            if (!strncmp(pstlhBTRCoreAVM->pcAVMediaPlayerPath, apcBTMediaPlayerPath,
                 (strlen(pstlhBTRCoreAVM->pcAVMediaPlayerPath) < strlen(apcBTMediaPlayerPath)) ? strlen(pstlhBTRCoreAVM->pcAVMediaPlayerPath) : strlen(apcBTMediaPlayerPath))) {
                BTRCORELOG_INFO ("Freeing 0x%p:%s\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, pstlhBTRCoreAVM->pcAVMediaPlayerPath);
         
                pstlhBTRCoreAVM->pcAVMediaPlayerPath = NULL;
                free(ptr);
            }
            else {
                BTRCORELOG_INFO ("Switching Media Player from  %s  to  %s\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, apcBTMediaPlayerPath);
                pstlhBTRCoreAVM->pcAVMediaPlayerPath = strndup(apcBTMediaPlayerPath, BTRCORE_MAX_STR_LEN - 1);
                free(ptr);
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
    stBTRCoreAVMediaStatusUpdate    mediaStatus;
    stBTRCoreAVMediaHdl*            pstlhBTRCoreAVM = NULL;
    BOOLEAN                         postEvent       = FALSE;
    int                             i32BtRet        = 0;

    if (!apcBtDevAddr || !apstBtMediaStUpdate || !apUserData) {
       BTRCORELOG_ERROR ("enBTRCoreInvalidArg!!!\n");
       return -1;
    }
    BTRCORELOG_DEBUG ("AV Media Status Cb\n");

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;


    switch (apstBtMediaStUpdate->aenBtOpIfceType) {
    case enBTMediaTransport:

        switch (apstBtMediaStUpdate->aunBtOpIfceProp.enBtMediaTransportProp) {
        case enBTMedTPropState:

            switch (apstBtMediaStUpdate->m_mediaTransportState) {
            case enBTMedTransportStIdle:
                pstlhBTRCoreAVM->eAVMTState = enAVMTransportStDisconnected;
                break;
            case enBTMedTransportStPending:
                pstlhBTRCoreAVM->eAVMTState = enAVMTransportStToBeConnected;
                break;
            case enBTMedTransportStActive:
                pstlhBTRCoreAVM->eAVMTState = enAVMTransportStConnected;
                break;
            default:
                break;
            }
            BTRCORELOG_DEBUG ("AV Media Transport State : %d\n", pstlhBTRCoreAVM->eAVMTState);
            break;

        case enBTMedTPropVol:
            pstlhBTRCoreAVM->ui16BTMediaTransportVolume = apstBtMediaStUpdate->m_mediaTransportVolume;
            BTRCORELOG_DEBUG ("AV Media Transport Volume : %d \n", (pstlhBTRCoreAVM->ui16BTMediaTransportVolume * 100)/127);
            break;
        default:
            break;
        }
        break;

    case enBTMediaControl:

        switch (apstBtMediaStUpdate->aunBtOpIfceProp.enBtMediaControlProp) {
        case enBTMedControlPropConnected:
            if (enBTDevAudioSource == aeBtDeviceType) {
                if (apstBtMediaStUpdate->m_mediaPlayerConnected) {
                    /* Among If-Added, Property-MediaPlayerConnected & Property-MediaPlayerPath CBs, lets use
                     * the Property-MediaPlayerConnected CB to spwan playback position polling thread as it is
                     * the last signal and a complete confirmation of readiness of the bottom layer.
                    */
                    if (!(pstlhBTRCoreAVM->bAVMediaPlayerConnected || pstlhBTRCoreAVM->pMediaPollingThread)) {
                        stBTRCoreAVMediaStatusUserData* pstAVMediaStUserData = (stBTRCoreAVMediaStatusUserData*) malloc (sizeof(stBTRCoreAVMediaStatusUserData));
                        memset (pstAVMediaStUserData, 0, sizeof(stBTRCoreAVMediaStatusUserData));
                        memset (&pstlhBTRCoreAVM->pstAVMediaPlayer, 0, sizeof(stBTRCoreAVMediaPlayer));

                        pstAVMediaStUserData->apvAVMUserData   = pstlhBTRCoreAVM->btIfceHdl; //TODO: This is redundant. There should be no need to do this
                        pstAVMediaStUserData->apcAVMDevAddress = strndup(apcBtDevAddr, BTRCORE_MAX_STR_LEN - 1);

                        pstlhBTRCoreAVM->pvThreadData = (void*)pstAVMediaStUserData;

                        pstlhBTRCoreAVM->pMediaPollingThread = g_thread_new ("btrCore_AVMedia_PlaybackPositionPolling", btrCore_AVMedia_PlaybackPositionPolling, (void*)(pstlhBTRCoreAVM));

                        if (!pstlhBTRCoreAVM->pMediaPollingThread) {
                            BTRCORELOG_ERROR ("Failed to thread create btrCore_AVMedia_PlaybackPositionPolling");
                            i32BtRet = -1;
                        }
                    }
                    else {
                        BTRCORELOG_WARN ("btrCore_AVMedia_PlaybackPositionPolling thread is running already!!!");
                    }
                }
                else {
                    if (pstlhBTRCoreAVM->bAVMediaPlayerConnected && pstlhBTRCoreAVM->pMediaPollingThread) {
                        /* Exit playback position polling thread */
                        pstlhBTRCoreAVM->pstAVMediaPlayer.eAVMediaStatusUpdate = eBTRCoreAVMediaPlaybackEnded;
                        g_thread_join (pstlhBTRCoreAVM->pMediaPollingThread);

                        if (pstlhBTRCoreAVM->pvThreadData) {
                            if (((stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData)->apcAVMDevAddress) {
                                free ((void*)((stBTRCoreAVMediaStatusUserData*)pstlhBTRCoreAVM->pvThreadData)->apcAVMDevAddress);
                            }
                            free (pstlhBTRCoreAVM->pvThreadData);
                            pstlhBTRCoreAVM->pvThreadData = NULL;
                        }
                        pstlhBTRCoreAVM->pMediaPollingThread  = NULL;
                        pstlhBTRCoreAVM->eAVMTState = enAVMTransportStDisconnected;

                        BTRCORELOG_INFO ("Successfully Exited Media Position Polling Thread\n");

                        /* Lets look for a better place inorder to be in sync with the bottom layer */
                        if (pstlhBTRCoreAVM->pcAVMediaPlayerPath) {
                            BTRCORELOG_INFO ("Freeing 0x%p:%s\n", pstlhBTRCoreAVM->pcAVMediaPlayerPath, pstlhBTRCoreAVM->pcAVMediaPlayerPath);
                            free (pstlhBTRCoreAVM->pcAVMediaPlayerPath);
                            pstlhBTRCoreAVM->pcAVMediaPlayerPath = NULL;
                        }
                    }
                    else {
                        BTRCORELOG_ERROR ("pstlhBTRCoreAVM->pMediaPollingThread doesn't exists!!!");
                        i32BtRet = -1;
                    }
                }
                pstlhBTRCoreAVM->bAVMediaPlayerConnected = apstBtMediaStUpdate->m_mediaPlayerConnected;
            }
            else {
                BTRCORELOG_DEBUG ("Device Type : %d\n", aeBtDeviceType);
            }

            BTRCORELOG_DEBUG ("AV Media Player Connected : %s - %d\n", apcBtDevAddr, apstBtMediaStUpdate->m_mediaPlayerConnected); 
            break;

        case enBTMedControlPropPath:
            BTRCORELOG_DEBUG ("AV Media Player connected Path : %s\n", apstBtMediaStUpdate->m_mediaPlayerPath);
            break;
        default:
            break;
        }
        break;

    case enBTMediaPlayer:
        {
            stBTRCoreAVMediaPlayer*  lstAVMediaPlayer = &pstlhBTRCoreAVM->pstAVMediaPlayer;

            switch (apstBtMediaStUpdate->aunBtOpIfceProp.enBtMediaPlayerProp) {
            case enBTMedPlayerPropName:
                strncpy(lstAVMediaPlayer->m_mediaPlayerName, apstBtMediaStUpdate->m_mediaPlayerName, BTRCORE_MAX_STR_LEN - 1);
                BTRCORELOG_DEBUG ("AV Media Player Name : %s\n", lstAVMediaPlayer->m_mediaPlayerName);
                break;

            case enBTMedPlayerPropType:
                switch (apstBtMediaStUpdate->enMediaPlayerType) {
                case enBTMedPlayerTypAudio:
                    lstAVMediaPlayer->eAVMediaPlayerType = eBTRCoreAVMPTypAudio;
                    break;
                case enBTMedPlayerTypVideo:
                    lstAVMediaPlayer->eAVMediaPlayerType = eBTRCoreAVMPTypVideo;
                    break;
                case enBTMedPlayerTypAudioBroadcasting:
                    lstAVMediaPlayer->eAVMediaPlayerType = eBTRCoreAVMPTypAudioBroadcasting;
                    break;
                case enBTMedPlayerTypVideoBroadcasting:
                    lstAVMediaPlayer->eAVMediaPlayerType = eBTRCoreAVMPTypVideoBroadcasting;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaPlayerType = eBTRCoreAVMPTypUnknown;
                }
                BTRCORELOG_DEBUG ("AV Media Player Type : %d\n", lstAVMediaPlayer->eAVMediaPlayerType);
                break;

            case enBTMedPlayerPropSubtype:
                switch (apstBtMediaStUpdate->enMediaPlayerSubtype) {
                case enBTMedPlayerSbTypAudioBook:
                    lstAVMediaPlayer->eAVMediaPlayerSubtype = eBTRCoreAVMPSbTypAudioBook;
                    break;
                case enBTMedPlayerSbTypPodcast:
                    lstAVMediaPlayer->eAVMediaPlayerSubtype = eBTRCoreAVMPSbTypPodcast;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaPlayerSubtype = eBTRCoreAVMPSbTypUnknown;
                }
                BTRCORELOG_DEBUG ("AV Media Player Subtype : %d\n", lstAVMediaPlayer->eAVMediaPlayerSubtype);
                break;

            case enBTMedPlayerPropTrack:
                mediaStatus.eAVMediaState = eBTRCoreAVMediaTrkStChanged;
                lstAVMediaPlayer->m_mediaTrackChanged = 1;

                /* As per the analysed results, property 'Duration' alone may get updated seperately apart from the entire Track Info.
                   This behavior of bluez along with UUID=0* for certain song items lead to this issue of replacing the TrackInfo at
                   bluez layer with only Duration info. Thus a TrackInfo Query to bluez will fetch only Duration or the last updated.
                   Hence the below logic is framed to store TrackInfo in our DB only if we have a valid 'Title'** val. Else our stack
                   will interpret it as a Duration update(which is occuring quite frequently, which caused RDK-23133).
                   *  - UUID!=0 for song item will create a iface for it at bluez layer, hence any updation will persist entire TrkInfo.
                      - UUID==0 for song item will not create a iface and its Trk details are stored in bluez's MP Track which will be
                                overwritten by any further updates.
                   ** - 'Title' property has been choosed as a key here, since it contains value for sure, even when album/artist/Genre
                                could be empty(as per observations). And it didn't show any updatation within a song's scope.
                */
                if (apstBtMediaStUpdate->m_mediaTrackInfo.pcTitle[0]) {
                    memset(&lstAVMediaPlayer->m_mediaTrackInfo, 0, sizeof(stBTRCoreAVMediaTrackInfo));
                    memcpy(&lstAVMediaPlayer->m_mediaTrackInfo, &apstBtMediaStUpdate->m_mediaTrackInfo, sizeof(stBTRCoreAVMediaTrackInfo));
                } else {
                    if (apstBtMediaStUpdate->m_mediaTrackInfo.ui32Duration) {
                        lstAVMediaPlayer->m_mediaTrackInfo.ui32Duration = apstBtMediaStUpdate->m_mediaTrackInfo.ui32Duration;
                    }
                    if (apstBtMediaStUpdate->m_mediaTrackInfo.pcArtist[0]) {
                        strncpy (lstAVMediaPlayer->m_mediaTrackInfo.pcArtist, apstBtMediaStUpdate->m_mediaTrackInfo.pcArtist, BTRCORE_MAX_STR_LEN - 1);
                    }
                    if (apstBtMediaStUpdate->m_mediaTrackInfo.pcAlbum[0]) {
                        strncpy (lstAVMediaPlayer->m_mediaTrackInfo.pcAlbum, apstBtMediaStUpdate->m_mediaTrackInfo.pcAlbum, BTRCORE_MAX_STR_LEN - 1);
                    }
                    if (apstBtMediaStUpdate->m_mediaTrackInfo.pcGenre[0]) {
                        strncpy (lstAVMediaPlayer->m_mediaTrackInfo.pcGenre, apstBtMediaStUpdate->m_mediaTrackInfo.pcGenre, BTRCORE_MAX_STR_LEN - 1);
                    }
                }

                memcpy(&mediaStatus.m_mediaTrackInfo, &lstAVMediaPlayer->m_mediaTrackInfo, sizeof(stBTRCoreAVMediaTrackInfo));
                BTRCORELOG_DEBUG ("AV Media Player Track Updated : %s", mediaStatus.m_mediaTrackInfo.pcTitle);
                postEvent = TRUE;
                break;

            case enBTMedPlayerPropEqualizer:
                lstAVMediaPlayer->m_mediaPlayerEqualizer    = apstBtMediaStUpdate->m_mediaPlayerEqualizer;
                BTRCORELOG_DEBUG ("AV Media Player Equalizer : %d\n", lstAVMediaPlayer->m_mediaPlayerEqualizer);
                break;

            case enBTMedPlayerPropShuffle:
                switch (apstBtMediaStUpdate->enMediaPlayerShuffle) {
                case enBTMedPlayerShuffleOff:
                    lstAVMediaPlayer->eAVMediaPlayerShuffle = enBTRCoreAVMediaCtrlShflOff;
                    break;
                case enBTMedPlayerShuffleAllTracks:
                    lstAVMediaPlayer->eAVMediaPlayerShuffle = enBTRCoreAVMediaCtrlShflAllTracks;
                    break;
                case enBTMedPlayerShuffleGroup:
                    lstAVMediaPlayer->eAVMediaPlayerShuffle = enBTRCoreAVMediaCtrlShflGroup;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaPlayerShuffle = enBTRCoreAVMediaCtrlUnknown;
                }
                BTRCORELOG_DEBUG ("AV Media Player Shuffle : %d\n", lstAVMediaPlayer->eAVMediaPlayerShuffle);
                break;

            case enBTMedPlayerPropRepeat:
                switch (apstBtMediaStUpdate->enMediaPlayerRepeat) {
                case enBTMedPlayerRpOff:
                    lstAVMediaPlayer->eAVMediaPlayerRepeat = enBTRCoreAVMediaCtrlRptOff;
                    break;
                case enBTMedPlayerRpSingleTrack:
                    lstAVMediaPlayer->eAVMediaPlayerRepeat = enBTRCoreAVMediaCtrlRptSingleTrack;
                    break;
                case enBTMedPlayerRpAllTracks:
                    lstAVMediaPlayer->eAVMediaPlayerRepeat = enBTRCoreAVMediaCtrlRptAllTracks;
                    break;
                case enBTMedPlayerRpGroup:
                    lstAVMediaPlayer->eAVMediaPlayerRepeat = enBTRCoreAVMediaCtrlRptGroup;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaPlayerRepeat = enBTRCoreAVMediaCtrlUnknown;
                }
                BTRCORELOG_DEBUG ("AV Media Player Repeat : %d\n", lstAVMediaPlayer->eAVMediaPlayerRepeat);
                break;

            case enBTMedPlayerPropScan:
                switch (apstBtMediaStUpdate->enMediaPlayerScan) {
                case enBTMedPlayerScanOff:
                    lstAVMediaPlayer->eAVMediaPlayerScan = eBTRCoreAVMPScanOff;
                    break;
                case enBTMedPlayerScanAllTracks:
                    lstAVMediaPlayer->eAVMediaPlayerScan = eBTRCoreAVMPScanAllTracks;
                    break;
                case enBTMedPlayerScanGroup:
                    lstAVMediaPlayer->eAVMediaPlayerScan = eBTRCoreAVMPScanGroup;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaPlayerScan = eBTRCoreAVMPScanUnknown;
                }
                BTRCORELOG_DEBUG ("AV Media Player Scan : %d\n", lstAVMediaPlayer->eAVMediaPlayerScan);
                break;

            case enBTMedPlayerPropPosition:
                lstAVMediaPlayer->m_mediaPlayerPosition    = apstBtMediaStUpdate->m_mediaPlayerPosition;
                BTRCORELOG_DEBUG ("AV Media Player Position : %d\n", lstAVMediaPlayer->m_mediaPlayerPosition);
                break;

            case enBTMedPlayerPropStatus:
                switch (apstBtMediaStUpdate->enMediaPlayerStatus) {
                case enBTMedPlayerStPlaying:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaTrkStPlaying;
                    break;
                case enBTMedPlayerStStopped:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaTrkStStopped;
                    break;
                case enBTMedPlayerStPaused:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaTrkStPaused;
                    break;
                case enBTMedPlayerStForwardSeek:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaTrkStForwardSeek;
                    break;
                case enBTMedPlayerStReverseSeek:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaTrkStReverseSeek;
                    break;
                case enBTMedPlayerStError:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaPlaybackError;
                    break;
                default:
                    lstAVMediaPlayer->eAVMediaStatusUpdate = eBTRCoreAVMediaPlaybackError;
                }
                BTRCORELOG_DEBUG ("AV Media Player Status : %d\n", lstAVMediaPlayer->eAVMediaStatusUpdate);
                break;

            case enBTMedPlayerPropBrowsable:
                lstAVMediaPlayer->m_mediaPlayerBrowsable    = apstBtMediaStUpdate->m_mediaPlayerBrowsable;
                BTRCORELOG_DEBUG ("AV Media Player Browsable : %d\n", lstAVMediaPlayer->m_mediaPlayerBrowsable);
                break;

            case enBTMedPlayerPropSearchable:
                lstAVMediaPlayer->m_mediaPlayerSearchable   = apstBtMediaStUpdate->m_mediaPlayerSearchable;
                BTRCORELOG_DEBUG ("AV Media Player Searchable : %d\n", lstAVMediaPlayer->m_mediaPlayerSearchable);
                break;

            default:
                break;
            }
        }
        break;

    case enBTMediaItem:
        break;
    case enBTMediaFolder:
        break;
    default:
        break;
    }

    if (!pstlhBTRCoreAVM->pcBMediaStatusUserData) {
       BTRCORELOG_ERROR ("pstlhBTRCoreAVM->pcBMediaStatusUserData is NULL!!!\n");
       postEvent = FALSE;
       i32BtRet  = -1;
    }

    /* post callback */
    if (postEvent && pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate) {
        if (pstlhBTRCoreAVM->fpcBBTRCoreAVMediaStatusUpdate(&mediaStatus, apcBtDevAddr, pstlhBTRCoreAVM->pcBMediaStatusUserData) != enBTRCoreSuccess) {
            BTRCORELOG_ERROR ("fpcBBTRCoreAVMediaStatusUpdate - Failure !!!\n");
            i32BtRet = -1;
        }
    }

    return i32BtRet;
}
