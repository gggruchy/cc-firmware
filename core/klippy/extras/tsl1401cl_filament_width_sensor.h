#ifndef TSL1401CL_FILAMENT_WIDTH_SENSOR_H
#define TSL1401CL_FILAMENT_WIDTH_SENSOR_H

#include <string>
#include <vector>
#include "gcode.h"
#include <math.h>
#include <algorithm>
#include <queue>
#include "mcu_io.h"
#include "reactor.h"

class FilamentWidthSensor
{
private:
    
public:
    std::string m_pin;
    double m_nominal_filament_dia;
    double m_measurement_delay;
    double m_measurement_max_difference;
    double m_max_diameter;
    double m_min_diameter;
    bool m_is_active;
    // filament array [position, filamentWidth]
    std::queue<std::pair<double, double>> m_filament_array;
    double m_lastFilamentWidthReading;
    MCU_adc* m_mcu_adc;
    ReactorTimerPtr m_extrude_factor_update_timer;


public:
    FilamentWidthSensor(std::string section_name);
    ~FilamentWidthSensor();
    void handle_ready();
    void adc_callback(double readtime, double read_value);
    void update_filament_array(double last_epos);
    double extrude_factor_update_event(double eventtime);
    void cmd_M407(GCodeCommand &gcmd);
    void cmd_ClearFilamentArray(GCodeCommand &gcmd);
    void cmd_M405(GCodeCommand &gcmd);
    void cmd_M406(GCodeCommand &gcmd);    
};




#endif