#ifndef SKEWCORRECTION_H
#define SKEWCORRECTION_H
#include "gcode_move.h"

class SkewCorrection{
    private:

    public:
        SkewCorrection();
        ~SkewCorrection(); 

    public:
        std::string m_cmd_GET_CURRENT_SKEW_help;
        std::string m_cmd_CALC_MEASURED_SKEW_help;
        std::string m_cmd_SET_SKEW_help;
        std::string m_cmd_SKEW_PROFILE_help;
        std::string m_name;
        double m_xy_factor;
        double m_xz_factor;
        double m_yz_factor;
        std::function<bool(std::vector<double>&, double)> m_next_move_transform;
        std::function<std::vector<double>()> m_next_get_position_transform;
        std::map<std::string, double>  m_skew_profiles_xy;
        std::map<std::string, double>  m_skew_profiles_xz;
        std::map<std::string, double>  m_skew_profiles_yz;
    public:
        void _handle_connect();
        void _load_storage();
        std::vector<double> calc_skew(std::vector<double> pos);
        std::vector<double> calc_unskew(std::vector<double> pos);
        std::vector<double> get_position();   
        bool move(std::vector<double> newpos, double speed);
        void _update_skew(double xy_factor, double xz_factor, double yz_factor);
        void cmd_GET_CURRENT_SKEW(GCodeCommand& gcmd);
        void cmd_CALC_MEASURED_SKEW(GCodeCommand& gcmd);  
        void cmd_SET_SKEW(GCodeCommand& gcmd);;
        void cmd_SKEW_PROFILE(GCodeCommand& gcmd);
};

#endif