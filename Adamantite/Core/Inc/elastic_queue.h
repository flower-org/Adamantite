#ifndef ELASTIC_QUEUE_H
#define ELASTIC_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Callback type for when a buffer is completely written and ready */
typedef void (*ElasticQueueReadyCb_t)(void *user_data);

/* Reference to a buffer allocated within the elastic queue */
typedef struct {
    uint8_t *data;  // Absolute pointer to the buffer memory
    size_t len;
    bool is_ready;  // True when DMA or background copy is finished
} ElasticQueueRef_t;

/* Elastic Queue tracking structure */
typedef struct {
    uint8_t *area_start;
    size_t area_len;

    ElasticQueueRef_t *refs;
    size_t max_refs;

    size_t head_ref;
    size_t tail_ref;
    size_t num_refs;

    // Multi-phase locking state
    bool is_locked;
    uint32_t remaining_ops;
} ElasticQueue_t;

/**
 * @brief Initialize the elastic queue
 */
void ElasticQueue_Init(ElasticQueue_t *q, uint8_t *area_start, size_t area_len, ElasticQueueRef_t *refs, size_t max_refs);

/**
 * @brief Allocate a continuous buffer of size n. 
 *        Returns pointer to the queue reference or NULL if not enough space/refs.
 *        The newly allocated buffer is implicitly marked as NOT ready.
 *        Use ref->data to get the actual memory pointer.
 */
ElasticQueueRef_t* ElasticQueue_Allocate(ElasticQueue_t *q, size_t n);

/**
 * @brief Check if the queue is full (cannot allocate n bytes or out of refs).
 * @param q Pointer to the queue.
 * @param n Size of the buffer to check for.
 * @return true if it cannot accommodate n bytes, false if it can.
 */
bool ElasticQueue_IsFull(ElasticQueue_t *q, size_t n);

/**
 * @brief Mark a previously allocated buffer as fully written (ready to read).
 *        Fires the queue's ready_cb if one is configured.
 */
void ElasticQueue_Commit(ElasticQueue_t *q, ElasticQueueRef_t *ref);

/**
 * @brief Abandon an allocated reference that failed to commit.
 *        Reclaims memory if possible, or safely marks it to be skipped.
 */
void ElasticQueue_Abandon(ElasticQueue_t *q, ElasticQueueRef_t *ref);

/**
 * @brief Find the queue reference associated with a specific buffer address.
 * @param buffer Pointer to the data buffer.
 * @return Pointer to the reference, or NULL if not found.
 */
ElasticQueueRef_t* ElasticQueue_GetRefByBuffer(ElasticQueue_t *q, const uint8_t *buffer);

/* Error codes for ElasticQueue operations */
#define ELASTIC_QUEUE_OK             0
#define ELASTIC_QUEUE_ERR_LOCKED    -1
#define ELASTIC_QUEUE_ERR_EMPTY     -2
#define ELASTIC_QUEUE_ERR_NOT_READY -3
#define ELASTIC_QUEUE_ERR_INVAL     -4

/**
 * @brief Check if the queue currently has a ready buffer that can be locked.
 * @return true if a buffer is ready and not already locked, false otherwise.
 */
bool ElasticQueue_IsLockable(ElasticQueue_t *q);

/**
 * @brief Lock the next available buffer for reading.
 * @param num_operations Number of ElasticQueue_Done calls required to free.
 * @param out_buf Pointer to receive the buffer start.
 * @param out_len Pointer to receive the buffer length.
 * @return ELASTIC_QUEUE_OK on success, or a negative ELASTIC_QUEUE_ERR_* code on failure.
 */
int ElasticQueue_Lock(ElasticQueue_t *q, uint32_t num_operations, uint8_t **out_buf, size_t *out_len);

/**
 * @brief Peek at the currently locked buffer.
 * @return Pointer to the locked reference, or NULL if not locked.
 */
ElasticQueueRef_t* ElasticQueue_PeekLocked(ElasticQueue_t *q);

/**
 * @brief Peek at the next available buffer without locking it.
 * @return Pointer to the next reference if ready, or NULL if empty/not ready.
 */
ElasticQueueRef_t* ElasticQueue_Peek(ElasticQueue_t *q);

/**
 * @brief Mark one operation as done. When operations reach 0, the buffer is freed.
 */
void ElasticQueue_Done(ElasticQueue_t *q);

/**
 * @brief Abort the current read lock immediately.
 *        Frees the locked buffer regardless of remaining operations.
 */
void ElasticQueue_Abort(ElasticQueue_t *q);

#endif // ELASTIC_QUEUE_H
