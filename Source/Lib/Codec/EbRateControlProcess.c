/*
* Copyright(c) 2019 Intel Corporation
* SPDX - License - Identifier: BSD - 2 - Clause - Patent
*/

/*
* Copyright (c) 2016, Alliance for Open Media. All rights reserved
*
* This source code is subject to the terms of the BSD 2 Clause License and
* the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
* was not distributed with this source code in the LICENSE file, you can
* obtain it at www.aomedia.org/license/software. If the Alliance for Open
* Media Patent License 1.0 was not distributed with this source code in the
* PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/
#include <stdlib.h>

#include "EbDefinitions.h"
#include "EbRateControlProcess.h"
#include "EbSystemResourceManager.h"
#include "EbSequenceControlSet.h"
#include "EbPictureControlSet.h"
#include "EbUtility.h"
#include "EbErrorCodes.h"

#include "EbRateControlResults.h"
#include "EbRateControlTasks.h"
#include "RateControlModel.h"


static uint8_t QP_OFFSET_LAYER_ARRAY[MAX_TEMPORAL_LAYERS] =
{
    1, 2, 4, 5, 6, 7
};

/*****************************
* Internal Typedefs
*****************************/
void RateControlLayerReset(
    RateControlLayerContext   *rateControlLayerPtr,
    PictureControlSet         *picture_control_set_ptr,
    RateControlContext        *rateControlContextPtr,
    uint32_t                       pictureAreaInPixel,
    EbBool                      was_used)
{

    SequenceControlSet *sequence_control_set_ptr = (SequenceControlSet*)picture_control_set_ptr->sequence_control_set_wrapper_ptr->object_ptr;
    uint32_t                sliceNum;
    uint32_t                temporal_layer_index;
    uint64_t                totalFrameInInterval;
    uint64_t                sumBitsPerSw = 0;

    rateControlLayerPtr->target_bit_rate = picture_control_set_ptr->parent_pcs_ptr->target_bit_rate*rate_percentage_layer_array[sequence_control_set_ptr->static_config.hierarchical_levels][rateControlLayerPtr->temporal_index] / 100;
    // update this based on temporal layers
    rateControlLayerPtr->frame_rate = sequence_control_set_ptr->frame_rate;

    totalFrameInInterval = sequence_control_set_ptr->static_config.intra_period_length + 1;

    if (sequence_control_set_ptr->static_config.look_ahead_distance != 0 && sequence_control_set_ptr->intra_period_length != -1) {
        if (picture_control_set_ptr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0) {
            totalFrameInInterval = 0;
            for (temporal_layer_index = 0; temporal_layer_index < EB_MAX_TEMPORAL_LAYERS; temporal_layer_index++) {
                rateControlContextPtr->frames_in_interval[temporal_layer_index] = picture_control_set_ptr->parent_pcs_ptr->frames_in_interval[temporal_layer_index];
                totalFrameInInterval += picture_control_set_ptr->parent_pcs_ptr->frames_in_interval[temporal_layer_index];
                sumBitsPerSw += picture_control_set_ptr->parent_pcs_ptr->bits_per_sw_per_layer[temporal_layer_index];
            }
#if ADAPTIVE_PERCENTAGE
            rateControlLayerPtr->target_bit_rate = picture_control_set_ptr->parent_pcs_ptr->target_bit_rate* picture_control_set_ptr->parent_pcs_ptr->bits_per_sw_per_layer[rateControlLayerPtr->temporal_index] / sumBitsPerSw;
#endif
        }
    }


    if (rateControlLayerPtr->temporal_index == 0) {
        rateControlLayerPtr->coeff_averaging_weight1 = 5;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 5;
    }
    else if (rateControlLayerPtr->temporal_index == 1) {
        rateControlLayerPtr->coeff_averaging_weight1 = 5;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 5;
    }
    else if (rateControlLayerPtr->temporal_index == 2) {
        rateControlLayerPtr->coeff_averaging_weight1 = 5;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 4;
    }
    else if (rateControlLayerPtr->temporal_index == 3) {
        rateControlLayerPtr->coeff_averaging_weight1 = 5;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 3;
    }
    else if (rateControlLayerPtr->temporal_index == 4) {
        rateControlLayerPtr->coeff_averaging_weight1 = 5;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 2;
    }
    else if (rateControlLayerPtr->temporal_index == 5) {
        rateControlLayerPtr->coeff_averaging_weight1 = 3;
        rateControlLayerPtr->frame_rate = rateControlLayerPtr->frame_rate >> 1;
    }
    if (sequence_control_set_ptr->static_config.intra_period_length != -1) {
        rateControlLayerPtr->frame_rate = sequence_control_set_ptr->frame_rate * rateControlContextPtr->frames_in_interval[rateControlLayerPtr->temporal_index] / totalFrameInInterval;
    }

    rateControlLayerPtr->coeff_averaging_weight2 = 16 - rateControlLayerPtr->coeff_averaging_weight1;
    if (rateControlLayerPtr->frame_rate == 0) { // no frame in that layer
        rateControlLayerPtr->frame_rate = 1 << RC_PRECISION;
    }
    rateControlLayerPtr->channel_bit_rate = (((rateControlLayerPtr->target_bit_rate << (2 * RC_PRECISION)) / rateControlLayerPtr->frame_rate) + RC_PRECISION_OFFSET) >> RC_PRECISION;
    rateControlLayerPtr->channel_bit_rate = (uint64_t)MAX((int64_t)1, (int64_t)rateControlLayerPtr->channel_bit_rate);
    rateControlLayerPtr->ec_bit_constraint = rateControlLayerPtr->channel_bit_rate;


    // This is only for the initial frame, because the feedback is from packetization now and all of these are considered
    // considering the bits for slice header
    // *Note - only one-slice-per picture is supported for UHD
    sliceNum = 1;

    rateControlLayerPtr->ec_bit_constraint -= SLICE_HEADER_BITS_NUM * sliceNum;

    rateControlLayerPtr->ec_bit_constraint = MAX(1, rateControlLayerPtr->ec_bit_constraint);

    rateControlLayerPtr->previous_bit_constraint = rateControlLayerPtr->channel_bit_rate;
    rateControlLayerPtr->bit_constraint = rateControlLayerPtr->channel_bit_rate;
    rateControlLayerPtr->dif_total_and_ec_bits = 0;

    rateControlLayerPtr->frame_same_sad_min_qp_count = 0;
    rateControlLayerPtr->max_qp = picture_control_set_ptr->picture_qp;

    rateControlLayerPtr->alpha = 1 << (RC_PRECISION - 1);
    {
        if (!was_used) {


            rateControlLayerPtr->same_sad_count = 0;

            rateControlLayerPtr->k_coeff = 3 << RC_PRECISION;
            rateControlLayerPtr->previous_k_coeff = 3 << RC_PRECISION;

            rateControlLayerPtr->c_coeff = (rateControlLayerPtr->channel_bit_rate << (2 * RC_PRECISION)) / pictureAreaInPixel / CCOEFF_INIT_FACT;
            rateControlLayerPtr->previous_c_coeff = (rateControlLayerPtr->channel_bit_rate << (2 * RC_PRECISION)) / pictureAreaInPixel / CCOEFF_INIT_FACT;

            // These are for handling Pred structure 2, when for higher temporal layer, frames can arrive in different orders
            // They should be modifed in a way that gets these from previous layers
            rateControlLayerPtr->previous_frame_qp = 32;
            rateControlLayerPtr->previous_frame_bit_actual = 1200;
            rateControlLayerPtr->previous_frame_quantized_coeff_bit_actual = 1000;
            rateControlLayerPtr->previous_frame_sad_me = 10000000;
            rateControlLayerPtr->previous_frame_qp = picture_control_set_ptr->picture_qp;
            rateControlLayerPtr->delta_qp_fraction = 0;
            rateControlLayerPtr->previous_frame_average_qp = picture_control_set_ptr->picture_qp;
            rateControlLayerPtr->previous_calculated_frame_qp = picture_control_set_ptr->picture_qp;
            rateControlLayerPtr->calculated_frame_qp = picture_control_set_ptr->picture_qp;
            rateControlLayerPtr->critical_states = 0;
        }
        else {
            rateControlLayerPtr->same_sad_count = 0;
            rateControlLayerPtr->critical_states = 0;
        }
    }
}


void RateControlLayerResetPart2(
    RateControlLayerContext   *rateControlLayerPtr,
    PictureControlSet         *picture_control_set_ptr)
{

    // update this based on temporal layers

    rateControlLayerPtr->max_qp = (uint32_t)CLIP3(0, 63, (int32_t)(picture_control_set_ptr->picture_qp + QP_OFFSET_LAYER_ARRAY[rateControlLayerPtr->temporal_index]));;
    {

        // These are for handling Pred structure 2, when for higher temporal layer, frames can arrive in different orders
        // They should be modifed in a way that gets these from previous layers
        rateControlLayerPtr->previous_frame_qp = rateControlLayerPtr->max_qp;
        rateControlLayerPtr->previous_frame_average_qp = rateControlLayerPtr->max_qp;
        rateControlLayerPtr->previous_calculated_frame_qp = rateControlLayerPtr->max_qp;
        rateControlLayerPtr->calculated_frame_qp = rateControlLayerPtr->max_qp;
    }
}

EbErrorType HighLevelRateControlContextCtor(
    HighLevelRateControlContext   **entry_dbl_ptr) {

    HighLevelRateControlContext *entryPtr;
    EB_MALLOC(HighLevelRateControlContext*, entryPtr, sizeof(HighLevelRateControlContext), EB_N_PTR);
    *entry_dbl_ptr = entryPtr;

    return EB_ErrorNone;
}


EbErrorType rate_control_layer_context_ctor(
    RateControlLayerContext   **entry_dbl_ptr) {

    RateControlLayerContext *entryPtr;
    EB_MALLOC(RateControlLayerContext*, entryPtr, sizeof(RateControlLayerContext), EB_N_PTR);

    *entry_dbl_ptr = entryPtr;

    entryPtr->first_frame = 1;
    entryPtr->first_non_intra_frame = 1;
    entryPtr->feedback_arrived = EB_FALSE;

    return EB_ErrorNone;
}



