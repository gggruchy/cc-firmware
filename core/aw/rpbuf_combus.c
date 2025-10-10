#include "rpbuf_combus.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "pollreactor.h"
#include "pyhelper.h"
#include "librpbuf/librpbuf.h"

static void *rpbuf_rx_thread(void *data);

typedef struct
{
    rpbuf_buffer_t *arm_to_dsp;
    rpbuf_buffer_t *dsp_to_arm;
    int pipe_fds[2];
    uint8_t rx_buffer[8 * 1024];
    uint32_t rx_length;
    sem_t exit;
    pthread_t pid;
    pthread_mutex_t mtx;
} rpbuf_t;

int rpbuf_open(struct combus *combus, const void *port, void *params)
{
    rpbuf_t *rp;

    rp = (rpbuf_t *)malloc(sizeof(rpbuf_t));
    if (!rp)
        return -1;
    memset(rp, 0, sizeof(*rp));

    // RPBUF
    rp->arm_to_dsp = rpbuf_alloc_buffer(0, "arm_to_dsp", 16 * 1024);
    if (!rp->arm_to_dsp)
    {
        printf("error: can't alloc rpbuf arm_to_dsp\n");
        free(rp);
        return -1;
    }
    rpbuf_set_sync_buffer(rp->arm_to_dsp, 1);

    // PIPE
    if (pipe(rp->pipe_fds) != 0)
    {
        printf("error: pipe failed\n");
        rpbuf_free_buffer(rp->arm_to_dsp);
        free(rp);
        return -1;
    }

    fd_set_non_blocking(rp->pipe_fds[0]);
    fd_set_non_blocking(rp->pipe_fds[1]);

    // THREAD
    sem_init(&rp->exit, 0, 0);
    pthread_mutex_init(&rp->mtx, 0);
    pthread_create(&rp->pid, NULL, rpbuf_rx_thread, rp);

    combus->fd = rp->pipe_fds[0];
    combus->drv_data = rp;
    return 0;
}

void rpbuf_close(struct combus *combus)
{
    rpbuf_t *rp = (rpbuf_t *)(combus->drv_data);
    printf("rpbuf_close\n");
    sem_post(&rp->exit);
    pthread_join(rp->pid, NULL);
    sem_destroy(&rp->exit);
    pthread_mutex_destroy(&rp->mtx);
    close(rp->pipe_fds[0]);
    close(rp->pipe_fds[1]);
    rpbuf_free_buffer(rp->arm_to_dsp);
    free(rp);
}

ssize_t rpbuf_write(struct combus *combus, const void *buf, size_t counts)
{
#if 0
    printf("rpbuf_write %d: ", counts);
    for (int i = 0; i < counts; i++)
        printf("%x ", ((uint8_t *)buf)[i]);
    printf("\n");
#endif
    rpbuf_t *rp = (rpbuf_t *)(combus->drv_data);
    void *buf_addr;
    if (rpbuf_buffer_is_available(rp->arm_to_dsp))
    {
        buf_addr = rpbuf_buffer_addr(rp->arm_to_dsp);
        memcpy(buf_addr, buf, counts);
        if (rpbuf_transmit_buffer(rp->arm_to_dsp, 0, counts) < 0)
        {
            printf("rpbuf_transmit_buffer failed\n");
            return -1;
        }
    }
    else
    {
        printf("arm_to_dsp not available\n");
        return -1;
    }
    return counts;
}

ssize_t rpbuf_read(struct combus *combus, void *buf, size_t counts)
{
    uint8_t dummy[16];
    rpbuf_t *rp = (rpbuf_t *)(combus->drv_data);
    read(rp->pipe_fds[0], dummy, sizeof(dummy));

    pthread_mutex_lock(&rp->mtx);
    size_t rlen = counts > rp->rx_length ? rp->rx_length : counts;
    memcpy(buf, rp->rx_buffer, rlen);
    memmove(rp->rx_buffer + rlen, rp->rx_buffer, rlen);
    rp->rx_length -= rlen;
    pthread_mutex_unlock(&rp->mtx);
    return rlen;
}

static void *rpbuf_rx_thread(void *data)
{
    rpbuf_t *rp = (rpbuf_t *)(data);
    uint32_t offset, data_len;
    uint32_t cp_len;
    void *buf_addr;

    while (sem_trywait(&rp->exit) != 0 && rpbuf_receive_buffer(rp->arm_to_dsp, &offset, &data_len, -1) >= 0)
    {
        pthread_mutex_lock(&rp->mtx);
        cp_len = data_len > (sizeof(rp->rx_buffer) - rp->rx_length) ? (sizeof(rp->rx_buffer) - rp->rx_length) : data_len;
        if (cp_len)
        {
            buf_addr = rpbuf_buffer_addr(rp->arm_to_dsp);
            memcpy(rp->rx_buffer + rp->rx_length, buf_addr + offset, cp_len);
            rp->rx_length += cp_len;
        }
        pthread_mutex_unlock(&rp->mtx);

        int rc;
        if ((rc = write(rp->pipe_fds[1], ".", 1)) < 0)
        {
            printf("write to pipe failed %d: %s\n", rc, strerror(rc));
        }
    }
    return NULL;
}