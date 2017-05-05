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


/* Local Headers */
#include "btrCore_avMedia.h"
#include "btrCore_bt_ifce.h"

#include "btrCore_priv.h"

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

#define BTR_SBC_HIGH_BITRATE_BITPOOL		51
#define BTR_SBC_MED_BITRATE_BITPOOL			33
#define BTR_SBC_LOW_BITRATE_BITPOOL			19

#define BTR_SBC_DEFAULT_BITRATE_BITPOOL		BTR_SBC_HIGH_BITRATE_BITPOOL


typedef struct _stBTRCoreAVMediaHdl {
    eBTRCoreAVMType eAVMediaType;
    a2dp_sbc_t*     pstBTMediaConfig;
    int             iBTMediaDefSampFreqPref;
    char*           pcAVMediaTransportPath;
} stBTRCoreAVMediaHdl;


/* Static Function Prototypes */
static uint8_t btrCore_AVMedia_GetA2DPDefaultBitpool (uint8_t au8SamplingFreq, uint8_t au8AudioChannelsMode);


/* Callbacks */
static void* btrCore_AVMedia_NegotiateMedia_cb (void* apBtMediaCaps, void* apUserData);
static const char* btrCore_AVMedia_TransportPath_cb (const char* apBtMediaTransportPath, void* apBtMediaCaps, void* apUserData);


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
            BTRCORELOG_ERROR ("%d	: %s - Invalid A2DP channels mode %u\n", __LINE__, __FUNCTION__, au8AudioChannelsMode);
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
            BTRCORELOG_ERROR ("%d	: %s - Invalid A2DP channels mode %u\n", __LINE__, __FUNCTION__, au8AudioChannelsMode);
            return 51;
        }
    default:
        BTRCORELOG_ERROR ("%d	: %s - Invalid Bluetooth SBC sampling freq %u\n", __LINE__, __FUNCTION__, au8SamplingFreq);
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
            BTRCORELOG_ERROR ("%d	: %s - Invalid A2DP channels mode %u\n", __LINE__, __FUNCTION__, au8AudioChannelsMode);
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
            BTRCORELOG_ERROR ("%d	: %s - Invalid A2DP channels mode %u\n", __LINE__, __FUNCTION__, au8AudioChannelsMode);
            return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
        }
    default:
        BTRCORELOG_ERROR ("%d	: %s - Invalid Bluetooth SBC sampling freq %u\n", __LINE__, __FUNCTION__, au8SamplingFreq);
        return BTR_SBC_DEFAULT_BITRATE_BITPOOL;
    }
}
#endif


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
    lstBtA2dpCapabilities.frequency          = BTR_SBC_SAMPLING_FREQ_16000 | BTR_SBC_SAMPLING_FREQ_32000 |
                                               BTR_SBC_SAMPLING_FREQ_44100 | BTR_SBC_SAMPLING_FREQ_48000;
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

    lBtAVMediaASinkRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    enBTDevAudioSink,
                                                    A2DP_SOURCE_UUID,
                                                    A2DP_CODEC_SBC,
                                                    (void*)&lstBtA2dpCapabilities,
                                                    sizeof(lstBtA2dpCapabilities),
                                                    lBtAVMediaDelayReport);


    lBtAVMediaASrcRegRet = BtrCore_BTRegisterMedia(apBtConn,
                                                   apBtAdapter,
                                                   enBTDevAudioSource,
                                                   A2DP_SINK_UUID,
                                                   A2DP_CODEC_SBC,
                                                   (void*)&lstBtA2dpCapabilities,
                                                   sizeof(lstBtA2dpCapabilities),
                                                   lBtAVMediaDelayReport);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet)
       lBtAVMediaNegotiateRet = BtrCore_BTRegisterNegotiateMediacB(apBtConn,
                                                                   apBtAdapter,
                                                                   &btrCore_AVMedia_NegotiateMedia_cb,
                                                                   pstlhBTRCoreAVM);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet)
        lBtAVMediaTransportPRet = BtrCore_BTRegisterTransportPathMediacB(apBtConn,
                                                                         apBtAdapter,
                                                                         &btrCore_AVMedia_TransportPath_cb,
                                                                         pstlhBTRCoreAVM);

    if (!lBtAVMediaASinkRegRet && !lBtAVMediaASrcRegRet && !lBtAVMediaNegotiateRet && !lBtAVMediaTransportPRet)
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

            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui32AVMSFreq          = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui32AVMSFreq);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui32AVMAChan          = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui32AVMAChan);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui8AVMSbcAllocMethod  = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcAllocMethod);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui8AVMSbcSubbands     = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcSubbands);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui8AVMSbcBlockLength  = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcBlockLength);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui8AVMSbcMinBitpool   = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcMinBitpool);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui8AVMSbcMaxBitpool   = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui8AVMSbcMaxBitpool);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui16AVMSbcFrameLen    = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui16AVMSbcFrameLen);
            BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_GetCurMediaInfo: ui16AVMSbcBitrate     = %d\n", __LINE__, __FUNCTION__, pstBtrCoreAVMediaSbcInfo-> ui16AVMSbcBitrate);
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

    if ((lBtAVMediaRet = BtrCore_BTGetProp(apBtConn, pstlhBTRCoreAVM->pcAVMediaTransportPath, enBTMediaTransport, "Delay", &ui16Delay)))
        lenBTRCoreRet = enBTRCoreFailure;

    BTRCORELOG_INFO ("%d	: %s - BTRCore_AVMedia_AcquireDataPath: Delay value = %d\n", __LINE__, __FUNCTION__, ui16Delay);

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