EbErrorType rate_control_interval_param_context_ctor(
    RateControlIntervalParamContext   **entry_dbl_ptr) {

    uint32_t temporal_index;
    EbErrorType return_error = EB_ErrorNone;
    RateControlIntervalParamContext *entryPtr;
    EB_MALLOC(RateControlIntervalParamContext*, entryPtr, sizeof(RateControlIntervalParamContext), EB_N_PTR);

    *entry_dbl_ptr = entryPtr;

    entryPtr->in_use = EB_FALSE;
    entryPtr->was_used = EB_FALSE;
    entryPtr->last_gop = EB_FALSE;
    entryPtr->processed_frames_number = 0;
    EB_MALLOC(RateControlLayerContext**, entryPtr->rate_control_layer_array, sizeof(RateControlLayerContext*)*EB_MAX_TEMPORAL_LAYERS, EB_N_PTR);

    for (temporal_index = 0; temporal_index < EB_MAX_TEMPORAL_LAYERS; temporal_index++) {
        return_error = rate_control_layer_context_ctor(&entryPtr->rate_control_layer_array[temporal_index]);
        entryPtr->rate_control_layer_array[temporal_index]->temporal_index = temporal_index;
        entryPtr->rate_control_layer_array[temporal_index]->frame_rate = 1 << RC_PRECISION;
        if (return_error == EB_ErrorInsufficientResources) {
            return EB_ErrorInsufficientResources;
        }
    }

    entryPtr->min_target_rate_assigned = EB_FALSE;

    entryPtr->intra_frames_qp = 0;
    entryPtr->next_gop_intra_frame_qp = 0;
    entryPtr->first_pic_pred_bits = 0;
    entryPtr->first_pic_actual_bits = 0;
    entryPtr->first_pic_pred_qp = 0;
    entryPtr->first_pic_actual_qp = 0;
    entryPtr->first_pic_actual_qp_assigned = EB_FALSE;
    entryPtr->scene_change_in_gop = EB_FALSE;
    entryPtr->extra_ap_bit_ratio_i = 0;

    return EB_ErrorNone;
}

EbErrorType rate_control_coded_frames_stats_context_ctor(
    CodedFramesStatsEntry   **entry_dbl_ptr,
    uint64_t                      picture_number) {

    CodedFramesStatsEntry *entryPtr;
    EB_MALLOC(CodedFramesStatsEntry*, entryPtr, sizeof(CodedFramesStatsEntry), EB_N_PTR);

    *entry_dbl_ptr = entryPtr;

    entryPtr->picture_number = picture_number;
    entryPtr->frame_total_bit_actual = -1;

    return EB_ErrorNone;
}


