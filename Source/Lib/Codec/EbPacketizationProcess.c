/*
* Copyright(c) 2019 Intel Corporation
* SPDX - License - Identifier: BSD - 2 - Clause - Patent
*/

#include <stdlib.h>

#include "EbDefinitions.h"
#include "EbPacketizationProcess.h"
#include "EbEntropyCodingResults.h"
#include "EbSystemResourceManager.h"
#include "EbSequenceControlSet.h"
#include "EbPictureControlSet.h"
#include "EbEntropyCoding.h"
#include "EbRateControlTasks.h"
#include "EbTime.h"

static EbBool IsPassthroughData(EbLinkedListNode* dataNode)
{
    return dataNode->passthrough;
}

// Extracts passthrough data from a linked list. The extracted data nodes are removed from the original linked list and
// returned as a linked list. Does not gaurantee the original order of the nodes.
static EbLinkedListNode* ExtractPassthroughData(EbLinkedListNode** llPtrPtr)
{
    EbLinkedListNode* llRestPtr = NULL;
    EbLinkedListNode* llPassPtr = split_eb_linked_list(*llPtrPtr, &llRestPtr, IsPassthroughData);
    *llPtrPtr = llRestPtr;
    return llPassPtr;
}


EbErrorType packetization_context_ctor(
    PacketizationContext **context_dbl_ptr,
    EbFifo                *entropy_coding_input_fifo_ptr,
    EbFifo                *rate_control_tasks_output_fifo_ptr)
{
    PacketizationContext *context_ptr;
    EB_MALLOC(PacketizationContext*, context_ptr, sizeof(PacketizationContext), EB_N_PTR);
    *context_dbl_ptr = context_ptr;

    context_ptr->entropy_coding_input_fifo_ptr = entropy_coding_input_fifo_ptr;
    context_ptr->rate_control_tasks_output_fifo_ptr = rate_control_tasks_output_fifo_ptr;


    return EB_ErrorNone;
}
#define TD_SIZE                     2
#define OBU_FRAME_HEADER_SIZE       3
#define TILES_GROUP_SIZE            1

// Write TD after offsetting the stream buffer 
static void write_td (
    EbBufferHeaderType  *out_str_ptr,
    EbBool               show_ex,
    EbBool               has_tiles){

    uint8_t  td_buff[TD_SIZE] = { 0,0 };
    uint8_t  obu_frame_header_size = has_tiles ? OBU_FRAME_HEADER_SIZE + TILES_GROUP_SIZE : OBU_FRAME_HEADER_SIZE;
    if (out_str_ptr &&
        (out_str_ptr->n_alloc_len > (out_str_ptr->n_filled_len + 2))) {

        uint8_t *src_address = (show_ex == EB_FALSE) ?  out_str_ptr->p_buffer :
                out_str_ptr->p_buffer + out_str_ptr->n_filled_len - (obu_frame_header_size);

        uint8_t *dst_address = src_address + TD_SIZE;

        uint32_t move_size   = (show_ex == EB_FALSE) ? out_str_ptr->n_filled_len :
                               (obu_frame_header_size);

        memmove(dst_address,
                src_address,
                move_size);

        encode_td_av1((uint8_t*)(&td_buff));

        EB_MEMCPY(src_address,
                  &td_buff, 
                  TD_SIZE);
    }
}

