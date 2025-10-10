#ifndef FORCE_MOVE_H
#define FORCE_MOVE_H

#include <math.h>
#include "stdlib.h"
#include "gcode.h"
extern "C"{
    #include "../chelper/trapq.h"
    #include "kin_cartesian.h"
}

#include "stepper.h"

#define FORCE_MOVE_STEPPER_NUM 4

#define BUZZ_DISTANCE 1.0
#define BUZZ_VELOCITY (BUZZ_DISTANCE / 0.250)
#define BUZZ_RADIANS_DISTANCE (1.0 / 180 * M_PI) 
#define BUZZ_RADIANS_VELOCITY  (BUZZ_RADIANS_DISTANCE / 0.250)
#define STALL_TIME 0.100

struct calc_move_time_ret{
    double axis_r;          //运动方向
    double accel_t;         //匀加速时间 匀减速时间
    double cruise_t;         //匀速时间
    double speed;         //最大速度
};

struct calc_move_time_ret calc_move_time(double dist, double speed, double accel);
class ForceMove{
private:

public:
    ForceMove(std::string section_name);
    ~ForceMove();
    struct trapq *trapq;
    struct stepper_kinematics *sk;
    MCU_stepper *steppers[FORCE_MOVE_STEPPER_NUM];

    std::string cmd_SET_KINEMATIC_POSITION_help;
    std::string cmd_FORCE_MOVE_help;
    std::string cmd_STEPPER_BUZZ_help;

    void register_stepper(MCU_stepper *stepper);
    MCU_stepper *lookup_stepper(std::string name);
    bool force_enable(MCU_stepper *stepper);
    void restore_enable(MCU_stepper *stepper, bool was_enable);
    void manual_move(MCU_stepper *stepper, double dist, double speed, double accel = 0);
    MCU_stepper *_lookup_stepper(GCodeCommand &gcmd);
    void cmd_STEPPER_BUZZ(GCodeCommand &gcmd);
    void cmd_FORCE_MOVE(GCodeCommand &gcmd);
    void cmd_SET_KINEMATIC_POSITION(GCodeCommand &gcmd);
};
#endif