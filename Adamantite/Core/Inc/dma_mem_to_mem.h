#ifndef DMA_MEM_TO_MEM_H
#define DMA_MEM_TO_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "elastic_queue.h"

#define DMA_MAX_BROADCAST_DESTS 5

/* Callback type for when a DMA transfer completes */
typedef void (*DmaCopyCompleteCb_t)(void *user_data);

/**
 * @brief Structure to track the state of a single DMA stream wrapper
 */
typedef struct {
    DMA_HandleTypeDef *hdma;
    bool is_busy;
    DmaCopyCompleteCb_t complete_cb;
    void *user_data;
    
    // Constant Pointer to the source queue so we can auto-unlock/done it
    ElasticQueue_t * const source_queue;
    
    // Lazy broadcast tracking
    const uint8_t *current_src;
    size_t current_len;
    
    ElasticQueue_t *dest_queues[DMA_MAX_BROADCAST_DESTS];
    ElasticQueueRef_t *current_allocated_ref;
    uint8_t num_dests;
    uint8_t current_dest_idx;
} DmaMemToMem_t;

/* Error codes for DMA operations */
#define DMA_BROADCAST_OK               0
#define DMA_BROADCAST_ERR_INVAL       -1
#define DMA_BROADCAST_ERR_BUSY        -2
#define DMA_BROADCAST_ERR_NO_QUEUES   -3

/**
 * @brief Initialize a DMA wrapper structure with its hardware handle
 */
void DmaMemToMem_Init(DmaMemToMem_t *dma_ctx, DMA_HandleTypeDef *hdma, ElasticQueue_t *src_q);

/**
 * @brief Start a background memory-to-memory broadcast with lazy allocation.
 * 
 * @param dma_ctx      The DMA context to use
 * @param src          Source memory address
 * @param dest_qs      Array of destination queues to allocate from as needed
 * @param num_dests    Number of destinations (up to DMA_MAX_BROADCAST_DESTS)
 * @param length       Number of bytes to copy per destination
 * @param complete_cb  Function to call when ALL copies are finished
 * @param user_data    Arbitrary pointer to pass to the callback
 * @param src_q        (Optional) Source queue to call ElasticQueue_Done() on when complete
 * 
 * @return DMA_BROADCAST_OK on success, or a negative DMA_BROADCAST_ERR_* code on failure
 */
int DmaMemToMem_StartBroadcast(DmaMemToMem_t *dma_ctx, 
                               const uint8_t *src, 
                               ElasticQueue_t **dest_qs,
                               uint8_t num_dests,
                               size_t length,
                               DmaCopyCompleteCb_t complete_cb,
                               void *user_data);

/**
 * @brief Must be called from the HAL_DMA_RegisterCallback or inside the TC interrupt.
 *        This clears the busy flag, cascades broadcasts, and fires the user callback.
 */
void DmaMemToMem_TransferComplete(DmaMemToMem_t *dma_ctx);

#endif /* DMA_MEM_TO_MEM_H */
