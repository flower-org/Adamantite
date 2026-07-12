#include "dma_mem_to_mem.h"

/* Static callback router that the HAL calls */
static void HAL_DMA_CopyComplete(DMA_HandleTypeDef *hdma)
{
    // The HAL unfortunately doesn't pass user context natively easily,
    // but we can retrieve our wrapper struct if we store it in the hdma->Parent
    // or by keeping an array. For safety and simplicity, we assume the user 
    // will register the specific instance.
    
    // We expect the user to hook this up.
    // For now, if we reach here, the transfer is complete.
}

void DmaMemToMem_Init(DmaMemToMem_t *dma_ctx, DMA_HandleTypeDef * const hdma, ElasticQueue_t * const src_q)
{
    if (!dma_ctx || !hdma || !src_q) { return; }
    
    // We cast away the const just for initialization. 
    // This enforces the "constant after init" contract for the rest of the application.
    ElasticQueue_t **sq_ptr = (ElasticQueue_t **)&dma_ctx->source_queue;
    *sq_ptr = src_q;

    DMA_HandleTypeDef **hdma_ptr = (DMA_HandleTypeDef **)&dma_ctx->hdma;
    *hdma_ptr = hdma;

    dma_ctx->state = DMA_STATE_IDLE;
    
    dma_ctx->num_dests = 0;
    dma_ctx->current_dest_idx = 0;
}

int DmaMemToMem_StartBroadcast(DmaMemToMem_t *dma_ctx, 
                               ElasticQueue_t **dest_qs, 
                               uint8_t num_dests)
{
    if (!dma_ctx || !dma_ctx->hdma || !dest_qs
            || num_dests == 0 || num_dests > DMA_MAX_BROADCAST_DESTS) {
        if (dma_ctx && dma_ctx->source_queue) {
            ElasticQueue_Abort(dma_ctx->source_queue);
        }
        if (dma_ctx) dma_ctx->state = DMA_STATE_ERROR;
        return DMA_BROADCAST_ERR_INVAL;
    }
    
    ElasticQueueRef_t *src_ref = ElasticQueue_PeekLocked(dma_ctx->source_queue);
    if (!src_ref || src_ref->len == 0) {
        dma_ctx->state = DMA_STATE_ERROR;
        return DMA_BROADCAST_ERR_INVAL;
    }
    
    // Check if channel is already running via the HAL state
    if (dma_ctx->hdma->State != HAL_DMA_STATE_READY) {
        ElasticQueue_Abort(dma_ctx->source_queue);
        dma_ctx->state = DMA_STATE_ERROR;
        return DMA_BROADCAST_ERR_BUSY;
    }
    
    dma_ctx->num_dests = num_dests;
    dma_ctx->current_dest_idx = 0;
    dma_ctx->current_allocated_ref = NULL;

    for (uint8_t i = 0; i < num_dests; i++) {
        dma_ctx->dest_queues[i] = dest_qs[i];
    }
    
    // Advance through destinations until we successfully allocate and start one
    while (dma_ctx->current_dest_idx < dma_ctx->num_dests) {
        ElasticQueueRef_t *ref = ElasticQueue_Allocate(dma_ctx->dest_queues[dma_ctx->current_dest_idx], src_ref->len);
        if (ref) {
            dma_ctx->current_allocated_ref = ref;
            if (HAL_DMA_Start_IT(dma_ctx->hdma, (uint32_t)src_ref->data, (uint32_t)ref->data, src_ref->len) == HAL_OK) {
                dma_ctx->state = DMA_STATE_RUNNING;
                return DMA_BROADCAST_OK; // Successfully started
            }
            // If HAL_DMA_Start fails, we allocated space but failed to copy. It will be abandoned.
            ElasticQueue_Abandon(dma_ctx->dest_queues[dma_ctx->current_dest_idx], ref);
            dma_ctx->current_allocated_ref = NULL;
        }
        
        // If allocation failed, or DMA failed to start, move to next queue
        ElasticQueue_Done(dma_ctx->source_queue);
        dma_ctx->current_dest_idx++;
    }
    
    // If we get here, no destinations could be started (queues full or DMA err)
    // The source queue was already fully consumed/aborted by the loop above via _Done.
    dma_ctx->state = DMA_STATE_ERROR;
    return DMA_BROADCAST_ERR_NO_QUEUES;
}

void DmaMemToMem_TransferComplete(DmaMemToMem_t *dma_ctx)
{
    if (dma_ctx && dma_ctx->state == DMA_STATE_RUNNING) {
        dma_ctx->state = DMA_STATE_TRANSFER_DONE;
    }
}

void DmaMemToMem_Process(DmaMemToMem_t *dma_ctx)
{
    if (!dma_ctx || dma_ctx->state != DMA_STATE_TRANSFER_DONE) {
        return;
    }
    
    ElasticQueueRef_t *src_ref = ElasticQueue_PeekLocked(dma_ctx->source_queue);
    if (!src_ref) {
        dma_ctx->state = DMA_STATE_ERROR; // Fatal: Ghost interrupt or unlocked mid-transfer
        return;
    }
    
    // We successfully finished the current copy. Commit it to the queue.
    if (dma_ctx->current_allocated_ref) {
        ElasticQueue_Commit(dma_ctx->dest_queues[dma_ctx->current_dest_idx], dma_ctx->current_allocated_ref);
        dma_ctx->current_allocated_ref = NULL;
    }
    
    // Attempt to start the next valid destination
    while (++dma_ctx->current_dest_idx < dma_ctx->num_dests) {
        ElasticQueueRef_t *ref = ElasticQueue_Allocate(dma_ctx->dest_queues[dma_ctx->current_dest_idx], src_ref->len);
        if (ref) {
            dma_ctx->current_allocated_ref = ref;
            if (HAL_DMA_Start_IT(dma_ctx->hdma, 
                                 (uint32_t)src_ref->data, 
                                 (uint32_t)ref->data, 
                                 src_ref->len) == HAL_OK) {
                dma_ctx->state = DMA_STATE_RUNNING;
                return; // Wait for the next transfer complete interrupt
            }
            ElasticQueue_Abandon(dma_ctx->dest_queues[dma_ctx->current_dest_idx], ref);
            dma_ctx->current_allocated_ref = NULL;
        }
        // If allocation failed, packet is dropped for this destination, loop continues to the next one
        ElasticQueue_Done(dma_ctx->source_queue);
    }
    
    // Done: this task (the successful transfer)
    ElasticQueue_Done(dma_ctx->source_queue);
    
    dma_ctx->state = DMA_STATE_IDLE;
}
