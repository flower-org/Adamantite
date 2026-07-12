#include "elastic_queue.h"

void ElasticQueue_Init(ElasticQueue_t *q, uint8_t *area_start, size_t area_len, ElasticQueueRef_t *refs, size_t max_refs)
{
    q->area_start = area_start;
    q->area_len = area_len;
    q->refs = refs;
    q->max_refs = max_refs;
    q->head_ref = 0;
    q->tail_ref = 0;
    q->num_refs = 0;
    q->is_locked = false;
    q->remaining_ops = 0;
}

uint8_t* ElasticQueue_Allocate(ElasticQueue_t *q, size_t n)
{
    if (n == 0 || n > q->area_len) { return NULL; }
    if (q->num_refs >= q->max_refs) { return NULL; } // No free slots in refs array

    size_t alloc_offset = 0;

    if (q->num_refs == 0) {
        // Queue is entirely empty
        alloc_offset = 0;
    } else {
        size_t head_idx = q->head_ref;
        size_t last_idx = (q->tail_ref == 0) ? (q->max_refs - 1) : (q->tail_ref - 1);

        ElasticQueueRef_t *first = &q->refs[head_idx];
        ElasticQueueRef_t *last = &q->refs[last_idx];

        size_t last_end = last->offset + last->len;

        if (last->offset >= first->offset) {
            // Memory used by queue is contiguous, free space is at the end or beginning
            if (q->area_len - last_end >= n) {
                // Fits exactly at the end
                alloc_offset = last_end;
            } else if (first->offset >= n) {
                // Doesn't fit at end, but fits at the beginning (wrap around)
                alloc_offset = 0;
            } else {
                return NULL; // Fragmented or not enough space
            }
        } else {
            // Used memory wraps around, free space is strictly in the middle
            if (first->offset - last_end >= n) {
                alloc_offset = last_end;
            } else {
                return NULL; // Not enough space
            }
        }
    }

    // Space found, register the reference
    ElasticQueueRef_t *new_ref = &q->refs[q->tail_ref];
    new_ref->offset = alloc_offset;
    new_ref->len = n;

    q->tail_ref = (q->tail_ref + 1) % q->max_refs;
    q->num_refs++;

    return q->area_start + alloc_offset;
}

int ElasticQueue_Lock(ElasticQueue_t *q, uint32_t num_operations, uint8_t **out_buf, size_t *out_len)
{
    if (q->is_locked) return -1;       // Already locked by a previous call
    if (q->num_refs == 0) return -1;   // Nothing to read
    if (num_operations == 0) return -1;

    ElasticQueueRef_t *first = &q->refs[q->head_ref];

    if (out_buf) *out_buf = q->area_start + first->offset;
    if (out_len) *out_len = first->len;

    q->is_locked = true;
    q->remaining_ops = num_operations;

    return 0;
}

void ElasticQueue_Done(ElasticQueue_t *q)
{
    if (!q->is_locked) return;

    if (q->remaining_ops > 0) {
        q->remaining_ops--;
    }

    // Once all operations acknowledge completion, advance the queue
    if (q->remaining_ops == 0) {
        q->is_locked = false;
        q->head_ref = (q->head_ref + 1) % q->max_refs;
        q->num_refs--;
    }
}
