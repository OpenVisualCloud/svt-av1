/*
* Copyright(c) 2019 Intel Corporation
* SPDX - License - Identifier: BSD - 2 - Clause - Patent
*/

#ifndef EbSimpleAppContext_h
#define EbSimpleAppContext_h

#include "EbApi.h"
#include <stdio.h>

typedef struct EbConfig_s
{
    /****************************************
     * File I/O
     ****************************************/
    FILE                        *configFile;
    FILE                        *inputFile;
    FILE                        *bitstreamFile;
    FILE                        *reconFile;
    FILE                        *errorLogFile;
    FILE                        *bufferFile;

    FILE                        *qpFile;

    uint8_t                      use_qp_file;

    int64_t                      frameRate;
    int64_t                      frameRateNumerator;
    int64_t                      frameRateDenominator;
    int64_t                      injector_frame_rate;
    uint32_t                     injector;
    uint32_t                     speed_control_flag;
    uint32_t                     encoderBitDepth;
    uint32_t                     compressedTenBitFormat;
    uint32_t                     sourceWidth;
    uint32_t                     sourceHeight;

    uint32_t                     inputPaddedWidth;
    uint32_t                     inputPaddedHeight;

    int64_t                      framesToBeEncoded;
    int64_t                      framesEncoded;
    int64_t                      bufferedInput;
    uint8_t                   **sequenceBuffer;

    uint8_t                     latencyMode;

    /****************************************
     * // Interlaced Video
     ****************************************/
    uint8_t                     interlacedVideo;
    uint8_t                     separateFields;

    /*****************************************
     * Coding Structure
     *****************************************/
    uint32_t                    base_layer_switch_mode;
    uint8_t                     encMode;
    int64_t                     intraPeriod;
    uint32_t                    intraRefreshType;
    uint32_t                    hierarchicalLevels;
    uint32_t                    predStructure;


    /****************************************
     * Quantization
     ****************************************/
    uint32_t                    qp;

    /****************************************
     * DLF
     ****************************************/
    uint8_t                     disable_dlf_flag;

    /****************************************
     * ME Tools
     ****************************************/
    uint8_t                     use_default_me_hme;
    uint8_t                     enableHmeFlag;
    uint8_t                     enableHmeLevel0Flag;
    uint8_t                     enableHmeLevel1Flag;
    uint8_t                     enableHmeLevel2Flag;

    /****************************************
     * ME Parameters
     ****************************************/
    uint32_t                    searchAreaWidth;
    uint32_t                    searchAreaHeight;

    /****************************************
     * HME Parameters
     ****************************************/
    uint32_t                    numberHmeSearchRegionInWidth ;
    uint32_t                    numberHmeSearchRegionInHeight;
    uint32_t                    hmeLevel0TotalSearchAreaWidth;
    uint32_t                    hmeLevel0TotalSearchAreaHeight;
    uint32_t                    hmeLevel0ColumnIndex;
    uint32_t                    hmeLevel0RowIndex;
    uint32_t                    hmeLevel1ColumnIndex;
    uint32_t                    hmeLevel1RowIndex;
    uint32_t                    hmeLevel2ColumnIndex;
    uint32_t                    hmeLevel2RowIndex;
    uint32_t                    hmeLevel0SearchAreaInWidthArray[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t                    hmeLevel0SearchAreaInHeightArray[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];
    uint32_t                    hmeLevel1SearchAreaInWidthArray[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t                    hmeLevel1SearchAreaInHeightArray[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];
    uint32_t                    hmeLevel2SearchAreaInWidthArray[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t                    hmeLevel2SearchAreaInHeightArray[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];

    /****************************************
     * MD Parameters
     ****************************************/
    uint8_t                     constrained_intra;

    /****************************************
     * Rate Control
     ****************************************/
    uint32_t                    scene_change_detection;
    uint32_t                    rateControlMode;
    uint32_t                    look_ahead_distance;
    uint32_t                    targetBitRate;
    uint32_t                    max_qp_allowed;
    uint32_t                    min_qp_allowed;

    /****************************************
    * TUNE
    ****************************************/
    uint8_t                     tune;

    /****************************************
     * Optional Features
     ****************************************/

    uint8_t                     bitRateReduction;
    uint8_t                     improve_sharpness;
    uint32_t                    video_usability_info;
    uint32_t                    high_dynamic_range_input;
    uint32_t                    access_unit_delimiter;
    uint32_t                    buffering_period_sei;
    uint32_t                    picture_timing_sei;
    uint8_t                     registered_user_data_sei_flag;
    uint8_t                     unregistered_user_data_sei_flag;
    uint8_t                     recovery_point_sei_flag;
    uint32_t                    enable_temporal_id;

    /****************************************
     * Annex A Parameters
     ****************************************/
    uint32_t                    profile;
    uint32_t                    tier;
    uint32_t                    level;

    /****************************************
     * On-the-fly Testing
     ****************************************/
    uint32_t                    testUserData;
    uint8_t                     eosFlag;

    /****************************************
    * Optimization Type
    ****************************************/
    uint32_t                    asmType;

    // Channel info
    uint32_t                    channel_id;
    uint32_t                    active_channel_count;
    uint32_t                    logicalProcessors;
    int32_t                     targetSocket;
    uint8_t                     stopEncoder;         // to signal CTRL+C Event, need to stop encoding.
} EbConfig_t;

/***************************************
 * App Callback data struct
 ***************************************/
typedef struct EbAppContext_s {
    EbSvtAv1EncConfiguration              ebEncParameters;

    // Component Handle
    EbComponentType*                   svtEncoderHandle;

    // Buffer Pools
    EbBufferHeaderType                 *inputPictureBuffer;
    EbBufferHeaderType                 *outputStreamBuffer;
    EbBufferHeaderType                 *recon_buffer;

    uint32_t instance_idx;

} EbAppContext_t;


/********************************
 * External Function
 ********************************/
extern EbErrorType EbAppContextCtor(EbAppContext_t *contextPtr, EbConfig_t *config);
extern void EbAppContextDtor(EbAppContext_t *contextPtr);
extern EbErrorType init_encoder(EbConfig_t *config, EbAppContext_t *callback_data, uint32_t instance_idx);
extern EbErrorType de_init_encoder(EbAppContext_t *callback_data_ptr, uint32_t instance_index);

#endif // EbAppContext_h
