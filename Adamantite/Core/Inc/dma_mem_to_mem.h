#ifndef DMA_MEM_TO_MEM_H
#define DMA_MEM_TO_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "elastic_queue.h"

#define DMA_MAX_BROADCAST_DESTS 5

typedef void (*DmaCopyCompleteCb_t)(void *user_data);

/* Internal states for DmaMemToMem_t */
typedef enum {
    DMA_STATE_IDLE = 0,
    DMA_STATE_RUNNING,
    DMA_STATE_TRANSFER_DONE, // IRQ fired, waiting for main loop to cascade
    DMA_STATE_ERROR
} DmaMemToMemState_t;

/**
 * @brief Structure to track the state of a single DMA stream wrapper
 */
typedef struct {
    // Hardware DMA handle
    DMA_HandleTypeDef * const hdma;

    // Status of the last/current operation
    DmaMemToMemState_t state;

    // Source queue
    ElasticQueue_t * const source_queue;

    // Destination queues
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
void DmaMemToMem_Init(DmaMemToMem_t *dma_ctx, DMA_HandleTypeDef * const hdma, ElasticQueue_t * const src_q);

/**
 * @brief Start a background memory-to-memory broadcast with lazy allocation.
 * 
 * @param dma_ctx      The DMA context to use
 * @param dest_qs      Array of destination queues to allocate from as needed
 * @param num_dests    Number of destinations (up to DMA_MAX_BROADCAST_DESTS)
 * 
 * @return DMA_BROADCAST_OK on success, or a negative DMA_BROADCAST_ERR_* code on failure
 */
int DmaMemToMem_StartBroadcast(DmaMemToMem_t *dma_ctx, 
                               ElasticQueue_t **dest_qs,
                               uint8_t num_dests);

/**
 * @brief Must be called from the HAL_DMA_RegisterCallback or inside the TC interrupt.
 *        This ONLY sets a flag to keep the IRQ handler as minimal as possible.
 */
void DmaMemToMem_TransferComplete(DmaMemToMem_t *dma_ctx);

/**
 * @brief Polled in the main application loop. Checks the status flag set by the IRQ, 
 *        commits the finished buffer, and cascades to the next destination if applicable.
 */
void DmaMemToMem_Process(DmaMemToMem_t *dma_ctx);

/**
 * @brief Checks if the DMA context is idle and ready for a new transfer.
 */
bool DmaMemToMem_IsReady(DmaMemToMem_t *dma_ctx);

#endif /* DMA_MEM_TO_MEM_H */
