// Serial port command queuing
//
// Copyright (C) 2016-2021  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

// This goal of this code is to handle low-level serial port
// communications with a microcontroller (mcu).  This code is written
// in C (instead of python) to reduce communication latencies and to
// reduce scheduling jitter.  The code queues messages to be
// transmitted, schedules transmission of commands at specified mcu
// clock times, prioritizes commands, and handles retransmissions.  A
// background thread is launched to do this work and minimize latency.

#include <linux/can.h> // // struct can_frame
#include <math.h>      // fabs

#include <stddef.h>      // offsetof
#include <stdint.h>      // uint64_t
#include <stdio.h>       // snprintf
#include <stdlib.h>      // malloc
#include <string.h>      // memset
#include <termios.h>     // tcflush
#include <unistd.h>      // pipe
#include "compiler.h"    // __visible
#include "list.h"        // list_add_tail
#include "msgblock.h"    // message_alloc
#include "pollreactor.h" //
#include "pyhelper.h"    // get_monotonic
#include "serialqueue.h" // struct queue_message
#include "msgbox.h"
#include "debug.h"

#include "combus.h"
#include "rpmsg_combus.h"
#include "uart_combus.h"
#include "mem_combus.h"
#include <unistd.h>
#define LOG_TAG "serialqueue"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#define LOG() //(printf("file: %s line: %d function: %s\n", __FILE__, __LINE__, __FUNCTION__))
#define SQPF_SERIAL 0
#define SQPF_PIPE 1
#define SQPF_NUM 2

#define SQPT_RETRANSMIT 0
#define SQPT_COMMAND 1
#define SQPT_NUM 2

#define SQT_UART 'u'
#define SQT_CAN 'c'
#define SQT_DEBUGFILE 'f'
#define SQT_RPBUF 'r'
#define SQT_MEM 'm'
#define SQT_USB_CDC 'd'

#define SQT_USB_CDC_FRAME_TIME 0.001

#define MIN_RTO 1.25 // 0.025----G-G-2022-08-15--------
#define MAX_RTO 10.000
#define MAX_PENDING_BLOCKS 12
#define MIN_REQTIME_DELTA 0.100
#define MIN_BACKGROUND_DELTA 0.001
#define IDLE_QUERY_TIME 1.0

#define DEBUG_QUEUE_SENT 30
#define DEBUG_QUEUE_RECEIVE 30
#define DEBUG_QUEUE 1

// Create a series of empty messages and add them to a list  创建一系列空消息并将它们添加到列表中
static void debug_queue_alloc(struct list_head *root, int count)
{
    int i;
    for (i = 0; i < count; i++)
    {
        struct queue_message *qm = message_alloc();
        list_add_head(&qm->node, root);
    }
}

// Copy a message to a debug queue and free old debug messages  将消息复制到调试队列并释放旧的调试消息
static void debug_queue_add(struct list_head *root, struct queue_message *qm)
{
    list_add_tail(&qm->node, root);
    struct queue_message *old = list_first_entry(
        root, struct queue_message, node);
    list_del(&old->node);
    message_free(old);
}

// Wake up the receiver thread if it is waiting  如果正在等待，唤醒接收线程
static void
check_wake_receive(struct serialqueue *sq)
{
    if (sq->receive_waiting)
    {
        sq->receive_waiting = 0;
        pthread_cond_signal(&sq->cond);
    }
}

// Write to the internal pipe to wake the background thread if in poll  如果在轮询中，写入内部管道以唤醒后台线程
static void
kick_bg_thread(struct serialqueue *sq)
{
    int ret = write(sq->pipe_fds[1], ".", 1);
    if (ret < 0)
        report_errno("pipe write", ret);
}

