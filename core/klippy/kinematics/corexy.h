#ifndef COREXYKINEMATICS_H
#define COREXYKINEMATICS_H
#include "kinematics_base.h"

class CoreXYKinematics : public Kinematics
{
    private:
        
    public:
        CoreXYKinematics(std::string section_name);
        ~CoreXYKinematics();
    public:
        std::vector<std::vector<MCU_stepper*>> get_steppers();
        std::vector<double> calc_position(std::map<std::string, double> stepper_positions);
        void set_position(double newpos[3], std::vector<int> homing_axes = std::vector<int>());
        void note_z_not_homed();
        void home(Homing &homing_state);
        void motor_off(double print_time);
        bool check_endstops(Move& move);
        bool check_endstops(std::vector<double> &end_pos);
        bool check_move(Move& move);
        std::map<std::string, std::string> get_status(double eventtime);

        void _activate_carriage(int carriage){};
        void cmd_SET_DUAL_CARRIAGE(GCodeCommand& gcmd){};
        bool home_axis(Homing &homing_state, int axis, PrinterRail *rail){};
        std::vector<double> _actuator_to_cartesian(std::vector<double> spos){};

};
#endif