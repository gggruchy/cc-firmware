#ifndef MOVE_H
#define MOVE_H
#include <vector>
#include <algorithm>
#include <queue>
#include <string>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <functional>

#include "debug.h"

class Move
{
public:
    Move(std::vector<double> &start_pos, std::vector<double> &end_pos, double speed);
    ~Move();

    double m_start_v;  // 开始速度 0
    double m_cruise_v; // 巡航速度
    double m_end_v;    // 结束速度

    double m_accel_t;  // 梯形加速时间
    double m_cruise_t; // 梯形巡航时间
    double m_decel_t;  // 梯形减速时间

    double m_accel_d;  // 梯形加速距离
    double m_cruise_d; // 梯形巡航距离
    double m_decel_d;  // 梯形减速距离

    std::vector<double> m_start_pos; // 开始位置  //起点坐标
    std::vector<double> m_end_pos;   // 结束位置  //终点坐标

    double m_accel; // 加速度

    std::vector<double> m_axes_d; // 运动向量
    std::vector<std::function<void(double, double)>> m_timing_callbacks;
    double m_velocity;            // 速度
    bool m_is_kinematic_move;     // XYZ移动距离够长  XYZ MOVE
    double m_move_d;              // 起点到终点XYZ移动距离  XYZ MOVE  E MOVE
    std::vector<double> m_axes_r; // 运动分向量占移动距离的百分比
    double m_min_move_t;          // 最小运动时间
    // 接点速度以速度平方进行跟踪，delta_v2可以在这个动作中平方速度可以改变的最大值。
    double m_max_start_v2;    // 最大开始速度平方
    double m_max_cruise_v2;   // 最大巡航速度平方
    double m_delta_v2;        // 2as=vt2-v02
    double m_max_smoothed_v2; // 最大平滑速度平方
    double m_smooth_delta_v2; // 2as/2   设计思路是一半用来加速，一半用来减速，确保速度可以减到0，也至少可以从0加到指定速度

    void limit_speed(Move &move, double speed, double accel);
    std::string move_error(std::string msg = "Move out of range");
    void calc_junction(Move &move, Move &prev_move);
    void set_junction(double start_v2, double cruise_v2, double end_v2);
    void reset_endpos(std::vector<double> &m_end_pos);

private:
};

struct Delayitem
{
    Move *move;
    double start_v2;
    double next_end_v2;
    Delayitem();
    Delayitem(Move *_move, double _start_v2, double _next_end_v2)
    {
        move = _move;
        start_v2 = _start_v2;
        next_end_v2 = _next_end_v2;
    }
};

class MoveQueue
{
public:
    MoveQueue();
    ~MoveQueue();

    std::vector<Move> moveq;
    double m_junction_flush; // 起步时至少运动时间超过2S才开始计算速度 运动中至少运动时间超过0.25S才开始计算速度

    void reset();
    void set_flush_time(double flushTime);
    Move *get_last();
    void flush(bool lazy = false);
    void add_move(Move &move);

private:
};

#endif
