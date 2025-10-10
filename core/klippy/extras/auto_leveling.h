#ifndef AUTO_LEVELING_H
#define AUTO_LEVELING_H
#include "mcu_io.h"
#include <unordered_map>
#include "gcode.h"
#include "print_rail.h"
#include "Define.h"
#include "configfile.h"
class AutoLeveling
{
public:
    double bed_mesh_temp;
    double extruder_temp;
    double move_speed;
    double extrude_length;
    double extrude_speed;
    double extruder_pullback_length;
    double extruder_pullback_speed;
    double extruder_cool_down_temp;
    double target_spot_x;
    double target_spot_y;
    double lifting_after_completion;
    double extruder_wipe_z;
    bool enable_fusion_trigger;
    bool enable_test_mode;
    bool m_enable_fast_probe; 
    bool auto_levelling_doing;
    std::string extrude_feed_gcode_str;
    std::vector<std::string> extrude_feed_gcode;
    AutoLeveling(std::string section_name);
    ~AutoLeveling();
    void cmd_G29(GCodeCommand &gcmd);
    void cmd_G29_1(GCodeCommand &gcmd);
    void cmd_WIPE_NOZZLE(GCodeCommand &gcmd);
    void cmd_M729(GCodeCommand &gcmd);
    void cmd_CALIBRATE_Z_OFFSET(GCodeCommand &gcmd);
    void cmd_LIFT_HOT_BED(GCodeCommand &gcmd);
    void wipe_nozzle();
    void handle_homing_status(std::string event, bool homing_status);
};
void auto_leveling_loop(void);
const char *get_auto_level_cur_state_name(void);
const char *get_auto_level_last_state_name(void);

enum
{
    AUTO_LEVELING_STATE_START = 0,
    AUTO_LEVELING_STATE_START_PREHEAT = 1,
    AUTO_LEVELING_STATE_START_EXTURDE = 2,
    AUTO_LEVELING_STATE_START_PROBE = 3,
    AUTO_LEVELING_STATE_FINISH = 4,
    AUTO_LEVELING_STATE_ERROR = 5,
    AUTO_LEVELING_STATE_RESET = 6,
};
typedef void (*auto_leveling_state_callback_t)(int state);
int auto_leveling_register_state_callback(auto_leveling_state_callback_t state_callback);
int auto_leveling_unregister_state_callback(auto_leveling_state_callback_t state_callback);
int auto_leveling_state_callback_call(int state);
#endif