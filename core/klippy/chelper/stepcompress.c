// MCU_stepper pulse schedule compression  步进脉冲调度压缩
//
// Copyright (C) 2016-2021  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

// The goal of this code is to take a series of scheduled stepper
// pulse times and compress them into a handful of commands that can
// be efficiently transmitted and executed on a microcontroller (mcu).
// The mcu accepts step pulse commands that take interval, count, and
// add parameters such that 'count' pulses occur, with each step event
// calculating the next step event time using:
//  next_wake_time = last_wake_time + interval; interval += add
// This code is written in C (instead of python) for processing
// efficiency - the repetitive integer math is vastly faster in C.

#include <math.h> // sqrt
#include <stddef.h> // offsetof
#include <stdint.h> // uint32_t
#include <stdio.h> // fprintf
#include <stdlib.h> // malloc
#include <string.h> // memset
#include "compiler.h" // DIV_ROUND_UP
#include "pyhelper.h" // errorf
#include "serialqueue.h" // struct queue_message
#include "stepcompress.h" // stepcompress_alloc
#include "debug.h"
#define LOG_TAG "stepcompress"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define CHECK_LINES 1
#define QUEUE_START_SIZE 1024
#define LOG() //(printf("file: %s line: %d function: %s\n", __FILE__, __LINE__, __FUNCTION__))

struct stepcompress {
    // Buffer management  缓冲区管理
    uint32_t *queue, *queue_end, *queue_pos, *queue_next;       //空间头 空间尾 使用位置头 使用位置尾部  存储每一步的步进时间
    // Internal tracking  内部跟踪
    uint32_t max_error;             //25 us
    double mcu_time_offset, mcu_freq, last_step_print_time;     //上一个move 消息的最后一个脉冲发出时间
    // Message generation  消息生成
    uint64_t last_step_clock;           //上一个move 消息的最后一个脉冲发出时钟
    uint64_t old_step_clock;           //用于查找脉冲异常的BUG
    struct list_head msg_queue;      //reset_step_clock  set_next_step_dir  queue_step  //queue_pos->add_move(move)->msg_queue->msgs -> stalled_queue -> ready_queue
    uint32_t oid;
    int32_t queue_step_msgtag, set_next_step_dir_msgtag;
    int sdir, invert_sdir;          //电机当前方向
    // Step+dir+step filter  阶梯+目录+步长过滤器
    uint64_t next_step_clock;
    int next_step_dir;          //电机下一步方向
    // History tracking  历史记录跟踪
    int64_t last_position;      //上一个move 消息的最后一个脉冲发出后电机位置 单位: 步数
    struct list_head history_list;
};

struct step_move {
    uint32_t interval;
    uint16_t count;
    int16_t add;
};

#define HISTORY_EXPIRE (30.0)

struct history_steps {
    struct list_node node;
    uint64_t first_clock, last_clock;
    int64_t start_position;
    int step_count, interval, add;
};


/****************************************************************
 * Step compression
 ****************************************************************/

static inline int32_t
idiv_up(int32_t n, int32_t d)
{
    //LOG();
    return (n>=0) ? DIV_ROUND_UP(n,d) : (n/d);
}

static inline int32_t
idiv_down(int32_t n, int32_t d)
{
    //LOG();
    return (n>=0) ? (n/d) : (n - d + 1) / d;
}

struct points {
    int32_t minp, maxp;
};

// Given a requested step time, return the minimum and maximum
// acceptable times
//给定请求的步骤时间，返回最小值和最大值
//可接受的时间
static inline struct points
minmax_point(struct stepcompress *sc, uint32_t *pos)
{
    uint32_t lsc = sc->last_step_clock, point = *pos - lsc;
    uint32_t prevpoint = pos > sc->queue_pos ? *(pos-1) - lsc : 0;
    uint32_t max_error = (point - prevpoint) / 2;
    if (max_error > sc->max_error)
        max_error = sc->max_error;
    return (struct points){ point - max_error, point };
}

// The maximum add delta between two valid quadratic sequences of the
// form "add*count*(count-1)/2 + interval*count" is "(6 + 4*sqrt(2)) *
// maxerror / (count*count)".  The "6 + 4*sqrt(2)" is 11.65685, but
// using 11 works well in practice.

#define QUADRATIC_DEV 11

// Find a 'step_move' that covers a series of step times  查找涵盖一系列步长时间的"step_move"
static struct step_move compress_bisect_add(struct stepcompress *sc)     //--14-home-2task-G-G--UI_control_task--    //---move-2task-G-G--UI_control_task-- 
{
    // GAM_DEBUG_send_UI("2-116-\n" );
    uint32_t *qlast = sc->queue_next;
    if (qlast > sc->queue_pos + 65535)      //最大65535步
        qlast = sc->queue_pos + 65535;
    struct points point = minmax_point(sc, sc->queue_pos);
    int32_t outer_mininterval = point.minp, outer_maxinterval = point.maxp;
    int32_t add = 0, minadd = -0x8000, maxadd = 0x7fff;
    int32_t bestinterval = 0, bestcount = 1, bestadd = 1, bestreach = INT32_MIN;
    int32_t zerointerval = 0, zerocount = 0;
    int32_t reach, interval;

