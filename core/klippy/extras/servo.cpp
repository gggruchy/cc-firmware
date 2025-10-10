#include "servo.h"
#include "klippy.h"

#define SERVO_SIGNAL_PERIOD 0.020
#define PIN_MIN_TIME 0.100

PrinterServo::PrinterServo(std::string section_name)
{
    m_min_width = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "minimum_pulse_width", .001, DBL_MIN, DBL_MAX, 0., SERVO_SIGNAL_PERIOD);
    m_max_width = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "maximum_pulse_width", .002, DBL_MIN, DBL_MAX, m_min_width, SERVO_SIGNAL_PERIOD);
    m_max_angle = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "maximum_servo_angle", 180.);
    m_angle_to_width = (m_max_width - m_min_width) / m_max_angle;
    m_width_to_value = 1. / SERVO_SIGNAL_PERIOD;
    m_last_value = 0.;
    m_last_value_time = 0.;
    double initial_pwm = 0.;
    double iangle = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_angle", DBL_MIN, 0., 360.);
    if (iangle != DBL_MIN)
        initial_pwm = _get_pwm_from_angle(iangle);
    else
    {
        double iwidth = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_pulse_width", 0., 0., m_max_width);
        initial_pwm = _get_pwm_from_pulse_width(iwidth);
    }
        
    // Setup mcu_servo pin
    m_mcu_servo = (MCU_pwm*)Printer::GetInstance()->m_ppins->setup_pin("pwm", Printer::GetInstance()->m_pconfig->GetString(section_name, "pin"));
    m_mcu_servo->setup_max_duration(0.);
    m_mcu_servo->setup_cycle_time(SERVO_SIGNAL_PERIOD);
    m_mcu_servo->setup_start_value(initial_pwm, 0.);
    // Register commands
    std::string servo_name = section_name;
    m_cmd_SET_SERVO_help = "Set servo angle";
    Printer::GetInstance()->m_gcode->register_mux_command("SET_SERVO", "SERVO", servo_name, std::bind(&PrinterServo::cmd_SET_SERVO, this, std::placeholders::_1), m_cmd_SET_SERVO_help);
}

PrinterServo::~PrinterServo()
{

}
        
servo_status_t PrinterServo::get_status(double eventtime)
{
    servo_status_t ret = {
        .value = m_last_value,
    };
    return ret;
}
        
void PrinterServo::_set_pwm(double print_time, double value)
{
    if (value == m_last_value)
        return;
    print_time = std::max(print_time, m_last_value_time + PIN_MIN_TIME);
    m_mcu_servo->set_pwm(print_time, value);
    m_last_value = value;
    m_last_value_time = print_time;
}
    
double PrinterServo::_get_pwm_from_angle(double angle)
{
    angle = std::max(0., std::min(m_max_angle, angle));
    double width = m_min_width + angle * m_angle_to_width;
    return width * m_width_to_value;
}
    
double PrinterServo::_get_pwm_from_pulse_width(double width)
{
    if (width)
        width = std::max(m_min_width, std::min(m_max_width, width));
    return width * m_width_to_value;
}
    
    
void PrinterServo::cmd_SET_SERVO(GCodeCommand& gcmd)
{
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    double width = gcmd.get_double("WIDTH", DBL_MIN);
    if (width != DBL_MIN)
        _set_pwm(print_time, _get_pwm_from_pulse_width(width));
    else
    {
        double angle = gcmd.get_double("ANGLE", DBL_MIN);
        _set_pwm(print_time, _get_pwm_from_angle(angle));
    }
        
}
    