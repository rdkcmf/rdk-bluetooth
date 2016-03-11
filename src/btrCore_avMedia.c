/*
 * btrCore_avMedia.c
 * Implementation of Audio Video & Media finctionalities of Bluetooth
 */

/* System Headers */


/* External Library Headers */
#include <bluetooth/uuid.h>
#include <bluetooth/audio/a2dp-codecs.h>
#include <bluetooth/audio/ipc.h>


/* Local Headers */
#include "btrCore_avMedia.h"
#include "btrCore_dbus_bt.h"



#define BTR_MEDIA_A2DP_SINK_ENDPOINT      "/MediaEndpoint/A2DPSink"
#define BTR_MEDIA_A2DP_SOURCE_ENDPOINT    "/MediaEndpoint/A2DPSource"



//////////////////
//  Interfaces  //
//////////////////
enBTRCoreRet
BTRCore_AVMedia_Init (
    void*       apBtConn,
    const char* apBtAdapter
) {
    int lBtAVMediaRet = -1;
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


    lBtAVMediaRet = BtrCore_BTRegisterMedia(apBtConn,
                                            apBtAdapter,
                                            BTR_MEDIA_A2DP_SINK_ENDPOINT,
                                            A2DP_SINK_UUID,
                                            A2DP_CODEC_SBC,
                                            (void*)&lstBtA2dpCapabilities,
                                            sizeof(lstBtA2dpCapabilities));
    if (!lBtAVMediaRet)
        lenBTRCoreRet = enBTRCoreSuccess;

    return lenBTRCoreRet;
}