// Minimum number of bits in a canbus message
#define CANBUS_PACKET_BITS ((1 + 11 + 3 + 4) + (16 + 2 + 7 + 3))
#define CANBUS_IFS_BITS 4
// Determine minimum time needed to transmit a given number of bytes
static double
calculate_bittime(struct serialqueue *sq, uint32_t bytes)
{
    if (sq->serial_fd_type == SQT_CAN)
    {
        uint32_t pkts = DIV_ROUND_UP(bytes, 8);
        uint32_t bits = bytes * 8 + pkts * CANBUS_PACKET_BITS - CANBUS_IFS_BITS;
        return sq->bittime_adjust * bits;
    }
    else
    {
        return sq->bittime_adjust * bytes;
    }
}
// Update internal state when the receive sequence increases  当接收序列增加时更新内部状态
static void update_receive_seq(struct serialqueue *sq, double eventtime, uint64_t rseq)
{
    // Remove from sent queue  从发送队列中移除
    uint64_t sent_seq = sq->receive_seq;
    for (;;)
    {
        struct queue_message *sent = list_first_entry(
            &sq->sent_queue, struct queue_message, node);
        if (list_empty(&sq->sent_queue))
        {
            // Got an ack for a message not sent; must be connection init  收到未发送消息的确认； 必须是连接初始化
            sq->send_seq = rseq;
            sq->last_receive_sent_time = 0.;
            break;
        }
        sq->need_ack_bytes -= sent->len;
        list_del(&sent->node); // 收到新的序列号应答，至少删除一个等待应答的消息
#if DEBUG_QUEUE
        debug_queue_add(&sq->old_sent, sent);
#endif
        sent_seq++;
        if (rseq == sent_seq)
        { // Found sent message corresponding with the received sequence  找到与接收序列对应的发送消息
            sq->last_receive_sent_time = sent->receive_time;
            sq->last_ack_bytes = sent->len;
            break;
        }
    }
    sq->receive_seq = rseq; // 已经收到的应答序列号
    pollreactor_update_timer(sq->pr, SQPT_COMMAND, PR_NOW);

    // Update retransmit info  更新重传信息
    if (sq->rtt_sample_seq && rseq > sq->rtt_sample_seq && sq->last_receive_sent_time)
    {
        // 考虑USB帧时间的RTT计算
        double delta = eventtime - sq->last_receive_sent_time;
        // 减去USB帧时间影响
        delta -= 0.002; // 减去2ms(发送和接收各1ms)
        
        if (!sq->srtt)
        {
            sq->rttvar = delta / 2.0;
            sq->srtt = delta;
        }
        else
        {
            sq->rttvar = (3.0 * sq->rttvar + fabs(sq->srtt - delta)) / 4.0;
            sq->srtt = (7.0 * sq->srtt + delta) / 8.0;
        }
        
        // 调整RTO计算
        double rttvar4 = sq->rttvar * 4.0;
        if (rttvar4 < 0.001)
            rttvar4 = 0.001;
        sq->rto = sq->srtt + rttvar4;
        if (sq->rto < MIN_RTO)
            sq->rto = MIN_RTO;
        else if (sq->rto > MAX_RTO)
            sq->rto = MAX_RTO;
        sq->rtt_sample_seq = 0;
    }
    if (list_empty(&sq->sent_queue))
    {
        pollreactor_update_timer(sq->pr, SQPT_RETRANSMIT, PR_NEVER);
    }
    else // 250ms 后没有应答重发
    {
        struct queue_message *sent = list_first_entry(
            &sq->sent_queue, struct queue_message, node);
        double nr;
        if (sq->serial_fd_type == SQT_USB_CDC)
            nr = eventtime + sq->rto + SQT_USB_CDC_FRAME_TIME;
        else
            nr = eventtime + sq->rto + calculate_bittime(sq, sent->len);
        pollreactor_update_timer(sq->pr, SQPT_RETRANSMIT, nr);
    }
}

// Process a well formed input message   处理格式良好的输入消息
static void handle_message(struct serialqueue *sq, double eventtime, int len)
{
    // LOG_D("handle_message\n");
    pthread_mutex_lock(&sq->lock);
    // Calculate receive sequence number  计算接收序列号
    uint64_t rseq = ((sq->receive_seq & ~MESSAGE_SEQ_MASK) | (sq->input_buf[MESSAGE_POS_SEQ] & MESSAGE_SEQ_MASK));
    if (rseq != sq->receive_seq)
    {
        // New sequence number  新序列号
        if (rseq < sq->receive_seq)
            rseq += MESSAGE_SEQ_MASK + 1;
        if (rseq > sq->send_seq && sq->receive_seq != 1)
        {
            // An ack for a message not sent?  Out of order message?  未发送消息的确认？ 乱序消息？
            sq->bytes_invalid += len;
            pthread_mutex_unlock(&sq->lock);
            return;
        }
        update_receive_seq(sq, eventtime, rseq);
    }
    sq->bytes_read += len;

    // Check for pending messages on notify_queue  检查 notify_queue 上的待处理消息
    int must_wake = 0;
    while (!list_empty(&sq->notify_queue))
    {
        struct queue_message *qm = list_first_entry(
            &sq->notify_queue, struct queue_message, node);
        uint64_t wake_seq = rseq - 1 - (len > MESSAGE_MIN ? 1 : 0);
        uint64_t notify_msg_sent_seq = qm->req_clock;
        if (notify_msg_sent_seq > wake_seq)
            break;
        list_del(&qm->node);
        qm->len = 0;
        qm->sent_time = sq->last_receive_sent_time;
        qm->receive_time = eventtime;
        list_add_tail(&qm->node, &sq->receive_queue);
        must_wake = 1;
    }
    // Process message
    if (len == MESSAGE_MIN)
    {
        // Ack/nak message
        if (sq->last_ack_seq < rseq)
            sq->last_ack_seq = rseq;
        else if (rseq > sq->ignore_nak_seq && !list_empty(&sq->sent_queue))
            // Duplicate Ack is a Nak - do fast retransmit
            pollreactor_update_timer(sq->pr, SQPT_RETRANSMIT, PR_NOW);
    }
    else
    {
        // Data message - add to receive queue
        struct queue_message *qm = message_fill(sq->input_buf, len);
        qm->sent_time = (rseq > sq->retransmit_seq
                             ? sq->last_receive_sent_time
                             : 0.);
        qm->receive_time = get_monotonic(); // must be time post read()
        if (sq->serial_fd_type == SQT_USB_CDC)
            qm->receive_time -= SQT_USB_CDC_FRAME_TIME;
        else
            qm->receive_time -= calculate_bittime(sq, len);
        list_add_tail(&qm->node, &sq->receive_queue);
        must_wake = 1;
    }
    // Check fast readers
    struct fastreader *fr;
    list_for_each_entry(fr, &sq->fast_readers, node)
    {
        if (len < fr->prefix_len + MESSAGE_MIN || memcmp(&sq->input_buf[MESSAGE_HEADER_SIZE], fr->prefix, fr->prefix_len) != 0)
            continue;
        // Release main lock and invoke callback  释放主锁并调用回调
        pthread_mutex_lock(&sq->fast_reader_dispatch_lock);
        if (must_wake)
            check_wake_receive(sq);
        pthread_mutex_unlock(&sq->lock);
        fr->func(fr, sq->input_buf, len);
        pthread_mutex_unlock(&sq->fast_reader_dispatch_lock);
        return;
    }

    if (must_wake)
        check_wake_receive(sq);
    pthread_mutex_unlock(&sq->lock);
}

