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
 * btrCore_avMedia.h
 * Includes information for Audio, Video & Media functionality over BT
 */

#ifndef __BTR_CORE_AV_MEDIA_H__
#define __BTR_CORE_AV_MEDIA_H__

#include "btrCoreTypes.h"

#define BTRCORE_MAX_STR_LEN 256
#define BTRCORE_STR_LEN     32

typedef void* tBTRCoreAVMediaHdl;

typedef enum _eBTRCoreAVMType {
    eBTRCoreAVMTypePCM,
    eBTRCoreAVMTypeSBC,
    eBTRCoreAVMTypeMPEG,
    eBTRCoreAVMTypeAAC,
    eBTRCoreAVMTypeUnknown
} eBTRCoreAVMType;

typedef enum _eBTRCoreAVMAChan {
    eBTRCoreAVMAChanMono,
    eBTRCoreAVMAChanDualChannel,
    eBTRCoreAVMAChanStereo,
    eBTRCoreAVMAChanJointStereo,
    eBTRCoreAVMAChan5_1,
    eBTRCoreAVMAChan7_1,
    eBTRCoreAVMAChanUnknown
} eBTRCoreAVMAChan;

typedef enum _enBTRCoreAVMediaCtrl {
    enBTRCoreAVMediaCtrlPlay,
    enBTRCoreAVMediaCtrlPause,
    enBTRCoreAVMediaCtrlStop,
    enBTRCoreAVMediaCtrlNext,
    enBTRCoreAVMediaCtrlPrevious,
    enBTRCoreAVMediaCtrlFastForward,
    enBTRCoreAVMediaCtrlRewind,
    enBTRCoreAVMediaCtrlVolumeUp,
    enBTRCoreAVMediaCtrlVolumeDown
} enBTRCoreAVMediaCtrl;

typedef enum _eBTRCoreAVMediaStatusUpdate {
    eBTRCoreAVMediaTrkStStarted,
    eBTRCoreAVMediaTrkStPlaying,
    eBTRCoreAVMediaTrkStPaused,
    eBTRCoreAVMediaTrkStStopped,
    eBTRCoreAVMediaTrkStChanged,
    eBTRCoreAVMediaTrkPosition,
    eBTRCoreAVMediaPlaybackEnded,
    eBTRCoreAVMediaPlaylistUpdate,
    eBTRCoreAVMediaBrowserUpdate
} eBTRCoreAVMediaStatusUpdate;


typedef struct _stBTRMgrAVMediaPcmInfo {
    eBTRCoreAVMAChan    eAVMAChan;
    unsigned int        ui32AVMAChan;           // num audio Channels
    unsigned int        ui32AVMSFreq;
    unsigned int        ui32AVMSFmt;
} stBTRMgrAVMediaPcmInfo;

typedef struct _stBTRCoreAVMediaSbcInfo {
    eBTRCoreAVMAChan    eAVMAChan;              // channel_mode
    unsigned int        ui32AVMAChan;           // num audio Channels
    unsigned int        ui32AVMSFreq;           // frequency
    unsigned char       ui8AVMSbcAllocMethod;   // allocation_method
    unsigned char       ui8AVMSbcSubbands;      // subbands
    unsigned char       ui8AVMSbcBlockLength;   // block_length
    unsigned char       ui8AVMSbcMinBitpool;    // min_bitpool
    unsigned char       ui8AVMSbcMaxBitpool;    // max_bitpool
    unsigned short      ui16AVMSbcFrameLen;     // frameLength
    unsigned short      ui16AVMSbcBitrate;      // bitrate
} stBTRCoreAVMediaSbcInfo;

typedef struct _stBTRCoreAVMediaMpegInfo {
    eBTRCoreAVMAChan    eAVMAChan;              // channel_mode
    unsigned int        ui32AVMAChan;           // num audio Channels
    unsigned int        ui32AVMSFreq;           // frequency
    unsigned char       ui8AVMMpegCrc;          // crc
    unsigned char       ui8AVMMpegLayer;        // layer
    unsigned char       ui8AVMMpegMpf;          // mpf
    unsigned char       ui8AVMMpegRfa;          // rfa
    unsigned short      ui16AVMMpegFrameLen;    // frameLength
    unsigned short      ui16AVMMpegBitrate;     // bitrate
} stBTRCoreAVMediaMpegInfo;

