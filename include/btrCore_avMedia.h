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
 * @file btrCore_avMedia.h
 * Includes information for Audio, Video & Media functionality over BT
 */

#ifndef __BTR_CORE_AV_MEDIA_H__
#define __BTR_CORE_AV_MEDIA_H__

#include "btrCoreTypes.h"

/**
 * @addtogroup BLUETOOTH_TYPES
 * @{
 */
#define BTR_AV_MEDIA_ELEMENT_MAX            64


typedef void* tBTRCoreAVMediaHdl;
typedef unsigned int tBTRCoreAVMediaItemId;

typedef unsigned long long int tBTRCoreAVMediaElementId;

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
    enBTRCoreAVMediaCtrlVolumeDown,
    enBTRcoreAVMediaCtrlEqlzrOff,
    enBTRcoreAVMediaCtrlEqlzrOn,
    enBTRCoreAVMediaCtrlShflOff,
    enBTRCoreAVMediaCtrlShflAllTracks,
    enBTRCoreAVMediaCtrlShflGroup,
    enBTRCoreAVMediaCtrlRptOff,
    enBTRCoreAVMediaCtrlRptSingleTrack,
    enBTRCoreAVMediaCtrlRptAllTracks,
    enBTRCoreAVMediaCtrlRptGroup,
    enBTRcoreAVMediaCtrlScanOff,
    enBTRcoreAVMediaCtrlScanAllTracks,
    enBTRcoreAVMediaCtrlScanGroup,
    enBTRCoreAVMediaCtrlUnknown
} enBTRCoreAVMediaCtrl;

typedef enum _eBTRCoreAVMediaStatusUpdate {
    eBTRCoreAVMediaTrkStStarted,
    eBTRCoreAVMediaTrkStPlaying,
    eBTRCoreAVMediaTrkStForwardSeek,
    eBTRCoreAVMediaTrkStReverseSeek,
    eBTRCoreAVMediaTrkStPaused,
    eBTRCoreAVMediaTrkStStopped,
    eBTRCoreAVMediaTrkStChanged,
    eBTRCoreAVMediaTrkPosition,
    eBTRCoreAVMediaPlaybackEnded,
    eBTRCoreAVMediaPlaybackError,
    eBTRCoreAVMediaPlyrName,
    eBTRCoreAVMediaPlyrEqlzrStOff,
    eBTRCoreAVMediaPlyrEqlzrStOn,
    eBTRCoreAVMediaPlyrShflStOff,
    eBTRCoreAVMediaPlyrShflStAllTracks,
    eBTRCoreAVMediaPlyrShflStGroup,
    eBTRCoreAVMediaPlyrRptStOff,
    eBTRCoreAVMediaPlyrRptStSingleTrack,
    eBTRCoreAVMediaPlyrRptStAllTracks,
    eBTRCoreAVMediaPlyrRptStGroup,
    eBTRCoreAVMediaPlyrScanStOff,
    eBTRCoreAVMediaPlyrScanStAllTracks,
    eBTRCoreAVMediaPlyrScanStGroup,
    eBTRCoreAVMediaPlyrVolume,
    eBTRCoreAVMediaElementAdded,
    eBTRCoreAVMediaElementRemoved,
    eBTRCoreAVMediaStUnknown
} eBTRCoreAVMediaStatusUpdate;

typedef enum _eBTRCoreAVMediaFlow {
    eBTRCoreAVMediaFlowIn,
    eBTRCoreAVMediaFlowOut,
    eBTRCoreAVMediaFlowInOut,
    eBTRCoreAVMediaFlowUnknown
} eBTRCoreAVMediaFlow;

typedef enum _eBTRCoreAVMElementType {
    eBTRCoreAVMETypeUnknown,
    eBTRCoreAVMETypeAlbum,
    eBTRCoreAVMETypeArtist,
    eBTRCoreAVMETypeGenre,
    eBTRCoreAVMETypeCompilation,
    eBTRCoreAVMETypePlayList,
    eBTRCoreAVMETypeTrackList,
    eBTRCoreAVMETypeTrack
} eBTRCoreAVMElementType;

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
    unsigned char       ui8AVMMpegVersion;      // version
    unsigned char       ui8AVMMpegLayer;        // layer
    unsigned char       ui8AVMMpegType;         // type
    unsigned char       ui8AVMMpegMpf;          // mpf
    unsigned char       ui8AVMMpegRfa;          // rfa
    unsigned short      ui16AVMMpegFrameLen;    // frameLength
    unsigned short      ui16AVMMpegBitrate;     // bitrate
} stBTRCoreAVMediaMpegInfo;

