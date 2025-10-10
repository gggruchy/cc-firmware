#ifndef GCODE_MOVE_H
#define GCODE_MOVE_H
#include "gcode.h"
#include "print_rail.h"
#include "homing.h"

enum axis
{
    AXIS_X = 0,
    AXIS_Y,
    AXIS_Z,
    AXIS_E
};

typedef struct gcode_move_state_tag{
    bool absolute_coord;
    bool absolute_extrude;
    double speed;
    double speed_factor;
    double extrude_factor;
    std::vector<double> last_position;
    std::vector<double> homing_position;
    std::vector<double> base_position;
}gcode_move_state_t;

class GCodeMove
{
private:
    
public:
    GCodeMove(std::string section_name);
    ~GCodeMove();

    void _handle_ready();   
    void _handle_shutdown();
    void _handle_activate_extruder();
    void _handle_home_rails_end(Homing* homing_state, std::vector<PrinterRail*> rails); 
    gcode_move_state_t get_status(double eventtime);
    void reset_last_position();
    
    std::vector<double> get_gcode_position();
    double get_gcode_speed();
    double get_gcode_speed_override();
    gcode_move_state_t get_gcode_state(std::string name);
    std::string cmd_GET_POSITION_help;
    void cmd_G1(GCodeCommand& gcmd);
    void cmd_MANUAL_MOVE(GCodeCommand &gcmd);
    void cmd_G20(GCodeCommand& gcmd);
    void cmd_G21(GCodeCommand& gcmd);
    void cmd_G90(GCodeCommand& gcmd);
    void cmd_G91(GCodeCommand& gcmd);
    void cmd_G92(GCodeCommand& gcmd);
    void cmd_M82(GCodeCommand& gcmd);
    void cmd_M83(GCodeCommand& gcmd);
    void cmd_M114(GCodeCommand& gcmd);
    void cmd_M220(GCodeCommand& gcmd);
    void cmd_M221(GCodeCommand& gcmd);

    void cmd_SET_GCODE_OFFSET(GCodeCommand &gcmd);
    void cmd_SAVE_GCODE_STATE(GCodeCommand &gcmd);
    void cmd_RESTORE_GCODE_STATE(GCodeCommand &gcmd);
    void cmd_GET_POSITION(GCodeCommand& gcmd);

    std::function<std::vector<double>()> set_get_position_transform(std::function<std::vector<double>()> get_position_fun, bool force=false);
    std::function<bool(std::vector<double>&, double)> set_move_transform(std::function<bool(std::vector<double>&, double)> move_fun, bool force=false);

public:
    std::map<std::string, gcode_move_state_t> saved_states;
    bool m_is_printer_ready;
    bool m_absolute_coord;
    bool m_absolute_extrude;
    std::vector<double> m_base_position;
    std::vector<double> m_last_position;
    std::vector<double> m_homing_position;
    std::atomic<double> m_speed;
    std::atomic<double> m_speed_factor;
    double m_extrude_factor;
    std::function<bool(std::vector<double>&, double)> m_move_transform;
    std::function<std::vector<double>()> m_get_position_transform;//如果参数有[bed_mesh]，则这个函数指向BedMesh类里面的get_position函数
};



#endif