// Callback for input activity on the serial fd  回调串行 fd 上的输入活动
static void input_event(struct serialqueue *sq, double eventtime) //---11-3task-G-G--RW_task-
{
    // LOG_D("input_event\n");
    if (sq->serial_fd_type == SQT_RPBUF)
    {
        int ret = combus_read(sq->bus, &sq->input_buf[sq->input_pos], sizeof(sq->input_buf) - sq->input_pos);

            // printf("debug_rx_on read %d: ", ret);
            // for (int i = 0; i < ret; i++)
            // {
            //     printf("%x ", sq->input_buf[sq->input_pos + i]);
            // }
            // printf("\n");
        // if (ret <= 0)
        // {
        //     if (ret < 0)
        //         report_errno("read", ret);
        //     else
        //         errorf("Got EOF when reading from device");
        //     pollreactor_do_exit(sq->pr);
        //     return;
        // }
        sq->input_pos += ret;
        // printf("sq->input_pos=%d\n", sq->input_pos);
    }
    else if (sq->serial_fd_type == SQT_MEM)
    { //---G-G-2022-
        if (msgbox_read_signal(MSGBOX_ALLOW_READ))
        {
            int ret = 0;
            // ret = do_read_sq(&sq->input_buf[sq->input_pos], sizeof(sq->input_buf) - sq->input_pos); //---12-3task-G-G--RW_task-
            ret = combus_read(sq->bus, &sq->input_buf[sq->input_pos], sizeof(sq->input_buf) - sq->input_pos);
            if (ret > 0)
            { //---gg-6.22
                sq->input_pos += ret;
            }
            // printf("combus read ret : %d\n", ret);
            if (ret <= 0)
            {
                if (ret < 0)
                {
                    report_errno("read", ret);
                    pollreactor_do_exit(sq->pr);
                }
                // printf("read error\n");
                return;
            }
        }
    }
    else
    {
        // extern int debug_rx_on;
        // 读取数据至输入缓冲区
        // static double time = 0;
        // if(get_monotonic() - time > 0.001)
        // {
        //     printf("read time : %lf\n", get_monotonic() - time);
        //     // time = get_monotonic();
        // }
        // time = get_monotonic();
        int ret = read(sq->serial_fd, &sq->input_buf[sq->input_pos], sizeof(sq->input_buf) - sq->input_pos);
        if (ret <= 0)
        {
            if (ret < 0)
                report_errno("read", ret);
            else
                errorf("Got EOF when reading from device");
            pollreactor_do_exit(sq->pr);
            return;
        }

        // if (debug_rx_on)
        // {
        //     printf("debug_rx_on read %d: ", ret);
        //     for (int i = 0; i < ret; i++)
        //     {
        //         printf("%x ", sq->input_buf[sq->input_pos + i]);
        //     }
        //     printf("\n");
        // }

        sq->input_pos += ret;
    }

    for (;;)
    {
        {
            int len = msgblock_check(&sq->need_sync, sq->input_buf, sq->input_pos);
            if (!len)
                // Need more data
                return;
            if (len > 0)
            {
                // Received a valid message
                // printf("handle_message\n");
                handle_message(sq, eventtime, len);
            }
            else
            {
                // Skip bad data at beginning of input
                len = -len;

                pthread_mutex_lock(&sq->lock);
                sq->bytes_invalid += len;
                pthread_mutex_unlock(&sq->lock);
            }
            sq->input_pos -= len;
            if (sq->input_pos)
                memmove(sq->input_buf, &sq->input_buf[len], sq->input_pos);
        }
    }
}

// Callback for input activity on the pipe fd (wakes command_event) 用于管道FD上的输入活动的回调
static void kick_event(struct serialqueue *sq, double eventtime) //---12-3task-G-G--RW_task-
{
    char dummy[4096];
    int ret = read(sq->pipe_fds[0], dummy, sizeof(dummy));
    if (ret < 0)
    {
        report_errno("pipe read", ret);
    }
    pollreactor_update_timer(sq->pr, SQPT_COMMAND, PR_NOW);
}

static void do_write(struct serialqueue *sq, void *buf, int buflen)
{
    // printf("---------------------------\n");
    // printf("do_write %d: ", buflen);
    // for(int i = 0; i < buflen; i++)
    // {
    //     printf("%x ", ((uint8_t *)buf)[i]);
    // }
    // printf("\n");
    if (sq->serial_fd_type != SQT_CAN)
    {
        if (sq->serial_fd_type == SQT_UART || sq->serial_fd_type == SQT_USB_CDC)
        {
            int ret = write(sq->serial_fd, buf, buflen);
            if (ret < 0)
                report_errno("write", ret);
            return;
        }
        else if (sq->serial_fd_type == SQT_MEM)
        {
            int ret = combus_write(sq->bus, buf, buflen);
            if (ret < 0)
                report_errno("write", ret);
            return;
        }
        else if (sq->serial_fd_type == SQT_RPBUF)
        {
            int ret = combus_write(sq->bus, buf, buflen);
            if (ret < 0)
                report_errno("write", ret);
            return;
        }
    }
    // Write to CAN fd
    struct can_frame cf;
    while (buflen)
    {
        int size = buflen > 8 ? 8 : buflen;
        cf.can_id = sq->client_id;
        cf.can_dlc = size;
        memcpy(cf.data, buf, size);
        int ret = write(sq->serial_fd, &cf, sizeof(cf));
        if (ret < 0)
        {
            report_errno("can write", ret);
            return;
        }
        buf += size;
        buflen -= size;
    }
}

