#ifndef __EXTRUDER_H__
#define __EXTRUDER_H__
extern "C"
{
    #include "chelper/trapq.h"
    #include "chelper/kin_extruder.h"
}

#include "move.h"
#include "stepper.h"
#include "heaters.h"
#include "adc_temperature.h"
#include "thermistor.h"
#include "stepper_enable.h"

void add_printer_extruder();

class PrinterExtruder{
public:
    PrinterExtruder(std::string section_name, int extruder_num);
    ~PrinterExtruder();
    bool m_wait;
    std::string m_name;
    Heater* m_heater;
    MCU_stepper *m_stepper;
    double m_nozzle_diameter;
    double m_filament_area;
    double m_max_extrude_ratio;
    double m_max_e_velocity;        //E轴最大速度
    double m_max_e_accel;   //E轴最大加速度
    double m_max_e_dist;
    double m_instant_corner_v;      //1  突然只有E轴运动的时候，XY最大拐角速度
    double m_pressure_advance;
    double m_pressure_advance_smooth_time;
    trapq *m_trapq;
    stepper_kinematics * m_sk_extruder;
    bool m_start_print_flag;
    bool m_flow_calibration;
    double m_last_speed;
    ReactorTimerPtr m_fan_timer;

    std::atomic_bool load_filament_flag;

    std::string m_cmd_SET_PRESSURE_ADVANCE_help;
    std::string m_cmd_SET_E_STEP_DISTANCE_help;     
    std::string m_cmd_ACTIVATE_EXTRUDER_help;

    void cmd_RESET_EXTRUDER(GCodeCommand &gcode);
    void reset_extruder();
    void update_move_time(double flush_time);
    void set_pressure_advance(double pressure_advance, double smooth_time);
    void get_status(double eventtime);
    std::string get_name();    
    trapq* get_trapq();
    Heater* get_heater();     
    void sync_stepper(MCU_stepper* stepper);
    void stats(double eventtime);
    bool check_move(Move& move);
    double calc_junction(Move& prev_move, Move& move);
    void move(double print_time, Move& move);
    double find_past_position(double print_time);
    double set_fan_callback(double eventtime);
    void unregister_fan_callback();
    void cmd_M104(GCodeCommand& gcmd);
    void cmd_M109(GCodeCommand& gcmd);  
    void cmd_default_SET_PRESSURE_ADVANCE(GCodeCommand& gcmd);  
    void cmd_M900(GCodeCommand &gcmd);
    void cmd_SET_PRESSURE_ADVANCE(GCodeCommand& gcmd);        
    void cmd_SET_E_STEP_DISTANCE(GCodeCommand&  gcmd);
    void cmd_ACTIVATE_EXTRUDER(GCodeCommand& gcmd);
    void cmd_FLOW_CALIBRATION(GCodeCommand& gcmd);
    void cmd_LOAD_FILAMENT(GCodeCommand& gcmd);
    void cmd_SET_MIN_EXTRUDE_TEMP(GCodeCommand& gcmd);
private:
};

class DummyExtruder{
    private:
        DummyExtruder();
        ~DummyExtruder();
    public:
        void update_move_time(double flush_time);
        void check_move(Move* move);
        double find_past_position(double print_time);
        double calc_junction(Move* prev_move, Move* move);   
        std::string get_name();   
        void get_heater();


};

enum
{
    LOAD_FILAMENT_STATE_START_HEAT = 0,
    LOAD_FILAMENT_STATE_HEAT_FINISH = 1,
    LOAD_FILAMENT_STATE_START_EXTRUDE = 2,
    LOAD_FILAMENT_STATE_STOP_EXTRUDE = 3,
    LOAD_FILAMENT_STATE_COMPLETE = 4,
};
typedef void (*load_filament_state_callback_t)(int state);
int load_filament_register_state_callback(load_filament_state_callback_t state_callback);
int load_filament_state_callback_call(int state);
#endif
