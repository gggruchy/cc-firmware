#ifndef FAN_H
#define FAN_H

#include "mcu_io.h"
#include "gcode.h"
#include "pulse_counter.h"
#include "hl_queue.h"

struct fan_state
{
    double speed;
    double rpm;
};

class FanTachometer
{
private:
    FrequencyCounter *m_freq_counter;
    int m_ppr;

public:
    FanTachometer(std::string section_name);
    ~FanTachometer();

    double get_status(double eventtime);
};
class Fan
{
private:
    double m_off_below;
    double m_max_power;
    double fan_value;
    double m_last_fan_value = 0.;
    double m_last_fan_time = 0.;
    double m_kick_start_time;
    double m_fan_min_time;
    double m_startup_voltage;
    FanTachometer *m_tachometer;
    int m_retry_times;

public:
    MCU_pwm *m_mcu_fan;
    double m_current_speed;
    // FanTachometer *m_tachometer;
    std::string m_name;
    int m_id;
    Fan(std::string section_name, double default_shutdown_speed = 0.);
    ~Fan();
    std::mutex m_lock;
    MCU *get_mcu();
    void set_speed(double print_time, double value = 0);
    double get_startup_voltage(void);
    void set_speed_from_command(double value);
    void set_all_fan_from_command(int type, double value);
    void handle_request_restart(double print_time);
    struct fan_state get_status(double eventtime);
    double check_event(double eventtime);
    void fan_pwm_value_setting(int oid);
};

class PrinterFan
{
private:
    int fan_silence_speed;
    int fan_normal_speed;
    int fan_crazy_speed;
public:
    Fan *m_fan;
    PrinterFan(std::string section_name);
    ~PrinterFan();
    struct fan_state get_status(double eventtime);
    void cmd_M106(GCodeCommand &gcmd);
    void cmd_M107(GCodeCommand &gcmd);
    void cmd_SET_FAN_SPEED(GCodeCommand &gcmd);
    int get_fan_speed_mode(int mode_id);
    // void reset(Fan *fan);
#if ENABLE_MANUTEST
    double get_speed();
#endif
};

enum
{
    FAN_SILENCE_MODE,
    FAN_NORMAL_MODE,
    FAN_CRAZY_MODE,
};

typedef enum
{
    FAN_STATE_MODEL_ERROR = 0,
    FAN_STATE_BOARD_COOLING_FAN_ERROR = 1,
    FAN_STATE_HOTEND_COOLING_FAN_ERROR = 2,

} ui_event_fan_state_id_t;
typedef void (*fan_state_callback_t)(int state);
void check_fan_init(hl_queue_t *queue, fan_state_callback_t state_callback);
int fan_register_state_callback(fan_state_callback_t state_callback);
int fan_state_callback_call(int state);
bool is_fan_state_arrayempty(void);

#endif