// void GAM_DEBUG_printf_HEX(uint8_t write ,uint8_t *buf, uint32_t buf_len  );
// Callback timer for when a retransmit should be done  应完成重传的回调计时器
static double retransmit_event(struct serialqueue *sq, double eventtime) //---5-3task-G-G--RW_task-//---send-G-G-2022-07-22---SQPT_RETRANSMIT
{
    LOG_I("retransmit event. serial name : %s\n", sq->port);
    if (sq->serial_fd_type == SQT_UART || sq->serial_fd_type == SQT_USB_CDC)
    {
        int ret = tcflush(sq->serial_fd, TCOFLUSH);
        if (ret < 0)
            report_errno("tcflush", ret);
    }
    pthread_mutex_lock(&sq->lock);
    // Retransmit all pending messages  重新传输所有挂起的消息
    uint8_t buf[MESSAGE_MAX * sq->max_pending_blocks + 1];
    int buflen = 0, first_buflen = 0;
    buf[buflen++] = MESSAGE_SYNC;
    struct queue_message *qm;
    list_for_each_entry(qm, &sq->sent_queue, node)
    {
        memcpy(&buf[buflen], qm->msg, qm->len);
        buflen += qm->len;
        if (!first_buflen)
            first_buflen = qm->len + 1;
    }
    do_write(sq, buf, buflen);
    sq->bytes_retransmit += buflen;

    // Update rto
    if (pollreactor_get_timer(sq->pr, SQPT_RETRANSMIT) == PR_NOW)
    {
        // Retransmit due to nak
        sq->ignore_nak_seq = sq->receive_seq;
        if (sq->receive_seq < sq->retransmit_seq)
            // Second nak for this retransmit - don't allow third
            sq->ignore_nak_seq = sq->retransmit_seq;
    }
    else
    {
        // Retransmit due to timeout
        sq->rto *= 2.0;
        if (sq->rto > MAX_RTO)
            sq->rto = MAX_RTO;
        sq->ignore_nak_seq = sq->send_seq;
    }
    sq->retransmit_seq = sq->send_seq;
    sq->rtt_sample_seq = 0;
    if (sq->serial_fd_type == SQT_USB_CDC)
        sq->idle_time = eventtime + SQT_USB_CDC_FRAME_TIME;
    else
        sq->idle_time = eventtime + calculate_bittime(sq, buflen);
    double waketime;
    if (sq->serial_fd_type == SQT_USB_CDC)
        waketime = eventtime + sq->rto + SQT_USB_CDC_FRAME_TIME;
    else
        waketime = eventtime + sq->rto + calculate_bittime(sq, first_buflen);

    pthread_mutex_unlock(&sq->lock);
    return waketime;
}

static int build_and_send_command(struct serialqueue *sq, uint8_t *buf, int pending, double eventtime) //----2-G-G-2022-04-08-----------------
{
    int len = MESSAGE_HEADER_SIZE;
    while (sq->ready_bytes)
    {
        // Find highest priority message (message with lowest req_clock)
        // 查找最高的优先级消息
        uint64_t min_clock = MAX_CLOCK;
        struct command_queue *q, *cq = NULL;
        struct queue_message *qm = NULL;
        list_for_each_entry(q, &sq->pending_queues, node)
        {
            if (!list_empty(&q->ready_queue))
            {
                struct queue_message *m = list_first_entry(
                    &q->ready_queue, struct queue_message, node);
                if (m->req_clock < min_clock)
                {
                    min_clock = m->req_clock;
                    cq = q;
                    qm = m;
                }
            }
        }
        // Append message to outgoing command   将消息附加到传出命令
        if (len + qm->len > MESSAGE_MAX - MESSAGE_TRAILER_SIZE)
            break;
        list_del(&qm->node);
        if (list_empty(&cq->ready_queue) && list_empty(&cq->stalled_queue))
            list_del(&cq->node);
        memcpy(&buf[len], qm->msg, qm->len);
        len += qm->len;
        sq->ready_bytes -= qm->len;
        if (qm->notify_id)
        {
            // Message requires notification - add to notify list  消息需要通知 - 添加到通知列表
            qm->req_clock = sq->send_seq;
            list_add_tail(&qm->node, &sq->notify_queue);
        }
        else
        {
            message_free(qm);
        }
    }
    // Fill header / trailer
    len += MESSAGE_TRAILER_SIZE;
    buf[MESSAGE_POS_LEN] = len;
    buf[MESSAGE_POS_SEQ] = MESSAGE_DEST | (sq->send_seq & MESSAGE_SEQ_MASK);
    uint16_t crc = msgblock_crc16_ccitt(buf, len - MESSAGE_TRAILER_SIZE);
    buf[len - MESSAGE_TRAILER_CRC] = crc >> 8;
    buf[len - MESSAGE_TRAILER_CRC + 1] = crc & 0xff;
    buf[len - MESSAGE_TRAILER_SYNC] = MESSAGE_SYNC;
    // Store message block
    double idletime = eventtime > sq->idle_time ? eventtime : sq->idle_time;
    if (sq->serial_fd_type == SQT_USB_CDC)
        idletime += SQT_USB_CDC_FRAME_TIME;
    else
        idletime += calculate_bittime(sq, pending + len);
    struct queue_message *out = message_alloc();
    memcpy(out->msg, buf, len);
    out->len = len;
    out->sent_time = eventtime;
    out->receive_time = idletime;
    if (list_empty(&sq->sent_queue))
        pollreactor_update_timer(sq->pr, SQPT_RETRANSMIT, idletime + sq->rto);
    if (!sq->rtt_sample_seq)
        sq->rtt_sample_seq = sq->send_seq;
    sq->send_seq++;
    sq->need_ack_bytes += len;
    list_add_tail(&out->node, &sq->sent_queue);
    return len;
}

