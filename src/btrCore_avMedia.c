/*
 * btrCore_avMedia.c
 * Implementation of Audio Video & Media finctionalities of Bluetooth
 */

/* System Headers */
#include <stdlib.h>


/* External Library Headers */
#include <bluetooth/uuid.h>
#include <bluetooth/audio/a2dp-codecs.h>
#include <bluetooth/audio/ipc.h>


/* Local Headers */
#include "btrCore_avMedia.h"
#include "btrCore_dbus_bt.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define BTR_MEDIA_A2DP_SINK_ENDPOINT      "/MediaEndpoint/A2DPSink"
#define BTR_MEDIA_A2DP_SOURCE_ENDPOINT    "/MediaEndpoint/A2DPSource"


/* Static Function Prototypes */
static uint8_t btrCore_AVMedia_GetA2DPDefaultBitpool (uint8_t au8SamplingFreq, uint8_t au8AudioChannelsMode);


/* Static Global Variables Defs */
static int          gBTMediaSBCSampFreqPref = BT_SBC_SAMPLING_FREQ_48000;
//TODO: Mutex protect this
static a2dp_sbc_t*  gpBTMediaSBCConfig = NULL;
static char*        gpcAVMediaTransportPath = NULL;


/* Callbacks */
static void* btrCore_AVMedia_NegotiateMedia_cb (void* apBtMediaCaps);
static const char* btrCore_AVMedia_TransportPath_cb (const char* apBtMediaTransportPath, void* apBtMediaCaps);


