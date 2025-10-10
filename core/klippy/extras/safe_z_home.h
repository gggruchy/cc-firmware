#ifndef SAFE_Z_HOME_H
#define SAFE_Z_HOME_H
#include "gcode.h"

class SafeZHoming{
    private:

    public:
        SafeZHoming(std::string section_name);
        ~SafeZHoming();

        double m_home_x_pos;
        double m_home_y_pos;
        double m_z_lift;
        double m_z_hop;
        double m_z_hop_speed;
        double m_max_z;
        double m_speed;
        int temp;
        bool m_move_to_previous;
        std::function<void(GCodeCommand&)> m_prev_G28;

        void cmd_G28(GCodeCommand& gcmd);
        void cmd_CUSTOM_ZERO(GCodeCommand& gcmd);
        void cmd_Z_AXIS_OFF_LIMIT_ACTION(GCodeCommand& gcmd);
};
#endif