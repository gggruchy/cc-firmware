#include "move.h"
#include "Define.h"
#include "klippy.h"
#define LOG_TAG "move"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

Move::Move(std::vector<double> &start_pos, std::vector<double> &end_pos, double speed)
{
    m_start_pos = start_pos;                                                           // 起点坐标
    m_end_pos = end_pos;                                                               // 终点坐标
    m_accel = Printer::GetInstance()->m_tool_head->m_max_accel;                        // 加速度
    m_velocity = std::min(speed, Printer::GetInstance()->m_tool_head->m_max_velocity); // 速度
    m_is_kinematic_move = true;

    for (int i = 0; i < end_pos.size(); i++) // 终点－起点坐标
    {
        m_axes_d.push_back(end_pos[i] - m_start_pos[i]);
    }

    std::vector<double> axes_d;
    axes_d = m_axes_d;

    m_move_d = sqrt(pow(m_axes_d[0], 2) + pow(m_axes_d[1], 2) + pow(m_axes_d[2], 2)); // 起点到终点距离
    // std---::cout << ""m_move_d 39 " << m_move_d << std::endl;
    double move_d = m_move_d;
    double inv_move_d = 0; //

    if (move_d < 10e-8) // 移动距离太短时，喷头位置不变，仅挤出机运动
    {
        m_end_pos[0] = start_pos[0];
        m_end_pos[1] = start_pos[1];
        m_end_pos[2] = start_pos[2];
        m_end_pos[3] = end_pos[3];

        axes_d[0] = axes_d[1] = axes_d[2] = 0.0;
        m_move_d = move_d = fabs(axes_d[3]);

        if (move_d > 10e-8) //-------------G-G-2022-08-11---
        {
            inv_move_d = 1.0 / move_d;
        }
        else // G1 F3000设置速度指令
        {
        }

        m_accel = 99999999.9;
        m_velocity = speed;
        m_is_kinematic_move = false;
    }
    else
    {
        inv_move_d = 1.0 / move_d;
    }

    for (int i = 0; i < axes_d.size(); i++) // 主向量和分向量　cos_theta
    {
        m_axes_r.push_back(inv_move_d * axes_d[i]);
    }
    m_min_move_t = move_d / m_velocity;   // 最小运动时间
    m_max_start_v2 = 0.0;                 // 最大开始速度
    m_max_cruise_v2 = pow(m_velocity, 2); // 最大匀速度
    m_delta_v2 = 2.0 * move_d * m_accel;  // 接点速度以速度平方进行跟踪，delta_v2可以在这个动作中平方速度可以改变的最大值。
    m_max_smoothed_v2 = 0.0;              // 最大平滑速度平方
    m_smooth_delta_v2 = 2.0 * move_d * Printer::GetInstance()->m_tool_head->m_max_accel_to_decel;

    m_start_v = 0;  // 开始速度
    m_cruise_v = 0; // 巡航速度
    m_end_v = 0;    // 结束速度
    m_accel_t = 0;  // 梯形加速时间
    m_cruise_t = 0; // 梯形巡航时间
    m_decel_t = 0;  // 梯形减速时间
    m_accel_d = 0;  // 梯形加速距离
    m_cruise_d = 0; // 梯形巡航距离
    m_decel_d = 0;  // 梯形减速距离
}

Move::~Move()
{
    // m_axes_d.size()
    // m_axes_r
    // m_timing_callbacks
}

void Move::limit_speed(Move &move, double speed, double accel) //--4-move-2task-G-G--UI_control_task---根据Z E轴移动相对距离限速, 修改巡航速度和最小运动时间 加速度  最大速度增量平方
{
    double speed2 = speed * speed;
    if (speed2 < move.m_max_cruise_v2)
    {
        move.m_max_cruise_v2 = speed2;
        move.m_min_move_t = move.m_move_d / speed;
    }
    move.m_accel = std::min(move.m_accel, accel);
    move.m_delta_v2 = 2.0 * move.m_move_d * move.m_accel;
    move.m_smooth_delta_v2 = std::min(move.m_smooth_delta_v2, move.m_delta_v2); //? move.m_delta_v2*0.5
}