// Determine the time the next serial data should be sent  确定应发送下一个串行数据的时间
static double
check_send_command(struct serialqueue *sq, int pending, double eventtime)
{
    if (sq->send_seq - sq->receive_seq >= sq->max_pending_blocks && sq->receive_seq != (uint64_t)-1)
        // Need an ack before more messages can be sent
        return PR_NEVER;

    if (sq->send_seq > sq->receive_seq && sq->receive_window)
    {
        int need_ack_bytes = sq->need_ack_bytes + MESSAGE_MAX;
        if (sq->last_ack_seq < sq->receive_seq)
            need_ack_bytes += sq->last_ack_bytes;
        if (need_ack_bytes > sq->receive_window)
            // Wait for ack from past messages before sending next message
            return PR_NEVER;
    }
    // Check for stalled messages now ready
    double idletime = eventtime > sq->idle_time ? eventtime : sq->idle_time;
    if (sq->serial_fd_type == SQT_USB_CDC)
        idletime += SQT_USB_CDC_FRAME_TIME;
    else
        idletime += calculate_bittime(sq, pending + MESSAGE_MIN);
    uint64_t ack_clock = clock_from_time(&sq->ce, idletime);
    uint64_t min_stalled_clock = MAX_CLOCK, min_ready_clock = MAX_CLOCK;
    struct command_queue *cq;
    list_for_each_entry(cq, &sq->pending_queues, node)
    {
        // Move messages from the stalled_queue to the ready_queue
        while (!list_empty(&cq->stalled_queue))
        {
            struct queue_message *qm = list_first_entry(
                &cq->stalled_queue, struct queue_message, node);
            if (ack_clock < qm->min_clock)
            {
                if (qm->min_clock < min_stalled_clock)
                    min_stalled_clock = qm->min_clock;
                break;
            }
            list_del(&qm->node);
            list_add_tail(&qm->node, &cq->ready_queue);
            sq->stalled_bytes -= qm->len;
            sq->ready_bytes += qm->len;
        }
        // Update min_ready_clock
        if (!list_empty(&cq->ready_queue))
        {
            struct queue_message *qm = list_first_entry(
                &cq->ready_queue, struct queue_message, node);
            uint64_t req_clock = qm->req_clock;
            double bgtime = pending ? idletime : sq->idle_time;
            double bgoffset = MIN_REQTIME_DELTA + MIN_BACKGROUND_DELTA;
            if (req_clock == BACKGROUND_PRIORITY_CLOCK)
                req_clock = clock_from_time(&sq->ce, bgtime + bgoffset);
            if (req_clock < min_ready_clock)
                min_ready_clock = req_clock;
        }
    }

    // Check for messages to send  检查要发送的消息
    if (sq->ready_bytes >= MESSAGE_PAYLOAD_MAX)
        return PR_NOW;
    if (!sq->ce.est_freq)
    {
        if (sq->ready_bytes)
            return PR_NOW;
        sq->need_kick_clock = MAX_CLOCK;
        return PR_NEVER;
    }
    uint64_t reqclock_delta = MIN_REQTIME_DELTA * sq->ce.est_freq;
    if (min_ready_clock <= ack_clock + reqclock_delta)
        return PR_NOW;
    uint64_t wantclock = min_ready_clock - reqclock_delta;
    if (min_stalled_clock < wantclock)
        wantclock = min_stalled_clock;
    sq->need_kick_clock = wantclock;
    return idletime + (wantclock - ack_clock) / sq->ce.est_freq;
}

// Callback timer to send data to the serial port  回调计时器将数据发送到串行端口
static double command_event(struct serialqueue *sq, double eventtime)
{
    pthread_mutex_lock(&sq->lock);
    uint8_t buf[MESSAGE_MAX * sq->max_pending_blocks];
    int buflen = 0;
    double waketime;

    // 批量处理发送队列中的消息
    for (;;) {
        waketime = check_send_command(sq, buflen, eventtime);
        if (waketime != PR_NOW || buflen + MESSAGE_MAX > sizeof(buf)) {
            if (buflen) {
                // 使用单次写入发送所有数据
                do_write(sq, buf, buflen);
                sq->bytes_write += buflen;
                
                // 更新空闲时间,考虑USB-CDC帧时间
                double idletime = (eventtime > sq->idle_time ? 
                                 eventtime : sq->idle_time);
                if (sq->serial_fd_type == SQT_USB_CDC)
                    sq->idle_time = idletime + SQT_USB_CDC_FRAME_TIME; // 增加1ms USB帧时间
                else
                    sq->idle_time = idletime + calculate_bittime(sq, buflen);
                buflen = 0;
            }
            if (waketime != PR_NOW)
                break;
        }
        buflen += build_and_send_command(sq, &buf[buflen], buflen, eventtime);
    }

    pthread_mutex_unlock(&sq->lock);
    return waketime;
}