    for (;;) {
        // Find longest valid sequence with the given 'add'  使用给定的"add"查找最长的有效序列
        struct points nextpoint;
        int32_t nextmininterval = outer_mininterval;
        int32_t nextmaxinterval = outer_maxinterval, 
        interval = nextmaxinterval;
        int32_t nextcount = 1;
        
        for (;;) {
            nextcount++;
            if (&sc->queue_pos[nextcount-1] >= qlast) {
                int32_t count = nextcount - 1;
                return (struct step_move){ interval, count, add };
            }
            nextpoint = minmax_point(sc, sc->queue_pos + nextcount - 1);
            int32_t nextaddfactor = nextcount*(nextcount-1)/2;
            int32_t c = add*nextaddfactor;
            if (nextmininterval*nextcount < nextpoint.minp - c)
                nextmininterval = idiv_up(nextpoint.minp - c, nextcount);           //减速 nextmininterval  变大
            if (nextmaxinterval*nextcount > nextpoint.maxp - c)
                nextmaxinterval = idiv_down(nextpoint.maxp - c, nextcount);         //加速 nextmaxinterval  变小
            if (nextmininterval > nextmaxinterval)
                break;
            interval = nextmaxinterval;
        }

        // Check if this is the best sequence found so far  检查这是否是迄今为止找到的最佳序列
        int32_t count = nextcount - 1, addfactor = count*(count-1)/2;
        reach = add*addfactor + interval*count;         //总间隔=梯形面积=三角形面积+矩形面积
        if (reach > bestreach  || (reach == bestreach && interval > bestinterval))      //
        {
            bestinterval = interval;
            bestcount = count;
            bestadd = add;
            bestreach = reach;
            if (!add) {
                zerointerval = interval;
                zerocount = count;
            }
            if (count > 0x200) // No 'add' will improve sequence; avoid integer overflow  没有"添加"会改善序列;避免整数溢出
                break;
        }

        // Check if a greater or lesser add could extend the sequence  检查更大或更少的添加是否可以扩展序列
        int32_t nextaddfactor = nextcount*(nextcount-1)/2;
        int32_t nextreach = add*nextaddfactor + interval*nextcount;
        if (nextreach < nextpoint.minp) {           //减速
            minadd = add + 1;
            outer_maxinterval = nextmaxinterval;
        } else {
            maxadd = add - 1;
            outer_mininterval = nextmininterval;
        }

        // The maximum valid deviation between two quadratic sequences
        // can be calculated and used to further limit the add range.
        //两个二次序列之间的最大有效偏差
        //可以计算并用于进一步限制添加范围。
        if (count > 1) {
            int32_t errdelta = sc->max_error*QUADRATIC_DEV / (count*count);
            if (minadd < add - errdelta)
                minadd = add - errdelta;
            if (maxadd > add + errdelta)
                maxadd = add + errdelta;
        }

        // See if next point would further limit the add range  看看下一点是否会进一步限制添加范围
        int32_t c = outer_maxinterval * nextcount;
        if (minadd*nextaddfactor < nextpoint.minp - c)
            minadd = idiv_up(nextpoint.minp - c, nextaddfactor);
        c = outer_mininterval * nextcount;
        if (maxadd*nextaddfactor > nextpoint.maxp - c)
            maxadd = idiv_down(nextpoint.maxp - c, nextaddfactor);

        // Bisect valid add range and try again with new 'add'  将有效添加范围一分为二，然后使用新的"添加"重试
        if (minadd > maxadd)
            break;
        add = maxadd - (maxadd - minadd) / 4;
    }
    if (zerocount + zerocount/16 >= bestcount)      //zerointerval 可能为0
    {
        if (zerointerval==0)
        {
            LOG_E("zerointerval:%d, zerocount:%d, bestcount:%d, reach:%d, add:%d, interval:%d\n",
            zerointerval,zerocount,bestcount,reach,add,interval);
        }
        return (struct step_move){ zerointerval, zerocount, 0 };        // Prefer add=0 if it's similar to the best found sequence  如果 add=0 与最佳找到的序列相似，则首选 add=0
    }
    return (struct step_move){ bestinterval, bestcount, bestadd };
}


/****************************************************************
 * Step compress checking
 ****************************************************************/

