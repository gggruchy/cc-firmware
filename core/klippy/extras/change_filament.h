#ifndef _CHANGE_FILAMENT_H_
#define _CHANGE_FILAMENT_H_

#include "gcode.h"

class ChangeFilament
{
private:
    double m_max_x;
    double m_max_y;
    double m_min_x;
    double m_min_y;
    double m_extrude_fan_speed;
    bool m_active;
    double m_pos_x;
    double m_pos_y;
    bool m_busy;     //是否正在执行进退料（true:是/false:否）
    bool m_check_move_ignore;
    bool feed_out_state;
public:
    ChangeFilament(std::string section_name);
    ~ChangeFilament();

    bool is_active();
    bool is_feed_busy();
    bool check_move(std::vector<double> &pos);
    void cmd_MOVE_TO_EXTRUDE(GCodeCommand &gcmd);
    void cmd_CHANGE_FILAMENT_SET_ACTIVE(GCodeCommand &gcmd);
    void cmd_CHANGE_FILAMENT_SET_BUSY(GCodeCommand &gcmd);
    void cmd_CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE(GCodeCommand &gcmd);
    void cmd_CUT_OFF_FILAMENT(GCodeCommand &gcmd);
    void cmd_EXTRUDE_FILAMENT(GCodeCommand &gcmd);
    void set_feet_out_state(bool state);
    bool get_feet_out_state();
};


enum
{
    CHANGE_FILAMENT_STATE_CHECK_MOVE = 0,
    CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE,
    CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT,
    CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT,
};

enum
{
    FEED_TYPE_IN_FEED,
    FEED_TYPE_OUT_FEED,
};
    
typedef void (*change_filament_state_callback_t)(int state);
int change_filament_register_state_callback(change_filament_state_callback_t state_callback);
int change_filament_unregister_state_callback(change_filament_state_callback_t state_callback);
int change_filament_state_callback_call(int state);

#endif