//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_AVMedia_Init (
    void*       apBtConn,
    const char* apBtAdapter
) {
    int lBtAVMediaRegisterRet   = -1;
    int lBtAVMediaNegotiateRet  = -1;
    int lBtAVMediaTransportPRet = -1;

    enBTRCoreRet lenBTRCoreRet = enBTRCoreFailure;

    if (apBtConn == NULL || apBtAdapter == NULL) {
        return enBTRCoreInvalidArg;
    }

    a2dp_sbc_t lstBtA2dpCapabilities;

    lstBtA2dpCapabilities.channel_mode       = BT_A2DP_CHANNEL_MODE_MONO | BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL |
                                               BT_A2DP_CHANNEL_MODE_STEREO | BT_A2DP_CHANNEL_MODE_JOINT_STEREO;
    lstBtA2dpCapabilities.frequency          = BT_SBC_SAMPLING_FREQ_16000 | BT_SBC_SAMPLING_FREQ_32000 |
                                               BT_SBC_SAMPLING_FREQ_44100 | BT_SBC_SAMPLING_FREQ_48000;
    lstBtA2dpCapabilities.allocation_method  = BT_A2DP_ALLOCATION_SNR | BT_A2DP_ALLOCATION_LOUDNESS;
    lstBtA2dpCapabilities.subbands           = BT_A2DP_SUBBANDS_4 | BT_A2DP_SUBBANDS_8;
    lstBtA2dpCapabilities.block_length       = BT_A2DP_BLOCK_LENGTH_4 | BT_A2DP_BLOCK_LENGTH_8 |
                                               BT_A2DP_BLOCK_LENGTH_12 | BT_A2DP_BLOCK_LENGTH_16;
    lstBtA2dpCapabilities.min_bitpool        = MIN_BITPOOL;
    lstBtA2dpCapabilities.max_bitpool        = MAX_BITPOOL;

    //TODO: Mutex protect this
    gpBTMediaSBCConfig = (a2dp_sbc_t*)malloc(sizeof(a2dp_sbc_t));
    if (!gpBTMediaSBCConfig)
        return enBTRCoreInitFailure;

    //TODO: Mutex protect this
    memcpy(gpBTMediaSBCConfig, &lstBtA2dpCapabilities, sizeof(a2dp_sbc_t));
    gpcAVMediaTransportPath = NULL;

    lBtAVMediaRegisterRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    BTR_MEDIA_A2DP_SOURCE_ENDPOINT,
                                                    A2DP_SOURCE_UUID,
                                                    A2DP_CODEC_SBC,
                                                    (void*)&lstBtA2dpCapabilities,
                                                    sizeof(lstBtA2dpCapabilities));


   lBtAVMediaRegisterRet = BtrCore_BTRegisterMedia(apBtConn,
                                                    apBtAdapter,
                                                    BTR_MEDIA_A2DP_SINK_ENDPOINT,
                                                    A2DP_SINK_UUID,
                                                    A2DP_CODEC_SBC,
                                                    (void*)&lstBtA2dpCapabilities,
                                                    sizeof(lstBtA2dpCapabilities));

    if (!lBtAVMediaRegisterRet)
       lBtAVMediaNegotiateRet = BtrCore_BTRegisterNegotiateMediacB(apBtConn,
                                                                   apBtAdapter,
                                                                   BTR_MEDIA_A2DP_SOURCE_ENDPOINT,
                                                                   &btrCore_AVMedia_NegotiateMedia_cb,
                                                                   NULL);

    if (!lBtAVMediaRegisterRet && !lBtAVMediaNegotiateRet)
        lBtAVMediaTransportPRet = BtrCore_BTRegisterTransportPathMediacB(apBtConn,
                                                                         apBtAdapter,
                                                                         BTR_MEDIA_A2DP_SOURCE_ENDPOINT,
                                                                         &btrCore_AVMedia_TransportPath_cb,
                                                                         NULL);

    if (!lBtAVMediaRegisterRet && !lBtAVMediaNegotiateRet && !lBtAVMediaTransportPRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_DeInit (
    void*       apBtConn,
    const char* apBtAdapter
) {
    int lBtAVMediaRet = -1;
    enBTRCoreRet lenBTRCoreRet = enBTRCoreFailure;

    if (apBtConn == NULL || apBtAdapter == NULL) {
        return enBTRCoreInvalidArg;
    }

    lBtAVMediaRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                              apBtAdapter,
                                              BTR_MEDIA_A2DP_SINK_ENDPOINT);

    lBtAVMediaRet = BtrCore_BTUnRegisterMedia(apBtConn,
                                              apBtAdapter,
                                              BTR_MEDIA_A2DP_SOURCE_ENDPOINT);

    //TODO: Mutex protect this
    if (gpBTMediaSBCConfig) {
        free(gpBTMediaSBCConfig);
        gpBTMediaSBCConfig = NULL;
    }

    //TODO: Mutex protect this
    if (gpcAVMediaTransportPath) {
        free(gpcAVMediaTransportPath);
        gpcAVMediaTransportPath = NULL;
    }

    if (!lBtAVMediaRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_AcquireDataPath (
    void*       apBtConn,
    const char* apBtAdapter,
    int*        apDataPath,
    int*        apDataReadMTU,
    int*        apDataWriteMTU
) {
    int lBtAVMediaRet = -1;
    enBTRCoreRet lenBTRCoreRet = enBTRCoreFailure;

    if (apBtConn == NULL || apBtAdapter == NULL) {
        return enBTRCoreInvalidArg;
    }

    if (gpcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }

    lBtAVMediaRet = BtrCore_BTAcquireDevDataPath (apBtConn, gpcAVMediaTransportPath, apDataPath, apDataReadMTU, apDataWriteMTU);

    if (!lBtAVMediaRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


enBTRCoreRet
BTRCore_AVMedia_ReleaseDataPath (
    void*       apBtConn,
    const char* apBtAdapter
) {
    int lBtAVMediaRet = -1;
    enBTRCoreRet lenBTRCoreRet = enBTRCoreFailure;

    if (apBtConn == NULL || apBtAdapter == NULL) {
        return enBTRCoreInvalidArg;
    }

    if (gpcAVMediaTransportPath == NULL) {
        return enBTRCoreFailure;
    }

    lBtAVMediaRet = BtrCore_BTReleaseDevDataPath(apBtConn, gpcAVMediaTransportPath);

    if (!lBtAVMediaRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}


static uint8_t 
btrCore_AVMedia_GetA2DPDefaultBitpool (
    uint8_t au8SamplingFreq, 
    uint8_t au8AudioChannelsMode
) {
    switch (au8SamplingFreq) {
    case BT_SBC_SAMPLING_FREQ_16000:
    case BT_SBC_SAMPLING_FREQ_32000:
        return 53;

    case BT_SBC_SAMPLING_FREQ_44100:
        switch (au8AudioChannelsMode) {
        case BT_A2DP_CHANNEL_MODE_MONO:
        case BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 31;

        case BT_A2DP_CHANNEL_MODE_STEREO:
        case BT_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return 53;

        default:
            fprintf (stderr, "Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return 53;
        }
    case BT_SBC_SAMPLING_FREQ_48000:
        switch (au8AudioChannelsMode) {
        case BT_A2DP_CHANNEL_MODE_MONO:
        case BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
            return 29;

        case BT_A2DP_CHANNEL_MODE_STEREO:
        case BT_A2DP_CHANNEL_MODE_JOINT_STEREO:
            return 51;

        default:
            fprintf (stderr, "Invalid A2DP channels mode %u\n", au8AudioChannelsMode);
            return 51;
        }
    default:
        fprintf (stderr, "Invalid Bluetooth SBC sampling freq %u\n", au8SamplingFreq);
        return 53;
    }
}


static void*
btrCore_AVMedia_NegotiateMedia_cb (
    void* apBtMediaCaps
) {
    a2dp_sbc_t* apBtMediaSBCCaps = NULL;
    a2dp_sbc_t  lstBTMediaSBCConfig;

    if (!apBtMediaCaps) {
        fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: Invalid input MT Media Capabilities\n");
        return NULL;
    } 

    apBtMediaSBCCaps = (a2dp_sbc_t*)apBtMediaCaps;

    memset(&lstBTMediaSBCConfig, 0, sizeof(a2dp_sbc_t));
    lstBTMediaSBCConfig.frequency = gBTMediaSBCSampFreqPref;

    if (apBtMediaSBCCaps->channel_mode & BT_A2DP_CHANNEL_MODE_JOINT_STEREO) {
        lstBTMediaSBCConfig.channel_mode = BT_A2DP_CHANNEL_MODE_JOINT_STEREO;
    }
    else if (apBtMediaSBCCaps->channel_mode & BT_A2DP_CHANNEL_MODE_STEREO) {
        lstBTMediaSBCConfig.channel_mode = BT_A2DP_CHANNEL_MODE_STEREO;
    }
    else if (apBtMediaSBCCaps->channel_mode & BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL) {
        lstBTMediaSBCConfig.channel_mode = BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL;
    }
    else if (apBtMediaSBCCaps->channel_mode & BT_A2DP_CHANNEL_MODE_MONO) {
        lstBTMediaSBCConfig.channel_mode = BT_A2DP_CHANNEL_MODE_MONO;
    } 
    else {
        fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: No supported channel modes\n");
        return NULL;
    }

    if (apBtMediaSBCCaps->block_length & BT_A2DP_BLOCK_LENGTH_16) {
        lstBTMediaSBCConfig.block_length = BT_A2DP_BLOCK_LENGTH_16;
    }
    else if (apBtMediaSBCCaps->block_length & BT_A2DP_BLOCK_LENGTH_12) {
        lstBTMediaSBCConfig.block_length = BT_A2DP_BLOCK_LENGTH_12;
    }
    else if (apBtMediaSBCCaps->block_length & BT_A2DP_BLOCK_LENGTH_8) {
        lstBTMediaSBCConfig.block_length = BT_A2DP_BLOCK_LENGTH_8;
    }
    else if (apBtMediaSBCCaps->block_length & BT_A2DP_BLOCK_LENGTH_4) {
        lstBTMediaSBCConfig.block_length = BT_A2DP_BLOCK_LENGTH_4;
    }
    else {
        fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: No supported block lengths\n");
        return NULL;
    }

    if (apBtMediaSBCCaps->subbands & BT_A2DP_SUBBANDS_8) {
        lstBTMediaSBCConfig.subbands = BT_A2DP_SUBBANDS_8;
    }
    else if (apBtMediaSBCCaps->subbands & BT_A2DP_SUBBANDS_4) {
        lstBTMediaSBCConfig.subbands = BT_A2DP_SUBBANDS_4;
    }
    else {
        fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: No supported subbands\n");
        return NULL;
    }

    if (apBtMediaSBCCaps->allocation_method & BT_A2DP_ALLOCATION_LOUDNESS) {
        lstBTMediaSBCConfig.allocation_method = BT_A2DP_ALLOCATION_LOUDNESS;
    }
    else if (apBtMediaSBCCaps->allocation_method & BT_A2DP_ALLOCATION_SNR) {
        lstBTMediaSBCConfig.allocation_method = BT_A2DP_ALLOCATION_SNR;
    }

    lstBTMediaSBCConfig.min_bitpool = (uint8_t) MAX(MIN_BITPOOL, apBtMediaSBCCaps->min_bitpool);
    lstBTMediaSBCConfig.max_bitpool = (uint8_t) MIN(btrCore_AVMedia_GetA2DPDefaultBitpool(lstBTMediaSBCConfig.frequency, 
                                                                                          lstBTMediaSBCConfig.channel_mode),
                                                    apBtMediaSBCCaps->max_bitpool);

    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: Negotiated Configuration\n");
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: channel_mode       = %d\n", lstBTMediaSBCConfig.channel_mode);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: frequency          = %d\n", lstBTMediaSBCConfig.frequency);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: allocation_method  = %d\n", lstBTMediaSBCConfig.allocation_method);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: subbands           = %d\n", lstBTMediaSBCConfig.subbands);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: block_length       = %d\n", lstBTMediaSBCConfig.block_length);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: min_bitpool        = %d\n", lstBTMediaSBCConfig.min_bitpool);
    fprintf (stderr, "btrCore_AVMedia_NegotiateMedia_cb: max_bitpool        = %d\n", lstBTMediaSBCConfig.max_bitpool);

    //TODO: Mutex protect this
    if (gpBTMediaSBCConfig) {
        gpBTMediaSBCConfig->channel_mode        =  lstBTMediaSBCConfig.channel_mode;
        gpBTMediaSBCConfig->frequency           =  lstBTMediaSBCConfig.frequency;
        gpBTMediaSBCConfig->allocation_method   =  lstBTMediaSBCConfig.allocation_method;
        gpBTMediaSBCConfig->subbands            =  lstBTMediaSBCConfig.subbands;
        gpBTMediaSBCConfig->block_length        =  lstBTMediaSBCConfig.block_length;
        gpBTMediaSBCConfig->min_bitpool         =  lstBTMediaSBCConfig.min_bitpool;
        gpBTMediaSBCConfig->max_bitpool         =  lstBTMediaSBCConfig.max_bitpool;
    }

    return (void*)gpBTMediaSBCConfig;
}


static const char*
btrCore_AVMedia_TransportPath_cb (
    const char* apBtMediaTransportPath,
    void*       apBtMediaCaps
) {
    if (!apBtMediaTransportPath) {
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: Invalid transport path\n");
        return NULL;
    }

    //TODO: Mutex protect this
    if (gpcAVMediaTransportPath) {
        if(!strncmp(gpcAVMediaTransportPath, apBtMediaTransportPath, strlen(gpcAVMediaTransportPath)))
            fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: Freeing 0x%8x:%s\n", (unsigned int)gpcAVMediaTransportPath, gpcAVMediaTransportPath);

        free(gpcAVMediaTransportPath);
        gpcAVMediaTransportPath = NULL;
    }
    else {
        gpcAVMediaTransportPath = strdup(apBtMediaTransportPath);
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

        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: Set Configuration\n");
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: channel_mode       = %d\n", lstBTMediaSBCConfig.channel_mode);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: frequency          = %d\n", lstBTMediaSBCConfig.frequency);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: allocation_method  = %d\n", lstBTMediaSBCConfig.allocation_method);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: subbands           = %d\n", lstBTMediaSBCConfig.subbands);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: block_length       = %d\n", lstBTMediaSBCConfig.block_length);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: min_bitpool        = %d\n", lstBTMediaSBCConfig.min_bitpool);
        fprintf (stderr, "btrCore_AVMedia_TransportPath_cb: max_bitpool        = %d\n", lstBTMediaSBCConfig.max_bitpool);

        //TODO: Mutex protect this
        if (gpBTMediaSBCConfig) {
            gpBTMediaSBCConfig->channel_mode        =  lstBTMediaSBCConfig.channel_mode;
            gpBTMediaSBCConfig->frequency           =  lstBTMediaSBCConfig.frequency;
            gpBTMediaSBCConfig->allocation_method   =  lstBTMediaSBCConfig.allocation_method;
            gpBTMediaSBCConfig->subbands            =  lstBTMediaSBCConfig.subbands;
            gpBTMediaSBCConfig->block_length        =  lstBTMediaSBCConfig.block_length;
            gpBTMediaSBCConfig->min_bitpool         =  lstBTMediaSBCConfig.min_bitpool;
            gpBTMediaSBCConfig->max_bitpool         =  lstBTMediaSBCConfig.max_bitpool;
        }
    }
    else {
        a2dp_sbc_t lstBtA2dpCapabilities;

        lstBtA2dpCapabilities.channel_mode       = BT_A2DP_CHANNEL_MODE_MONO | BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL |
                                                   BT_A2DP_CHANNEL_MODE_STEREO | BT_A2DP_CHANNEL_MODE_JOINT_STEREO;
        lstBtA2dpCapabilities.frequency          = BT_SBC_SAMPLING_FREQ_16000 | BT_SBC_SAMPLING_FREQ_32000 |
                                                   BT_SBC_SAMPLING_FREQ_44100 | BT_SBC_SAMPLING_FREQ_48000;
        lstBtA2dpCapabilities.allocation_method  = BT_A2DP_ALLOCATION_SNR | BT_A2DP_ALLOCATION_LOUDNESS;
        lstBtA2dpCapabilities.subbands           = BT_A2DP_SUBBANDS_4 | BT_A2DP_SUBBANDS_8;
        lstBtA2dpCapabilities.block_length       = BT_A2DP_BLOCK_LENGTH_4 | BT_A2DP_BLOCK_LENGTH_8 |
                                                   BT_A2DP_BLOCK_LENGTH_12 | BT_A2DP_BLOCK_LENGTH_16;
        lstBtA2dpCapabilities.min_bitpool        = MIN_BITPOOL;
        lstBtA2dpCapabilities.max_bitpool        = MAX_BITPOOL;

        //TODO: Mutex protect this
        if (gpBTMediaSBCConfig) {
            memcpy(gpBTMediaSBCConfig, &lstBtA2dpCapabilities, sizeof(a2dp_sbc_t));
        }
    }

    return gpcAVMediaTransportPath;
}