std::string Move::move_error(std::string msg)
{
    std::vector<double> ep = m_end_pos;
    std::stringstream m;
    m << msg << ": " << ep[0] << " " << ep[1] << " " << ep[2] << " " << ep[3];
    // return m_toolhead.printer.command_error(m.str);  //---??---Move
    return m.str();
}

void Move::reset_endpos(std::vector<double> &end_pos)
{
    m_end_pos = end_pos;
    m_accel = Printer::GetInstance()->m_tool_head->m_max_accel; // 加速度
    m_is_kinematic_move = true;

    std::vector<double>().swap(m_axes_d);
    for (int i = 0; i < end_pos.size(); i++) // 终点－起点坐标
    {
        m_axes_d.push_back(end_pos[i] - m_start_pos[i]);
    }

    std::vector<double> axes_d;
    axes_d = m_axes_d;

    m_move_d = sqrt(pow(m_axes_d[0], 2) + pow(m_axes_d[1], 2) + pow(m_axes_d[2], 2)); // 起点到终点距离
    // std---::cout << ""m_move_d 39 " << m_move_d << std::endl;
    double move_d = m_move_d;
    double inv_move_d = 0; //

    if (move_d < 10e-8) // 移动距离太短时，喷头位置不变，仅挤出机运动
    {
        m_end_pos[0] = m_start_pos[0];
        m_end_pos[1] = m_start_pos[1];
        m_end_pos[2] = m_start_pos[2];
        m_end_pos[3] = end_pos[3];

        axes_d[0] = axes_d[1] = axes_d[2] = 0.0;
        m_move_d = move_d = fabs(axes_d[3]);

        if (move_d > 10e-8) //-------------G-G-2022-08-11---
        {
            inv_move_d = 1.0 / move_d;
        }
        else // G1 F3000设置速度指令
        {
        }

        m_accel = 99999999.9;
        m_is_kinematic_move = false;
    }
    else
    {
        inv_move_d = 1.0 / move_d;
    }

    std::vector<double>().swap(m_axes_r);
    for (int i = 0; i < axes_d.size(); i++) // 主向量和分向量　cos_theta
    {
        m_axes_r.push_back(inv_move_d * axes_d[i]);
    }
    m_min_move_t = move_d / m_velocity;   // 最小运动时间
    m_max_start_v2 = 0.0;                 // 最大开始速度
    m_max_cruise_v2 = pow(m_velocity, 2); // 最大匀速度
    m_delta_v2 = 2.0 * move_d * m_accel;  // 接点速度以速度平方进行跟踪，delta_v2可以在这个动作中平方速度可以改变的最大值。
    m_max_smoothed_v2 = 0.0;              // 最大平滑速度平方
    m_smooth_delta_v2 = 2.0 * move_d * Printer::GetInstance()->m_tool_head->m_max_accel_to_decel;

    m_start_v = 0;  // 开始速度
    m_cruise_v = 0; // 巡航速度
    m_end_v = 0;    // 结束速度
    m_accel_t = 0;  // 梯形加速时间
    m_cruise_t = 0; // 梯形巡航时间
    m_decel_t = 0;  // 梯形减速时间
    m_accel_d = 0;  // 梯形加速距离
    m_cruise_d = 0; // 梯形巡航距离
    m_decel_d = 0;  // 梯形减速距离
    Printer::GetInstance()->m_gcode_move->m_last_position = m_end_pos;
}