EbErrorType rate_control_context_ctor(
    RateControlContext   **context_dbl_ptr,
    EbFifo                *rate_control_input_tasks_fifo_ptr,
    EbFifo                *rate_control_output_results_fifo_ptr,
    int32_t                   intra_period_length)
{
    uint32_t temporal_index;
    uint32_t intervalIndex;

#if OVERSHOOT_STAT_PRINT
    uint32_t pictureIndex;
#endif

    EbErrorType return_error = EB_ErrorNone;
    RateControlContext *context_ptr;
    EB_MALLOC(RateControlContext*, context_ptr, sizeof(RateControlContext), EB_N_PTR);

    *context_dbl_ptr = context_ptr;

    context_ptr->rate_control_input_tasks_fifo_ptr = rate_control_input_tasks_fifo_ptr;
    context_ptr->rate_control_output_results_fifo_ptr = rate_control_output_results_fifo_ptr;

    // High level RC
    return_error = HighLevelRateControlContextCtor(
        &context_ptr->high_level_rate_control_ptr);
    if (return_error == EB_ErrorInsufficientResources) {
        return EB_ErrorInsufficientResources;
    }

    for (temporal_index = 0; temporal_index < EB_MAX_TEMPORAL_LAYERS; temporal_index++) {
        context_ptr->frames_in_interval[temporal_index] = 0;
    }

    EB_MALLOC(RateControlIntervalParamContext**, context_ptr->rateControlParamQueue, sizeof(RateControlIntervalParamContext*)*PARALLEL_GOP_MAX_NUMBER, EB_N_PTR);

    context_ptr->rateControlParamQueueHeadIndex = 0;
    for (intervalIndex = 0; intervalIndex < PARALLEL_GOP_MAX_NUMBER; intervalIndex++) {
        return_error = rate_control_interval_param_context_ctor(
            &context_ptr->rateControlParamQueue[intervalIndex]);
        context_ptr->rateControlParamQueue[intervalIndex]->first_poc = (intervalIndex*(uint32_t)(intra_period_length + 1));
        context_ptr->rateControlParamQueue[intervalIndex]->last_poc = ((intervalIndex + 1)*(uint32_t)(intra_period_length + 1)) - 1;
        if (return_error == EB_ErrorInsufficientResources) {
            return EB_ErrorInsufficientResources;
        }
    }

#if OVERSHOOT_STAT_PRINT
    context_ptr->coded_frames_stat_queue_head_index = 0;
    context_ptr->coded_frames_stat_queue_tail_index = 0;
    EB_MALLOC(CodedFramesStatsEntry**, context_ptr->coded_frames_stat_queue, sizeof(CodedFramesStatsEntry*)*CODED_FRAMES_STAT_QUEUE_MAX_DEPTH, EB_N_PTR);

    for (pictureIndex = 0; pictureIndex < CODED_FRAMES_STAT_QUEUE_MAX_DEPTH; ++pictureIndex) {
        return_error = rate_control_coded_frames_stats_context_ctor(
            &context_ptr->coded_frames_stat_queue[pictureIndex],
            pictureIndex);
        if (return_error == EB_ErrorInsufficientResources) {
            return EB_ErrorInsufficientResources;
        }
    }
    context_ptr->max_bit_actual_per_sw = 0;
    context_ptr->max_bit_actual_per_gop = 0;
#endif

    context_ptr->base_layer_frames_avg_qp = 0;
    context_ptr->base_layer_intra_frames_avg_qp = 0;


    context_ptr->intra_coef_rate = 4;
    context_ptr->extra_bits = 0;
    context_ptr->extra_bits_gen = 0;
    context_ptr->max_rate_adjust_delta_qp = 0;


    return EB_ErrorNone;
}
void HighLevelRcInputPictureMode2(
    PictureParentControlSet         *picture_control_set_ptr,
    SequenceControlSet              *sequence_control_set_ptr,
    EncodeContext                   *encode_context_ptr,
    RateControlContext              *context_ptr,
    HighLevelRateControlContext     *high_level_rate_control_ptr)
{

    EbBool                             end_of_sequence_flag = EB_TRUE;

    HlRateControlHistogramEntry      *hlRateControlHistogramPtrTemp;
    // Queue variables
    uint32_t                             queueEntryIndexTemp;
    uint32_t                             queueEntryIndexTemp2;
    uint32_t                             queueEntryIndexHeadTemp;


    uint64_t                              minLaBitDistance;
    uint32_t                              selectedRefQpTableIndex;
    uint32_t                              selectedRefQp;
#if RC_UPDATE_TARGET_RATE
    uint32_t                              selectedOrgRefQp;
#endif
    uint32_t                                previous_selected_ref_qp = encode_context_ptr->previous_selected_ref_qp;
    uint64_t                                max_coded_poc = encode_context_ptr->max_coded_poc;
    uint32_t                                max_coded_poc_selected_ref_qp = encode_context_ptr->max_coded_poc_selected_ref_qp;


    uint32_t                              refQpIndex;
    uint32_t                              refQpIndexTemp;
    uint32_t                              refQpTableIndex;

    uint32_t                              area_in_pixel;
    uint32_t                              numOfFullLcus;
    uint32_t                              qpSearchMin;
    uint32_t                              qpSearchMax;
    int32_t                              qpStep = 1;
    EbBool                             bestQpFound;
    uint32_t                              temporal_layer_index;
    EbBool                             tables_updated;

    uint64_t                              bit_constraint_per_sw = 0;

    RateControlTables                    *rateControlTablesPtr;
    EbBitNumber                        *sadBitsArrayPtr;
    EbBitNumber                        *intraSadBitsArrayPtr;
    uint32_t                               pred_bits_ref_qp;

    for (temporal_layer_index = 0; temporal_layer_index < EB_MAX_TEMPORAL_LAYERS; temporal_layer_index++) {
        picture_control_set_ptr->bits_per_sw_per_layer[temporal_layer_index] = 0;
    }
    picture_control_set_ptr->total_bits_per_gop = 0;

    area_in_pixel = sequence_control_set_ptr->luma_width * sequence_control_set_ptr->luma_height;;

    eb_block_on_mutex(sequence_control_set_ptr->encode_context_ptr->rate_table_update_mutex);

    tables_updated = sequence_control_set_ptr->encode_context_ptr->rate_control_tables_array_updated;
    picture_control_set_ptr->percentage_updated = EB_FALSE;

    if (sequence_control_set_ptr->static_config.look_ahead_distance != 0) {

        // Increamenting the head of the hl_rate_control_historgram_queue and clean up the entores
        hlRateControlHistogramPtrTemp = (encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index]);
        while ((hlRateControlHistogramPtrTemp->lifeCount == 0) && hlRateControlHistogramPtrTemp->passedToHlrc) {

            eb_block_on_mutex(sequence_control_set_ptr->encode_context_ptr->hl_rate_control_historgram_queue_mutex);
            // Reset the Reorder Queue Entry
            hlRateControlHistogramPtrTemp->picture_number += INITIAL_RATE_CONTROL_REORDER_QUEUE_MAX_DEPTH;
            hlRateControlHistogramPtrTemp->lifeCount = -1;
            hlRateControlHistogramPtrTemp->passedToHlrc = EB_FALSE;
            hlRateControlHistogramPtrTemp->isCoded = EB_FALSE;
            hlRateControlHistogramPtrTemp->totalNumBitsCoded = 0;

            // Increment the Reorder Queue head Ptr
            encode_context_ptr->hl_rate_control_historgram_queue_head_index =
                (encode_context_ptr->hl_rate_control_historgram_queue_head_index == HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ? 0 : encode_context_ptr->hl_rate_control_historgram_queue_head_index + 1;
            eb_release_mutex(sequence_control_set_ptr->encode_context_ptr->hl_rate_control_historgram_queue_mutex);
            hlRateControlHistogramPtrTemp = encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index];

        }
        // For the case that number of frames in the sliding window is less than size of the look ahead or intra Refresh. i.e. end of sequence
        if ((picture_control_set_ptr->frames_in_sw < MIN(sequence_control_set_ptr->static_config.look_ahead_distance + 1, (uint32_t)sequence_control_set_ptr->intra_period_length + 1))) {

            selectedRefQp = max_coded_poc_selected_ref_qp;

            // Update the QP for the sliding window based on the status of RC
            if ((context_ptr->extra_bits_gen > (int64_t)(context_ptr->virtual_buffer_size << 3))) {
                selectedRefQp = (uint32_t)MAX((int32_t)selectedRefQp - 2, 0);
            }
            else if ((context_ptr->extra_bits_gen > (int64_t)(context_ptr->virtual_buffer_size << 2))) {
                selectedRefQp = (uint32_t)MAX((int32_t)selectedRefQp - 1, 0);
            }
            if ((context_ptr->extra_bits_gen < -(int64_t)(context_ptr->virtual_buffer_size << 2))) {
                selectedRefQp += 2;
            }
            else if ((context_ptr->extra_bits_gen < -(int64_t)(context_ptr->virtual_buffer_size << 1))) {
                selectedRefQp += 1;
            }

            if ((picture_control_set_ptr->frames_in_sw < (uint32_t)(sequence_control_set_ptr->intra_period_length + 1)) &&
                (picture_control_set_ptr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0)) {
                selectedRefQp = (uint32_t)CLIP3(
                    sequence_control_set_ptr->static_config.min_qp_allowed,
                    sequence_control_set_ptr->static_config.max_qp_allowed,
                    selectedRefQp + 1);
            }

            queueEntryIndexHeadTemp = (int32_t)(picture_control_set_ptr->picture_number - encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index]->picture_number);
            queueEntryIndexHeadTemp += encode_context_ptr->hl_rate_control_historgram_queue_head_index;
            queueEntryIndexHeadTemp = (queueEntryIndexHeadTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ?
                queueEntryIndexHeadTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH :
                queueEntryIndexHeadTemp;

            queueEntryIndexTemp = queueEntryIndexHeadTemp;
            {

                hlRateControlHistogramPtrTemp = (encode_context_ptr->hl_rate_control_historgram_queue[queueEntryIndexTemp]);
                refQpIndexTemp = selectedRefQp + QP_OFFSET_LAYER_ARRAY[hlRateControlHistogramPtrTemp->temporal_layer_index];
                refQpIndexTemp = (uint32_t)CLIP3(
                    sequence_control_set_ptr->static_config.min_qp_allowed,
                    sequence_control_set_ptr->static_config.max_qp_allowed,
                    refQpIndexTemp);

                if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                    refQpIndexTemp = (uint32_t)MAX((int32_t)refQpIndexTemp + RC_INTRA_QP_OFFSET, 0);
                }

                hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = 0;
                rateControlTablesPtr = &encode_context_ptr->rate_control_tables_array[refQpIndexTemp];
                sadBitsArrayPtr = rateControlTablesPtr->sad_bits_array[hlRateControlHistogramPtrTemp->temporal_layer_index];
                intraSadBitsArrayPtr = rateControlTablesPtr->intra_sad_bits_array[hlRateControlHistogramPtrTemp->temporal_layer_index];
                pred_bits_ref_qp = 0;
                numOfFullLcus = 0;

                if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                    // Loop over block in the frame and calculated the predicted bits at reg QP
                    {
                        unsigned i;
                        uint32_t accum = 0;
                        for (i = 0; i < NUMBER_OF_INTRA_SAD_INTERVALS; ++i)
                        {
                            accum += (uint32_t)(hlRateControlHistogramPtrTemp->ois_distortion_histogram[i] * intraSadBitsArrayPtr[i]);
                        }

                        pred_bits_ref_qp = accum;
                        numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;
                    }
                    hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                }

                else {
                    {
                        unsigned i;
                        uint32_t accum = 0;
                        for (i = 0; i < NUMBER_OF_SAD_INTERVALS; ++i)
                        {
                            accum += (uint32_t)(hlRateControlHistogramPtrTemp->me_distortion_histogram[i] * sadBitsArrayPtr[i]);
                        }

                        pred_bits_ref_qp = accum;
                        numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;

                    }
                    hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                }

                // Scale for in complete
                //  pred_bits_ref_qp is normalized based on the area because of the LCUs at the picture boundries
                hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] * (uint64_t)area_in_pixel / (numOfFullLcus << 12);

                // Store the pred_bits_ref_qp for the first frame in the window to PCS
                picture_control_set_ptr->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];

            }
        }
        else {
            // Loop over the QPs and find the best QP
            minLaBitDistance = MAX_UNSIGNED_VALUE;
            qpSearchMin = (uint8_t)CLIP3(
                sequence_control_set_ptr->static_config.min_qp_allowed,
                sequence_control_set_ptr->static_config.max_qp_allowed,
                (uint32_t)MAX((int32_t)sequence_control_set_ptr->qp - 20, 0));

            qpSearchMax = (uint8_t)CLIP3(
                sequence_control_set_ptr->static_config.min_qp_allowed,
                sequence_control_set_ptr->static_config.max_qp_allowed,
                sequence_control_set_ptr->qp + 20);

            for (refQpTableIndex = qpSearchMin; refQpTableIndex < qpSearchMax; refQpTableIndex++) {
                high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpTableIndex] = 0;
            }

            bit_constraint_per_sw = high_level_rate_control_ptr->bit_constraint_per_sw * picture_control_set_ptr->frames_in_sw / (sequence_control_set_ptr->static_config.look_ahead_distance + 1);

            // Update the target rate for the sliding window based on the status of RC
            if ((context_ptr->extra_bits_gen > (int64_t)(context_ptr->virtual_buffer_size * 10))) {
                bit_constraint_per_sw = bit_constraint_per_sw * 130 / 100;
            }
            else if ((context_ptr->extra_bits_gen > (int64_t)(context_ptr->virtual_buffer_size << 3))) {
                bit_constraint_per_sw = bit_constraint_per_sw * 120 / 100;
            }
            else if ((context_ptr->extra_bits_gen > (int64_t)(context_ptr->virtual_buffer_size << 2))) {
                bit_constraint_per_sw = bit_constraint_per_sw * 110 / 100;
            }
            if ((context_ptr->extra_bits_gen < -(int64_t)(context_ptr->virtual_buffer_size << 3))) {
                bit_constraint_per_sw = bit_constraint_per_sw * 80 / 100;
            }
            else if ((context_ptr->extra_bits_gen < -(int64_t)(context_ptr->virtual_buffer_size << 2))) {
                bit_constraint_per_sw = bit_constraint_per_sw * 90 / 100;
            }

            // Loop over proper QPs and find the Predicted bits for that QP. Find the QP with the closest total predicted rate to target bits for the sliding window.
            previous_selected_ref_qp = CLIP3(
                qpSearchMin,
                qpSearchMax,
                previous_selected_ref_qp);
            refQpTableIndex = previous_selected_ref_qp;
            selectedRefQpTableIndex = refQpTableIndex;
            selectedRefQp = ref_qp_list_table[selectedRefQpTableIndex];
            bestQpFound = EB_FALSE;
            while (refQpTableIndex >= qpSearchMin && refQpTableIndex <= qpSearchMax && !bestQpFound) {

                refQpIndex = CLIP3(
                    sequence_control_set_ptr->static_config.min_qp_allowed,
                    sequence_control_set_ptr->static_config.max_qp_allowed,
                    ref_qp_list_table[refQpTableIndex]);
                high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] = 0;

                // Finding the predicted bits for each frame in the sliding window at the reference Qp(s)
                queueEntryIndexHeadTemp = (int32_t)(picture_control_set_ptr->picture_number - encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index]->picture_number);
                queueEntryIndexHeadTemp += encode_context_ptr->hl_rate_control_historgram_queue_head_index;
                queueEntryIndexHeadTemp = (queueEntryIndexHeadTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ?
                    queueEntryIndexHeadTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH :
                    queueEntryIndexHeadTemp;

                queueEntryIndexTemp = queueEntryIndexHeadTemp;
                // This is set to false, so the last frame would go inside the loop
                end_of_sequence_flag = EB_FALSE;

                while (!end_of_sequence_flag &&
                    queueEntryIndexTemp <= queueEntryIndexHeadTemp + sequence_control_set_ptr->static_config.look_ahead_distance) {

                    queueEntryIndexTemp2 = (queueEntryIndexTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ? queueEntryIndexTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH : queueEntryIndexTemp;
                    hlRateControlHistogramPtrTemp = (encode_context_ptr->hl_rate_control_historgram_queue[queueEntryIndexTemp2]);

                    refQpIndexTemp = refQpIndex + QP_OFFSET_LAYER_ARRAY[hlRateControlHistogramPtrTemp->temporal_layer_index];
                    refQpIndexTemp = (uint32_t)CLIP3(
                        sequence_control_set_ptr->static_config.min_qp_allowed,
                        sequence_control_set_ptr->static_config.max_qp_allowed,
                        refQpIndexTemp);

                    if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                        refQpIndexTemp = (uint32_t)MAX((int32_t)refQpIndexTemp + RC_INTRA_QP_OFFSET, 0);
                    }

                    hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = 0;

                    if (refQpTableIndex == previous_selected_ref_qp) {
                        hlRateControlHistogramPtrTemp->lifeCount--;
                    }
                    if (hlRateControlHistogramPtrTemp->isCoded) {
                        // If the frame is already coded, use the actual number of bits
                        hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->totalNumBitsCoded;
                    }
                    else {
                        rateControlTablesPtr = &encode_context_ptr->rate_control_tables_array[refQpIndexTemp];
                        sadBitsArrayPtr = rateControlTablesPtr->sad_bits_array[hlRateControlHistogramPtrTemp->temporal_layer_index];
                        intraSadBitsArrayPtr = rateControlTablesPtr->intra_sad_bits_array[0];
                        pred_bits_ref_qp = 0;
                        numOfFullLcus = 0;

                        if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                            // Loop over block in the frame and calculated the predicted bits at reg QP
                            unsigned i;
                            uint32_t accum = 0;
                            for (i = 0; i < NUMBER_OF_INTRA_SAD_INTERVALS; ++i)
                            {
                                accum += (uint32_t)(hlRateControlHistogramPtrTemp->ois_distortion_histogram[i] * intraSadBitsArrayPtr[i]);
                            }

                            pred_bits_ref_qp = accum;
                            numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;
                            hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                        }
                        else {
                            unsigned i;
                            uint32_t accum = 0;
                            uint32_t accumIntra = 0;
                            for (i = 0; i < NUMBER_OF_SAD_INTERVALS; ++i)
                            {
                                accum += (uint32_t)(hlRateControlHistogramPtrTemp->me_distortion_histogram[i] * sadBitsArrayPtr[i]);
                                accumIntra += (uint32_t)(hlRateControlHistogramPtrTemp->ois_distortion_histogram[i] * intraSadBitsArrayPtr[i]);

                            }
                            if (accum > accumIntra * 3)
                                pred_bits_ref_qp = accumIntra;
                            else
                                pred_bits_ref_qp = accum;
                            numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;
                            hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                        }

                        // Scale for in complete LCSs
                        //  pred_bits_ref_qp is normalized based on the area because of the LCUs at the picture boundries
                        hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] * (uint64_t)area_in_pixel / (numOfFullLcus << 12);

                    }
                    high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] += hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];
                    // Store the pred_bits_ref_qp for the first frame in the window to PCS
                    if (queueEntryIndexHeadTemp == queueEntryIndexTemp2)
                        picture_control_set_ptr->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];

                    end_of_sequence_flag = hlRateControlHistogramPtrTemp->end_of_sequence_flag;
                    queueEntryIndexTemp++;
                }

                if (minLaBitDistance >= (uint64_t)ABS((int64_t)high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] - (int64_t)bit_constraint_per_sw)) {
                    minLaBitDistance = (uint64_t)ABS((int64_t)high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] - (int64_t)bit_constraint_per_sw);
                    selectedRefQpTableIndex = refQpTableIndex;
                    selectedRefQp = refQpIndex;
                }
                else {
                    bestQpFound = EB_TRUE;
                }

                if (refQpTableIndex == previous_selected_ref_qp) {
                    if (high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] > bit_constraint_per_sw) {
                        qpStep = +1;
                    }
                    else {
                        qpStep = -1;
                    }
                }
                refQpTableIndex = (uint32_t)(refQpTableIndex + qpStep);

            }
        }

