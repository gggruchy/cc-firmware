#include "stepper_enable.h"
#include "klippy.h"

StepperEnablePin::StepperEnablePin(MCU_digital_out *mcu_enable, int enable_count)
{
    m_mcu_enable = mcu_enable;
    m_enable_count = enable_count;
    m_is_dedicated = true;
}

StepperEnablePin::~StepperEnablePin()
{
}

void StepperEnablePin::set_enable(double print_time)
{
    if (!m_enable_count)
    {
        m_mcu_enable->set_digital(print_time, 1);
    }
    m_enable_count += 1;
}

void StepperEnablePin::set_disable(double print_time)
{
    m_enable_count -= 1;
    if (!m_enable_count)
    {
        m_mcu_enable->set_digital(print_time, 0);
    }
}

// StepperEnablePin* setup_enable_pin(std::string pin)
// {
//     if(pin == "")
//     {
//         MCU_digital_out* mcu;
//         StepperEnablePin* enable = new StepperEnablePin(mcu, 9999);
//         enable->m_is_dedicated = false;
//         return enable;
//     }
//     pinParams *pin_params = Printer::GetInstance()->m_ppins->lookup_pin(pin, true, false, "stepper_enable");
//     // StepperEnablePin* enable = pin_params.pclass;
//     // if(enable != nullptr)
//     // {
//     //     enable->m_is_dedicated = false;
//     //     return enable;
//     // }
//     MCU_digital_out * mcu_digital_out = new MCU_digital_out(pin_params.chip, &pin_params);
//     mcu_digital_out->setup_max_duration(0);
//     StepperEnablePin *enable = new StepperEnablePin(mcu_digital_out, 0);
//     return enable;
// }

EnableTracking::EnableTracking(MCU_stepper *stepper, std::string pin)
{
    m_stepper = stepper;
    if (pin == "")
    {
        m_stepper_enable = new StepperEnablePin(NULL, 9999);
        m_stepper_enable->m_is_dedicated = false;
        return;
    }
    pinParams *pin_params = Printer::GetInstance()->m_ppins->lookup_pin(pin, true, false, "stepper_enable");
    MCU_digital_out *mcu_digital_out = (MCU_digital_out *)(pin_params->chip)->setup_pin("digital_out", pin_params);
    mcu_digital_out->setup_max_duration(0);
    m_stepper_enable = new StepperEnablePin(mcu_digital_out, 0);
    // pin_params->class = m_stepper_enable;

    m_is_enabled = false;
    m_stepper->add_active_callback(std::bind(&EnableTracking::motor_enable, this, std::placeholders::_1));
}

EnableTracking::~EnableTracking()
{
}

void EnableTracking::register_state_callback(std::function<void(double, bool)> cb)
{
    m_callbacks.push_back(cb);
}

void EnableTracking::motor_enable(double print_time)
{
    if (!m_is_enabled)
    {
        for (int i = 0; i < m_callbacks.size(); i++)
        {
            m_callbacks[i](print_time, true);
        }
        m_stepper_enable->set_enable(print_time);
        m_is_enabled = true;
    }
}

void EnableTracking::motor_disable(double print_time)
{
    if (m_is_enabled)
    {
        for (int i = 0; i < m_callbacks.size(); i++)
        {
            m_callbacks[i](print_time, true);
        }
        m_stepper_enable->set_disable(print_time);
        m_is_enabled = false;
        m_stepper->add_active_callback(std::bind(&EnableTracking::motor_enable, this, std::placeholders::_1));
    }
}

bool EnableTracking::is_motor_enabled()
{
    return m_is_enabled;
}

bool EnableTracking::has_dedicated_enable()
{
    return m_stepper_enable->m_is_dedicated;
}

PrinterStepperEnable::PrinterStepperEnable(std::string section_name)
{
    m_section_name = section_name;
    Printer::GetInstance()->register_event_double_handler("gcode:request_restart:PrinterStepperEnable", std::bind(&PrinterStepperEnable::handle_request_restart, this, std::placeholders::_1));
    std::string cmd_SET_STEPPER_ENABLE_help = "Enable/disable individual stepper by name";
    Printer::GetInstance()->m_gcode->register_command("M18", std::bind(&PrinterStepperEnable::cmd_M18, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M84", std::bind(&PrinterStepperEnable::cmd_M18, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("SET_STEPPER_ENABLE", std::bind(&PrinterStepperEnable::cmd_SET_STEPPER_ENABLE, this, std::placeholders::_1), false, cmd_SET_STEPPER_ENABLE_help);
}

PrinterStepperEnable::~PrinterStepperEnable()
{
}

void PrinterStepperEnable::register_stepper(MCU_stepper *mcu_stepper, std::string pin)
{
    std::string name = mcu_stepper->get_name();
    m_enable_lines[name] = new EnableTracking(mcu_stepper, pin);
}

void PrinterStepperEnable::motor_off()
{
    Printer::GetInstance()->m_tool_head->dwell(DISABLE_STALL_TIME);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (auto el : m_enable_lines)
    {
        el.second->motor_disable(print_time);
    }
    Printer::GetInstance()->send_event("stepper_enable:motor_off", print_time);
    Printer::GetInstance()->m_tool_head->dwell(DISABLE_STALL_TIME);
}

void PrinterStepperEnable::motor_debug_enable(std::string stepper_name, int enable)
{
    Printer::GetInstance()->m_tool_head->dwell(DISABLE_STALL_TIME);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    EnableTracking *el = m_enable_lines[stepper_name];
    if (enable)
    {
        el->motor_enable(print_time);
        std::cout << stepper_name << " has been manually enabled" << std::endl;
    }
    else
    {
        el->motor_disable(print_time);
        std::cout << stepper_name << " has been manually disabled" << std::endl;
    }
    Printer::GetInstance()->m_tool_head->dwell(DISABLE_STALL_TIME);
}

void PrinterStepperEnable::handle_request_restart(double print_time)
{
    this->motor_off();
}

void PrinterStepperEnable::cmd_M18(GCodeCommand &gcmd)
{
    this->motor_off();
}

bool PrinterStepperEnable::get_stepper_state(std::string stepper_name)
{
    if (m_enable_lines.find(stepper_name) == m_enable_lines.end())
    {
        //gcmd.m_respond_info("SET_STEPPER_ENABLE: Invalid stepper " + stepper_name, true);
        return false;
    }
    EnableTracking *el = m_enable_lines[stepper_name];
    return el->is_motor_enabled();
}

void PrinterStepperEnable::cmd_SET_STEPPER_ENABLE(GCodeCommand &gcmd)
{
    std::string stepper_name = gcmd.get_string("STEPPER", "");
    if (m_enable_lines.find(stepper_name) == m_enable_lines.end())
    {
        gcmd.m_respond_info("SET_STEPPER_ENABLE: Invalid stepper " + stepper_name, true);
        return;
    }
    int stepper_enable = gcmd.get_int("ENABLE", 1);
    this->motor_debug_enable(stepper_name, stepper_enable);
}

EnableTracking *PrinterStepperEnable::lookup_enable(std::string stepper_name)
{
    if (m_enable_lines.find(stepper_name) == m_enable_lines.end())
    {
        std::cout << "unknown stepper " << stepper_name << std::endl;
    }
    return m_enable_lines[stepper_name];
}

std::vector<string> PrinterStepperEnable::get_steppers()
{
    std::vector<string> ret;
    for (auto el : m_enable_lines)
    {
        ret.push_back(el.first);
    }
    return ret;
}