static void print_stepcompress_info(struct stepcompress *sc)
{
    LOG_E("----------------------------------\n");
    LOG_E("stepcompress o=%d last_step_clock=%llu next_step_clock=%llu\n", sc->oid, sc->last_step_clock, sc->next_step_clock);
    LOG_E("stepcompress queue_pos=%p queue_next=%p queue_end=%p\n", sc->queue_pos, sc->queue_next, sc->queue_end);
    LOG_E("stepcompress queue_step_msgtag=%d invert_sdir=%d max_error=%d\n", sc->queue_step_msgtag, sc->invert_sdir, sc->max_error);
    LOG_E("stepcompress sdir=%d queue_step_msgtag=%d set_next_step_dir_msgtag=%d\n", sc->sdir, sc->queue_step_msgtag, sc->set_next_step_dir_msgtag);
    LOG_E("stepcompress freq=%f offset=%f\n", sc->mcu_freq, sc->mcu_time_offset);
    LOG_E("stepcompress last_step_print_time=%f last_position=%d\n", sc->last_step_print_time, sc->last_position);
    // LOG_E("stepcompress m_print_time=%f\n", sc->m);
    LOG_E("----------------------------------\n");
}

// Verify that a given 'step_move' matches the actual step times  验证给定的"step_move"是否与实际步长时间匹配
static int check_line(struct stepcompress *sc, struct step_move move)      //---home-2task-G-G--UI_control_task-//---move-2task-G-G--UI_control_task--
{
//    GAM_DEBUG_send_UI("2-218-\n" );
    if (!CHECK_LINES)
        return 0;
    if (!move.count || (!move.interval && !move.add && move.count > 1) || move.interval >= 0x80000000) {
        LOG_E("stepcompress1 o=%d i=%x c=%x a=%d: Invalid sequence\n", sc->oid, move.interval, move.count, move.add);
        print_stepcompress_info(sc);
        return -1;
    }
    uint32_t interval = move.interval, p = 0;
    uint16_t i;
    for (i=0; i<move.count; i++) {
        struct points point = minmax_point(sc, sc->queue_pos + i);
        p += interval;
        if (p < point.minp || p > point.maxp) {
            LOG_E("stepcompress2 o=%d i=%x c=%x a=%d: Point %d: %d not in %d:%d\n", sc->oid, move.interval, move.count, move.add , i+1, p, point.minp, point.maxp);
            print_stepcompress_info(sc);
            return -2;
        }
        if (interval >= 0x80000000) {
            LOG_E("stepcompress3 o=%d i=%x c=%x a=%d:" " Point %d: interval overflow %x\n", sc->oid, move.interval, move.count, move.add, i+1, interval);
            print_stepcompress_info(sc);
            return ERROR_RET;
        }
        interval += move.add;
    }
    return 0;
}


/****************************************************************
 * Step compress interface
 ****************************************************************/

// Allocate a new 'stepcompress' object  分配新的"逐步压缩"对象
struct stepcompress * __visible 
stepcompress_alloc(uint32_t oid)
{
    struct stepcompress *sc = malloc(sizeof(*sc));
    memset(sc, 0, sizeof(*sc));
    list_init(&sc->msg_queue);
    list_init(&sc->history_list);
    sc->oid = oid;
    sc->sdir = -1;
    return sc;
}

// Fill message id information  填写邮件 ID 信息
void __visible 
stepcompress_fill(struct stepcompress *sc, uint32_t max_error
                  , uint32_t invert_sdir, int32_t queue_step_msgtag
                  , int32_t set_next_step_dir_msgtag)
{
    sc->max_error = max_error;
    sc->invert_sdir = !!invert_sdir;
    sc->queue_step_msgtag = queue_step_msgtag;
    sc->set_next_step_dir_msgtag = set_next_step_dir_msgtag;
}

// Set the inverted stepper direction flag
void __visible
stepcompress_set_invert_sdir(struct stepcompress *sc, uint32_t invert_sdir)
{
    invert_sdir = !!invert_sdir;
    if (invert_sdir != sc->invert_sdir) {
        sc->invert_sdir = invert_sdir;
        if (sc->sdir >= 0)
            sc->sdir ^= 1;
    }
}

// Helper to free items from the history_list  从history_list中释放项目的助手
static void
free_history(struct stepcompress *sc, uint64_t end_clock)
{
    while (!list_empty(&sc->history_list)) {
        struct history_steps *hs = list_last_entry(
            &sc->history_list, struct history_steps, node);
        if (hs->last_clock > end_clock)
            break;
        list_del(&hs->node);
        free(hs);
    }
}

// Free memory associated with a 'stepcompress' object  与"步进压缩"对象关联的可用内存
void __visible 
stepcompress_free(struct stepcompress *sc)
{
    if (!sc)
        return;
    free(sc->queue);
    message_queue_free(&sc->msg_queue);
    free_history(sc, UINT64_MAX);
    free(sc);
}

uint32_t 
stepcompress_get_oid(struct stepcompress *sc)
{
    return sc->oid;
}

int 
stepcompress_get_step_dir(struct stepcompress *sc)
{
    return sc->next_step_dir;
}