typedef struct _stBTRCoreAVMediaInfo {
    eBTRCoreAVMType eBtrCoreAVMType;
    void*           pstBtrCoreAVMCodecInfo;
} stBTRCoreAVMediaInfo;

typedef struct _stBTRCoreAVMediaTrackInfo {
    char            pcAlbum[BTRCORE_MAX_STR_LEN];
    char            pcGenre[BTRCORE_MAX_STR_LEN];
    char            pcTitle[BTRCORE_MAX_STR_LEN];
    char            pcArtist[BTRCORE_MAX_STR_LEN];
    unsigned int    ui32TrackNumber;
    unsigned int    ui32Duration;
    unsigned int    ui32NumberOfTracks;
} stBTRCoreAVMediaTrackInfo;

typedef struct _stBTRCoreAVMediaPositionInfo {
    unsigned int    ui32Duration;
    unsigned int    ui32Position;
} stBTRCoreAVMediaPositionInfo;

typedef struct _stBTRCoreAVMediaStatusUpdate {
    eBTRCoreAVMediaStatusUpdate     eAVMediaState;

    union {
      stBTRCoreAVMediaTrackInfo       m_mediaTrackInfo;
      stBTRCoreAVMediaPositionInfo    m_mediaPositionInfo;
    };
} stBTRCoreAVMediaStatusUpdate;


/* Fptr Callbacks types */
typedef enBTRCoreRet (*fPtr_BTRCore_AVMediaStatusUpdateCb) (void* pBTRCoreAVMediaStreamStatus, const char* apcAVMediaDevAddress, void* apvUserCbData);

/* Interfaces */
enBTRCoreRet BTRCore_AVMedia_Init (tBTRCoreAVMediaHdl* phBTRCoreAVM, void* apBtConn, const char* apBtAdapter);
enBTRCoreRet BTRCore_AVMedia_DeInit (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtAdapter);
enBTRCoreRet BTRCore_AVMedia_GetCurMediaInfo (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, stBTRCoreAVMediaInfo* apstBtrCoreAVMediaInfo);
enBTRCoreRet BTRCore_AVMedia_AcquireDataPath (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, int* apDataPath, int* apDataReadMTU, int* apDataWriteMTU);
enBTRCoreRet BTRCore_AVMedia_ReleaseDataPath (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr);
enBTRCoreRet BTRCore_AVMedia_MediaControl (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, enBTRCoreAVMediaCtrl aenBTRCoreAVMediaCtrl);
enBTRCoreRet BTRCore_AVMedia_GetTrackInfo (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, stBTRCoreAVMediaTrackInfo* apstBTAVMediaTrackInfo);
enBTRCoreRet BTRCore_AVMedia_GetPositionInfo (tBTRCoreAVMediaHdl  hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, stBTRCoreAVMediaPositionInfo* apstBTAVMediaPositionInfo);
enBTRCoreRet BTRCore_AVMedia_GetMediaProperty (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtDevAddr, const char* mediaPropertyKey, void* mediaPropertyValue);
enBTRCoreRet BTRCore_AVMedia_StartMediaPositionPolling (tBTRCoreAVMediaHdl  hBTRCoreAVM, void* apBtConn, const char* apBtDevPath, const char* apBtDevAddr);
enBTRCoreRet BTRCore_AVMedia_ExitMediaPositionPolling (tBTRCoreAVMediaHdl  hBTRCoreAVM);
// Outgoing callbacks Registration Interfaces
enBTRCoreRet BTRCore_AVMedia_RegisterMediaStatusUpdateCb (tBTRCoreAVMediaHdl hBTRCoreAVM, fPtr_BTRCore_AVMediaStatusUpdateCb afpcBBTRCoreAVMediaStatusUpdate, void* apcBMediaStatusUserData);

#endif // __BTR_CORE_AV_MEDIA_H__
