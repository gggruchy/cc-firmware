#ifndef __TOOLHEAD_H__
#define __TOOLHEAD_H__

#include <vector>
#include <algorithm>
#include <queue>
#include <string>
#include <stdlib.h>
#include <math.h>
#include <iostream>
extern "C"
{
#include "../chelper/pyhelper.h"
#include "../chelper/trapq.h"
#include "../chelper/serialqueue.h"
}
#include "cartesian.h"
#include "corexy.h"
#include "corexz.h"
#include "delta.h"
#include "extruder.h"
#include "stepper.h"
#include "gcode.h"
#include "move.h"
#include "reactor.h"

#define I_OPT_ABSOLUTE_AXIAL 1
#define I_OPT_RELATIVE_AXIAL 0
class ToolHead
{
public:
    ToolHead(std::string section_name);
    ~ToolHead();

    enum special_queuing_state
    {               // 主要在归0使用
        main_state, //print  连续打印状态 命令连续
        Priming,        //引爆 home END 突发运行 运动的第一条指令
        Flushed,        //冲刷   home start  手动运行 一段段独立不连续的运行命令
        Drip                     //home Endstop  用于水滴式归0 保证一小段一小段的发送归0命令
    };
    std::mutex m_trigger_mtx;
    std::atomic_bool is_trigger;
    // std::atomic_bool m_stop_move;
    // std::atomic_bool m_stop_bed_mesh;
    bool is_drip_move;
    bool is_home;
    PrinterExtruder *m_extruder;
    trapq *m_trapq;
    std::vector<std::function<void(double)>> m_step_generators;
    ClockSync *clocksync;
    MCU_stepper *m_stepper;

    // CartKinematics *m_kin;
    Kinematics *m_kin;
    std::vector<MCU *> m_all_mcus;
    MCU *m_mcu;
    std::vector<double> m_commanded_pos; // 当前位置，归零后他就是对应position_endstop
    double m_move_speed;
    MoveQueue *m_movequeue; //
    bool m_can_pause;
    ReactorTimerPtr m_flush_timer;
    ReactorTimerPtr m_stats_timer;

    // Velocity and acceleration control  速度和加速度控制
    double m_max_velocity;             // 最大速度
    double m_max_velocity_limit;
    double m_max_accel;                // 最大加速度
    double m_max_accel_limit;
    double m_requested_accel_to_decel; // 最大加速度/2
    double m_max_accel_to_decel;       // 最大加速度/2
    double m_square_corner_velocity;   // 5  π/2 时对应的最大安全拐角速度
    double m_junction_deviation;       // 25 * (sqrt(2) - 1) / 最大加速度=0.00345    π/2 时最大安全拐角速度对应的最小圆半径
    // Print time tracking  打印时间跟踪
    double m_buffer_time_low;
    double m_buffer_time_high;
    double m_buffer_time_start; // 0.25
    double m_last_kin_move_time;
    double m_last_kin_flush_time;
    double m_move_flush_time; // 0.05
    double m_print_time;      // 计算预估主MCU计数器计数时间 s
    double m_last_flush_time;
    double m_need_flush_time;
    double m_min_restart_time;
    double m_step_gen_time;


    double m_need_check_stall;

    // m_flush_timer;  //self.flush_timer = self.reactor.register_timer(self._flush_handler)
    double m_idle_flush_print_time;
    double m_print_stall;
    // m_drip_completion;

    double m_kin_flush_delay;              // 0.001
    std::vector<double> m_kin_flush_times; // self.kin_flush_times = []
    std::string m_kin_name;
    int m_special_queuing_state; // 主要在归0使用
    std::string m_cmd_SET_VELOCITY_LIMIT_help;

    double m_start_z_time;
    double m_end_z_time;

    double check_chip_state(double eventtime);
    void update_move_time(double next_print_time);
    void advance_flush_time(double flush_time);
    void advance_move_time(double next_print_time);
    void calc_print_time();
    void process_moves(std::vector<Move> &moves);
    void stop_move();
    void stop_move_clear();
    void flush_step_generation();
    void flush_lookahead();
    double get_last_move_time();
    void check_stall();
    double flush_handler(double eventtime);
    std::vector<double> get_position();
    void set_position(std::vector<double> new_pos, std::vector<int> homing_axes = std::vector<int>());
    bool move1(double *new_pos, double speed);
    bool move(std::vector<double> &new_pos, double speed);
    bool manual_move(std::vector<double> coord, double speed);
    void dwell(double delay);
    void wait_moves();
    void set_extruder(PrinterExtruder *extruder, double extrude_pos);
    PrinterExtruder *get_extruder();
    void update_drip_move_time(double next_print_time);
    void drip_move(std::vector<double> newpos, double speed, int drip_completion);
    double stats(double eventtime);
    std::vector<double> check_busy(double eventtime);
    std::map<std::string, std::string> get_status(double eventtime);
    void handle_shutdown();
    Kinematics *get_kinematics();
    trapq *get_trapq();
    void register_step_generator(std::function<void(double)> handler);
    void note_step_generation_scan_time(double delay, double old_delay = 0);
    void register_lookahead_callback(std::function<void(double, double)> callback);
    void note_kinematic_activity(double kin_time);
    std::vector<double> get_max_velocity();
    void calc_junction_deviation();
    void load_kinematics(std::string section_name);
    void set_silent_mode(bool silent_mode);

    void cmd_G4(GCodeCommand &gcmd);
    void cmd_M400(GCodeCommand &gcmd);
    void cmd_SET_VELOCITY_LIMIT(GCodeCommand &gcmd);
    void cmd_RESET_PRINTER_PARAM(GCodeCommand &gcmd);
    void cmd_M204(GCodeCommand &gcmd);
    void cmd_M222(GCodeCommand &gcmd);
    void cmd_M223(GCodeCommand &gcmd);
    void cmd_M211(GCodeCommand &gcmd);

private:
};

typedef enum
{
    WAIT_MOVE_COMPLETED,  //打印中等待移动完成
} wait_move_state_s;
typedef void (*M400_state_callback_t)(int event);
int M400_register_state_callback(M400_state_callback_t state_callback);
int M400_unregister_state_callback(M400_state_callback_t state_callback);
int M400_state_callback_call(int event);

#endif