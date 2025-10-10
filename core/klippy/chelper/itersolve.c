// Iterative solver for kinematic moves  运动学运动的迭代求解器
//
// Copyright (C) 2018-2020  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <math.h> // fabs
#include <stddef.h> // offsetof
#include <string.h> // memset
#include "compiler.h" // __visible
#include "itersolve.h" // itersolve_generate_steps
#include "pyhelper.h" // errorf
#include "stepcompress.h" // queue_append_start
#include "trapq.h" // struct move
#include "debug.h"
/****************************************************************
 * Main iterative solver
 ****************************************************************/

#define LOG_TAG "intersolve"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

struct  timepos {
    double time, position;
};

#define SEEK_TIME_RESET 0.000100        //0.1ms
//sk->tq->moves
//trapq_append generate_steps itersolve_generate_steps itersolve_gen_steps_range stepcompress_append
// Generate step times for a portion of a move  为移动的一部分生成步骤时间
static int32_t itersolve_gen_steps_range(struct stepper_kinematics *sk, struct move *m  , double abs_start, double abs_end)        //--18-move-2task-G-G--UI_control_task--
{
    sk_calc_callback calc_position_cb = sk->calc_position_cb;       //shaper_x_calc_position
    
    double half_step = .5 * sk->step_dist;          //步距
    double start = abs_start - m->print_time, end = abs_end - m->print_time;
    if (start < 0.)
        start = 0.;
    if (end > m->move_t)
        end = m->move_t;
    struct timepos old_guess = {start, sk->commanded_pos}, guess = old_guess;
    int sdir = stepcompress_get_step_dir(sk->sc);       //上一次move方向
    int is_dir_change = 0, have_bracket = 0, check_oscillate = 0;
    double target = sk->commanded_pos + (sdir ? half_step : -half_step);           //预估目标位置  继续向前走半步的位置
    double last_time=start, low_time=start, high_time=start + SEEK_TIME_RESET;
    if (high_time > end)
        high_time = end;
    for (;;) {
        // Use the "secant method" to guess a new time from previous guesses  使用"正割法"从以前的猜测中猜出新的时间
        double guess_dist = guess.position - target;        //猜测运动距离
        double og_dist = old_guess.position - target;
        double next_time = ((old_guess.time*guess_dist - guess.time*og_dist) / (guess_dist - og_dist));         //除以0就是NaN 会下面会检查
        if (!(next_time > low_time && next_time < high_time)) { // or NaN       // Next guess is outside bounds checks - validate it   下一个猜测是超出边界检查 - 验证它
            if (have_bracket) {  // A poor guess - fall back to bisection   一个糟糕的猜测 - 回落到二分
                next_time = (low_time + high_time) * .5;
                check_oscillate = 0;
            } else if (guess.time >= end) {  //只有一步的运动   请求的时间范围内没有更多步骤
                if (!(next_time < high_time ) && low_time < end)  //避免 guess_dist = og_dist 的时候计算特别离谱
                {
                    // GAM_DEBUG_printf("----G_G_1----------%f-%f-%f--%f-%f--------------------\n",next_time,guess.time,low_time,high_time,end);       //--G-G-2023-05-11
                }
                if (!(next_time > low_time)  && low_time < end )        //必须要用为假
                {
                    // GAM_DEBUG_printf("----G_G_2----------%f-%f-%f--%f-%f--------------------\n",next_time,guess.time,low_time,high_time,end);
                }
                break;          //跳出
            } else {            //   可能是一个糟糕的猜测 - 限制指数搜索
                next_time = high_time;
                high_time = 2. * high_time - last_time;             //0.1ms 0.2ms 0.4ms 0.8ms 1.6ms 3.2ms  指数级增加1步运动的时间
                if (high_time > end)
                    high_time = end;
            }
        }
        // Calculate position at next_time guess  计算next_time猜测的位置
        old_guess = guess;
        guess.time = next_time;
        guess.position = calc_position_cb(sk, m, next_time);        //s = s0 + v0t+0.5at2
        guess_dist = guess.position - target;                           //ds
        if (fabs(guess_dist) > .000000001) {        //时间没有预测对   猜测看起来不够近 - 更新边界
            double rel_dist = sdir ? guess_dist : -guess_dist;      
            if (rel_dist > 0.) {         // 方向未变 Found position past target, so step is definitely present  找到位置超过目标，所以步骤肯定存在
                if (have_bracket && old_guess.time <= low_time) 
                {
                    if (check_oscillate)                  // Force bisect next to avoid persistent oscillations  强制将下一个一分为二以避免持续振荡
                        old_guess = guess;
                    check_oscillate = 1;
                }
                high_time = guess.time;
                have_bracket = 1;
            } else if (rel_dist < -(half_step + half_step + .000000010)) {            //  发现方向变化 且有1步
                sdir = !sdir;
                target = (sdir ? target + half_step + half_step : target - half_step - half_step);
                low_time = last_time;
                high_time = guess.time;
                is_dir_change = have_bracket = 1;
                check_oscillate = 0;
            } else {     //时间太小 还没有一步
                low_time = guess.time; 
            }
            if (!have_bracket || high_time - low_time > .000000001) {           //猜测不够接近 - 用新的时间再次猜测
                if (!is_dir_change && rel_dist >= -half_step)            // Avoid rollback if stepper fully reaches step position  避免在步进器完全达到步进位置时回滚
                {
                    stepcompress_commit(sk->sc);        // 保存上一次步进数据 避免因快速换向回滚
                }
                continue;
            }
        }


        int ret = stepcompress_append(sk->sc, sdir, m->print_time, guess.time);         //-保存新的一步步进参数--14-add_move-G-G---m->print_time 是 move 步进开始时间 guess.time 是这一步 与当前move第一个步进脉冲间隔时间
        if (ret)
            return ret;
        target = sdir ? target+half_step+half_step : target-half_step-half_step;    //  下一步目标位置  重置边界检查
        double seek_time_delta = 1.5 * (guess.time - last_time);
        if (seek_time_delta < .000000001)
            seek_time_delta = .000000001;
        if (is_dir_change && seek_time_delta > SEEK_TIME_RESET)
            seek_time_delta = SEEK_TIME_RESET;
        last_time = low_time = guess.time;
        high_time = guess.time + seek_time_delta;
        if (high_time > end)
            high_time = end;
        is_dir_change = have_bracket = check_oscillate = 0;
    }
    sk->commanded_pos = target - (sdir ? half_step : -half_step);       //记录当前位置
    if (sk->post_cb)
        sk->post_cb(sk);
    return 0;
}


