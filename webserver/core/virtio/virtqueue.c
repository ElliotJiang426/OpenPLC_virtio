#include "virtqueue.h"
#include <stdlib.h>

#define POWEROF2(n)			~(n & (n - 1))

/*
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

/* Virtqueue initialization */
struct Virtqueue *virtqueue_init(uint16_t num) {
    if (!POWEROF2(num)){
        // TODO: error: virtq size must be power of 2
    }

    struct Virtqueue *vq = calloc(1, sizeof(struct Virtqueue));
    vq->num = num;
    vq->last_used_idx = 0;
    vq->free_head = 0;
    vq->num_free = num;
    vq->num_added = 0;
    // vq->num_added_since_last_notify = 0;
    // vq->size = size;

    vq->desc = calloc(num, sizeof(struct virtq_desc));
    vq->avail = calloc(1, sizeof(struct virtq_avail) + num * sizeof(uint16_t));
    vq->used = calloc(1, sizeof(struct virtq_used) + num * sizeof(struct virtq_used_elem));
    vq->avail_ring = vq->avail->ring;
    vq->used_ring = (uint16_t *)vq->used->ring;

    /* descriptor table initialization */
    for (int i = 0; i < num - 1; i++)
		vq->desc[i].next = i + 1;
	vq->desc[num-1].next = VQ_RING_DESC_CHAIN_END;

    /* avail ring initialization */
    vq->avail->flags = 0;
    vq->avail->idx = 0;

    /* used ring initialization */
    vq->used->flags = 0;
    vq->used->idx = 0;

    return vq;
}

/* 添加描述符到 Virtqueue 中 */
int virtqueue_add_desc(struct Virtqueue *vq, void *data, uint32_t len, uint16_t flags) {
    /* 检查空闲条目是否存在 */
    if (vq->num_free == 0) {
        return -1; /* Virtqueue 已满 */
    }

    /* 获取空闲条目的索引 */
    uint16_t desc_idx = vq->free_head;
    vq->free_head = vq->desc[desc_idx].next;
    vq->num_free--;

    /* 将描述符数据填入描述符 */
    struct virtq_desc *desc = &vq->desc[desc_idx];
    desc->addr = (uint64_t)data;
    desc->len = len;
    desc->flags = flags;
    desc->next = 0;

    /* 将描述符添加到可用环中 */
    vq->avail_ring[vq->avail->idx % vq->num] = desc_idx;
    vq->avail->idx++;
    vq->num_added++;
    // vq->num_added_since_last_notify++;

    return desc_idx;
}

/* 获取 Virtqueue 中的已使用条目 */
int virtqueue_get_used(struct Virtqueue *vq, uint16_t *id, uint32_t *len) {
    /* 检查是否存在已使用的条目 */
    if (vq->used->idx == vq->last_used_idx) {
        return -1; /* 没有新的已使用条目 */
    }

    /* 从已使用环中获取已使用条目的索引 */
    uint16_t used_idx = vq->used_ring[vq->last_used_idx % vq->num];
    
    /* 获取描述符中的标识符和长度 */
    *id = vq->desc[used_idx].flags;
    len = vq->used->ring[vq->last_used_idx % vq->num].len;

    /* 更新已使用条目索引 */
    vq->last_used_idx++;

    /* 将描述符添加到空闲链表中 */
    vq->desc[used_idx].next = vq->free_head;
    vq->free_head = used_idx;
    vq->num_free++;

    return used_idx;
}

/* 通知设备 Virtqueue 中有新的可用条目 */
void virtqueue_notify(struct Virtqueue *vq) {
    /* 检查是否需要通知设备 */
    if (vq->num_added == 0) {
        return;
    }

    /* 更新通知索引 */
    vq->num_added = 0;

    /* 通知设备 */
    /* TODO: 添加通知设备的代码 */
}

/* 销毁 Virtqueue */
void virtqueue_destroy(struct Virtqueue *vq) {
    free(vq->desc);
    free(vq->avail);
    free(vq->used);
    free(vq);
}