#if RC_UPDATE_TARGET_RATE
        selectedOrgRefQp = selectedRefQp;
        if (sequence_control_set_ptr->intra_period_length != -1 && picture_control_set_ptr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0 &&
            (int32_t)picture_control_set_ptr->frames_in_sw > sequence_control_set_ptr->intra_period_length) {
            if (picture_control_set_ptr->picture_number > 0) {
                picture_control_set_ptr->intra_selected_org_qp = (uint8_t)selectedRefQp;
            }
            else {
                selectedOrgRefQp = selectedRefQp + 1;
                selectedRefQp = selectedRefQp + 1;
            }
            refQpIndex = selectedRefQp;
            high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] = 0;

            if (high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] == 0) {

                // Finding the predicted bits for each frame in the sliding window at the reference Qp(s)
                //queueEntryIndexTemp = encode_context_ptr->hl_rate_control_historgram_queue_head_index;
                queueEntryIndexHeadTemp = (int32_t)(picture_control_set_ptr->picture_number - encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index]->picture_number);
                queueEntryIndexHeadTemp += encode_context_ptr->hl_rate_control_historgram_queue_head_index;
                queueEntryIndexHeadTemp = (queueEntryIndexHeadTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ?
                    queueEntryIndexHeadTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH :
                    queueEntryIndexHeadTemp;

                queueEntryIndexTemp = queueEntryIndexHeadTemp;

                // This is set to false, so the last frame would go inside the loop
                end_of_sequence_flag = EB_FALSE;

                while (!end_of_sequence_flag &&
                    //queueEntryIndexTemp <= encode_context_ptr->hl_rate_control_historgram_queue_head_index+sequence_control_set_ptr->static_config.look_ahead_distance){
                    queueEntryIndexTemp <= queueEntryIndexHeadTemp + sequence_control_set_ptr->static_config.look_ahead_distance) {

                    queueEntryIndexTemp2 = (queueEntryIndexTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ? queueEntryIndexTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH : queueEntryIndexTemp;
                    hlRateControlHistogramPtrTemp = (encode_context_ptr->hl_rate_control_historgram_queue[queueEntryIndexTemp2]);


                    refQpIndexTemp = refQpIndex + QP_OFFSET_LAYER_ARRAY[hlRateControlHistogramPtrTemp->temporal_layer_index];
                    refQpIndexTemp = (uint32_t)CLIP3(
                        sequence_control_set_ptr->static_config.min_qp_allowed,
                        sequence_control_set_ptr->static_config.max_qp_allowed,
                        refQpIndexTemp);

                    if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                        refQpIndexTemp = (uint32_t)MAX((int32_t)refQpIndexTemp + RC_INTRA_QP_OFFSET, 0);
                    }

                    hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = 0;

                    if (hlRateControlHistogramPtrTemp->isCoded) {
                        hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->totalNumBitsCoded;
                    }
                    else {
                        rateControlTablesPtr = &encode_context_ptr->rate_control_tables_array[refQpIndexTemp];
                        sadBitsArrayPtr = rateControlTablesPtr->sad_bits_array[hlRateControlHistogramPtrTemp->temporal_layer_index];
                        intraSadBitsArrayPtr = rateControlTablesPtr->intra_sad_bits_array[hlRateControlHistogramPtrTemp->temporal_layer_index];
                        pred_bits_ref_qp = 0;

                        numOfFullLcus = 0;

                        if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                            // Loop over block in the frame and calculated the predicted bits at reg QP

                            {
                                unsigned i;
                                uint32_t accum = 0;
                                for (i = 0; i < NUMBER_OF_INTRA_SAD_INTERVALS; ++i)
                                {
                                    accum += (uint32_t)(hlRateControlHistogramPtrTemp->ois_distortion_histogram[i] * intraSadBitsArrayPtr[i]);
                                }

                                pred_bits_ref_qp = accum;
                                numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;
                            }
                            hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                        }

                        else {
                            unsigned i;
                            uint32_t accum = 0;
                            uint32_t accumIntra = 0;
                            for (i = 0; i < NUMBER_OF_SAD_INTERVALS; ++i)
                            {
                                accum += (uint32_t)(hlRateControlHistogramPtrTemp->me_distortion_histogram[i] * sadBitsArrayPtr[i]);
                                accumIntra += (uint32_t)(hlRateControlHistogramPtrTemp->ois_distortion_histogram[i] * intraSadBitsArrayPtr[i]);

                            }
                            if (accum > accumIntra * 3)
                                pred_bits_ref_qp = accumIntra;
                            else
                                pred_bits_ref_qp = accum;
                            numOfFullLcus = hlRateControlHistogramPtrTemp->full_sb_count;
                            hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] += pred_bits_ref_qp;
                        }

                        // Scale for in complete
                        //  pred_bits_ref_qp is normalized based on the area because of the LCUs at the picture boundries
                        hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp] * (uint64_t)area_in_pixel / (numOfFullLcus << 12);

                    }
                    high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[refQpIndex] += hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];
                    // Store the pred_bits_ref_qp for the first frame in the window to PCS
                    //  if(encode_context_ptr->hl_rate_control_historgram_queue_head_index == queueEntryIndexTemp2)
                    if (queueEntryIndexHeadTemp == queueEntryIndexTemp2)
                        picture_control_set_ptr->pred_bits_ref_qp[refQpIndexTemp] = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];

                    end_of_sequence_flag = hlRateControlHistogramPtrTemp->end_of_sequence_flag;
                    queueEntryIndexTemp++;
                }
            }
        }