// Determine the "print time" of the last_step_clock  确定last_step_clock的"打印时间"
static void 
calc_last_step_print_time(struct stepcompress *sc)    //---home-2task-G-G--UI_control_task-//---move-2task-G-G--UI_control_task--
{
//    GAM_DEBUG_send_UI("2-321-\n" );
    double lsc = sc->last_step_clock;
    sc->last_step_print_time = sc->mcu_time_offset + (lsc - .5) / sc->mcu_freq;

    if (lsc > sc->mcu_freq * HISTORY_EXPIRE)
        free_history(sc, lsc - sc->mcu_freq * HISTORY_EXPIRE);
}

// Set the conversion rate of 'print_time' to mcu clock  设置"print_time"到mcu时钟的转换率
static void 
stepcompress_set_time(struct stepcompress *sc   , double time_offset, double mcu_freq)
{
    sc->mcu_time_offset = time_offset;
    sc->mcu_freq = mcu_freq;
    calc_last_step_print_time(sc);
}

// Maximium clock delta between messages in the queue  队列中消息之间的最大时钟增量
#define CLOCK_DIFF_MAX (3<<28)
void GAM_printf_sendMSG(uint8_t *buf,   uint32_t len,   uint8_t msg_id);
//  帮助从"结构step_move"创建queue_step命令
static void add_move(struct stepcompress *sc, uint64_t first_clock, struct step_move *move)         //---20-add_move-G-G-每0.05s至少冲刷一次  DRIP_SEGMENT_TIME
{
    int32_t addfactor = move->count*(move->count-1)/2;
    uint32_t ticks = move->add*addfactor + move->interval*(move->count-1);
    uint64_t last_clock = first_clock + ticks;

    // Create and queue a queue_step command
    uint32_t msg[5] = {
        sc->queue_step_msgtag, sc->oid, move->interval, move->count, move->add
    };
    struct queue_message *qm = message_alloc_and_encode(msg, 5);
    qm->min_clock = qm->req_clock = sc->last_step_clock;
    if (move->count == 1 && first_clock >= sc->last_step_clock + CLOCK_DIFF_MAX)
        qm->req_clock = first_clock;
    list_add_tail(&qm->node, &sc->msg_queue);                    //---20-add_move-G-G---
    sc->last_step_clock = last_clock;
    // Create and store move in history tracking
    struct history_steps *hs = malloc(sizeof(*hs));
    hs->first_clock = first_clock;
    hs->last_clock = last_clock;
    hs->start_position = sc->last_position;
    hs->interval = move->interval;
    hs->add = move->add;
    hs->step_count = sc->sdir ? move->count : -move->count;
    sc->last_position += hs->step_count;
    list_add_head(&hs->node, &sc->history_list);
}
int gi_errer = 0;
static void err_move(struct stepcompress *sc, uint64_t move_clock, struct step_move *move)         //---20-add_move-G-G---
{
    // gi_errer = -1;
    GAM_DEBUG_printf( "queue_flush interval:%x add:%d  count:%x move_clock:%llx  \n" ,move->interval ,move->add  ,move->count,move_clock  );
    GAM_DEBUG_printf( "err_move last_step_clock:%llx next_step_clock:%llx count:%d   \n" ,sc->last_step_clock ,sc->next_step_clock ,move->count  );
    GAM_DEBUG_printf( "err_move queue:%p queue_pos:%p queue_next:%p queue_end:%p   \n" ,sc->queue ,sc->queue_pos ,sc->queue_next,sc->queue_end  );
    uint32_t *queue;
    for(queue= sc->queue;(queue+15)<sc->queue_end;queue+=16)
    {
        GAM_DEBUG_printf( "%p: %8x %8x %8x %8x   %8x %8x %8x %8x        %8x %8x %8x %8x   %8x %8x %8x %8x\n" ,queue,*queue, *(queue+1), *(queue+2), *(queue+3), *(queue+4), 
        *(queue+5), *(queue+6), *(queue+7), *(queue+8), *(queue+9), *(queue+10), *(queue+11), *(queue+12), *(queue+13), *(queue+14), *(queue+15) );
    }
    for(;(queue)<sc->queue_end;queue++)
    {
        GAM_DEBUG_printf( "%d " ,*queue  );
    }
    GAM_DEBUG_printf( "\n"  );
}
// Convert previously scheduled steps into commands for the mcu  将以前计划的步骤转换为 mcu 的命令
static int 
queue_flush(struct stepcompress *sc, uint64_t move_clock)       //把已经生成的步数生成move命令发出去--12-home-2task-G-G--UI_control_task--      //--16-move-2task-G-G--UI_control_task--
{
    // GAM_DEBUG_send_UI("2-381-\n" );
    
    GAM_DEBUG_send_UI_home("2-H10-H12-H14-H16-\n" );
    if (sc->queue_pos >= sc->queue_next)
        return 0;
    while (sc->last_step_clock < move_clock) {
        struct step_move move = compress_bisect_add(sc);   //--9-move_generateSteps---
        int ret = check_line(sc, move);   //--10-move_generateSteps---
        if (ret)
        {
            // err_move(sc, move_clock, &move);       //打印错误数据，后期稳定后屏蔽
            sc->queue_pos += move.count;
            if (sc->queue_pos >= sc->queue_next) {
                sc->queue_pos = sc->queue_next = sc->queue;
            }
            return ret;
        }
        add_move(sc, sc->last_step_clock + move.interval, &move);          //---19-add_move-G-G---

        if (sc->queue_pos + move.count >= sc->queue_next) {
            sc->queue_pos = sc->queue_next = sc->queue;
            break;
        }
        sc->queue_pos += move.count;
    }
    calc_last_step_print_time(sc);
    return 0;
}