static void*
btrCore_AVMedia_NegotiateMedia_cb (
    void* apBtMediaCaps,
    void* apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;
    a2dp_sbc_t*             apBtMediaSBCCaps = NULL;
    a2dp_sbc_t              lstBTMediaSBCConfig;

    if (!apBtMediaCaps) {
        BTRCORELOG_ERROR ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: Invalid input MT Media Capabilities\n", __LINE__, __FUNCTION__);
        return NULL;
    } 

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCaps;

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
        BTRCORELOG_ERROR ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: No supported channel modes\n", __LINE__, __FUNCTION__);
        return NULL;
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
        BTRCORELOG_ERROR ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: No supported block lengths\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    if (apBtMediaSBCCaps->subbands & BTR_A2DP_SUBBANDS_8) {
        lstBTMediaSBCConfig.subbands = BTR_A2DP_SUBBANDS_8;
    }
    else if (apBtMediaSBCCaps->subbands & BTR_A2DP_SUBBANDS_4) {
        lstBTMediaSBCConfig.subbands = BTR_A2DP_SUBBANDS_4;
    }
    else {
        BTRCORELOG_ERROR ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: No supported subbands\n", __LINE__, __FUNCTION__);
        return NULL;
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

    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: Negotiated Configuration\n", __LINE__, __FUNCTION__);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: channel_mode       = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.channel_mode);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: frequency          = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.frequency);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: allocation_method  = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.allocation_method);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: subbands           = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.subbands);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: block_length       = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.block_length);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: min_bitpool        = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.min_bitpool);
    BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_NegotiateMedia_cb: max_bitpool        = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.max_bitpool);

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
    }

    if (pstlhBTRCoreAVM)
        return (void*)(pstlhBTRCoreAVM->pstBTMediaConfig);
    else
        return NULL;
}


static const char*
btrCore_AVMedia_TransportPath_cb (
    const char* apBtMediaTransportPath,
    void*       apBtMediaCaps,
    void*       apUserData
) {
    stBTRCoreAVMediaHdl*    pstlhBTRCoreAVM = NULL;

    if (!apBtMediaTransportPath) {
        BTRCORELOG_ERROR ("%d	: %s - btrCore_AVMedia_TransportPath_cb: Invalid transport path\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    pstlhBTRCoreAVM = (stBTRCoreAVMediaHdl*)apUserData;

    if (pstlhBTRCoreAVM) { 
        if (pstlhBTRCoreAVM->pcAVMediaTransportPath) {
            if(!strncmp(pstlhBTRCoreAVM->pcAVMediaTransportPath, apBtMediaTransportPath, strlen(pstlhBTRCoreAVM->pcAVMediaTransportPath)))
                BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: Freeing 0x%p:%s\n", __LINE__, __FUNCTION__, 
                            pstlhBTRCoreAVM->pcAVMediaTransportPath, pstlhBTRCoreAVM->pcAVMediaTransportPath);

            free(pstlhBTRCoreAVM->pcAVMediaTransportPath);
            pstlhBTRCoreAVM->pcAVMediaTransportPath = NULL;
        }
        else {
            pstlhBTRCoreAVM->pcAVMediaTransportPath = strdup(apBtMediaTransportPath);
        }
    }

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

        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: Set Configuration\n", __LINE__, __FUNCTION__);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: channel_mode       = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.channel_mode);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: frequency          = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.frequency);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: allocation_method  = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.allocation_method);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: subbands           = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.subbands);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: block_length       = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.block_length);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: min_bitpool        = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.min_bitpool);
        BTRCORELOG_INFO ("%d	: %s - btrCore_AVMedia_TransportPath_cb: max_bitpool        = %d\n", __LINE__, __FUNCTION__, lstBTMediaSBCConfig.max_bitpool);

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
                memcpy(pstlhBTRCoreAVM->pstBTMediaConfig, &lstBtA2dpCapabilities, sizeof(a2dp_sbc_t));
            }
        }
    }

    if (pstlhBTRCoreAVM)
        return pstlhBTRCoreAVM->pcAVMediaTransportPath;
    else
        return NULL;
}