#endif
        picture_control_set_ptr->tables_updated = tables_updated;
        EbBool expensiveISlice = EB_FALSE;
        // Looping over the window to find the percentage of bit allocation in each layer
        if ((sequence_control_set_ptr->intra_period_length != -1) &&
            ((int32_t)picture_control_set_ptr->frames_in_sw > sequence_control_set_ptr->intra_period_length) &&
            ((int32_t)picture_control_set_ptr->frames_in_sw > sequence_control_set_ptr->intra_period_length)) {
            uint64_t iSliceBits = 0;

            if (picture_control_set_ptr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0) {

                queueEntryIndexHeadTemp = (int32_t)(picture_control_set_ptr->picture_number - encode_context_ptr->hl_rate_control_historgram_queue[encode_context_ptr->hl_rate_control_historgram_queue_head_index]->picture_number);
                queueEntryIndexHeadTemp += encode_context_ptr->hl_rate_control_historgram_queue_head_index;
                queueEntryIndexHeadTemp = (queueEntryIndexHeadTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ?
                    queueEntryIndexHeadTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH :
                    queueEntryIndexHeadTemp;

                queueEntryIndexTemp = queueEntryIndexHeadTemp;

                // This is set to false, so the last frame would go inside the loop
                end_of_sequence_flag = EB_FALSE;

                while (!end_of_sequence_flag &&
                    queueEntryIndexTemp <= queueEntryIndexHeadTemp + sequence_control_set_ptr->intra_period_length) {

                    queueEntryIndexTemp2 = (queueEntryIndexTemp > HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH - 1) ? queueEntryIndexTemp - HIGH_LEVEL_RATE_CONTROL_HISTOGRAM_QUEUE_MAX_DEPTH : queueEntryIndexTemp;
                    hlRateControlHistogramPtrTemp = (encode_context_ptr->hl_rate_control_historgram_queue[queueEntryIndexTemp2]);

                    refQpIndexTemp = selectedRefQp + QP_OFFSET_LAYER_ARRAY[hlRateControlHistogramPtrTemp->temporal_layer_index];
                    refQpIndexTemp = (uint32_t)CLIP3(
                        sequence_control_set_ptr->static_config.min_qp_allowed,
                        sequence_control_set_ptr->static_config.max_qp_allowed,
                        refQpIndexTemp);

                    if (hlRateControlHistogramPtrTemp->slice_type == I_SLICE) {
                        refQpIndexTemp = (uint32_t)MAX((int32_t)refQpIndexTemp + RC_INTRA_QP_OFFSET, 0);
                    }
                    if (queueEntryIndexTemp == queueEntryIndexHeadTemp) {
                        iSliceBits = hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];
                    }
                    picture_control_set_ptr->total_bits_per_gop += hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];
                    picture_control_set_ptr->bits_per_sw_per_layer[hlRateControlHistogramPtrTemp->temporal_layer_index] += hlRateControlHistogramPtrTemp->pred_bits_ref_qp[refQpIndexTemp];
                    picture_control_set_ptr->percentage_updated = EB_TRUE;

                    end_of_sequence_flag = hlRateControlHistogramPtrTemp->end_of_sequence_flag;
                    queueEntryIndexTemp++;
                }
                if (iSliceBits * 100 > 85 * picture_control_set_ptr->total_bits_per_gop) {
                    expensiveISlice = EB_TRUE;
                }
                if (picture_control_set_ptr->total_bits_per_gop == 0) {
                    for (temporal_layer_index = 0; temporal_layer_index < EB_MAX_TEMPORAL_LAYERS; temporal_layer_index++) {
                        picture_control_set_ptr->bits_per_sw_per_layer[temporal_layer_index] = rate_percentage_layer_array[sequence_control_set_ptr->static_config.hierarchical_levels][temporal_layer_index];
                    }
                }
            }
        }
        else {
            for (temporal_layer_index = 0; temporal_layer_index < EB_MAX_TEMPORAL_LAYERS; temporal_layer_index++) {
                picture_control_set_ptr->bits_per_sw_per_layer[temporal_layer_index] = rate_percentage_layer_array[sequence_control_set_ptr->static_config.hierarchical_levels][temporal_layer_index];
            }
        }
        if (expensiveISlice) {
            if (tables_updated) {
                selectedRefQp = (uint32_t)MAX((int32_t)selectedRefQp - 1, 0);
            }
            else {
                selectedRefQp = (uint32_t)MAX((int32_t)selectedRefQp - 3, 0);
            }
            selectedRefQp = (uint32_t)CLIP3(
                sequence_control_set_ptr->static_config.min_qp_allowed,
                sequence_control_set_ptr->static_config.max_qp_allowed,
                selectedRefQp);
        }
        // Set the QP
        previous_selected_ref_qp = selectedRefQp;
        if (picture_control_set_ptr->picture_number > max_coded_poc && picture_control_set_ptr->temporal_layer_index < 2 && !picture_control_set_ptr->end_of_sequence_region) {

            max_coded_poc = picture_control_set_ptr->picture_number;
            max_coded_poc_selected_ref_qp = previous_selected_ref_qp;
            encode_context_ptr->previous_selected_ref_qp = previous_selected_ref_qp;
            encode_context_ptr->max_coded_poc = max_coded_poc;
            encode_context_ptr->max_coded_poc_selected_ref_qp = max_coded_poc_selected_ref_qp;

        }
        picture_control_set_ptr->best_pred_qp = (uint8_t)CLIP3(
            sequence_control_set_ptr->static_config.min_qp_allowed,
            sequence_control_set_ptr->static_config.max_qp_allowed,
            selectedRefQp + QP_OFFSET_LAYER_ARRAY[picture_control_set_ptr->temporal_layer_index]);
        if (picture_control_set_ptr->slice_type == I_SLICE) {
            picture_control_set_ptr->best_pred_qp = (uint8_t)MAX((int32_t)picture_control_set_ptr->best_pred_qp + RC_INTRA_QP_OFFSET, 0);
        }
#if RC_UPDATE_TARGET_RATE
        if (picture_control_set_ptr->picture_number == 0) {
            high_level_rate_control_ptr->prev_intra_selected_ref_qp = selectedRefQp;
            high_level_rate_control_ptr->prev_intra_org_selected_ref_qp = selectedRefQp;
        }
        if (sequence_control_set_ptr->intra_period_length != -1) {
            if (picture_control_set_ptr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0) {
                high_level_rate_control_ptr->prev_intra_selected_ref_qp = selectedRefQp;
                high_level_rate_control_ptr->prev_intra_org_selected_ref_qp = selectedOrgRefQp;
            }
        }
#endif
        picture_control_set_ptr->target_bits_best_pred_qp = picture_control_set_ptr->pred_bits_ref_qp[picture_control_set_ptr->best_pred_qp];
        //if (picture_control_set_ptr->slice_type == 2)
        // {
        //printf("\nTID: %d\t", picture_control_set_ptr->temporal_layer_index);
        //printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
        //    picture_control_set_ptr->picture_number,
        //    picture_control_set_ptr->best_pred_qp,
        //    (int32_t)picture_control_set_ptr->target_bits_best_pred_qp,
        //    (int32_t)high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[selectedRefQp - 1],
        //    (int32_t)high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[selectedRefQp],
        //    (int32_t)high_level_rate_control_ptr->pred_bits_ref_qp_per_sw[selectedRefQp + 1],
        //    (int32_t)high_level_rate_control_ptr->bit_constraint_per_sw,
        //    (int32_t)bit_constraint_per_sw,
        //    (int32_t)high_level_rate_control_ptr->virtual_buffer_level);
        //}
    }
    eb_release_mutex(sequence_control_set_ptr->encode_context_ptr->rate_table_update_mutex);
}

#if ADD_DELTA_QP_SUPPORT ||  NEW_QPS

static const uint8_t quantizer_to_qindex[] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48,
    52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100,
    104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
    156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
    208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
};
#endif

#if NEW_QPS
#define MAX_Q_INDEX 255
#define MIN_Q_INDEX 0

extern int16_t av1_ac_quant_Q3(int32_t qindex, int32_t delta, AomBitDepth bit_depth);
// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream

double av1_convert_qindex_to_q(int32_t qindex, AomBitDepth bit_depth) {
    // Convert the index to a real Q value (scaled down to match old Q values)
    switch (bit_depth) {
    case AOM_BITS_8: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 4.0;
    case AOM_BITS_10: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 16.0;
    case AOM_BITS_12: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 64.0;
    default:
        assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
        return -1.0;
    }
}
int32_t av1_compute_qdelta(double qstart, double qtarget,
    AomBitDepth bit_depth) {
    int32_t start_index = MAX_Q_INDEX;
    int32_t target_index = MAX_Q_INDEX;
    int32_t i;

    // Convert the average q value to an index.
    for (i = MIN_Q_INDEX; i < MAX_Q_INDEX; ++i) {
        start_index = i;
        if (av1_convert_qindex_to_q(i, bit_depth) >= qstart) break;
    }

    // Convert the q target to an index
    for (i = MIN_Q_INDEX; i < MAX_Q_INDEX; ++i) {
        target_index = i;
        if (av1_convert_qindex_to_q(i, bit_depth) >= qtarget) break;
    }

    return target_index - start_index;
}
#endif