// Generate a queue_step for a step far in the future from the last step  为距离上一步更远的未来某个步骤生成queue_step
static int 
stepcompress_flush_far(struct stepcompress *sc, uint64_t abs_step_clock)
{
    struct step_move move = { abs_step_clock - sc->last_step_clock, 1, 0 };
    add_move(sc, abs_step_clock, &move);
    calc_last_step_print_time(sc);
    return 0;
}

// Send the  command  发送set_next_step_dir命令
static int 
set_next_step_dir(struct stepcompress *sc, int sdir)      //--16-home-2task-G-G--UI_control_task--           //--17-move-2task-G-G--UI_control_task--
{
    if (sc->sdir == sdir)
        return 0;
    int ret = queue_flush(sc, UINT64_MAX);
    if (ret)
    {
        return ret;
    }
    sc->sdir = sdir;
    uint32_t msg[3] = {     //"set_next_step_dir oid=%c dir=%c";
        sc->set_next_step_dir_msgtag, sc->oid, sdir ^ sc->invert_sdir
    };
    struct queue_message *qm = message_alloc_and_encode(msg, 3);   
    qm->req_clock = sc->last_step_clock;
    list_add_tail(&qm->node, &sc->msg_queue);      //---23-2task-G-G--UI_control_task--
    return 0;
}

// Slow path for queue_append() - handle next step far in future  queue_append（） 的慢速路径 - 在遥远的未来处理下一步
static int 
queue_append_far(struct stepcompress *sc)      //--17-home-2task-G-G--UI_control_task--           //--18-move-2task-G-G--UI_control_task--
{
    // GAM_DEBUG_send_UI("2-435-\n" );
    uint64_t step_clock = sc->next_step_clock;
    sc->next_step_clock = 0;
    int ret = queue_flush(sc, step_clock - CLOCK_DIFF_MAX + 1);          //---17-add_move-G-G---
    if (ret)
    {
        LOG_D("queue_append_far ret:%d\n", ret);
        return ret;
    }

    if (step_clock >= sc->last_step_clock + CLOCK_DIFF_MAX)
    {
        return stepcompress_flush_far(sc, step_clock);
    }
    *sc->queue_next++ = step_clock;
    return 0;
}

// Slow path for queue_append() - expand the internal queue storage  queue_append（） 的慢速路径 - 扩展内部队列存储
static int queue_append_extend(struct stepcompress *sc)
{
    if (sc->queue_next - sc->queue_pos > 65535 + 2000) {
        // No point in keeping more than 64K steps in memory
        uint32_t flush = (*(sc->queue_next-65535)  - (uint32_t)sc->last_step_clock);
        int ret = queue_flush(sc, sc->last_step_clock + flush);             //---18-add_move-G-G---
        if (ret)
        {
            return ret;
        }
    }

    if (sc->queue_next >= sc->queue_end) {
        // Make room in the queue
        int in_use = sc->queue_next - sc->queue_pos;
        if (sc->queue_pos > sc->queue) {
            // Shuffle the internal queue to avoid having to allocate more ram
            memmove(sc->queue, sc->queue_pos, in_use * sizeof(*sc->queue));
        } else {
            // Expand the internal queue of step times
            int alloc = sc->queue_end - sc->queue;
            if (!alloc)
                alloc = QUEUE_START_SIZE;
            while (in_use >= alloc)
                alloc *= 2;
            sc->queue = realloc(sc->queue, alloc * sizeof(*sc->queue));     //根据需要分配存储空间
            sc->queue_end = sc->queue + alloc;
        }
        sc->queue_pos = sc->queue;
        sc->queue_next = sc->queue + in_use;
    }

    *sc->queue_next++ = sc->next_step_clock;
    sc->next_step_clock = 0;
    return 0;
}

// Add a step time to the queue (flushing the queue if needed)  向队列添加步骤时间（如果需要，请刷新队列）
static int queue_append(struct stepcompress *sc)      //----12-M-G-G-2022-04-08-----------------
{
    if (unlikely(sc->next_step_dir != sc->sdir)) {
        int ret = set_next_step_dir(sc, sc->next_step_dir);         //改变方向直接生成move命令 和方向指令  
        if (ret)
        {
            LOG_E("queue_append ret:%d\n", ret);
            return ret;
        }
    }
    if (unlikely(sc->next_step_clock >= sc->last_step_clock + CLOCK_DIFF_MAX))
    {
        return queue_append_far(sc);            //很久没有运动 第一次运动直接生成move
    }
    if (unlikely(sc->queue_next >= sc->queue_end))
    {
        return queue_append_extend(sc);         //分配内存或直接生成move
    }
    *sc->queue_next++ = sc->next_step_clock;
    sc->next_step_clock = 0;
    return 0;
}