typedef struct _stBTRCoreAVMediaInfo {
    eBTRCoreAVMType     eBtrCoreAVMType;
    eBTRCoreAVMediaFlow eBtrCoreAVMFlow;
    void*               pstBtrCoreAVMCodecInfo;
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

typedef struct _stBTRCoreAVMediaElementInfo {
    eBTRCoreAVMElementType       eAVMElementType;
    tBTRCoreAVMediaElementId     ui32AVMediaElementId;
    unsigned char                bIsPlayable;
    char                         m_mediaElementName[BTRCORE_MAX_STR_LEN];
    stBTRCoreAVMediaTrackInfo    m_mediaTrackInfo;
} stBTRCoreAVMediaElementInfo;

typedef struct _stBTRCoreAVMediaElementInfoList {
    unsigned short               m_numOfElements;
    stBTRCoreAVMediaElementInfo  m_mediaElementInfo[BTR_AV_MEDIA_ELEMENT_MAX];
} stBTRCoreAVMediaElementInfoList;

typedef struct _stBTRCoreAVMediaStatusUpdate {
    eBTRCoreAVMediaStatusUpdate     eAVMediaState;
    eBTRCoreAVMediaFlow             eAVMediaDataFlow;
    unsigned char                   bIsAVMediaCtrlAvail;

    union {
        stBTRCoreAVMediaTrackInfo       m_mediaTrackInfo;
        stBTRCoreAVMediaPositionInfo    m_mediaPositionInfo;
        stBTRCoreAVMediaElementInfo     m_mediaElementInfo;
        char                            m_mediaPlayerName[BTRCORE_MAX_STR_LEN];
        unsigned char                   m_mediaPlayerTransportVolume;
    };
} stBTRCoreAVMediaStatusUpdate;


typedef struct _stBTRCoreAVMediaCtData {
    unsigned char                   m_mediaAbsTransportVolume;
    //TODO: When we implement a Player for Audio-Out check if you can use a common union
    //      similar to one use in stBTRCoreAVMediaStatusUpdate
    //      union {
    //          stBTRCoreAVMediaTrackInfo       m_mediaTrackInfo;
    //          stBTRCoreAVMediaPositionInfo    m_mediaPositionInfo;
    //          stBTRCoreAVMediaElementInfo     m_mediaElementInfo;
    //          char                            m_mediaPlayerName[BTRCORE_MAX_STR_LEN];
    //          unsigned char                   m_mediaPlayerTransportVolume;
    //      };
} stBTRCoreAVMediaCtData;

/* @} */ // End of group BLUETOOTH_TYPES


/* Fptr Callbacks types */
typedef enBTRCoreRet (*fPtr_BTRCore_AVMediaStatusUpdateCb) (stBTRCoreAVMediaStatusUpdate* pBTRCoreAVMediaStreamStatus, const char* apcAVMediaDevAddress, void* apvUserData);

/**
 * @addtogroup BLUETOOTH_APIS
 * @{
 */

/* Interfaces */
/**
 * @brief This API Initializes the media device by registering both source and sink.
 *
 * @param[in]  phBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTDeInitReleaseConnection. NULL is valid for this API.
 * @param[in]  apBtAdapter      Bluetooth adapter path.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_AVMedia_Init (tBTRCoreAVMediaHdl* phBTRCoreAVM, void* apBtConn, const char* apBtAdapter);

/**
 * @brief This API DeInitializes the media device by unregistering both source and sink.
 *
 * @param[in]  hBTRCoreAVM      Bluetooth core AV media handle.
 * @param[in]  apBtConn         The Dbus connection handle as returned by BtrCore_BTDeInitReleaseConnection. NULL is valid for this API.
 * @param[in]  apBtAdapter      Bluetooth adapter path.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_AVMedia_DeInit (tBTRCoreAVMediaHdl hBTRCoreAVM, void* apBtConn, const char* apBtAdapter);
/**
 * @brief This API gets current media information of the media device by unregistering both source and sink.
 *
 * @param[in]  hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in]  apBtDevAddr     Bluetooth device address..
 * @param[out] apstBtrCoreAVMediaInfo  A structure pointer that fetches media information.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_AVMedia_GetCurMediaInfo (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, stBTRCoreAVMediaInfo* apstBtrCoreAVMediaInfo);
/**
 * @brief This API acquires the data path and MTU of a media device.
 *
 * @param[in] hBTRCoreAVM      Bluetooth core AV media handle.
 * @param[in] apBtDevAddr      Bluetooth device address.
 * @param[out] apDataPath      Data path that has to be fetched.
 * @param[out] apDataReadMTU   Fetches MTU of data reading.
 * @param[out] apDataWriteMTU  Fetches MTU of data writing.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_AVMedia_AcquireDataPath (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, int* apDataPath, int* apDataReadMTU, int* apDataWriteMTU, unsigned int* apui32Delay);
/**
 * @brief This API releases the acquired data path of the media device.
 *
 * @param[in] hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in] apBtDevAddr     Bluetooth device address.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AVMedia_ReleaseDataPath (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr);
/**
 * @brief  This API is used to control the media device. BTRCore_MediaControl() invokes this API.
 *
 * @param[in] hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in] apBtDevAddr     Bluetooth device address.
 * @param[in] aenBTRCoreAVMediaCtrl    Indicates which operation needs to be performed.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 *
 */
enBTRCoreRet BTRCore_AVMedia_MediaControl (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, enBTRCoreAVMediaCtrl aenBTRCoreAVMediaCtrl, eBTRCoreAVMediaFlow aenBTRCoreAVMediaFlow, stBTRCoreAVMediaCtData* apstBTRCoreAVMediaCtrlData);
/**
 * @brief  This API is used to retrieve the information about the track that is being played on the media device.
 *
 * @param[in] hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in] apBtDevAddr     Bluetooth device address.
 * @param[out] apstBTAVMediaTrackInfo   Track information that has to be retrieved.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AVMedia_GetTrackInfo (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, stBTRCoreAVMediaTrackInfo* apstBTAVMediaTrackInfo);


/**
 * @brief  This API is used to retrieve the information about the track that is being played on the media device.
 *
 * @param[in] hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in] apBtDevAddr     Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id.
 * @param[out] apstBTAVMediaTrackInfo   Track information that has to be retrieved.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AVMedia_GetElementTrackInfo (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId, stBTRCoreAVMediaTrackInfo* apstBTAVMediaTrackInfo);


/**
 * @brief  This API is used to retrieve the position information about the media device.
 *
 * @param[in] hBTRCoreAVM     Bluetooth core AV media handle.
 * @param[in] apBtDevAddr     Bluetooth device address.
 * @param[out] apstBTAVMediaPositionInfo   Position information that has to be retrieved.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AVMedia_GetPositionInfo (tBTRCoreAVMediaHdl  hBTRCoreAVM, const char* apBtDevAddr, stBTRCoreAVMediaPositionInfo* apstBTAVMediaPositionInfo);

/**
 * @brief  This API is used to get media property value using the device address and media property key.
 *
 * @param[in] hBTRCoreAVM      Bluetooth core AV media handle.
 * @param[in] apBtDevAddr      Bluetooth device address.
 * @param[in] mediaPropertyKey     Property key of the Mediaplayer.
 * @param[out] mediaPropertyValue  Property keyvalue of media player.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_GetMediaProperty (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, const char* mediaPropertyKey, void* mediaPropertyValue);

/**
 * @brief  This API is used to change the media browsing location in the connected AudioIn device.
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id
 * @param[in] aeBtrAVMElementType   media element type (Albums, Artist, ...)
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_ChangeBrowserLocation (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId, eBTRCoreAVMElementType aeBtrAVMElementType);

/**
 * @brief  This API is used to List(implicitly) the media elements in the current browsing location of the connected AudioIn device.
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aui16StartIdx         Starting index of the list to retrive.
 * @param[in] aui16EndIdx           Ending index of the list to retrive.
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_SelectMediaBrowserElements (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, unsigned short aui16StartIdx, unsigned short aui16EndIdx);

/**
 * @brief  This API is used to fetch the media item list.
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id
 * @param[in] aui16StartIdx         Starting index of the list to retrive.
 * @param[in] aui16EndIdx           Ending index of the list to retrive.
 * @param[in] aeBtrAVMElementType   media element type (Albums, Artist, ...)
 * @param[out] aAVMediaElementInfoList  Fetched media element list
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_GetMediaElementList (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId, unsigned short aui16StartIdx, unsigned short aui16EndIdx, eBTRCoreAVMElementType aeBtrAVMElementType, stBTRCoreAVMediaElementInfoList*  aAVMediaElementInfoList);

/**
 * @brief  This API starts playing the mentioned media item.
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_PlayTrack (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId);

/**
 * @brief  This API select track the mentioned media item.
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
*/
enBTRCoreRet BTRCore_AVMedia_SelectTrack (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId);

/**
 * @brief  This API is used to find if a MediaElement is Playable or Non-Playable
 *
 * @param[in] hBTRCoreAVM           Bluetooth core AV media handle
 * @param[in] apBtDevAddr           Bluetooth device address.
 * @param[in] aBtrAVMediaElementId  target browsing locations' media element id
 * @Param[out] isPlayable           Playable/Non-Playable state
 *
 * @return Returns the status of the operation.
 * @retval enBTRCoreSuccess on success, appropriate error code otherwise.
 */
enBTRCoreRet BTRCore_AVMedia_IsMediaElementPlayable (tBTRCoreAVMediaHdl hBTRCoreAVM, const char* apBtDevAddr, tBTRCoreAVMediaElementId aBtrAVMediaElementId, char* isPlayable);

// Outgoing callbacks Registration Interfaces
/** Callback to notify the BT Core about Mediaplayer path and its Userdata */
enBTRCoreRet BTRCore_AVMedia_RegisterMediaStatusUpdateCb (tBTRCoreAVMediaHdl hBTRCoreAVM, fPtr_BTRCore_AVMediaStatusUpdateCb afpcBBTRCoreAVMediaStatusUpdate, void* apcBMediaStatusUserData);

/* @} */    //BLUETOOTH_APIS


#endif // __BTR_CORE_AV_MEDIA_H__
