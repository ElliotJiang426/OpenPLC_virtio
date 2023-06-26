#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/virtio_config.h>
#include <virtqueue.h>
#include <linux/virtio_blk.h>

#define QUEUE_SIZE 256
#define SECTOR_SIZE 512
#define BLOCK_SIZE (128 * SECTOR_SIZE)

struct virtio_gpio_req {
    // TODO: to be done
};

struct virtio_gpio_config {
    // TODO: to be done
};

struct virtio_blk_buffer {
    struct virtio_blk_req req;
    uint8_t data[BLOCK_SIZE];
};

struct virtio_gpio {
    int fd;
    void *addr;
    struct virtio_gpio_config *config;
    struct virtio_blk_buffer *buffers;
    struct Virtqueue *queue;
};

int virtio_gpio_init(struct virtio_gpio *gpio, const char *path) {
    // TODO: path to device, may determined by docker
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        sprintf("Virtio Init Error: Failed to open path: %s", path)
        return -1;
    }

    void *addr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        sprintf("Virtio Init Error: mmap failed");
        return -1;
    }

    struct virtio_gpio_config *config = addr;
    struct Virtqueue *queue = addr + sizeof(struct virtio_gpio_config);

    if (!(config->wce & VIRTIO_BLK_F_CONFIG_WCE)) {
        fprintf(stderr, "write-back caching not supported\n");
        return -1;
    }

    if (!(config->capacity > 0)) {
        fprintf(stderr, "invalid disk capacity\n");
        return -1;
    }

    gpio->fd = fd;
    gpio->addr = addr;
    gpio->config = config;
    gpio->buffers = malloc(sizeof(struct virtio_blk_buffer) * QUEUE_SIZE);
    gpio->queue = queue;

    return 0;
}

void virtio_blk_exit(struct virtio_blk *blk) {
    munmap(blk->addr, 0x1000);
    close(blk->fd);
    free(blk->buffers);
}

int virtio_blk_read(struct virtio_blk *blk, uint64_t sector, uint8_t *data, uint32_t count) {
    struct virtio_blk_buffer *buffer = &blk->buffers[blk->queue->last_usedbuffer->req.type = VIRTIO_BLK_T_IN;
    buffer->req.sector = sector;

    struct iovec iov[2] = {
        { &buffer->req, sizeof(buffer->req) },
        { data, count },
    };
    struct virtqueue *queue = blk->queue;
    uint16_t head = queue->last_avail;
    uint16_t next = queue->vring->avail->ring[head % QUEUE_SIZE];
    if (next == QUEUE_SIZE) {
        return -1;
    }
    queue->vring->desc[head].addr = (uint64_t)buffer;
    queue->vring->desc[head].len = sizeof(*buffer);
    queue->vring->desc[head].flags = VRING_DESC_F_NEXT;
    queue->vring->desc[head].next = (head + 1) % QUEUE_SIZE;
    queue->vring->desc[head + 1].addr = (uint64_t)data;
    queue->vring->desc[head + 1].len = count;
    queue->vring->desc[head + 1].flags = VRING_DESC_F_WRITE;
    queue->vring->desc[head + 1].next = QUEUE_SIZE;
    queue->vring->avail->ring[head % QUEUE_SIZE] = head;
    queue->vring->avail->idx = head + 1;
    queue->last_avail++;

    ioctl(blk->fd, VIRTIO_BLK_T_IN | VIRTIO_BLK_T_OUT, queue->idx);

    while (queue->last_used == head) {
        usleep(10);
    }

    uint16_t used = queue->vring->used->ring[head % QUEUE_SIZE];
    uint8_t status = buffer->data[0];
    if (status != VIRTIO_BLK_S_OK) {
        return -1;
    }

    return 0;
}

int virtio_blk_write(struct virtio_blk *blk, uint64_t sector, const uint8_t *data, uint32_t count) {
    struct virtio_blk_buffer *buffer = &blk->buffers[blk->queue->last_used];

    buffer->req.type = VIRTIO_BLK_T_OUT;
    buffer->req.sector = sector;

    struct iovec iov[2] = {
        { &buffer->req, sizeof(buffer->req) },
        { buffer->data, count },
    };
    memcpy(buffer->data, data, count);

    struct virtqueue *queue = blk->queue;
    uint16_t head = queue->last_avail;
    uint16_t next = queue->vring->avail->ring[head % QUEUE_SIZE];
    if (next == QUEUE_SIZE) {
        return -1;
    }
    queue->vring->desc[head].addr = (uint64_t)buffer;
    queue->vring->desc[head].len = sizeof(*buffer);
    queue->vring->desc[head].flags = VRING_DESC_F_NEXT;
    queue->vring->desc[head].next = (head + 1) % QUEUE_SIZE;
    queue->vring->desc[head + 1].addr = (uint64_t)buffer->data;
    queue->vring->desc[head + 1].len = count;
    queue->vring->desc[head + 1].flags = 0;
    queue->vring->desc[head + 1].next = QUEUE_SIZE;
    queue->vring->avail->ring[head % QUEUE_SIZE] = head;
    queue->vring->avail->idx = head + 1;
    queue->last_avail++;

    ioctl(blk->fd, VIRTIO_BLK_T_IN | VIRTIO_BLK_T_OUT, queue->idx);

    while (queue->last_used == head) {
        usleep(10);
    }

    uint16_t used = queue->vring->used->ring[head % QUEUE_SIZE];
    uint8_t status = buffer->data[0];
    if (status != VIRTIO_BLK_S_OK) {
        return -1;
    }

    return 0;
}

int virtio_blk_reset(struct virtio_blk *blk) {
    ioctl(blk->fd, VIRTIO_BLK_T_RESET, 0);
    return 0;
}

int main(int argc, char *argv[]) {
    struct virtio_blk blk;
    if (virtio_blk_init(&blk, "/dev/vda") != 0) {
        return 1;
    }

    uint8_t data[SECTOR_SIZE];
    if (virtio_blk_read(&blk, 0, data, SECTOR_SIZE) != 0) {
        fprintf(stderr, "read failed\n");
    }

    if (virtio_blk_write(&blk, 0, data, SECTOR_SIZE) != 0) {
        fprintf(stderr, "write failed\n");
    }

    virtio_blk_reset(&blk);

    virtio_blk_exit(&blk);
    return 0;
}