void Move::set_junction(double start_v2, double cruise_v2, double end_v2) // 在移动中实现"梯形发生器"
{

    if (cruise_v2 <= 1e-15) // 速度 cruise_v2 不能为0，否则导致 除以0错误死机
    {
        LOG_E("m_accel_t  : %.4f : %.4f  : %.4f  : %.4f \n", m_accel_t, start_v2, cruise_v2, end_v2);
        // GAM_ERR_printf("m_accel_t  : %.4f : %.4f  : %.4f  : %.4f \n", m_accel_t, start_v2, cruise_v2, end_v2);
        // cruise_v2 = 100.0;
        // if (!Printer::GetInstance()->is_shutdown())
        // {
        //     LOG_E("cruise_v2 < 0.01 \n");
        //     Printer::GetInstance()->error_move_shutdown("cruise_v2 < 0.01");
        // }
    }
    // 确定移动距离的加速、巡航和减速部分
    double half_inv_accel = 0.5 / m_accel;
    m_accel_d = (cruise_v2 - start_v2) * half_inv_accel; // 加速距离   (vt2-v02)*0.5/a=sa
    m_decel_d = (cruise_v2 - end_v2) * half_inv_accel;   // 减速距离   sd
    m_cruise_d = m_move_d - m_accel_d - m_decel_d;       // 巡航距离      sc

    // 确定移动速度
    double start_v = sqrt(start_v2);
    m_start_v = start_v;
    double cruise_v = sqrt(cruise_v2);
    m_cruise_v = cruise_v;
    double end_v = sqrt(end_v2);
    m_end_v = end_v;
    // 确定每个移动部分花费的时间（时间是距离除以平均速度）
    m_accel_t = m_accel_d / ((start_v + cruise_v) * 0.5);
    m_cruise_t = m_cruise_d / cruise_v;
    m_decel_t = m_decel_d / ((end_v + cruise_v) * 0.5);

    if (isnan(m_accel_t))
    {
        LOG_E("m_accel_t is nan. m_accel_t  : %.4f : %.4f  : %.4f  : %.4f \n", m_accel_t, start_v2, cruise_v2, end_v2)
        GAM_ERR_printf("m_accel_t  : %.4f : %.4f  : %.4f  : %.4f \n", m_accel_t, start_v2, cruise_v2, end_v2);
    }
}

void Move::calc_junction(Move &move, Move &prev_move) //--6-move-2task-G-G--UI_control_task--计算其最大拐角速度
{
    if (move.m_is_kinematic_move == false || prev_move.m_is_kinematic_move == false) // XYZ NOT MOVE
    {
        return;
    }
    // GAM_DEBUG_send_UI("2-127-\n");
    double extruder_v2 = Printer::GetInstance()->m_tool_head->m_extruder->calc_junction(prev_move, move); // 根据E轴占比变化来求运动降速情况

    // 使用“近似向心速度”找到最大速度
    std::vector<double> axes_r;
    axes_r = move.m_axes_r; // cos   (axes_r[0],axes_r[1],axes_r[2]) 是单位向量或0向量

    std::vector<double> prev_axes_r;
    prev_axes_r = prev_move.m_axes_r; // cos   (prev_axes_r[0],prev_axes_r[1],prev_axes_r[2]) 是单位向量或0向量

    // 转接点角度计算
    double junction_cos_theta = -(axes_r[0] * prev_axes_r[0] + axes_r[1] * prev_axes_r[1] + axes_r[2] * prev_axes_r[2]); // cosθ    x0*x1+y0*y1+z0*z1  向量点乘 计算两个向量之间的夹角 ab=|a|*|b|*cosθ
    if (junction_cos_theta > 0.999999)                                                                                   // cosθ=-1   180度，无接点，退出
    {
        return;
    }
    junction_cos_theta = std::max(junction_cos_theta, -0.999999);                                                 //-0.999999 ~~~~ 0.999999
    double sin_theta_d2 = sqrt(0.5 * (1.0 - junction_cos_theta));                                                 // 2cos²(θ/2) = 1+cosθ
    double R = (Printer::GetInstance()->m_tool_head->m_junction_deviation * sin_theta_d2 / (1.0 - sin_theta_d2)); // 0.414*vi²/a * sin_theta_d2/(1.0 - sin_theta_d2)      0~π   ∞~0        π/2->vi²/a

    // Approximated circle must contact moves no further away than mid-move  近似圆必须接触移动不超过中间移动
    double tan_theta_d2 = sin_theta_d2 / sqrt(0.5 * (1.0 + junction_cos_theta));                     // 2sin²(θ/2) =1-cosθ
    double move_centripetal_v2 = 0.5 * move.m_move_d * tan_theta_d2 * move.m_accel;                  // 0.5*s*a/tan(θ/2)     =m_delta_v2/(4*tan(θ/2))     0~π   ∞~0     π/2->m_delta_v2/4
    double prev_move_centripetal_v2 = (0.5 * prev_move.m_move_d * tan_theta_d2 * prev_move.m_accel); // 0.5*s*a/tan(θ/2)

    // Apply limits  应用限制
    move.m_max_start_v2 = std::min(std::min(std::min(R * move.m_accel, R * prev_move.m_accel), std::min(move_centripetal_v2, prev_move_centripetal_v2)),
                                   std::min(std::min(extruder_v2, move.m_max_cruise_v2), std::min(prev_move.m_max_cruise_v2, prev_move.m_max_start_v2 + prev_move.m_delta_v2)));
    move.m_max_smoothed_v2 = std::min(move.m_max_start_v2, prev_move.m_max_smoothed_v2 + prev_move.m_smooth_delta_v2);
}

