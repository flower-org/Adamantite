#ifndef ELASTIC_QUEUE_H
#define ELASTIC_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Reference to a buffer allocated within the elastic queue */
typedef struct {
    size_t offset;
    size_t len;
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
 *        Returns pointer to buffer or NULL if not enough space/refs.
 */
uint8_t* ElasticQueue_Allocate(ElasticQueue_t *q, size_t n);

/**
 * @brief Lock the next available buffer for reading.
 * @param num_operations Number of ElasticQueue_Done calls required to free.
 * @param out_buf Pointer to receive the buffer start.
 * @param out_len Pointer to receive the buffer length.
 * @return 0 on success, -1 if already locked or empty.
 */
int ElasticQueue_Lock(ElasticQueue_t *q, uint32_t num_operations, uint8_t **out_buf, size_t *out_len);

/**
 * @brief Mark one operation as done. When operations reach 0, the buffer is freed.
 */
void ElasticQueue_Done(ElasticQueue_t *q);

#endif // ELASTIC_QUEUE_H
