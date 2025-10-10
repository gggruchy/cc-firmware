#ifndef MANUAL_PROBE_H
#define MANUAL_PROBE_H
#include "gcode.h"
#include <functional>

void verify_no_manual_probe();
class ManualProbe{
    private:

    public:
        ManualProbe(std::string section_name);
        ~ManualProbe();

        std::string m_cmd_MANUAL_PROBE_help;
        std::string m_cmd_Z_ENDSTOP_CALIBRATE_help;
        double m_z_position_endstop;

        void manual_probe_finalize(std::vector<double> kin_pos);
        void cmd_MANUAL_PROBE(GCodeCommand& gcmd); 
        void z_endstop_finalize(std::vector<double> kin_pos);
        void cmd_Z_ENDSTOP_CALIBRATE(GCodeCommand& gcmd);
        void verify_no_manual_probe();
};

class ManualProbeHelper{
    private:

    public:
        ManualProbeHelper(GCodeCommand& gcmd, std::function<void(std::vector<double>)> finalize_callback);
        ~ManualProbeHelper();

        std::function<void(std::vector<double>)> m_finalize_callback;
        double m_speed;
        std::vector<double> m_past_positions;
        std::vector<double> m_last_toolhead_pos;
        std::vector<double> m_last_kinematics_pos;
        // Register commands
        std::string m_cmd_ACCEPT_help;
        std::string m_cmd_ABORT_help;
        std::string m_cmd_TESTZ_help;
        std::vector<double> m_start_position;

        std::vector<double> get_kinematics_pos();
        void move_z(double z_pos);
        void report_z_status(bool warn_no_change=false, double prev_pos = DBL_MIN);
        void cmd_ACCEPT(GCodeCommand& gcmd);
        void cmd_ABORT(GCodeCommand& gcmd);
        void cmd_TESTZ(GCodeCommand& gcmd);
        void finalize(bool success);
};

#endif
