#ifndef PAUSE_RESUME_H
#define PAUSE_RESUME_H

#include "gcode.h"
#include "virtual_sdcard.h"
#include "webhooks.h"

typedef struct PauseResume_status_tag
{
    bool is_paused;
    bool is_busying;
    double extruder_temp;
    double hotbed_temp;
    double model_fan_speed;
    double model_helper_fan_speed;
}PauseResume_status_t;

class PauseResume
{
private:
    
public:
    double m_recover_velocity;
    VirtualSD* m_v_sd;
    bool m_is_paused;
    bool m_sd_paused;
    bool m_wait;
    bool m_pause_command_sent;
    bool m_is_busying;    //正在执行PAUSE/CANCEL/RESUME 命令

    double m_save_extruder_temp;
    double m_save_hotbed_temp;
    double m_save_model_fan_speed;
    double m_save_model_helper_fan_speed; 
    
    int m_last_pause;
    ReactorTimerPtr m_pause_timer;
    double m_pause_abs_pos_x;
    double m_pause_abs_pos_y;
    double m_pause_rel_pos_z;
    double m_pause_move_speed;
    double m_pause_move_z_speed;
    double m_pause_extruder_timeout;
    double m_pause_extruder_temp;
    bool m_last_extruder_temp;
    double m_pause_fan_timeout;
    double m_pause_fan_speed;
    double m_resume_extrude_fan_speed;
    bool m_last_fan_speed;
public:
    PauseResume(std::string section_name);
    ~PauseResume();
    void handle_connect();
    // void handle_cancel_request(WebRequest* web_request);
    // void handle_pause_request(WebRequest* web_request);
    // void handle_resume_request(WebRequest* web_request);
    PauseResume_status_t get_status();
    double get_save_extruder_temp(void);
    bool is_sd_active();
    void clear_pause_timer();
    double pause_callback(double eventtime);
    void send_pause_command();
    void cmd_PAUSE(GCodeCommand &gcmd);
    void send_resume_command();
    void cmd_RESUME(GCodeCommand &gcmd);
    void cmd_CLEAR_PAUSE(GCodeCommand &gcmd);
    void cmd_CANCEL_PRINT(GCodeCommand &gcmd);
    void cmd_STOP_RESUME(GCodeCommand &gcmd);
    void cmd_STOP_PRINT(GCodeCommand &gcmd);
    void cmd_M600(GCodeCommand &gcmd);
};




#endif