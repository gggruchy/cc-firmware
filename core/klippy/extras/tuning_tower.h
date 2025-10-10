#ifndef TUNING_TOWER_H
#define TUNING_TOWER_H
#include <string>
#include <functional>
#include <vector>
#include "gcode.h"

class TuningTower{
    private:

    public:
        TuningTower(std::string section_name);
        ~TuningTower();
        std::function<bool(std::vector<double>&, double)> m_normal_move_transform;
        std::function<std::vector<double>()> m_normal_get_position_transform;
        std::vector<double> m_last_position;
        double m_last_z;
        double m_start;
        double m_factor;
        double m_band;
        double m_last_command_value;
        std::string m_command_fmt;
        // Register command
        std::string m_cmd_TUNING_TOWER_help;

    public:
        void cmd_TUNING_TOWER(GCodeCommand& gcmd);
        std::vector<double> get_position();
        double calc_value(double z);
        bool move(std::vector<double> newpos, double speed);
        void end_test();
        bool is_active();

};


#endif