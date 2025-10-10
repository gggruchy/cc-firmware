#include "rpmsg_combus.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#include "pollreactor.h"
#include "pyhelper.h"
#include "librpmsg/rpmsg.h"
#include "librpmsg/librpmsg.h"

#define PRINTF_CNT 1000

static int do_verbose = 0;
static unsigned int rx_threshold = 0;
static unsigned int tx_threshold = 0;
const char *ctrl_name;

static void *rpmsg_rx_thread(void *data);

typedef struct rpmsg_ept_instance
{
    int ept_id;
    int ept_fd;
    int pipe_fds[2];
    uint8_t rx_buffer[8 * 1024];
    uint32_t rx_length;
    pthread_t recv_thread;
    pthread_mutex_t mtx;
    sem_t exit;
} rpmsg_t;

static long long get_currenttime()
{
	struct timespec time;

	clock_gettime(CLOCK_REALTIME_COARSE, &time);

	return (time.tv_sec * 1000 * 1000 + time.tv_nsec / 1000);
}

int rpmsg_open(struct combus *combus, const void *port, void *params)
{
    rpmsg_t *eptdev;

    eptdev = (rpmsg_t *)malloc(sizeof(rpmsg_t));
    if (!eptdev)
        return -1;
    memset(eptdev, 0, sizeof(*eptdev));

    // RPMSG
    eptdev->ept_id = rpmsg_alloc_ept("dsp_rproc@0", "arm_to_dsp");
    if (eptdev->ept_id < 0)
    {
        printf("rpmsg_alloc_ept for ept failed\n");
        return -1;
    }

    char ept_file_path[64];
    snprintf(ept_file_path, sizeof(ept_file_path),
             "/dev/rpmsg%d", eptdev->ept_id);
    eptdev->ept_fd = open(ept_file_path, O_RDWR);
    if (eptdev->ept_fd < 0)
    {
        printf("open %s failed\r\n", ept_file_path);
        return -1;
    }

    // PIPE
    if (pipe(eptdev->pipe_fds) != 0)
    {
        printf("error: pipe failed\n");
        // rpbuf_free_buffer(eptdev->arm_to_dsp);
        free(eptdev);
        return -1;
    }

    fd_set_non_blocking(eptdev->pipe_fds[0]);
    fd_set_non_blocking(eptdev->pipe_fds[1]);

    // THREAD
    sem_init(&eptdev->exit, 0, 0);
    pthread_mutex_init(&eptdev->mtx, 0);
    pthread_create(&eptdev->recv_thread, NULL, rpmsg_rx_thread, eptdev);

    combus->fd = eptdev->pipe_fds[0];
    combus->drv_data = eptdev;
    return 0;
}

void rpmsg_close(struct combus *combus)
{
    rpmsg_t *eptdev = (rpmsg_t *)(combus->drv_data);
    printf("rpmsg_close\n");
    sem_post(&eptdev->exit);
    pthread_join(eptdev->recv_thread, NULL);
    sem_destroy(&eptdev->exit);
    pthread_mutex_destroy(&eptdev->mtx);
    close(eptdev->pipe_fds[0]);
    close(eptdev->pipe_fds[1]);
    close(eptdev->ept_fd);
    free(eptdev);
}

ssize_t rpmsg_write(struct combus *combus, const void *buf, size_t counts)
{
#if 0
    printf("rpbuf_write %d: ", counts);
    for (int i = 0; i < counts; i++)
        printf("%x ", ((uint8_t *)buf)[i]);
    printf("\n");
#endif
    rpmsg_t *eptdev = (rpmsg_t *)(combus->drv_data);
    // if(counts >= 496)
    // {
    //     printf("--------------------------------------send %d--------------------------\n", counts);
    // }
    // else
    // {
    //     printf("send %d \n", counts);
    // }
    int ret = write(eptdev->ept_fd, (uint8_t *)buf, counts);
    if (ret < 0)
    {
        printf("write error=%s\n", strerror(ret));
        return -1;
    }
}