void* PacketizationKernel(void *input_ptr)
{
    // Context
    PacketizationContext         *context_ptr = (PacketizationContext*)input_ptr;

    PictureControlSet            *picture_control_set_ptr;

    // Config
    SequenceControlSet           *sequence_control_set_ptr;

    // Encoding Context
    EncodeContext                *encode_context_ptr;

    // Input
    EbObjectWrapper              *entropyCodingResultsWrapperPtr;
    EntropyCodingResults         *entropyCodingResultsPtr;

    // Output
    EbObjectWrapper              *output_stream_wrapper_ptr;
    EbBufferHeaderType             *output_stream_ptr;
    EbObjectWrapper              *rateControlTasksWrapperPtr;
    RateControlTasks             *rateControlTasksPtr;
    
    // Queue variables
    int32_t                         queueEntryIndex;
    PacketizationReorderEntry    *queueEntryPtr;
    EbLinkedListNode               *appDataLLHeadTempPtr;

    context_ptr->totShownFrames = 0;
    context_ptr->dispOrderContinuityCount = 0;

    for (;;) {

        // Get EntropyCoding Results
        eb_get_full_object(
            context_ptr->entropy_coding_input_fifo_ptr,
            &entropyCodingResultsWrapperPtr);
        entropyCodingResultsPtr = (EntropyCodingResults*)entropyCodingResultsWrapperPtr->object_ptr;
        picture_control_set_ptr = (PictureControlSet*)entropyCodingResultsPtr->picture_control_set_wrapper_ptr->object_ptr;
        sequence_control_set_ptr = (SequenceControlSet*)picture_control_set_ptr->sequence_control_set_wrapper_ptr->object_ptr;
        encode_context_ptr = (EncodeContext*)sequence_control_set_ptr->encode_context_ptr;

        //****************************************************
        // Input Entropy Results into Reordering Queue
        //****************************************************

        //get a new entry spot
        queueEntryIndex = picture_control_set_ptr->parent_pcs_ptr->decode_order % PACKETIZATION_REORDER_QUEUE_MAX_DEPTH;
        queueEntryPtr = encode_context_ptr->packetization_reorder_queue[queueEntryIndex];
        queueEntryPtr->start_time_seconds = picture_control_set_ptr->parent_pcs_ptr->start_time_seconds;
        queueEntryPtr->start_time_u_seconds = picture_control_set_ptr->parent_pcs_ptr->start_time_u_seconds;

        //TODO: The output buffer should be big enough to avoid a deadlock here. Add an assert that make the warning
        // Get  Output Bitstream buffer
        output_stream_wrapper_ptr = picture_control_set_ptr->parent_pcs_ptr->output_stream_wrapper_ptr;
        output_stream_ptr = (EbBufferHeaderType*)output_stream_wrapper_ptr->object_ptr;
        output_stream_ptr->flags = 0;
        output_stream_ptr->flags |= (encode_context_ptr->terminating_sequence_flag_received == EB_TRUE && picture_control_set_ptr->parent_pcs_ptr->decode_order == encode_context_ptr->terminating_picture_number) ? EB_BUFFERFLAG_EOS : 0;
        output_stream_ptr->n_filled_len = 0;
        output_stream_ptr->pts = picture_control_set_ptr->parent_pcs_ptr->input_ptr->pts;
#if NEW_PRED_STRUCT
        output_stream_ptr->dts = picture_control_set_ptr->parent_pcs_ptr->decode_order - (uint64_t)(1 << picture_control_set_ptr->parent_pcs_ptr->hierarchical_levels) + 1;
#else
        output_stream_ptr->dts = picture_control_set_ptr->parent_pcs_ptr->decode_order - (uint64_t)(1 << sequence_control_set_ptr->static_config.hierarchical_levels) + 1;
#endif     
        output_stream_ptr->pic_type = picture_control_set_ptr->parent_pcs_ptr->is_used_as_reference_flag ?
            picture_control_set_ptr->parent_pcs_ptr->idr_flag ? EB_IDR_PICTURE :
            picture_control_set_ptr->slice_type : EB_NON_REF_PICTURE;
        output_stream_ptr->p_app_private = picture_control_set_ptr->parent_pcs_ptr->input_ptr->p_app_private;

        // Get Empty Rate Control Input Tasks
        eb_get_empty_object(
            context_ptr->rate_control_tasks_output_fifo_ptr,
            &rateControlTasksWrapperPtr);
        rateControlTasksPtr = (RateControlTasks*)rateControlTasksWrapperPtr->object_ptr;
        rateControlTasksPtr->picture_control_set_wrapper_ptr = picture_control_set_ptr->picture_parent_control_set_wrapper_ptr;
        rateControlTasksPtr->task_type = RC_PACKETIZATION_FEEDBACK_RESULT;

        // slice_type = picture_control_set_ptr->slice_type;
         // Reset the bitstream before writing to it
        reset_bitstream(
            picture_control_set_ptr->bitstream_ptr->outputBitstreamPtr);

        // Code the SPS
        if (picture_control_set_ptr->parent_pcs_ptr->av1_frame_type == KEY_FRAME) {
            encode_sps_av1(
                picture_control_set_ptr->bitstream_ptr,
                sequence_control_set_ptr);
        }

        write_frame_header_av1(
            picture_control_set_ptr->bitstream_ptr,
            sequence_control_set_ptr,
            picture_control_set_ptr,
            0);

        // Copy Slice Header to the Output Bitstream
        copy_rbsp_bitstream_to_payload(
            picture_control_set_ptr->bitstream_ptr,
            output_stream_ptr->p_buffer,
            (uint32_t*) &(output_stream_ptr->n_filled_len),
            (uint32_t*) &(output_stream_ptr->n_alloc_len),
            encode_context_ptr);
        if (picture_control_set_ptr->parent_pcs_ptr->has_show_existing) {
            // Reset the bitstream before writing to it
            reset_bitstream(
                picture_control_set_ptr->bitstream_ptr->outputBitstreamPtr);
            write_frame_header_av1(
                picture_control_set_ptr->bitstream_ptr,
                sequence_control_set_ptr,
                picture_control_set_ptr,
                1);

            // Copy Slice Header to the Output Bitstream
            copy_rbsp_bitstream_to_payload(
                picture_control_set_ptr->bitstream_ptr,
                output_stream_ptr->p_buffer,
                (uint32_t*)&(output_stream_ptr->n_filled_len),
                (uint32_t*)&(output_stream_ptr->n_alloc_len),
                encode_context_ptr);

            output_stream_ptr->flags |= EB_BUFFERFLAG_SHOW_EXT;

#if TILES
            if (picture_control_set_ptr->parent_pcs_ptr->av1_cm->tile_cols * picture_control_set_ptr->parent_pcs_ptr->av1_cm->tile_rows > 1)
                output_stream_ptr->flags |= EB_BUFFERFLAG_TG;
#endif
        }

        // Send the number of bytes per frame to RC
        picture_control_set_ptr->parent_pcs_ptr->total_num_bits = output_stream_ptr->n_filled_len << 3;
        queueEntryPtr->av1_frame_type = picture_control_set_ptr->parent_pcs_ptr->av1_frame_type;
        queueEntryPtr->poc = picture_control_set_ptr->picture_number;
        memcpy(&queueEntryPtr->av1_ref_signal, &picture_control_set_ptr->parent_pcs_ptr->av1_ref_signal, sizeof(Av1RpsNode));

        queueEntryPtr->slice_type = picture_control_set_ptr->slice_type;
        queueEntryPtr->ref_poc_list0 = picture_control_set_ptr->parent_pcs_ptr->ref_pic_poc_array[REF_LIST_0];
        queueEntryPtr->ref_poc_list1 = picture_control_set_ptr->parent_pcs_ptr->ref_pic_poc_array[REF_LIST_1];

        queueEntryPtr->show_frame = picture_control_set_ptr->parent_pcs_ptr->show_frame;
        queueEntryPtr->has_show_existing = picture_control_set_ptr->parent_pcs_ptr->has_show_existing;
        queueEntryPtr->show_existing_loc = picture_control_set_ptr->parent_pcs_ptr->show_existing_loc;

        //Store the output buffer in the Queue
        queueEntryPtr->output_stream_wrapper_ptr = output_stream_wrapper_ptr;

        // Note: last chance here to add more output meta data for an encoded picture -->

        // collect output meta data
        queueEntryPtr->out_meta_data = concat_eb_linked_list(ExtractPassthroughData(&(picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr)),
            picture_control_set_ptr->parent_pcs_ptr->app_out_data_ll_head_ptr);
        picture_control_set_ptr->parent_pcs_ptr->app_out_data_ll_head_ptr = (EbLinkedListNode *)EB_NULL;

        // Calling callback functions to release the memory allocated for data linked list in the application
        while (picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr != EB_NULL) {
            appDataLLHeadTempPtr = picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr->next;
            if (picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr->release_cb_fnc_ptr != EB_NULL) {
                picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr->release_cb_fnc_ptr(picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr);
            }

            picture_control_set_ptr->parent_pcs_ptr->data_ll_head_ptr = appDataLLHeadTempPtr;
        }

        if (sequence_control_set_ptr->static_config.speed_control_flag) {
            // update speed control variables
            eb_block_on_mutex(encode_context_ptr->sc_buffer_mutex);
            encode_context_ptr->sc_frame_out++;
            eb_release_mutex(encode_context_ptr->sc_buffer_mutex);
        }

        // Post Rate Control Taks
        eb_post_full_object(rateControlTasksWrapperPtr);

        //Release the Parent PCS then the Child PCS
        eb_release_object(entropyCodingResultsPtr->picture_control_set_wrapper_ptr);//Child

        // Release the Entropy Coding Result
        eb_release_object(entropyCodingResultsWrapperPtr);


        //****************************************************
        // Process the head of the queue
        //****************************************************
        // Look at head of queue and see if any picture is ready to go
        queueEntryPtr = encode_context_ptr->packetization_reorder_queue[encode_context_ptr->packetization_reorder_queue_head_index];

        while (queueEntryPtr->output_stream_wrapper_ptr != EB_NULL) {
#if TILES
            EbBool has_tiles = (EbBool)(sequence_control_set_ptr->static_config.tile_columns || sequence_control_set_ptr->static_config.tile_rows);
#else
            EbBool has_tiles = EB_FALSE;
#endif
            output_stream_wrapper_ptr = queueEntryPtr->output_stream_wrapper_ptr;
            output_stream_ptr = (EbBufferHeaderType*)output_stream_wrapper_ptr->object_ptr;

            if (queueEntryPtr->has_show_existing) {
                write_td(output_stream_ptr, EB_TRUE, has_tiles);
                output_stream_ptr->n_filled_len += TD_SIZE;
            }

            if (encode_context_ptr->td_needed == EB_TRUE){
                output_stream_ptr->flags |= (uint32_t)EB_BUFFERFLAG_HAS_TD;
                write_td(output_stream_ptr, EB_FALSE, has_tiles);
                encode_context_ptr->td_needed = EB_FALSE;
                output_stream_ptr->n_filled_len += TD_SIZE;
            }

            if (queueEntryPtr->has_show_existing || queueEntryPtr->show_frame)
                encode_context_ptr->td_needed = EB_TRUE;

#if DETAILED_FRAME_OUTPUT
            {
                int32_t i;
                uint8_t  showTab[] = { 'H', 'V' };

                //Countinuity count check of visible frames
                if (queueEntryPtr->show_frame) {
                    if (context_ptr->dispOrderContinuityCount == queueEntryPtr->poc)
                        context_ptr->dispOrderContinuityCount++;
                    else
                    {
                        SVT_LOG("SVT [ERROR]: dispOrderContinuityCount Error1 POC:%i\n", (int32_t)queueEntryPtr->poc);
                        exit(0);
                    }
                }

                if (queueEntryPtr->has_show_existing) {
                    if (context_ptr->dispOrderContinuityCount == context_ptr->dpbDispOrder[queueEntryPtr->show_existing_loc])
                        context_ptr->dispOrderContinuityCount++;
                    else
                    {
                        SVT_LOG("SVT [ERROR]: dispOrderContinuityCount Error2 POC:%i\n", (int32_t)queueEntryPtr->poc);
                        exit(0);
                    }
                }

                //update total number of shown frames
                if (queueEntryPtr->show_frame)
                    context_ptr->totShownFrames++;
                if (queueEntryPtr->has_show_existing)
                    context_ptr->totShownFrames++;

                //implement the GOP here - Serial dec order
                if (queueEntryPtr->av1_frame_type == KEY_FRAME)
                {
                    //reset the DPB on a Key frame
                    for (i = 0; i < 8; i++)
                    {
                        context_ptr->dpbDecOrder[i] = queueEntryPtr->picture_number;
                        context_ptr->dpbDispOrder[i] = queueEntryPtr->poc;
                    }
                    SVT_LOG("%i  %i  %c ****KEY***** %i frames\n", (int32_t)queueEntryPtr->picture_number, (int32_t)queueEntryPtr->poc, showTab[queueEntryPtr->show_frame], (int32_t)context_ptr->totShownFrames);

                }
                else
                {
                    int32_t LASTrefIdx = queueEntryPtr->av1_ref_signal.refDpbIndex[0];
                    int32_t BWDrefIdx = queueEntryPtr->av1_ref_signal.refDpbIndex[4];

                    if (queueEntryPtr->av1_frame_type == INTER_FRAME)
                    {
                        if (queueEntryPtr->has_show_existing)
                            SVT_LOG("%i (%i  %i)    %i  (%i  %i)   %c  showEx: %i   %i frames\n",
                            (int32_t)queueEntryPtr->picture_number, (int32_t)context_ptr->dpbDecOrder[LASTrefIdx], (int32_t)context_ptr->dpbDecOrder[BWDrefIdx],
                                (int32_t)queueEntryPtr->poc, (int32_t)context_ptr->dpbDispOrder[LASTrefIdx], (int32_t)context_ptr->dpbDispOrder[BWDrefIdx],
                                showTab[queueEntryPtr->show_frame], (int32_t)context_ptr->dpbDispOrder[queueEntryPtr->show_existing_loc], (int32_t)context_ptr->totShownFrames);
                        else
                            SVT_LOG("%i (%i  %i)    %i  (%i  %i)   %c  %i frames\n",
                            (int32_t)queueEntryPtr->picture_number, (int32_t)context_ptr->dpbDecOrder[LASTrefIdx], (int32_t)context_ptr->dpbDecOrder[BWDrefIdx],
                                (int32_t)queueEntryPtr->poc, (int32_t)context_ptr->dpbDispOrder[LASTrefIdx], (int32_t)context_ptr->dpbDispOrder[BWDrefIdx],
                                showTab[queueEntryPtr->show_frame], (int32_t)context_ptr->totShownFrames);

                        if (queueEntryPtr->ref_poc_list0 != context_ptr->dpbDispOrder[LASTrefIdx])
                        {
                            SVT_LOG("L0 MISMATCH POC:%i\n", (int32_t)queueEntryPtr->poc);
                            exit(0);
                        }
                        if (sequence_control_set_ptr->static_config.hierarchical_levels == 3 && queueEntryPtr->slice_type == B_SLICE && queueEntryPtr->ref_poc_list1 != context_ptr->dpbDispOrder[BWDrefIdx])
                        {
                            SVT_LOG("L1 MISMATCH POC:%i\n", (int32_t)queueEntryPtr->poc);
                            exit(0);
                        }
                    }
                    else
                    {
                        if (queueEntryPtr->has_show_existing)
                            SVT_LOG("%i  %i  %c   showEx: %i ----INTRA---- %i frames \n", (int32_t)queueEntryPtr->picture_number, (int32_t)queueEntryPtr->poc,
                                showTab[queueEntryPtr->show_frame], (int32_t)context_ptr->dpbDispOrder[queueEntryPtr->show_existing_loc], (int32_t)context_ptr->totShownFrames);
                        else
                            printf("%i  %i  %c   ----INTRA---- %i frames\n", (int32_t)queueEntryPtr->picture_number, (int32_t)queueEntryPtr->poc,
                            (int32_t)showTab[queueEntryPtr->show_frame], (int32_t)context_ptr->totShownFrames);
                    }


                    //Update the DPB
                    for (i = 0; i < 8; i++)
                    {
                        if ((queueEntryPtr->av1_ref_signal.refreshFrameMask >> i) & 1)
                        {
                            context_ptr->dpbDecOrder[i] = queueEntryPtr->picture_number;
                            context_ptr->dpbDispOrder[i] = queueEntryPtr->poc;
                        }
                    }
                }
            }
#endif

            // Calculate frame latency in milli_seconds
            double latency = 0.0;
            uint64_t finishTimeSeconds = 0;
            uint64_t finishTimeuSeconds = 0;
            eb_finish_time(&finishTimeSeconds, &finishTimeuSeconds);

            eb_compute_overall_elapsed_time_ms(
                queueEntryPtr->start_time_seconds,
                queueEntryPtr->start_time_u_seconds,
                finishTimeSeconds,
                finishTimeuSeconds,
                &latency);

            output_stream_ptr->n_tick_count = (uint32_t)latency;
            output_stream_ptr->p_app_private = queueEntryPtr->out_meta_data;
            eb_post_full_object(output_stream_wrapper_ptr);
            queueEntryPtr->out_meta_data = (EbLinkedListNode *)EB_NULL;

            // Reset the Reorder Queue Entry
            queueEntryPtr->picture_number += PACKETIZATION_REORDER_QUEUE_MAX_DEPTH;
            queueEntryPtr->output_stream_wrapper_ptr = (EbObjectWrapper *)EB_NULL;

            if (encode_context_ptr->statistics_port_active) {
                queueEntryPtr->output_statistics_wrapper_ptr = (EbObjectWrapper *)EB_NULL;
            }
            // Increment the Reorder Queue head Ptr
            encode_context_ptr->packetization_reorder_queue_head_index =
                (encode_context_ptr->packetization_reorder_queue_head_index == PACKETIZATION_REORDER_QUEUE_MAX_DEPTH - 1) ? 0 : encode_context_ptr->packetization_reorder_queue_head_index + 1;

            queueEntryPtr = encode_context_ptr->packetization_reorder_queue[encode_context_ptr->packetization_reorder_queue_head_index];

        }

    }
    return EB_NULL;
}
