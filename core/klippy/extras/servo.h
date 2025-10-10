#ifndef SERVO_H
#define SERVO_H

#include "mcu_io.h"
#include "gcode.h"

typedef struct servo_status_tag{
    double value;
}servo_status_t;

class PrinterServo{
    private:

    public:
        PrinterServo(std::string section_name);
        ~PrinterServo();

        double m_min_width;
        double m_max_width;
        double m_max_angle;
        double m_angle_to_width;
        double m_width_to_value;
        double m_last_value;
        double m_last_value_time;
            
        MCU_pwm *m_mcu_servo;
        std::string m_cmd_SET_SERVO_help;

        servo_status_t get_status(double eventtime);
        void _set_pwm(double print_time, double value);
        double _get_pwm_from_angle(double angle);
        double _get_pwm_from_pulse_width(double width);
        void cmd_SET_SERVO(GCodeCommand& gcmd);
};
#endif