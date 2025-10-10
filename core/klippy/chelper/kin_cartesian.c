// Cartesian kinematics stepper pulse time generation  笛卡尔运动学步进脉冲时间生成
//
// Copyright (C) 2018-2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdlib.h> // malloc
#include <string.h> // memset
#include "compiler.h" // __visible
#include "itersolve.h" // struct stepper_kinematics
#include "pyhelper.h" // errorf
#include "trapq.h" // move_get_coord

static double
cart_stepper_x_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
    
    return move_get_coord(m, move_time).x;
}

static double
cart_stepper_y_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
     
    return move_get_coord(m, move_time).y;
}

static double
cart_stepper_z_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
    
    return move_get_coord(m, move_time).z;
}

struct stepper_kinematics * __visible
cartesian_stepper_alloc(char axis)
{
    
    struct stepper_kinematics *sk = (struct stepper_kinematics *)malloc(sizeof(*sk));
    memset(sk, 0, sizeof(*sk));
    if (axis == 'x') {
        sk->calc_position_cb = cart_stepper_x_calc_position;
        sk->active_flags = AF_X;
        sk->axis=0;
    } else if (axis == 'y') {
        sk->calc_position_cb = cart_stepper_y_calc_position;
        sk->active_flags = AF_Y;
        sk->axis=1;
    } else if (axis == 'z') {
        sk->calc_position_cb = cart_stepper_z_calc_position;
        sk->active_flags = AF_Z;
        sk->axis=2;
    }
    return sk;
}

static double
cart_reverse_stepper_x_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    
    return -move_get_coord(m, move_time).x;
}

static double
cart_reverse_stepper_y_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    
    return -move_get_coord(m, move_time).y;
}

static double
cart_reverse_stepper_z_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    
    return -move_get_coord(m, move_time).z;
}

struct stepper_kinematics * __visible
cartesian_reverse_stepper_alloc(char axis)
{
    
    struct stepper_kinematics *sk = (struct stepper_kinematics *)malloc(sizeof(*sk));
    memset(sk, 0, sizeof(*sk));
    if (axis == 'x') {
        sk->calc_position_cb = cart_reverse_stepper_x_calc_position;
        sk->active_flags = AF_X;
        sk->axis=0;
    } else if (axis == 'y') {
        sk->calc_position_cb = cart_reverse_stepper_y_calc_position;
        sk->active_flags = AF_Y;
        sk->axis=1;
    } else if (axis == 'z') {
        sk->calc_position_cb = cart_reverse_stepper_z_calc_position;
        sk->active_flags = AF_Z;
        sk->axis=2;
    }
    return sk;
}