#define SDS_FILTER_TIME .000750         //平衡 打印细节和拐角失步

// Add next step time  添加下一步时间
int stepcompress_append(struct stepcompress *sc, int sdir   , double print_time, double step_time)   //--新的一步 步进起始计时时间-步进间隔----
{                //----11-G-G-2022-04-08-----------------------------
    // Calculate step clock

    double offset = print_time - sc->last_step_print_time;
    if (offset < 0) {
        // LOG_E("Time reversal detected: print_time=%f, last_print_time=%f, offset=%f",
            //   print_time, sc->last_step_print_time, offset);
        // return -1;  // 或者使用其他错误处理策略
    }
    double rel_sc = (step_time + offset) * sc->mcu_freq;

    // 1. 检查是否为NaN
    if (isnan(rel_sc)) {
        LOG_E("rel_sc is NaN: step_time=%f, offset=%f, mcu_freq=%f\n",
              step_time, offset, sc->mcu_freq);
        // return -1;
    }
    
    // 2. 检查是否为负数或过大
    if (rel_sc < -0.000001) {  // 使用epsilon值
        LOG_E("rel_sc is negative: ;rel_rc:%f, step_time:%f, offset:%f, print_time:%f, last_step_print_time:5f\n",
         rel_sc,step_time,offset,print_time,sc->last_step_print_time);
        // return -1;
    } else if (rel_sc > (double)UINT64_MAX) {
        LOG_E("rel_sc too large: rel_rc:%f, step_time:%f, offset:%f, print_time:%f, last_step_print_time:5f\n",
         rel_sc,step_time,offset,print_time,sc->last_step_print_time);
        // return -1;
    }

    // 3. 安全转换（包含四舍五入）
    uint64_t rel_sc_int = (uint64_t)(rel_sc + 0.5);

    // 4. 检查加法溢出
    if (UINT64_MAX - sc->last_step_clock < rel_sc_int) {
        LOG_E("step_clock would overflow: last=%llu, add=%llu\n",
              sc->last_step_clock, rel_sc_int);
        // return -1;
    }

    uint64_t step_clock = sc->last_step_clock + rel_sc_int;
    if (step_clock <= sc->last_step_clock) {
        LOG_E("Invalid step clock calculation: new=%llu, last=%llu, rel_sc=%f\n",
              step_clock, sc->last_step_clock, rel_sc);
        // return -1;
    }

    if(  isnan(print_time))
    {
        // gi_errer = -2;
        return 0;
    }
    // Flush previous pending step (if any)
    if (sc->next_step_clock) {
        if (unlikely(sdir != sc->next_step_dir))    //换向
        {
            double diff = (int64_t)(step_clock - sc->next_step_clock);
            if (diff < SDS_FILTER_TIME * sc->mcu_freq)      //750us以内换向  后退最后一步 避免出现快速换向  前进一步->换向->后退一步 => 直接换向    相当于少走一步尖角细节没有了
            {
                // Rollback last step to avoid rapid step+dir+step
                sc->old_step_clock = 0;
                sc->next_step_clock = 0;
                sc->next_step_dir = sdir;
                return 0;
            }
        }

        if(sc->next_step_clock == step_clock )   //把错误先打印出来
        {
            GAM_DEBUG_printf(" :%llx s: %f o:%f\n",step_clock,step_time,offset);
        }

        sc->old_step_clock = sc->next_step_clock;
        int ret = queue_append(sc);         //---15-add_move-G-G---
        if (ret)
        {
            LOG_D("stepcompress_append ret:%d\n", ret);
            return ret;
        }
    }
    // Store this step as the next pending step
    sc->next_step_clock = step_clock;
    sc->next_step_dir = sdir;
    return 0;
}

 // Commit next pending step (ie, do not allow a rollback)  提交下一个挂起的步骤（即，不允许回滚）
int 
stepcompress_commit(struct stepcompress *sc)
{
    // GAM_DEBUG_send_UI("2-527-\n" );
    if (sc->next_step_clock)
        return queue_append(sc);
    return 0;
}

// Flush pending steps  刷新挂起的步骤
static int stepcompress_flush(struct stepcompress *sc, uint64_t move_clock)     //--10-home-2task-G-G--UI_control_task--       //--27-move-2task-G-G--UI_control_task--
{
    if (sc->next_step_clock && move_clock >= sc->next_step_clock) {
        int ret = queue_append(sc);        //--11-home-2task-G-G--UI_control_task-- //---28-2task-G-G--UI_control_task--
        if (ret)
        {
            // GAM_DEBUG_printf( "stepcompress_flush queue_appen ret:%d\n" ,ret );
            return ret;
        }
    }
    return queue_flush(sc, move_clock);         //--12-home-2task-G-G--UI_control_task--每0.05s至少调用 queue_flush 冲刷一次
}