MoveQueue::MoveQueue()
{
    m_junction_flush = LOOKAHEAD_FLUSH_TIME;
}

MoveQueue::~MoveQueue()
{
}

void MoveQueue::reset()
{
    std::vector<Move>().swap(moveq);
    m_junction_flush = LOOKAHEAD_FLUSH_TIME;
}

void MoveQueue::set_flush_time(double flushTime)
{
    m_junction_flush = flushTime;
    return;
}

Move *MoveQueue::get_last()
{
    if (!moveq.empty())
    {
        return &(moveq.back());
    }
    return nullptr;
}

void MoveQueue::flush(bool lazy) //--6-home-2task-G-G--UI_control_task--       //--7-move-2task-G-G--UI_control_task--//----5-M-G-G-2022-04-08----//确定每个移动的开始和结束速度。
{
    GAM_DEBUG_send_UI_home("2-H6-\n");
    m_junction_flush = LOOKAHEAD_FLUSH_TIME;
    bool update_flush_count = lazy; // HOME false  MOVE true
    int flush_count = moveq.size();
    double next_end_v2 = 0;
    double next_smoothed_v2 = 0;
    double peak_cruise_v2 = 0;
    std::vector<Delayitem> delayed;
    // GAM_DEBUG_send_UI_SPEED("61-%d-\n",flush_count);
    for (int i = flush_count - 1; i >= 0; i--) // 从最后一个移动到第一个移动遍历队列并确定最大值连接速度,假设机器完全停止在最后一步之后。
    {
        double reachable_start_v2 = next_end_v2 + moveq[i].m_delta_v2;                    // 根据下一段倒推当前段最大初速度 v0m2=2as+vt2
        double start_v2 = std::min(moveq[i].m_max_start_v2, reachable_start_v2);          // 根据上一段计算的拐角速度推出的当前段最大初速度  v02 <= v0m2
        double reachable_smoothed_v2 = next_smoothed_v2 + moveq[i].m_smooth_delta_v2;     // 根据下一段倒推当前段最大平滑初速度 v0ms2=as+vts2
        double smoothed_v2 = std::min(moveq[i].m_max_smoothed_v2, reachable_smoothed_v2); // 根据上一段计算的拐角速度推出的当前段最大平滑初速度 v0s2 <= v0ms2
        //    GAM_DEBUG_send_UI_SPEED("68-%f-%f-\n",smoothed_v2,reachable_smoothed_v2);
        if (smoothed_v2 < reachable_smoothed_v2) // 根据上一段确定的初速度小于根据这一段末速度要求的最大平滑初速度，这一段初速度已经可以确定  // 有可能加速
        {
            // GAM_DEBUG_send_UI_SPEED("71-%f-%f-\n",smoothed_v2 + moveq[i].m_smooth_delta_v2,next_smoothed_v2);
            if (smoothed_v2 + moveq[i].m_smooth_delta_v2 > next_smoothed_v2 || !delayed.empty()) // 这个动作可以减速或者这是一个完全加速, 在完全减速移动后移动
            {
                if (update_flush_count && peak_cruise_v2)
                {
                    flush_count = i;
                    update_flush_count = false;
                }
                peak_cruise_v2 = std::min(moveq[i].m_max_cruise_v2, (smoothed_v2 + reachable_smoothed_v2) * 0.5);
                if (!delayed.empty())
                {
                    if (!update_flush_count && i < flush_count) // 将 peak_cruise_v2 传播到任何延迟的移动
                    {
                        double mc_v2 = peak_cruise_v2;
                        for (int j = delayed.size() - 1; j >= 0; j--)
                        {
                            mc_v2 = std::min(mc_v2, delayed[j].start_v2);
                            delayed[j].move->set_junction(std::min(delayed[j].start_v2, mc_v2), mc_v2, std::min(delayed[j].next_end_v2, mc_v2));
                            // GAM_DEBUG_send_UI_SPEED("92-%d-%d-%f-%f-%f-\n",i,j,delayed[j].move->m_start_v,delayed[j].move->m_cruise_v,delayed[j].move->m_end_v);
                        }
                    }
                    std::vector<Delayitem>().swap(delayed);
                }
            }
            // GAM_DEBUG_send_UI_SPEED("97-%f-%d-\n",peak_cruise_v2,i);
            if (!update_flush_count && i < flush_count) // 最后一段和倒数第二段不保存
            {
                double cruise_v2 = std::min(std::min((start_v2 + reachable_start_v2) * 0.5, moveq[i].m_max_cruise_v2), peak_cruise_v2);
                moveq[i].set_junction(std::min(start_v2, cruise_v2), cruise_v2, std::min(next_end_v2, cruise_v2)); // 设置 开始速度 加速距离 加速时间
                // GAM_DEBUG_send_UI("102-%d-%f-%f-%f-\n",i,moveq[i].m_start_v,moveq[i].m_cruise_v,moveq[i].m_end_v);
            }
        }
        else // 这一段初速度还不可以确定
        {
            delayed.push_back(Delayitem(&moveq[i], start_v2, next_end_v2)); // 延迟计算此移动，直到 peak_cruise_v2 已知
        }
        next_end_v2 = start_v2;
        next_smoothed_v2 = smoothed_v2;
    }
    if (update_flush_count || !flush_count)
    {
        return;
    }
    // std::cout  << "get_monotonic:" << get_monotonic() << " flush_count:" << flush_count  << " moveq size:" << moveq.size()<< std::endl;
    std::vector<Move> process_moves(moveq.begin(), moveq.begin() + flush_count); //---8-2task-G-G--UI_control_task--
    Printer::GetInstance()->m_tool_head->process_moves(process_moves);           //--13-home-2task-G-G--UI_control_task--      //---9-2task-G-G--UI_control_task--//----6-M-G-G-2022-04-08---------- //为所有准备被刷新的动作生成步骤时间
    // Remove processed moves from the queue  从队列中删除已处理的移动
    if (Printer::GetInstance()->m_tool_head->is_trigger && Printer::GetInstance()->m_tool_head->is_drip_move)
    {
        return;
    }

    if (moveq.size() >= flush_count)
    {
        std::vector<Move> remain_moves(moveq.begin() + flush_count, moveq.end());
        moveq.swap(remain_moves);
        // system("free");
    }
}
// moveq                                   process_moves  tq->moves                 sc->next_step_clock
// MoveQueue::add_move   flush                     trapq_add_move    stepcompress_append
#define GAM_DEBUG_send_UI_SPEED(fmt, ...) // printf(fmt, ##__VA_ARGS__)
void MoveQueue::add_move(Move &move)      //--5-move-2task-G-G--UI_control_task---------//将移动对象置于"前瞻"队列中
{
    moveq.push_back(move); // 增加到队列最后
    if (moveq.size() == 1)
    {
        return;
    }
    GAM_DEBUG_send_UI_SPEED("40-%f-\n", m_junction_flush);
    moveq.back().calc_junction(moveq.back(), moveq[moveq.size() - 2]); // 计算新加的和前一个的最大拐角速度

    m_junction_flush -= move.m_min_move_t;
    // std::cout << "get_monotonic:" << get_monotonic() << " m_junction_flush = " << m_junction_flush << std::endl;

    if (m_junction_flush <= 0) // 至少2S 已排队足够的移动以达到目标刷新时间。
    {
        flush(true); //--7-move-2task-G-G--UI_control_task--
    }
}