/****************************************************************
 * Interface functions
 ****************************************************************/

// Check if a move is likely to cause movement on a stepper  检查移动是否可能导致步进器上的移动
static inline int
check_active(struct stepper_kinematics *sk, struct move *m)
{
    int af = sk->active_flags;
    return ((af & AF_X && m->axes_r.x != 0.) || (af & AF_Y && m->axes_r.y != 0.) || (af & AF_Z && m->axes_r.z != 0.));
}

// Generate step times for a range of moves on the trapq  为 trapq 上的一系列移动生成步进时间
int32_t __visible itersolve_generate_steps(struct stepper_kinematics *sk, double flush_time)          //--17-move-2task-G-G--UI_control_task--
{
    // GAM_DEBUG_send_UI("2-154-\n" );
    double last_flush_time = sk->last_flush_time;
    sk->last_flush_time = flush_time;
    if (!sk->tq)
    {
        return 0;
    }
    trapq_check_sentinels(sk->tq);   //----
    struct move *m = list_first_entry(&sk->tq->moves, struct move, node);
    while (last_flush_time >= m->print_time + m->move_t)            //找到第一个上次没有处理完的 move
        m = list_next_entry(m, node);
    double force_steps_time = sk->last_move_time + sk->gen_steps_post_active;
    int skip_count = 0;
    for (;;) {
        double move_start = m->print_time, move_end = move_start + m->move_t;
        if (check_active(sk, m)) {
            if (skip_count && sk->gen_steps_pre_active) 
            {
                // Must generate steps leading up to stepper activity  必须生成导致步进器活动的步骤
                double abs_start = move_start - sk->gen_steps_pre_active;
                if (abs_start < last_flush_time)
                    abs_start = last_flush_time;
                if (abs_start < force_steps_time)
                    abs_start = force_steps_time;
                struct move *pm = list_prev_entry(m, node);
                while (--skip_count && pm->print_time > abs_start)
                    pm = list_prev_entry(pm, node);
                do {
                    // GAM_DEBUG_printf("pm0 t0:%f  dt :%f v0 :%f ha :%f sd :%f %f->%f\n",pm->print_time , pm->move_t,pm->start_v,pm->half_accel,sk->step_dist,abs_start,flush_time);
                    int32_t ret = itersolve_gen_steps_range(sk, pm, abs_start , flush_time);
                    if (ret)
                    {
                        return ret;
                    }
                    pm = list_next_entry(pm, node);
                } while (pm != m);
            }
            // GAM_DEBUG_printf("pm1 %f t0:%f  dt :%f v0 :%.10f ha :%f %f->%f\n",sk->commanded_pos,m->print_time , m->move_t,m->start_v,m->half_accel,last_flush_time,flush_time);
            int32_t ret = itersolve_gen_steps_range(sk, m, last_flush_time  , flush_time);        //---13-add_move-G-G---
            if (ret)
            {
                return ret;
            }
            if (move_end >= flush_time) {
                sk->last_move_time = flush_time;
                return 0;
            }
            skip_count = 0;
            sk->last_move_time = move_end;
            force_steps_time = sk->last_move_time + sk->gen_steps_post_active;
        } else {
            if (move_start < force_steps_time) {          // Must generates steps just past stepper activity  必须生成刚过步进器活动的步骤
                double abs_end = force_steps_time;
                if (abs_end > flush_time)
                    abs_end = flush_time;
                // GAM_DEBUG_printf("pm2 print_time:%f  move_t :%f start_v :%.10f half_accel :%.10f\n",m->print_time , m->move_t,m->start_v,m->half_accel);
                int32_t ret = itersolve_gen_steps_range(sk, m, last_flush_time, abs_end);          //--G-G-2023-05-11
                if (ret)
                {
                    return ret;
                }
                skip_count = 1;
            } else {        // This move doesn't impact this stepper - skip it  此移动不会影响此步进器 - 跳过它
                skip_count++;
            }
            if (flush_time + sk->gen_steps_pre_active <= move_end)
            {
                return 0;
            }
        }
        m = list_next_entry(m, node);
    }
}