// Reset the internal state of the stepcompress object  重置步进压缩对象的内部状态
int __visible
stepcompress_reset(struct stepcompress *sc, uint64_t last_step_clock)
{
    int ret = stepcompress_flush(sc, UINT64_MAX);
    if (ret)
        return ret;
    sc->last_step_clock = last_step_clock;
    sc->sdir = -1;
    calc_last_step_print_time(sc);
    return 0;
}

// Set last_position in the stepcompress object  在步进压缩对象中设置last_position
int __visible
stepcompress_set_last_position(struct stepcompress *sc, int64_t last_position)
{
    //LOG();
    int ret = stepcompress_flush(sc, UINT64_MAX);
    if (ret)
        return ret;
    sc->last_position = last_position;
    return 0;
}

// Search history of moves to find a past position at a given clock  搜索移动历史记录以查找给定时钟的过去位置
int64_t __visible
stepcompress_find_past_position(struct stepcompress *sc, uint64_t clock)
{
    int64_t last_position = sc->last_position;
    struct history_steps *hs;
    list_for_each_entry(hs, &sc->history_list, node) {
        if (clock < hs->first_clock) {
            last_position = hs->start_position;
            continue;
        }
        if (clock >= hs->last_clock)
            return hs->start_position + hs->step_count;
        int32_t interval = hs->interval, add = hs->add;
        int32_t ticks = (int32_t)(clock - hs->first_clock) + interval, offset;
        if (!add) {
            offset = ticks / interval;
        } else {
            // Solve for "count" using quadratic formula
            double a = .5 * add, b = interval - .5 * add, c = -ticks;
            offset = (sqrt(b*b - 4*a*c) - b) / (2. * a);
        }
        if (hs->step_count < 0)
            return hs->start_position - offset;
        return hs->start_position + offset;
    }
    return last_position;
}

// Queue an mcu command to go out in order with stepper commands  将 mcu 命令排入队列，以便使用步进器命令按顺序发出
int __visible 
stepcompress_queue_msg(struct stepcompress *sc, uint32_t *data, int len)  //---home-2task-G-G--UI_control_task--    
{
    GAM_DEBUG_send_UI_home("2-623-\n" );
    int ret = stepcompress_flush(sc, UINT64_MAX);
    if (ret)
        return ret;

    struct queue_message *qm = message_alloc_and_encode(data, len);  //"reset_step_clock"
    qm->req_clock = sc->last_step_clock;
    list_add_tail(&qm->node, &sc->msg_queue);      //---23-2task-G-G--UI_control_task--
    return 0;
}

// Return history of queue_step commands
int __visible
stepcompress_extract_old(struct stepcompress *sc, struct pull_history_steps *p
                         , int max, uint64_t start_clock, uint64_t end_clock)
{
    int res = 0;
    struct history_steps *hs;
    list_for_each_entry(hs, &sc->history_list, node) {
        if (start_clock >= hs->last_clock || res >= max)
            break;
        if (end_clock <= hs->first_clock)
            continue;
        p->first_clock = hs->first_clock;
        p->last_clock = hs->last_clock;
        p->start_position = hs->start_position;
        p->step_count = hs->step_count;
        p->interval = hs->interval;
        p->add = hs->add;
        p++;
        res++;
    }
    return res;
}


/****************************************************************
 * Step compress synchronization
 ****************************************************************/

// The steppersync object is used to synchronize the output of mcu
// step commands.  The mcu can only queue a limited number of step
// commands - this code tracks when items on the mcu step queue become
// free so that new commands can be transmitted.  It also ensures the
// mcu step queue is ordered between steppers so that no stepper
// starves the other steppers of space in the mcu step queue.

struct steppersync {
    // Serial port
    struct serialqueue *sq;
    struct command_queue *cq;
    // Storage for associated stepcompress objects
    struct stepcompress **sc_list;
    int sc_num;
    // Storage for list of pending move clocks
    uint64_t *move_clocks;
    int num_move_clocks;
};

// Allocate a new 'steppersync' object  分配新的"步进同步"对象
struct steppersync * __visible
steppersync_alloc(struct serialqueue *sq, struct stepcompress **sc_list
                  , int sc_num, int move_num)
{
    //LOG();
    struct steppersync *ss = malloc(sizeof(*ss));
    memset(ss, 0, sizeof(*ss));
    ss->sq = sq;
    ss->cq = serialqueue_alloc_commandqueue();

    ss->sc_list = malloc(sizeof(*sc_list)*sc_num);
    memcpy(ss->sc_list, sc_list, sizeof(*sc_list)*sc_num);
    ss->sc_num = sc_num;

    ss->move_clocks = malloc(sizeof(*ss->move_clocks)*move_num);
    memset(ss->move_clocks, 0, sizeof(*ss->move_clocks)*move_num);
    ss->num_move_clocks = move_num;

    return ss;
}

