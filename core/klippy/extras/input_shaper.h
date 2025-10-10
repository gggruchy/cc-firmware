#ifndef __INPUTSHAPER__H__
#define __INPUTSHAPER__H__

#include <string>
#include <vector>
#include "gcode.h"

extern "C"
{
    #include "../chelper/kin_shaper.h"
}

class InputShaper
{
private:
    std::map<std::string, int> m_shapers;
    double m_old_delay;
    int m_shaper_type_x;
    int m_shaper_type_y;
    double m_saved_shaper_freq_x;
    double m_saved_shaper_freq_y;
    double m_shaper_freq_x;
    double m_shaper_freq_y;
    double m_damping_ratio_x;
    double m_damping_ratio_y;
    std::string m_cmd_SET_INPUT_SHAPER_help;
    std::vector<stepper_kinematics *> m_stepper_kinematics;
    std::vector<stepper_kinematics *> m_orig_stepper_kinematics;
    
public:
    enum SHAPER_TYPE{
        zv,
        zvd,
        mzv,
        ei,
        hump_ei2,
        hump_ei3
    };
    InputShaper(std::string section_name);
    ~InputShaper();
    void connect();
    void set_input_shaper(int shaper_type_x, int shaper_type_y,
                        double shaper_freq_x, double shaper_freq_y,
                        double damping_ratio_x, double damping_ratio_y);
    void disable_shaping();
    bool enable_shaping();
    int parse_shaper(std::string shaper_type_str);
    void cmd_SET_INPUT_SHAPER(GCodeCommand& gcmd);
};


#endif // !__INPUTSHAPER__H__