// Main background thread for reading/writing to serial port
static void *background_thread(void *data)
{
    struct serialqueue *sq = data;
    pollreactor_run(sq->pr);

    pthread_mutex_lock(&sq->lock);
    check_wake_receive(sq);
    pthread_mutex_unlock(&sq->lock);

    return NULL;
}

// Create a new 'struct serialqueue' object
struct serialqueue *__visible serialqueue_alloc(const char *port, void *params, char serial_fd_type, int client_id, int max_pending_blocks)
{
    struct serialqueue *sq = malloc(sizeof(*sq));
    memset(sq, 0, sizeof(*sq));
    sq->serial_fd = -1;
    sq->serial_fd_type = serial_fd_type;
    sq->client_id = client_id;
    sq->max_pending_blocks = max_pending_blocks;
    strncpy(sq->port, port, sizeof(sq->port));
    LOG_I("serialqueue_alloc port %s type %c\n", sq->port, sq->serial_fd_type);
    // 通讯总线
    sq->bus = malloc(sizeof(*sq->bus));
    memset(sq->bus, 0, sizeof(*sq->bus));
    // printf("serialqueue_alloc port %s type %c\n", port, serial_fd_type);

    int rc;
    int simple_retry = 0;
    int simple_delay = 100*1000;
    if (sq->serial_fd_type == SQT_RPBUF)
        combus_attach_drvier(sq->bus, &rpmsg_combus_ops);
    else if (sq->serial_fd_type == SQT_UART)
        combus_attach_drvier(sq->bus, &uart_combus_ops);
    else if (sq->serial_fd_type == SQT_MEM)
        combus_attach_drvier(sq->bus, &mem_combus_ops);
    else if (sq->serial_fd_type == SQT_USB_CDC)
        combus_attach_drvier(sq->bus, &uart_combus_ops);
    do
    {
        rc = combus_open(sq->bus, port, params);
        if (rc < 0) {
            usleep(simple_delay * simple_retry);
        }
    } while (rc != 0 && simple_retry++ < 10);
    if (simple_retry)
        LOG_W("simple_retry %d\n", simple_retry);
    if (simple_retry >= 10)
    {
        LOG_E("combus open %s failed.\n", port);
        return NULL;
    }

    LOG_I("combus open %s \n", port);
    sq->serial_fd = sq->bus->fd;

    int ret = pipe(sq->pipe_fds);
    if (ret)
        goto fail;

    sq->pr = pollreactor_alloc(SQPF_NUM, SQPT_NUM, sq);
    pollreactor_add_fd(sq->pr, SQPF_SERIAL, sq->serial_fd, input_event, sq->serial_fd_type == SQT_DEBUGFILE);

    // Reactor setup
    pollreactor_add_fd(sq->pr, SQPF_PIPE, sq->pipe_fds[0], kick_event, 0);
    pollreactor_add_timer(sq->pr, SQPT_RETRANSMIT, retransmit_event); //---3task-G-G--RW_task- //---send-G-G-2022-07-22---
    pollreactor_add_timer(sq->pr, SQPT_COMMAND, command_event);       //---send-G-G-2022-07-22---
    if (sq->serial_fd_type != SQT_MEM)
    {
        fd_set_non_blocking(sq->serial_fd);
    }
    fd_set_non_blocking(sq->pipe_fds[0]);
    fd_set_non_blocking(sq->pipe_fds[1]);

    // Retransmit setup
    if (sq->serial_fd_type == SQT_MEM)
    {
        sq->send_seq = 1;
    }
    else
    {
        sq->send_seq = 0;
    }
    if (sq->serial_fd_type == SQT_DEBUGFILE)
    {
        // Debug file output
        sq->receive_seq = -1;
        sq->rto = PR_NEVER;
    }
    else
    {
        sq->receive_seq = 1;
        sq->rto = MIN_RTO;
    }

    // Queues
    sq->need_kick_clock = MAX_CLOCK;
    list_init(&sq->pending_queues);
    list_init(&sq->sent_queue);
    list_init(&sq->receive_queue);
    list_init(&sq->notify_queue);
    list_init(&sq->fast_readers);

    // Debugging
#if DEBUG_QUEUE
    list_init(&sq->old_sent);
    list_init(&sq->old_receive);
    debug_queue_alloc(&sq->old_sent, DEBUG_QUEUE_SENT);
    debug_queue_alloc(&sq->old_receive, DEBUG_QUEUE_RECEIVE);
#endif

    // Thread setup
    ret = pthread_mutex_init(&sq->lock, NULL);
    if (ret)
        goto fail;
    ret = pthread_cond_init(&sq->cond, NULL);
    if (ret)
        goto fail;
    ret = pthread_mutex_init(&sq->fast_reader_dispatch_lock, NULL);
    if (ret)
        goto fail;

