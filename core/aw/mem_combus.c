#include "mem_combus.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "pollreactor.h"
#include "pyhelper.h"
#include "msgbox.h"
#include "dspMemOps.h"
#include "Sharespace.h"

typedef struct
{
    struct DspMemOps *pDspMemOps;
    // int pipe_fds[2];
    // sem_t exit;
    // pthread_t pid;
    // pthread_mutex_t mtx;
} mem_t;

int mem_open(struct combus *combus, const void *port, void *params)
{
    mem_t *rp;

    rp = (mem_t *)malloc(sizeof(mem_t));
    if (!rp)
        return -1;
    memset(rp, 0, sizeof(*rp));

    rp->pDspMemOps = GetDspMemOps();
    if (rp->pDspMemOps->init)
    {
        rp->pDspMemOps->init();
    }
    // if (pipe(rp->pipe_fds) != 0)
    // {
    //     printf("error: pipe failed\n");
    //     free(rp);
    //     return -1;
    // }
    // fd_set_non_blocking(rp->pipe_fds[0]);
    // fd_set_non_blocking(rp->pipe_fds[1]);
    fd_msgbox = msgbox_rpmsg_init();
    printf("mem_combus open fd : %d\n", fd_msgbox);

    // THREAD
    // sem_init(&rp->exit, 0, 0);
    // pthread_mutex_init(&rp->mtx, 0);
    // pthread_create(&rp->pid, NULL, mem_rx_thread, rp);

    combus->fd = fd_msgbox;
    combus->drv_data = rp;
    return 0;
}

void mem_close(struct combus *combus)
{
    mem_t *rp = (mem_t *)combus->drv_data;
    if (rp->pDspMemOps->de_init)
    {
        rp->pDspMemOps->de_init();
    }
    free(rp);
}

ssize_t mem_write(struct combus *combus, const void *buf, size_t counts)
{
    // printf("mem_write %d: ", counts);
    // for (int i = 0; i < counts; i++)
    //     printf("%x ", ((uint8_t *)buf)[i]);
    // printf("\n");
    mem_t *rp = (mem_t *)combus->drv_data;
    rp->pDspMemOps->mem_write(buf, counts);
// #if WRITE_MSGBOX_ENABLE || SYNC_MSGBOX_ENABLE
    msgbox_send_signal(MSGBOX_IS_WRITE, sharespace_arm_addr[SHARESPACE_WRITE]);
// #endif
    rp->pDspMemOps->write_lenth += counts;
    return counts;
    // mem_do_write(buf, counts);
}

ssize_t mem_read(struct combus *combus, void *buf, size_t counts)
{
    uint8_t dummy[16];
    mem_t *rp = (mem_t *)combus->drv_data;
    // read(rp->pipe_fds[0], dummy, sizeof(dummy));

    int ret = 0;
    ret = rp->pDspMemOps->mem_read(buf);
    rp->pDspMemOps->set_read_pos(ret); //    set_read_dsp_space_pos(ret);
    if (ret > 0)
    {
        rp->pDspMemOps->read_lenth += ret;
    }
    // if (ret == 0)
    //     return 0;

    // printf("mem_read %d: ", ret);
    // for (int i = 0; i < ret; i++)
    //     printf("%x ", ((uint8_t *)buf)[i]);
    // printf("\n");
    return ret;
    // mem_do_read((uint8_t *)buf, counts);
}

// static void *rpbuf_rx_thread(void *data)
// {
//     mem_t *rp = (mem_t *)(data);
//     while (1)
//     {
//         if (msgbox_read_signal(MSGBOX_ALLOW_READ))
//         {
//             int rc;
//             if ((rc = write(rp->pipe_fds[1], ".", 1)) < 0)
//             {
//                 printf("write to pipe failed %d: %s\n", rc, strerror(rc));
//             }
//         }

//     }
// }