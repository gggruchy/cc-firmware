#ifndef SERIALQUEUE_H
#define SERIALQUEUE_H

#include <stdint.h>   // uint8_t
#include "list.h"     // struct list_head
#include "msgblock.h" // MESSAGE_MAX
#include <pthread.h>  // pthread_mutex_lock

#include "combus.h"

#define MAX_CLOCK 0x7fffffffffffffffLL
#define BACKGROUND_PRIORITY_CLOCK 0x7fffffff00000000LL

// msg_queue->msgs -> stalled_queue -> ready_queue->sent_queue->old_sent                          //"reset_step_clock"  "set_next_step_dir"  "queue_step"
// serialqueue_send_one->msgs -> stalled_queue -> ready_queue->sent_queue->old_sent      //"trsync_trigger"  "trsync_set_timeout"  //"trsync_trigger"  "stepper_get_position"    "get_uptime"  "get_clock"  "get_config" "identify"
struct command_queue
{                                   //----2-G-G-2022-04-08-----------------
    struct list_head stalled_queue; // 没有消息头尾的等待时间发送队列
    struct list_head ready_queue;   // 没有消息头尾的待发送队列
    struct list_node node;
};

struct serialqueue
{
    // Input reading
    struct pollreactor *pr;
    void *m_serial;
    int serial_fd, serial_fd_type, client_id;
    int pipe_fds[2];
    uint8_t input_buf[4096];
    uint8_t need_sync;
    int input_pos;
    char port[32];

    // Threading
    pthread_t tid;
    pthread_mutex_t lock; // protects variables below
    pthread_cond_t cond;
    int receive_waiting;

    // Baud / clock tracking
    int receive_window;
    double bittime_adjust, idle_time;
    struct clock_estimate ce;
    double last_receive_sent_time;

    // Retransmit support
    uint64_t send_seq, receive_seq;
    uint64_t ignore_nak_seq, last_ack_seq, retransmit_seq, rtt_sample_seq;
    struct list_head sent_queue; // queue_message
    double srtt, rttvar, rto;

    // Pending transmission message queues
    struct list_head pending_queues; // command_queue
    int ready_bytes, stalled_bytes, need_ack_bytes, last_ack_bytes;
    uint64_t need_kick_clock;
    struct list_head notify_queue; // queue_message
    uint32_t max_pending_blocks;   // MAX_PENDING_BLOCKS

    // Received messages
    struct list_head receive_queue; // queue_message

    // Fastreader support
    pthread_mutex_t fast_reader_dispatch_lock;
    struct list_head fast_readers;

    // Debugging
    struct list_head old_sent, old_receive;

    // Stats
    uint32_t bytes_write, bytes_read, bytes_retransmit, bytes_invalid;

    combus_t *bus;
};

struct fastreader;
typedef void (*fastreader_cb)(struct fastreader *fr, uint8_t *data, int len);

struct fastreader
{
    struct list_node node;
    fastreader_cb func;
    int prefix_len;
    uint8_t prefix[MESSAGE_MAX];
};

struct pull_queue_message
{
    uint8_t msg[MESSAGE_MAX];
    int len;
    double sent_time, receive_time;
    uint64_t notify_id;
};

struct serialqueue;
struct serialqueue * serialqueue_alloc(const char *port, void *params, char serial_fd_type, int client_id, int max_pending_blocks);
void serialqueue_exit(struct serialqueue *sq);
void serialqueue_free(struct serialqueue *sq);
struct command_queue *serialqueue_alloc_commandqueue(void);
void serialqueue_free_commandqueue(struct command_queue *cq);
void serialqueue_add_fastreader(struct serialqueue *sq, struct fastreader *fr);
void serialqueue_rm_fastreader(struct serialqueue *sq, struct fastreader *fr);
void serialqueue_send_batch(struct serialqueue *sq, struct command_queue *cq, struct list_head *msgs);
void serialqueue_send_one(struct serialqueue *sq, struct command_queue *cq, struct queue_message *qm);
void serialqueue_send(struct serialqueue *sq, struct command_queue *cq, uint8_t *msg, int len, uint64_t min_clock, uint64_t req_clock, uint64_t notify_id);
void serialqueue_pull(struct serialqueue *sq, struct pull_queue_message *pqm);
void serialqueue_set_wire_frequency(struct serialqueue *sq, double baud_adjust);
void serialqueue_set_receive_window(struct serialqueue *sq, int receive_window);
void serialqueue_set_clock_est(struct serialqueue *sq, double est_freq, double conv_time, uint64_t conv_clock, uint64_t last_clock);
void serialqueue_get_clock_est(struct serialqueue *sq, struct clock_estimate *ce);
void serialqueue_get_stats(struct serialqueue *sq, char *buf, int len);
int serialqueue_extract_old(struct serialqueue *sq, int sentq, struct pull_queue_message *q, int max);

#endif // serialqueue.h
