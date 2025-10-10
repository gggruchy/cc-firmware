#ifndef MCU_IO_H
#define MCU_IO_H

#include "mcu.h"
#include "pins.h"
#include "stepper.h"
class MCU_trsync
{
private:
    
public:
    MCU_trsync(MCU* mcu, trdispatch *trdispatch);
    ~MCU_trsync();

    std::mutex m_trigger_mtx; 
    MCU* m_mcu;
    uint32_t m_oid;
    uint64_t m_home_end_clock;
    CommandWrapper* m_trsync_start_cmd;
    CommandWrapper* m_trsync_set_timeout_cmd;
    CommandWrapper* m_trsync_trigger_cmd;
    CommandWrapper* m_trsync_query_cmd;
    CommandWrapper* m_stepper_stop_cmd;
    command_queue *m_cmd_queue;
    trdispatch_mcu *m_trdispatch_mcu;
    trdispatch *m_trdispatch;
    std::vector<MCU_stepper*> m_steppers;

    MCU* get_mcu();
    int get_oid();
    command_queue* get_command_queue();
    void add_stepper(MCU_stepper* stepper);
    std::vector<MCU_stepper*> get_steppers();
    void build_config(int para);
    void shutdown();
    void handle_trsync_state(ParseResult params);
    void start(double print_time, double expire_timeout);
    void start_z(double print_time, double expire_timeout);
    void set_home_end_time(double home_end_time);
    uint32_t stop();
    uint32_t stop_z();
};

#define TRSYNC_TIMEOUT 0.025
#define TRSYNC_SINGLE_MCU_TIMEOUT 0.250
#define RETRY_QUERY 1.000
class MCU_endstop
{
public:
    MCU_endstop(MCU* mcu, pinParams* params);
    ~MCU_endstop();
    void add_stepper(MCU_stepper* stepper);
    std::vector<MCU_stepper*> get_steppers();
    MCU* get_mcu();
    void build_config(int para);
    void home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered = true);
    void home_start_z(double print_time, double sample_time, int sample_count, double rest_time, bool triggered = true);
    int home_wait(double home_end_time);
    int home_wait_z(double home_end_time);
    int query_endstop(double print_time);

    MCU_trsync *m_trsync;
    std::vector<MCU_trsync*> m_trsyncs;
    int m_oid;
    MCU* m_mcu;
    int m_pin;
    int m_pullup;
    int m_invert;
    struct trdispatch * m_trdispatch;
    uint64_t m_rest_ticks;
    CommandWrapper* m_home_cmd;
    CommandQueryWrapper* m_query_cmd;
    command_queue *m_cmd_queue;
private:
    
};


class MCU_digital_out{
public:
    MCU_digital_out(MCU* mcu, pinParams* params);
    ~MCU_digital_out();

    MCU *m_mcu;
    int m_oid;
    int m_pin;
    CommandWrapper* m_set_cmd;
    int m_invert;
    int m_start_value;
    int m_shutdown_value;
    bool m_is_static;
    double m_max_duration;
    double m_last_clock;
    command_queue * m_cmd_queue;
    // CommandWrapper m_set_command;

    MCU* get_mcu();
    void setup_max_duration(double max_duration);
    void setup_start_value(int start_value, int shutdown_value, bool is_static = false);
    void build_config(int para);
    void set_digital(double print_time, int value);

private:

};

class MCU_pwm{
public:
    MCU_pwm(MCU* mcu, pinParams* params);
    ~MCU_pwm();

    MCU *m_mcu;
    command_queue * m_cmd_queue;
    std::function<void(int)> callback_function;
    bool m_hardware_pwm;
    double m_cycle_time;
    double m_max_duration;
    int m_oid;
    int m_pin;
    int m_invert;
    double m_start_value;
    double m_shutdown_value;
    bool m_is_static;
    uint64_t m_last_clock;
    uint32_t m_last_cycle_ticks;
    double m_pwm_max;
    CommandWrapper* m_set_cmd;
    CommandWrapper* m_set_cycle_ticks;

    MCU* get_mcu();
    void setup_max_duration(double max_duration);
    void setup_cycle_time(double cycle_time, bool hardware_pwm = false);
    void setup_start_value(double start_value, double shutdown_value, bool is_static = false);
    void build_config(int para);
    void set_pwm(double print_time, double value, double cycle_time = 0);
private:


};



class MCU_adc
{
private:
    
public:
    uint32_t m_pin;
    MCU_adc(MCU* mcu, pinParams* pin_params);
    ~MCU_adc();

    MCU* m_mcu;
    double m_min_sample;
    double m_max_sample;
    double m_sample_time;
    double m_report_time;
    int m_sample_count;     //8
    int m_range_check_count;
    uint64_t m_report_clock;
    std::pair<double, double> m_last_state;
    int m_oid;
    std::function<void(double, double)> m_callback;
    double m_inv_max_adc;       //1.0/(8*4095)

    MCU* get_mcu();
    void setup_minmax(double sample_time, int sample_count, double minval = 0, double maxval = 1, int range_check_count = 0);
    void setup_adc_callback(double report_time, std::function<void(double, double)> callback);
    std::pair<double, double> get_last_value();
    void build_config(int para);
    void handle_analog_in_state(ParseResult &response_params);
};

#endif