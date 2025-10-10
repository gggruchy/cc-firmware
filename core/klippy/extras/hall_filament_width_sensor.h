#ifndef HALL_FILAMENT_WIDTH_SENSOR_H
#define HALL_FILAMENT_WIDTH_SENSOR_H
#include "gcode.h"
#include <queue>
#include "filament_switch_sensor.h"
#include "mcu_io.h"
#include "reactor.h"

struct hall_status
{
    double Diameter;
    double Raw;
    bool is_active;
};

class HallFilamentWidthSensor
{
private:
    
public:
    std::string m_pin1;
    std::string m_pin2;
    double m_dia1;
    double m_dia2;
    int m_rawdia1;
    int m_rawdia2;
    int m_MEASUREMENT_INTERVAL_MM;
    double m_nominal_filament_dia;
    double m_measurement_delay;
    double m_measurement_max_difference;
    double m_max_diameter;
    double m_min_diameter;
    double m_diameter;
    bool m_is_active;
    double m_runout_dia;
    double m_is_log;
    bool m_use_current_dia_while_delay;
    // filament array [position, filamentWidth]
    std::queue<std::pair<double, double>> m_filament_array;
    double m_lastFilamentWidthReading;
    double m_lastFilamentWidthReading2;
    double m_firstExtruderUpdatePosition;
    double m_filament_width;
    MCU_adc* m_mcu_adc;
    MCU_adc* m_mcu_adc2; 
    ReactorTimerPtr m_extrude_factor_update_timer;
    RunoutHelper* m_runout_helper;


public:
    HallFilamentWidthSensor(std::string section_name);
    ~HallFilamentWidthSensor();
    void handle_ready();
    void adc_callback(double read_time, double read_value);
    void adc2_callbcak(double read_time, double read_value);
    void update_filament_array(double last_epos);
    double extrude_factor_update_event(double eventtime);
    void cmd_M407(GCodeCommand &gcmd);
    void cmd_ClearFilamentArray(GCodeCommand &gcmd);
    void cmd_M405(GCodeCommand &gcmd);
    void cmd_M406(GCodeCommand &gcmd);
    void cmd_Get_Raw_Values(GCodeCommand &gcmd);
    struct hall_status get_status();
    void cmd_log_enable(GCodeCommand &gcmd);
    void cmd_log_disable(GCodeCommand &gcmd);


};




#endif