    pthread_attr_t attr;
    struct sched_param param;
    // Initialize thread attribute
    pthread_attr_init(&attr);
    // Set scheduling policy
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    // Set thread priority
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(&sq->tid, &attr, background_thread, sq);
    if (ret)
        goto fail;

    // USB-CDC特定配置
    if (sq->serial_fd_type == SQT_USB_CDC) {
        // USB设备特定配置
        sq->max_pending_blocks = 8;
        sq->receive_window = 4096;  // 增大接收窗口
        
        // 设置USB特定的传输延迟
        sq->bittime_adjust = SQT_USB_CDC_FRAME_TIME; // 1ms基准延迟
    }

    return sq;

fail:
    report_errno("init", ret);
    return NULL;
}

// Request that the background thread exit
void __visible
serialqueue_exit(struct serialqueue *sq)
{
    LOG_D("serialqueue_exit\n");
    pollreactor_do_exit(sq->pr);
    kick_bg_thread(sq);
    int ret = pthread_join(sq->tid, NULL);
    if (ret)
        report_errno("pthread_join", ret);
    combus_close(sq->bus);
}

// Free all resources associated with a serialqueue
void __visible
serialqueue_free(struct serialqueue *sq)
{
    if (!sq)
        return;
    if (!pollreactor_is_exit(sq->pr))
        serialqueue_exit(sq);
    pthread_mutex_lock(&sq->lock);
    message_queue_free(&sq->sent_queue);
    message_queue_free(&sq->receive_queue);
    message_queue_free(&sq->notify_queue);
#if DEBUG_QUEUE
    message_queue_free(&sq->old_sent);
    message_queue_free(&sq->old_receive);
#endif
    while (!list_empty(&sq->pending_queues))
    {
        struct command_queue *cq = list_first_entry(
            &sq->pending_queues, struct command_queue, node);
        list_del(&cq->node);
        message_queue_free(&cq->ready_queue);
        message_queue_free(&cq->stalled_queue);
    }
    pthread_mutex_unlock(&sq->lock);

    pollreactor_free(sq->pr);
    free(sq);
}

// Allocate a 'struct command_queue'
struct command_queue *__visible
serialqueue_alloc_commandqueue(void)
{
    struct command_queue *cq = malloc(sizeof(*cq));
    memset(cq, 0, sizeof(*cq));
    list_init(&cq->ready_queue);
    list_init(&cq->stalled_queue);
    return cq;
}

// Free a 'struct command_queue'
void __visible
serialqueue_free_commandqueue(struct command_queue *cq)
{
    if (!cq)
        return;
    if (!list_empty(&cq->ready_queue) || !list_empty(&cq->stalled_queue))
    {
        errorf("Memory leak! Can't free non-empty commandqueue");
        return;
    }
    free(cq);
}

// Add a low-latency message handler
void serialqueue_add_fastreader(struct serialqueue *sq, struct fastreader *fr)
{
    pthread_mutex_lock(&sq->lock);
    list_add_tail(&fr->node, &sq->fast_readers);
    pthread_mutex_unlock(&sq->lock);
}

// Remove a previously registered low-latency message handler
void serialqueue_rm_fastreader(struct serialqueue *sq, struct fastreader *fr)
{
    pthread_mutex_lock(&sq->lock);
    list_del(&fr->node);
    pthread_mutex_unlock(&sq->lock);

    pthread_mutex_lock(&sq->fast_reader_dispatch_lock); // XXX - goofy locking
    pthread_mutex_unlock(&sq->fast_reader_dispatch_lock);
}

// Add a batch of messages to the given command_queue
void serialqueue_send_batch(struct serialqueue *sq, struct command_queue *cq, struct list_head *msgs) //---29-2task-G-G--UI_control_task--        //-5-5task-G-G---get_clock---
{
    int len = 0;
    struct queue_message *qm;
    list_for_each_entry(qm, msgs, node)
    {
        if (qm->min_clock + (1LL << 31) < qm->req_clock && qm->req_clock != BACKGROUND_PRIORITY_CLOCK)
            qm->min_clock = qm->req_clock - (1LL << 31);
        len += qm->len;
    }
    if (!len)
        return;
    qm = list_first_entry(msgs, struct queue_message, node);
    pthread_mutex_lock(&sq->lock);
    if (list_empty(&cq->ready_queue) && list_empty(&cq->stalled_queue))
        list_add_tail(&cq->node, &sq->pending_queues);

    list_join_tail(msgs, &cq->stalled_queue);
    sq->stalled_bytes += len;
    int mustwake = 0;

    if (qm->min_clock < sq->need_kick_clock)
    {
        sq->need_kick_clock = 0;
        mustwake = 1;
    }
    pthread_mutex_unlock(&sq->lock);

    if (mustwake)
        kick_bg_thread(sq);
}

// Helper to send a single message
void serialqueue_send_one(struct serialqueue *sq, struct command_queue *cq, struct queue_message *qm)
{
    struct list_head msgs;
    list_init(&msgs);
    list_add_tail(&qm->node, &msgs);
    serialqueue_send_batch(sq, cq, &msgs);
}

// Schedule the transmission of a message on the serial port at a
// given time and priority.
void __visible
serialqueue_send(struct serialqueue *sq, struct command_queue *cq, uint8_t *msg, int len, uint64_t min_clock, uint64_t req_clock, uint64_t notify_id)
{
    struct queue_message *qm = message_fill(msg, len);
    qm->min_clock = min_clock;
    qm->req_clock = req_clock;
    qm->notify_id = notify_id;

    serialqueue_send_one(sq, cq, qm);
}