ssize_t rpmsg_read(struct combus *combus, void *buf, size_t counts)
{
    uint8_t dummy[16];
    rpmsg_t *eptdev = (rpmsg_t *)(combus->drv_data);
    read(eptdev->pipe_fds[0], dummy, sizeof(dummy));

    pthread_mutex_lock(&eptdev->mtx);
    size_t rlen = counts > eptdev->rx_length ? eptdev->rx_length : counts;
    memcpy(buf, eptdev->rx_buffer, rlen);
    // printf("rpmsg_read %d : ", rlen);
    // for (int i = 0; i < rlen; i++)
    // {
    //     printf("%x ", ((uint8_t *)buf)[i]);
    // }
    // printf("\n");
    memmove(eptdev->rx_buffer + rlen, eptdev->rx_buffer, rlen);
    eptdev->rx_length -= rlen;
    pthread_mutex_unlock(&eptdev->mtx);
    return rlen;
}

static void *rpmsg_rx_thread(void *data)
{
    rpmsg_t *eptdev = (rpmsg_t *)data;
    uint8_t rx_buf[RPMSG_DATA_MAX_LEN];
    char name[64];
    uint32_t ret;
    long long start_usec, end_usec;
    float delta_usec = 0;
    unsigned long rev_cnt = 0, rev_bytes = 0;
    // struct pollfd poll_fds[1] = {
    //     {
    //         .fd = eptdev->ept_fd,
    //         .events = POLLIN,
    //     }};
    while (1)
    {
        if (sem_trywait(&eptdev->exit) == 0)
            break;

        // start_usec = get_currenttime();
        // ret = poll(poll_fds, 1, 1000);
        // if (ret < 0)
        // {
        //     if (errno == EINTR)
        //     {
        //         printf("ept %s: signal occurred\n", name);
        //         break;
        //     }
        //     else
        //     {
        //         printf("ept %s: poll error (%s)\n", name, strerror(errno));
        //         continue;
        //     }
        // }
        // else if (ret == 0)
        // { /* timeout */
        //     if (sem_trywait(&eptdev->exit) == 0)
        //     {
        //         break;
        //     }
        //     else
        //     {
        //         printf("polling /dev/%s timeout.\r\n", name);
        //         continue;
        //     }
        // }
        ret = read(eptdev->ept_fd, rx_buf, sizeof(rx_buf));
        if (ret < 0)
        {
            printf("%s read file error=%s.\r\n", name, strerror(ret));
            continue;
        }
        eptdev->rx_length += ret;
        // printf("read : \n");
        // for(int i =0; i<ret; i++)
        // {
        //     printf("%02x ", rx_buf[i]);
        // }
        // printf("\n");

        pthread_mutex_lock(&eptdev->mtx);
        memcpy(eptdev->rx_buffer, rx_buf, sizeof(rx_buf));
        // printf("eptdev->rx_buffer read : ");
        // for (int i = 0; i < 20; i++)
        // {
        //     printf("%x ", eptdev->rx_buffer[i]);
        // }
        // printf("\n");
        pthread_mutex_unlock(&eptdev->mtx);
        // end_usec = get_currenttime();
        delta_usec += (end_usec - start_usec);
        rev_cnt++;
        rev_bytes += ret;
        if (rx_threshold && (end_usec - start_usec) > (rx_threshold * 1000))
        {
            printf("[%s] receive too long: expect < %dus,cur %dus  \n",
                   name,
                   rx_threshold * 1000,
                   (int)(end_usec - start_usec));
        }
        if (do_verbose || !(rev_cnt % PRINTF_CNT))
        {
            // printf("[%s] receive : %fKb %fms %fMb/s\n", name,
                //    rev_bytes / 1000.0,
                //    delta_usec / 1000.0,
                //    rev_bytes / delta_usec);
            rev_bytes = 0;
            delta_usec = 0;
        }
        int rc;
        if ((rc = write(eptdev->pipe_fds[1], ".", 1)) < 0)
        {
            printf("write to pipe failed %d: %s\n", rc, strerror(rc));
        }
    }

    printf("%s recv_thread exit.\r\n", name);
    pthread_exit(NULL);
}