// Free memory associated with a 'steppersync' object  与"步进同步"对象关联的可用内存
void __visible
steppersync_free(struct steppersync *ss)
{
    //LOG();
    if (!ss)
        return;
    free(ss->sc_list);
    free(ss->move_clocks);
    serialqueue_free_commandqueue(ss->cq);
    free(ss);
}

// Set the conversion rate of 'print_time' to mcu clock  设置"print_time"到mcu时钟的转换率
void __visible
steppersync_set_time(struct steppersync *ss, double time_offset , double mcu_freq)
{
    int i;
    for (i=0; i<ss->sc_num; i++) {
        struct stepcompress *sc = ss->sc_list[i];
        stepcompress_set_time(sc, time_offset, mcu_freq);
    }
}

// Implement a binary heap algorithm to track when the next available
// 'struct move' in the mcu will be available
//实现二进制堆算法以跟踪下一个可用时间
//mcu中的"结构移动"将可用
static void
heap_replace(struct steppersync *ss, uint64_t req_clock)
{
    //LOG();
    uint64_t *mc = ss->move_clocks;
    int nmc = ss->num_move_clocks, pos = 0;
    // printf("nmc %d\n ", nmc);
    for (;;) {
        int child1_pos = 2*pos+1, child2_pos = 2*pos+2;
        uint64_t child2_clock = child2_pos < nmc ? mc[child2_pos] : UINT64_MAX;
        uint64_t child1_clock = child1_pos < nmc ? mc[child1_pos] : UINT64_MAX;
        if (req_clock <= child1_clock && req_clock <= child2_clock) {
            mc[pos] = req_clock;
            break;
        }
        if (child1_clock < child2_clock) {
            mc[pos] = child1_clock;
            pos = child1_pos;
        } else {
            mc[pos] = child2_clock;
            pos = child2_pos;
        }
    }
}

// Find and transmit any scheduled steps prior to the given 'move_clock'  在给定的"move_clock"之前查找并传输任何计划的步骤
int __visible steppersync_flush(struct steppersync *ss, uint64_t move_clock)        //--9-home-2task-G-G--UI_control_task--       //--26-move-2task-G-G--UI_control_task-----msg_queue->msgs
{
    int i;
    for (i=0; i<ss->sc_num; i++) {          //所有电机
        int ret = stepcompress_flush(ss->sc_list[i], move_clock);        //--10-home-2task-G-G--UI_control_task--   //---27-2task-G-G--UI_control_task--
        if (ret)
        {
            LOG_E("steppersync_flush sc_num:%d i:%d last_step_clock:%llx next_step_clock :%llx ret:%d\n" ,ss->sc_num,i,ss->sc_list[i]->last_step_clock,ss->sc_list[i]->next_step_clock,ret );
            GAM_DEBUG_printf( "steppersync_flush sc_num:%d i:%d last_step_clock:%llx next_step_clock :%llx ret:%d\n" ,ss->sc_num,i,ss->sc_list[i]->last_step_clock,ss->sc_list[i]->next_step_clock,ret );
            printf("steppersync_flush sc_num:%d i:%d last_step_clock:%llx next_step_clock :%llx ret:%d\n" ,ss->sc_num,i,ss->sc_list[i]->last_step_clock,ss->sc_list[i]->next_step_clock,ret );
            return ret;
        }
    }
    // Order commands by the reqclock of each pending command
    struct list_head msgs;
    list_init(&msgs);
    for (;;) {
        uint64_t req_clock = MAX_CLOCK;
        struct queue_message *qm = NULL;
        for (i=0; i<ss->sc_num; i++) {
            struct stepcompress *sc = ss->sc_list[i];
            if (!list_empty(&sc->msg_queue)) {
                struct queue_message *m = list_first_entry( &sc->msg_queue, struct queue_message, node);
                if (m->req_clock < req_clock) {
                    qm = m;
                    req_clock = m->req_clock;
                }
            }
        }
        if (!qm || (qm->min_clock && req_clock > move_clock))
        {
            break;
        }

        uint64_t next_avail = ss->move_clocks[0];
        if (qm->min_clock)
        {
            heap_replace(ss, qm->min_clock);
        }
        // Reset the min_clock to its normal meaning (minimum transmit time)
        qm->min_clock = next_avail;

        // Batch this command
        list_del(&qm->node);
        list_add_tail(&qm->node, &msgs);             //---22-add_move-G-G---
    }

    // Transmit commands
    if (!list_empty(&msgs))
    {
        // GAM_DEBUG_send_UI("2-782-\n" );
        serialqueue_send_batch(ss->sq, ss->cq, &msgs);         //---23-add_move-G-G---
    }
    return 0;
}