// Check if the given stepper is likely to be active in the given time range  检查给定的步进器是否可能在给定的时间范围内处于活动状态
double __visible itersolve_check_active(struct stepper_kinematics *sk, double flush_time)
{
    if (!sk->tq)
        return 0.;
    trapq_check_sentinels(sk->tq);
    struct move *m = list_first_entry(&sk->tq->moves, struct move, node);
    while (sk->last_flush_time >= m->print_time + m->move_t)
    {
        m = list_next_entry(m, node);
    }
    for (;;) 
    {
        if (check_active(sk, m))
        {
            return m->print_time;
        }
        if (flush_time <= m->print_time + m->move_t)
            return 0.;
        m = list_next_entry(m, node);
    }
}

// Report if the given stepper is registered for the given axis  报告给定步进器是否已注册给定轴
int32_t __visible itersolve_is_active_axis(struct stepper_kinematics *sk, char axis)
{
    if (axis < 'x' || axis > 'z')
        return 0;
    return (sk->active_flags & (AF_X << (axis - 'x'))) != 0;
}

void __visible itersolve_set_trapq(struct stepper_kinematics *sk, struct trapq *tq)
{
    sk->tq = tq;
}

void __visible itersolve_set_stepcompress(struct stepper_kinematics *sk , struct stepcompress *sc, double step_dist)
{
    sk->sc = sc;                                    //m_step_queue
    sk->step_dist = step_dist;      //m_step_dist
}

double __visible itersolve_calc_position_from_coord(struct stepper_kinematics *sk  , double x, double y, double z)
{
    struct move m;
    memset(&m, 0, sizeof(m));       //V0 和 a 都是0 ，没有运动
    m.start_pos.x = x;
    m.start_pos.y = y;
    m.start_pos.z = z;
    m.move_t = 1000.;
    return sk->calc_position_cb(sk, &m, 500.);          //move_get_coord
}

void __visible
itersolve_set_position(struct stepper_kinematics *sk , double x, double y, double z)        //设置当前位置
{
    //LOG();
    sk->commanded_pos = itersolve_calc_position_from_coord(sk, x, y, z);            //这里只是选对应轴 设置,x轴就是 sk->commanded_pos = x;
}

double __visible
itersolve_get_commanded_pos(struct stepper_kinematics *sk)
{
    //LOG();
    return sk->commanded_pos;
}