// Return a message read from the serial port (or wait for one if none
// available)
void __visible serialqueue_pull(struct serialqueue *sq, struct pull_queue_message *pqm)
{
    // LOG_D("serialqueue_pull\n");
    pthread_mutex_lock(&sq->lock);
    // Wait for message to be available
    while (list_empty(&sq->receive_queue))
    {
        if (pollreactor_is_exit(sq->pr))
            goto exit;
        sq->receive_waiting = 1;
        int ret = pthread_cond_wait(&sq->cond, &sq->lock);
        if (ret)
            report_errno("pthread_cond_wait", ret);
    }
    // Remove message from queue
    struct queue_message *qm = list_first_entry(
        &sq->receive_queue, struct queue_message, node);
    list_del(&qm->node);
    memcpy(pqm->msg, qm->msg, qm->len);
    pqm->len = qm->len;
    pqm->sent_time = qm->sent_time;
    pqm->receive_time = qm->receive_time;
    pqm->notify_id = qm->notify_id;
    if (qm->len)
    {
#if DEBUG_QUEUE
        debug_queue_add(&sq->old_receive, qm);
#endif
    }
    else
        message_free(qm);

    pthread_mutex_unlock(&sq->lock);

    return;

exit:
    LOG_D("serialqueue_pull exit\n");
    pqm->len = -1;
    pthread_mutex_unlock(&sq->lock);
}

void __visible
serialqueue_set_wire_frequency(struct serialqueue *sq, double frequency)
{
    // 设置调整后的速率
    pthread_mutex_lock(&sq->lock);
    if (sq->serial_fd_type == SQT_CAN)
    {
        sq->bittime_adjust = 1. / frequency;
    }
    else
    {
        // An 8N1 serial line is 10 bits per byte (1 start, 8 data, 1 stop)
        sq->bittime_adjust = 10. / frequency;
    }
    pthread_mutex_unlock(&sq->lock);
}

void __visible
serialqueue_set_receive_window(struct serialqueue *sq, int receive_window)
{
    pthread_mutex_lock(&sq->lock);
    sq->receive_window = receive_window;
    pthread_mutex_unlock(&sq->lock);
}

// Set the estimated clock rate of the mcu on the other end of the
// serial port
void __visible
serialqueue_set_clock_est(struct serialqueue *sq, double est_freq, double conv_time, uint64_t conv_clock, uint64_t last_clock)
{
    pthread_mutex_lock(&sq->lock);
    sq->ce.est_freq = est_freq;
    sq->ce.conv_time = conv_time;
    sq->ce.conv_clock = conv_clock;
    sq->ce.last_clock = last_clock;
    pthread_mutex_unlock(&sq->lock);
}

// Return the latest clock estimate
void serialqueue_get_clock_est(struct serialqueue *sq, struct clock_estimate *ce)
{
    pthread_mutex_lock(&sq->lock);
    memcpy(ce, &sq->ce, sizeof(sq->ce));
    pthread_mutex_unlock(&sq->lock);
}

// Return a string buffer containing statistics for the serial port
void __visible
serialqueue_get_stats(struct serialqueue *sq, char *buf, int len)
{
    struct serialqueue stats;
    pthread_mutex_lock(&sq->lock);
    memcpy(&stats, sq, sizeof(stats));
    pthread_mutex_unlock(&sq->lock);

    snprintf(buf, len, "bytes_write=%u bytes_read=%u"
                       " bytes_retransmit=%u bytes_invalid=%u"
                       " send_seq=%u receive_seq=%u retransmit_seq=%u"
                       " srtt=%.3f rttvar=%.3f rto=%.3f"
                       " ready_bytes=%u stalled_bytes=%u",
             stats.bytes_write, stats.bytes_read, stats.bytes_retransmit, stats.bytes_invalid, (int)stats.send_seq, (int)stats.receive_seq, (int)stats.retransmit_seq, stats.srtt, stats.rttvar, stats.rto, stats.ready_bytes, stats.stalled_bytes);
}

// Extract old messages stored in the debug queues
int __visible
serialqueue_extract_old(struct serialqueue *sq, int sentq, struct pull_queue_message *q, int max)
{
    int count = sentq ? DEBUG_QUEUE_SENT : DEBUG_QUEUE_RECEIVE;
    struct list_head *rootp = sentq ? &sq->old_sent : &sq->old_receive;
    struct list_head replacement, current;
    list_init(&replacement);
    debug_queue_alloc(&replacement, count);
    list_init(&current);

    // Atomically replace existing debug list with new zero'd list
    pthread_mutex_lock(&sq->lock);
    list_join_tail(rootp, &current);
    list_init(rootp);
    list_join_tail(&replacement, rootp);
    pthread_mutex_unlock(&sq->lock);

    // Walk the debug list
    int pos = 0;
    while (!list_empty(&current))
    {
        struct queue_message *qm = list_first_entry(
            &current, struct queue_message, node);
        if (qm->len && pos < max)
        {
            struct pull_queue_message *pqm = q++;
            pos++;
            memcpy(pqm->msg, qm->msg, qm->len);
            pqm->len = qm->len;
            pqm->sent_time = qm->sent_time;
            pqm->receive_time = qm->receive_time;
        }
        list_del(&qm->node);
        message_free(qm);
    }
    return pos;
}
