// PrinterExtruder stepper pulse time generation
//
// Copyright (C) 2018-2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memset
#include "compiler.h" // __visible
#include "itersolve.h" // struct stepper_kinematics
#include "pyhelper.h" // errorf
#include "trapq.h" // move_get_distance

struct extruder_stepper {
    struct stepper_kinematics sk;
    double pressure_advance, half_smooth_time, inv_half_smooth_time2;
};

void extruder_set_pressure_advance(struct stepper_kinematics *sk, double pressure_advance, double smooth_time);
struct stepper_kinematics * extruder_stepper_alloc(void);