void* rate_control_kernel(void *input_ptr)
{
    // Context
    RateControlContext        *context_ptr = (RateControlContext*)input_ptr;
    // EncodeContext             *encode_context_ptr;

    RateControlIntervalParamContext *rateControlParamPtr;

    RateControlIntervalParamContext *prevGopRateControlParamPtr;
    RateControlIntervalParamContext *nextGopRateControlParamPtr;

    PictureControlSet         *picture_control_set_ptr;
    PictureParentControlSet   *parentPictureControlSetPtr;

    // Config
    SequenceControlSet        *sequence_control_set_ptr;

    // Input
    EbObjectWrapper           *rateControlTasksWrapperPtr;
    RateControlTasks          *rateControlTasksPtr;

    // Output
    EbObjectWrapper           *rateControlResultsWrapperPtr;
    RateControlResults        *rateControlResultsPtr;

    // SB Loop variables
    LargestCodingUnit         *sb_ptr;
    uint32_t                       lcuCodingOrder;
    uint64_t                       totalNumberOfFbFrames = 0;

    RateControlTaskTypes       task_type;
    EbRateControlModel          *rc_model_ptr;

    rate_control_model_ctor(&rc_model_ptr);

    for (;;) {

        // Get RateControl Task
        eb_get_full_object(
            context_ptr->rate_control_input_tasks_fifo_ptr,
            &rateControlTasksWrapperPtr);

        rateControlTasksPtr = (RateControlTasks*)rateControlTasksWrapperPtr->object_ptr;
        task_type = rateControlTasksPtr->task_type;

        // Modify these for different temporal layers later
        switch (task_type) {

        case RC_PICTURE_MANAGER_RESULT:

            picture_control_set_ptr = (PictureControlSet*)rateControlTasksPtr->picture_control_set_wrapper_ptr->object_ptr;
            sequence_control_set_ptr = (SequenceControlSet*)picture_control_set_ptr->sequence_control_set_wrapper_ptr->object_ptr;

            // High level RC
            if (picture_control_set_ptr->picture_number == 0) {

                rate_control_model_init(rc_model_ptr, sequence_control_set_ptr);
                context_ptr->high_level_rate_control_ptr->target_bit_rate = sequence_control_set_ptr->static_config.target_bit_rate;
                context_ptr->high_level_rate_control_ptr->frame_rate = sequence_control_set_ptr->frame_rate;
                context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_frame = (uint64_t)MAX((int64_t)1, (int64_t)((context_ptr->high_level_rate_control_ptr->target_bit_rate << RC_PRECISION) / context_ptr->high_level_rate_control_ptr->frame_rate));

                context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_sw = context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_frame * (sequence_control_set_ptr->static_config.look_ahead_distance + 1);
                context_ptr->high_level_rate_control_ptr->bit_constraint_per_sw = context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_sw;

#if RC_UPDATE_TARGET_RATE
                context_ptr->high_level_rate_control_ptr->previous_updated_bit_constraint_per_sw = context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_sw;
#endif

                int32_t totalFrameInInterval = sequence_control_set_ptr->intra_period_length;
                uint32_t gopPeriod = (1 << picture_control_set_ptr->parent_pcs_ptr->hierarchical_levels);
                context_ptr->frame_rate = sequence_control_set_ptr->frame_rate;
                while (totalFrameInInterval >= 0) {
                    if (totalFrameInInterval % (gopPeriod) == 0)
                        context_ptr->frames_in_interval[0] ++;
                    else if (totalFrameInInterval % (gopPeriod >> 1) == 0)
                        context_ptr->frames_in_interval[1] ++;
                    else if (totalFrameInInterval % (gopPeriod >> 2) == 0)
                        context_ptr->frames_in_interval[2] ++;
                    else if (totalFrameInInterval % (gopPeriod >> 3) == 0)
                        context_ptr->frames_in_interval[3] ++;
                    else if (totalFrameInInterval % (gopPeriod >> 4) == 0)
                        context_ptr->frames_in_interval[4] ++;
                    else if (totalFrameInInterval % (gopPeriod >> 5) == 0)
                        context_ptr->frames_in_interval[5] ++;
                    totalFrameInInterval--;
                }
                context_ptr->virtual_buffer_size = (((uint64_t)sequence_control_set_ptr->static_config.target_bit_rate * 3) << RC_PRECISION) / (context_ptr->frame_rate);
                context_ptr->rate_average_periodin_frames = (uint64_t)sequence_control_set_ptr->static_config.intra_period_length + 1;
                context_ptr->virtual_buffer_level_initial_value = context_ptr->virtual_buffer_size >> 1;
                context_ptr->virtual_buffer_level = context_ptr->virtual_buffer_size >> 1;
                context_ptr->previous_virtual_buffer_level = context_ptr->virtual_buffer_size >> 1;
                context_ptr->vb_fill_threshold1 = (context_ptr->virtual_buffer_size * 6) >> 3;
                context_ptr->vb_fill_threshold2 = (context_ptr->virtual_buffer_size << 3) >> 3;
                context_ptr->base_layer_frames_avg_qp = sequence_control_set_ptr->qp;
                context_ptr->base_layer_intra_frames_avg_qp = sequence_control_set_ptr->qp;
            }
            if (sequence_control_set_ptr->static_config.rate_control_mode)
            {
                picture_control_set_ptr->parent_pcs_ptr->intra_selected_org_qp = 0;
                HighLevelRcInputPictureMode2(
                    picture_control_set_ptr->parent_pcs_ptr,
                    sequence_control_set_ptr,
                    sequence_control_set_ptr->encode_context_ptr,
                    context_ptr,
                    context_ptr->high_level_rate_control_ptr);


            }

            // Frame level RC
            if (sequence_control_set_ptr->intra_period_length == -1 || sequence_control_set_ptr->static_config.rate_control_mode == 0) {
                rateControlParamPtr = context_ptr->rateControlParamQueue[0];
                prevGopRateControlParamPtr = context_ptr->rateControlParamQueue[0];
                nextGopRateControlParamPtr = context_ptr->rateControlParamQueue[0];
            }
            else {
                uint32_t intervalIndexTemp = 0;
                EbBool intervalFound = EB_FALSE;
                while ((intervalIndexTemp < PARALLEL_GOP_MAX_NUMBER) && !intervalFound) {

                    if (picture_control_set_ptr->picture_number >= context_ptr->rateControlParamQueue[intervalIndexTemp]->first_poc &&
                        picture_control_set_ptr->picture_number <= context_ptr->rateControlParamQueue[intervalIndexTemp]->last_poc) {
                        intervalFound = EB_TRUE;
                    }
                    else {
                        intervalIndexTemp++;
                    }
                }
                CHECK_REPORT_ERROR(
                    intervalIndexTemp != PARALLEL_GOP_MAX_NUMBER,
                    sequence_control_set_ptr->encode_context_ptr->app_callback_ptr,
                    EB_ENC_RC_ERROR2);

                rateControlParamPtr = context_ptr->rateControlParamQueue[intervalIndexTemp];

                prevGopRateControlParamPtr = (intervalIndexTemp == 0) ?
                    context_ptr->rateControlParamQueue[PARALLEL_GOP_MAX_NUMBER - 1] :
                    context_ptr->rateControlParamQueue[intervalIndexTemp - 1];
                nextGopRateControlParamPtr = (intervalIndexTemp == PARALLEL_GOP_MAX_NUMBER - 1) ?
                    context_ptr->rateControlParamQueue[0] :
                    context_ptr->rateControlParamQueue[intervalIndexTemp + 1];
            }

            if (sequence_control_set_ptr->static_config.rate_control_mode == 0) {
                // if RC mode is 0,  fixed QP is used
                // QP scaling based on POC number for Flat IPPP structure
#if NEW_QPS
                picture_control_set_ptr->parent_pcs_ptr->base_qindex = quantizer_to_qindex[picture_control_set_ptr->picture_qp];
#endif
                if (sequence_control_set_ptr->static_config.enable_qp_scaling_flag && picture_control_set_ptr->parent_pcs_ptr->qp_on_the_fly == EB_FALSE) {
#if NEW_QPS
                    const int32_t qindex = quantizer_to_qindex[(uint8_t)sequence_control_set_ptr->qp];
                    const double q_val = av1_convert_qindex_to_q(qindex, (AomBitDepth)sequence_control_set_ptr->static_config.encoder_bit_depth);
                    if (picture_control_set_ptr->slice_type == I_SLICE) {

                        const int32_t delta_qindex = av1_compute_qdelta(
                            q_val,
                            q_val * 0.25,
                            (AomBitDepth)sequence_control_set_ptr->static_config.encoder_bit_depth);
                        picture_control_set_ptr->parent_pcs_ptr->base_qindex =
                            (uint8_t)CLIP3(
                            (int32_t)quantizer_to_qindex[sequence_control_set_ptr->static_config.min_qp_allowed],
                                (int32_t)quantizer_to_qindex[sequence_control_set_ptr->static_config.max_qp_allowed],
                                (int32_t)(qindex + delta_qindex));

                    }
                    else {
#if NEW_PRED_STRUCT                    
                        const  double delta_rate_new[2][6] =
                                { { 0.40, 0.7, 0.85, 1.0, 1.0, 1.0 },
                                { 0.35, 0.6, 0.8,  0.9, 1.0, 1.0 } };

#else
                       const double delta_rate_new[6] = { 0.40, 0.7, 0.85, 1.0, 1.0, 1.0 };

#endif
                        const int32_t delta_qindex = av1_compute_qdelta(
                            q_val,
#if NEW_PRED_STRUCT
                            q_val * delta_rate_new[picture_control_set_ptr->parent_pcs_ptr->hierarchical_levels == 4][picture_control_set_ptr->parent_pcs_ptr->temporal_layer_index],

#else
                            q_val * delta_rate_new[picture_control_set_ptr->parent_pcs_ptr->temporal_layer_index],
#endif     
                            (AomBitDepth)sequence_control_set_ptr->static_config.encoder_bit_depth);

                        picture_control_set_ptr->parent_pcs_ptr->base_qindex =
                            (uint8_t)CLIP3(
                            (int32_t)quantizer_to_qindex[sequence_control_set_ptr->static_config.min_qp_allowed],
                                (int32_t)quantizer_to_qindex[sequence_control_set_ptr->static_config.max_qp_allowed],
                                (int32_t)(qindex + delta_qindex));

                    }
#endif
                    picture_control_set_ptr->picture_qp = (uint8_t)CLIP3((int32_t)sequence_control_set_ptr->static_config.min_qp_allowed, (int32_t)sequence_control_set_ptr->static_config.max_qp_allowed, picture_control_set_ptr->parent_pcs_ptr->base_qindex >> 2);
                }

                else if (picture_control_set_ptr->parent_pcs_ptr->qp_on_the_fly == EB_TRUE) {

                    picture_control_set_ptr->picture_qp = (uint8_t)CLIP3((int32_t)sequence_control_set_ptr->static_config.min_qp_allowed, (int32_t)sequence_control_set_ptr->static_config.max_qp_allowed, picture_control_set_ptr->parent_pcs_ptr->picture_qp);
#if NEW_QPS
                    picture_control_set_ptr->parent_pcs_ptr->base_qindex = quantizer_to_qindex[picture_control_set_ptr->picture_qp];
#endif
                }
                picture_control_set_ptr->parent_pcs_ptr->picture_qp = picture_control_set_ptr->picture_qp;
            }
            else {
                picture_control_set_ptr->picture_qp = rate_control_get_quantizer(rc_model_ptr, picture_control_set_ptr->parent_pcs_ptr);

                if (picture_control_set_ptr->picture_number == rateControlParamPtr->first_poc && picture_control_set_ptr->picture_number != 0 && !prevGopRateControlParamPtr->scene_change_in_gop) {
                    int16_t deltaApQp = (int16_t)prevGopRateControlParamPtr->first_pic_actual_qp - (int16_t)prevGopRateControlParamPtr->first_pic_pred_qp;
                    int64_t extraApBitRatio = (prevGopRateControlParamPtr->first_pic_pred_bits != 0) ?
                        (((int64_t)prevGopRateControlParamPtr->first_pic_actual_bits - (int64_t)prevGopRateControlParamPtr->first_pic_pred_bits) * 100) / ((int64_t)prevGopRateControlParamPtr->first_pic_pred_bits) :
                        0;
                    extraApBitRatio += (int64_t)deltaApQp * 15;
                    if (extraApBitRatio > 200) {
                        picture_control_set_ptr->picture_qp = picture_control_set_ptr->picture_qp + 3;
                    }
                    else if (extraApBitRatio > 100) {
                        picture_control_set_ptr->picture_qp = picture_control_set_ptr->picture_qp + 2;
                    }
                    else if (extraApBitRatio > 50) {
                        picture_control_set_ptr->picture_qp++;
                    }
                }

                if (picture_control_set_ptr->picture_number == rateControlParamPtr->first_poc && picture_control_set_ptr->picture_number != 0) {
                    uint8_t qpIncAllowed = 3;
                    uint8_t qpDecAllowed = 4;
                    if (picture_control_set_ptr->parent_pcs_ptr->intra_selected_org_qp + 10 <= prevGopRateControlParamPtr->first_pic_actual_qp)
                    {
                        qpDecAllowed = (uint8_t)(prevGopRateControlParamPtr->first_pic_actual_qp - picture_control_set_ptr->parent_pcs_ptr->intra_selected_org_qp) >> 1;
                    }

                    if (picture_control_set_ptr->parent_pcs_ptr->intra_selected_org_qp >= prevGopRateControlParamPtr->first_pic_actual_qp + 10)
                    {
                        qpIncAllowed = (uint8_t)(picture_control_set_ptr->parent_pcs_ptr->intra_selected_org_qp - prevGopRateControlParamPtr->first_pic_actual_qp) * 2 / 3;
                        if (prevGopRateControlParamPtr->first_pic_actual_qp <= 15)
                            qpIncAllowed += 5;
                        else if (prevGopRateControlParamPtr->first_pic_actual_qp <= 20)
                            qpIncAllowed += 4;
                        else if (prevGopRateControlParamPtr->first_pic_actual_qp <= 25)
                            qpIncAllowed += 3;
                    }
                    else if (prevGopRateControlParamPtr->scene_change_in_gop) {
                        qpIncAllowed = 5;
                    }
                    if (picture_control_set_ptr->parent_pcs_ptr->end_of_sequence_region) {
                        qpIncAllowed += 2;
                        qpDecAllowed += 4;
                    }
                    picture_control_set_ptr->picture_qp = (uint8_t)CLIP3(
                        (uint32_t)MAX((int32_t)prevGopRateControlParamPtr->first_pic_actual_qp - (int32_t)qpDecAllowed, 0),
                        (uint32_t)prevGopRateControlParamPtr->first_pic_actual_qp + qpIncAllowed,
                        picture_control_set_ptr->picture_qp);
                }

                // Scene change
                if (picture_control_set_ptr->slice_type == I_SLICE && picture_control_set_ptr->picture_number != rateControlParamPtr->first_poc) {
                    if (nextGopRateControlParamPtr->first_pic_actual_qp_assigned) {

                        picture_control_set_ptr->picture_qp = (uint8_t)CLIP3(
                            (uint32_t)MAX((int32_t)nextGopRateControlParamPtr->first_pic_actual_qp - (int32_t)1, 0),
                            (uint32_t)nextGopRateControlParamPtr->first_pic_actual_qp + 8,
                            picture_control_set_ptr->picture_qp);
                    }
                    else {
                        if (rateControlParamPtr->first_pic_actual_qp < 20) {
                            picture_control_set_ptr->picture_qp = (uint8_t)CLIP3(
                                (uint32_t)MAX((int32_t)rateControlParamPtr->first_pic_actual_qp - (int32_t)4, 0),
                                (uint32_t)rateControlParamPtr->first_pic_actual_qp + 10,
                                picture_control_set_ptr->picture_qp);
                        }
                        else {
                            picture_control_set_ptr->picture_qp = (uint8_t)CLIP3(
                                (uint32_t)MAX((int32_t)rateControlParamPtr->first_pic_actual_qp - (int32_t)4, 0),
                                (uint32_t)rateControlParamPtr->first_pic_actual_qp + 8,
                                picture_control_set_ptr->picture_qp);

                        }

                    }
                }
                picture_control_set_ptr->picture_qp = (uint8_t)CLIP3(
                    sequence_control_set_ptr->static_config.min_qp_allowed,
                    sequence_control_set_ptr->static_config.max_qp_allowed,
                    picture_control_set_ptr->picture_qp);
#if NEW_QPS
                picture_control_set_ptr->parent_pcs_ptr->base_qindex = quantizer_to_qindex[picture_control_set_ptr->picture_qp];
#endif
            }

            picture_control_set_ptr->parent_pcs_ptr->picture_qp = picture_control_set_ptr->picture_qp;
            if (picture_control_set_ptr->parent_pcs_ptr->temporal_layer_index == 0 && sequence_control_set_ptr->static_config.look_ahead_distance != 0) {
                context_ptr->base_layer_frames_avg_qp = (3 * context_ptr->base_layer_frames_avg_qp + picture_control_set_ptr->picture_qp + 2) >> 2;
            }
            if (picture_control_set_ptr->slice_type == I_SLICE) {
                if (picture_control_set_ptr->picture_number == rateControlParamPtr->first_poc) {
                    rateControlParamPtr->first_pic_pred_qp = (uint16_t)picture_control_set_ptr->parent_pcs_ptr->best_pred_qp;
                    rateControlParamPtr->first_pic_actual_qp = (uint16_t)picture_control_set_ptr->picture_qp;
                    rateControlParamPtr->scene_change_in_gop = picture_control_set_ptr->parent_pcs_ptr->scene_change_in_gop;
                    rateControlParamPtr->first_pic_actual_qp_assigned = EB_TRUE;
                }
                {
                    if (picture_control_set_ptr->picture_number == rateControlParamPtr->first_poc) {
                        if (sequence_control_set_ptr->static_config.look_ahead_distance != 0) {
                            context_ptr->base_layer_intra_frames_avg_qp = (3 * context_ptr->base_layer_intra_frames_avg_qp + picture_control_set_ptr->picture_qp + 2) >> 2;
                        }
                    }

                    if (picture_control_set_ptr->picture_number == rateControlParamPtr->first_poc) {
                        rateControlParamPtr->intra_frames_qp = picture_control_set_ptr->picture_qp;
                        rateControlParamPtr->next_gop_intra_frame_qp = picture_control_set_ptr->picture_qp;

                    }
                }
            }
            picture_control_set_ptr->parent_pcs_ptr->average_qp = 0;
            for (lcuCodingOrder = 0; lcuCodingOrder < sequence_control_set_ptr->sb_tot_cnt; ++lcuCodingOrder) {

                sb_ptr = picture_control_set_ptr->sb_ptr_array[lcuCodingOrder];
#if ADD_DELTA_QP_SUPPORT

                sb_ptr->qp = quantizer_to_qindex[picture_control_set_ptr->picture_qp];
#else
                sb_ptr->qp = (uint8_t)picture_control_set_ptr->picture_qp;
#endif
                picture_control_set_ptr->parent_pcs_ptr->average_qp += sb_ptr->qp;
            }

            // Get Empty Rate Control Results Buffer
            eb_get_empty_object(
                context_ptr->rate_control_output_results_fifo_ptr,
                &rateControlResultsWrapperPtr);
            rateControlResultsPtr = (RateControlResults*)rateControlResultsWrapperPtr->object_ptr;
            rateControlResultsPtr->picture_control_set_wrapper_ptr = rateControlTasksPtr->picture_control_set_wrapper_ptr;

            // Post Full Rate Control Results
            eb_post_full_object(rateControlResultsWrapperPtr);

            // Release Rate Control Tasks
            eb_release_object(rateControlTasksWrapperPtr);

            break;

        case RC_PACKETIZATION_FEEDBACK_RESULT:

            parentPictureControlSetPtr = (PictureParentControlSet*)rateControlTasksPtr->picture_control_set_wrapper_ptr->object_ptr;
            sequence_control_set_ptr = (SequenceControlSet*)parentPictureControlSetPtr->sequence_control_set_wrapper_ptr->object_ptr;

            if (sequence_control_set_ptr->static_config.rate_control_mode) {
                rate_control_update_model(rc_model_ptr, parentPictureControlSetPtr);
            }

            // Frame level RC
            if (sequence_control_set_ptr->intra_period_length == -1 || sequence_control_set_ptr->static_config.rate_control_mode == 0) {
                rateControlParamPtr = context_ptr->rateControlParamQueue[0];
                prevGopRateControlParamPtr = context_ptr->rateControlParamQueue[0];
                if (parentPictureControlSetPtr->slice_type == I_SLICE) {

                    if (parentPictureControlSetPtr->total_num_bits > MAX_BITS_PER_FRAME) {
                        context_ptr->max_rate_adjust_delta_qp++;
                    }
                    else if (context_ptr->max_rate_adjust_delta_qp > 0 && parentPictureControlSetPtr->total_num_bits < MAX_BITS_PER_FRAME * 85 / 100) {
                        context_ptr->max_rate_adjust_delta_qp--;
                    }
                    context_ptr->max_rate_adjust_delta_qp = CLIP3(0, 63, context_ptr->max_rate_adjust_delta_qp);
                    context_ptr->max_rate_adjust_delta_qp = 0;
                }
            }
            else {
                uint32_t intervalIndexTemp = 0;
                EbBool intervalFound = EB_FALSE;
                while ((intervalIndexTemp < PARALLEL_GOP_MAX_NUMBER) && !intervalFound) {

                    if (parentPictureControlSetPtr->picture_number >= context_ptr->rateControlParamQueue[intervalIndexTemp]->first_poc &&
                        parentPictureControlSetPtr->picture_number <= context_ptr->rateControlParamQueue[intervalIndexTemp]->last_poc) {
                        intervalFound = EB_TRUE;
                    }
                    else {
                        intervalIndexTemp++;
                    }
                }
                CHECK_REPORT_ERROR(
                    intervalIndexTemp != PARALLEL_GOP_MAX_NUMBER,
                    sequence_control_set_ptr->encode_context_ptr->app_callback_ptr,
                    EB_ENC_RC_ERROR2);

                rateControlParamPtr = context_ptr->rateControlParamQueue[intervalIndexTemp];

                prevGopRateControlParamPtr = (intervalIndexTemp == 0) ?
                    context_ptr->rateControlParamQueue[PARALLEL_GOP_MAX_NUMBER - 1] :
                    context_ptr->rateControlParamQueue[intervalIndexTemp - 1];

            }
            if (sequence_control_set_ptr->static_config.rate_control_mode != 0) {

                context_ptr->previous_virtual_buffer_level = context_ptr->virtual_buffer_level;

                context_ptr->virtual_buffer_level =
                    (int64_t)context_ptr->previous_virtual_buffer_level +
                    (int64_t)parentPictureControlSetPtr->total_num_bits - (int64_t)context_ptr->high_level_rate_control_ptr->channel_bit_rate_per_frame;

            }

            // Queue variables
#if OVERSHOOT_STAT_PRINT
            if (sequence_control_set_ptr->intra_period_length != -1) {

                int32_t                       queueEntryIndex;
                uint32_t                       queueEntryIndexTemp;
                uint32_t                       queueEntryIndexTemp2;
                CodedFramesStatsEntry     *queueEntryPtr;
                EbBool                      moveSlideWondowFlag = EB_TRUE;
                EbBool                      end_of_sequence_flag = EB_TRUE;
                uint32_t                       frames_in_sw;

                // Determine offset from the Head Ptr
                queueEntryIndex = (int32_t)(parentPictureControlSetPtr->picture_number - context_ptr->coded_frames_stat_queue[context_ptr->coded_frames_stat_queue_head_index]->picture_number);
                queueEntryIndex += context_ptr->coded_frames_stat_queue_head_index;
                queueEntryIndex = (queueEntryIndex > CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? queueEntryIndex - CODED_FRAMES_STAT_QUEUE_MAX_DEPTH : queueEntryIndex;
                queueEntryPtr = context_ptr->coded_frames_stat_queue[queueEntryIndex];

                queueEntryPtr->frame_total_bit_actual = (uint64_t)parentPictureControlSetPtr->total_num_bits;
                queueEntryPtr->picture_number = parentPictureControlSetPtr->picture_number;
                queueEntryPtr->end_of_sequence_flag = parentPictureControlSetPtr->end_of_sequence_flag;
                context_ptr->rate_average_periodin_frames = (uint64_t)sequence_control_set_ptr->static_config.intra_period_length + 1;

                //printf("\n0_POC: %d\n",
                //    queueEntryPtr->picture_number);
                moveSlideWondowFlag = EB_TRUE;
                while (moveSlideWondowFlag) {
                    //  printf("\n1_POC: %d\n",
                    //      queueEntryPtr->picture_number);
                      // Check if the sliding window condition is valid
                    queueEntryIndexTemp = context_ptr->coded_frames_stat_queue_head_index;
                    if (context_ptr->coded_frames_stat_queue[queueEntryIndexTemp]->frame_total_bit_actual != -1) {
                        end_of_sequence_flag = context_ptr->coded_frames_stat_queue[queueEntryIndexTemp]->end_of_sequence_flag;
                    }
                    else {
                        end_of_sequence_flag = EB_FALSE;
                    }
                    while (moveSlideWondowFlag && !end_of_sequence_flag &&
                        queueEntryIndexTemp < context_ptr->coded_frames_stat_queue_head_index + context_ptr->rate_average_periodin_frames) {
                        // printf("\n2_POC: %d\n",
                        //     queueEntryPtr->picture_number);

                        queueEntryIndexTemp2 = (queueEntryIndexTemp > CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? queueEntryIndexTemp - CODED_FRAMES_STAT_QUEUE_MAX_DEPTH : queueEntryIndexTemp;

                        moveSlideWondowFlag = (EbBool)(moveSlideWondowFlag && (context_ptr->coded_frames_stat_queue[queueEntryIndexTemp2]->frame_total_bit_actual != -1));

                        if (context_ptr->coded_frames_stat_queue[queueEntryIndexTemp2]->frame_total_bit_actual != -1) {
                            // check if it is the last frame. If we have reached the last frame, we would output the buffered frames in the Queue.
                            end_of_sequence_flag = context_ptr->coded_frames_stat_queue[queueEntryIndexTemp]->end_of_sequence_flag;
                        }
                        else {
                            end_of_sequence_flag = EB_FALSE;
                        }
                        queueEntryIndexTemp =
                            (queueEntryIndexTemp == CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? 0 : queueEntryIndexTemp + 1;

                    }

                    if (moveSlideWondowFlag) {
                        //get a new entry spot
                        queueEntryPtr = (context_ptr->coded_frames_stat_queue[context_ptr->coded_frames_stat_queue_head_index]);
                        queueEntryIndexTemp = context_ptr->coded_frames_stat_queue_head_index;
                        // This is set to false, so the last frame would go inside the loop
                        end_of_sequence_flag = EB_FALSE;
                        frames_in_sw = 0;
                        context_ptr->total_bit_actual_per_sw = 0;

                        while (!end_of_sequence_flag &&
                            queueEntryIndexTemp < context_ptr->coded_frames_stat_queue_head_index + context_ptr->rate_average_periodin_frames) {
                            frames_in_sw++;

                            queueEntryIndexTemp2 = (queueEntryIndexTemp > CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? queueEntryIndexTemp - CODED_FRAMES_STAT_QUEUE_MAX_DEPTH : queueEntryIndexTemp;

                            context_ptr->total_bit_actual_per_sw += context_ptr->coded_frames_stat_queue[queueEntryIndexTemp2]->frame_total_bit_actual;
                            end_of_sequence_flag = context_ptr->coded_frames_stat_queue[queueEntryIndexTemp2]->end_of_sequence_flag;

                            queueEntryIndexTemp =
                                (queueEntryIndexTemp == CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? 0 : queueEntryIndexTemp + 1;

                        }
                        //

                        //if(frames_in_sw == context_ptr->rate_average_periodin_frames)
                        //    printf("POC:%d\t %.3f\n", queueEntryPtr->picture_number, (double)context_ptr->total_bit_actual_per_sw*(sequence_control_set_ptr->frame_rate>> RC_PRECISION)/(double)frames_in_sw/1000);
                        if (frames_in_sw == (uint32_t)sequence_control_set_ptr->intra_period_length + 1) {
                            context_ptr->max_bit_actual_per_sw = MAX(context_ptr->max_bit_actual_per_sw, context_ptr->total_bit_actual_per_sw*(sequence_control_set_ptr->frame_rate >> RC_PRECISION) / frames_in_sw / 1000);
                            if (queueEntryPtr->picture_number % ((sequence_control_set_ptr->intra_period_length + 1)) == 0) {
                                context_ptr->max_bit_actual_per_gop = MAX(context_ptr->max_bit_actual_per_gop, context_ptr->total_bit_actual_per_sw*(sequence_control_set_ptr->frame_rate >> RC_PRECISION) / frames_in_sw / 1000);
                                if (context_ptr->total_bit_actual_per_sw > sequence_control_set_ptr->static_config.maxBufferSize) {
                                    printf("\nPOC:%d\tOvershoot:%.0f%% \n",
                                        (int32_t)queueEntryPtr->picture_number,
                                        (double)((int64_t)context_ptr->total_bit_actual_per_sw * 100 / (int64_t)sequence_control_set_ptr->static_config.maxBufferSize - 100));
                                }
                            }
                        }
                        if (frames_in_sw == context_ptr->rate_average_periodin_frames - 1) {
                            printf("\n%d MAX\n", (int32_t)context_ptr->max_bit_actual_per_sw);
                            printf("\n%d GopMa\n", (int32_t)context_ptr->max_bit_actual_per_gop);
                        }

                        // Reset the Queue Entry
                        queueEntryPtr->picture_number += CODED_FRAMES_STAT_QUEUE_MAX_DEPTH;
                        queueEntryPtr->frame_total_bit_actual = -1;

                        // Increment the Reorder Queue head Ptr
                        context_ptr->coded_frames_stat_queue_head_index =
                            (context_ptr->coded_frames_stat_queue_head_index == CODED_FRAMES_STAT_QUEUE_MAX_DEPTH - 1) ? 0 : context_ptr->coded_frames_stat_queue_head_index + 1;

                        queueEntryPtr = (context_ptr->coded_frames_stat_queue[context_ptr->coded_frames_stat_queue_head_index]);

                    }
                }
            }
#endif
            totalNumberOfFbFrames++;

            // Release the SequenceControlSet
            eb_release_object(parentPictureControlSetPtr->sequence_control_set_wrapper_ptr);

            // Release the input buffer 
            eb_release_object(parentPictureControlSetPtr->input_picture_wrapper_ptr);

            // Release the ParentPictureControlSet
            eb_release_object(rateControlTasksPtr->picture_control_set_wrapper_ptr);

            // Release Rate Control Tasks
            eb_release_object(rateControlTasksWrapperPtr);

            break;

        case RC_ENTROPY_CODING_ROW_FEEDBACK_RESULT:

            // Extract bits-per-lcu-row

            // Release Rate Control Tasks
            eb_release_object(rateControlTasksWrapperPtr);

            break;

        default:
            picture_control_set_ptr = (PictureControlSet*)rateControlTasksPtr->picture_control_set_wrapper_ptr->object_ptr;
            sequence_control_set_ptr = (SequenceControlSet*)picture_control_set_ptr->sequence_control_set_wrapper_ptr->object_ptr;
            //encode_context_ptr            = sequence_control_set_ptr->encode_context_ptr;
            //CHECK_REPORT_ERROR_NC(
            //             encode_context_ptr->app_callback_ptr,
            //             EB_ENC_RC_ERROR1);

            break;
        }
    }
    return EB_NULL;
}
