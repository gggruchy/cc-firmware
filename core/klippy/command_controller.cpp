#include "command_controller.h"

#define LOG_TAG "command_controller"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

SafeQueue<std::string> manual_control_sq;
SafeQueue<std::string> highest_priority_cmd_sq;
SafeQueue<std::string> fan_cmd_sq;

CommandController::CommandController(std::string section_name)
{
    m_reactor = Printer::GetInstance()->m_reactor;
    ui_command_timer = Printer::GetInstance()->m_reactor->register_timer("ui_command_timer", std::bind(&CommandController::ui_command_handler, this, std::placeholders::_1), get_monotonic());
    // serial_command_timer = Printer::GetInstance()->m_reactor->register_timer(std::bind(&CommandController::serial_command_handler, this, std::placeholders::_1), get_monotonic());
    highest_priority_cmd_timer = Printer::GetInstance()->m_reactor->register_timer("highest_priority_cmd_timer", std::bind(&CommandController::highest_priority_cmd_handler, this, std::placeholders::_1), get_monotonic());
    fan_cmd_timer = Printer::GetInstance()->m_reactor->register_timer("fan_cmd_timer", std::bind(&CommandController::fan_cmd_handler, this, std::placeholders::_1), get_monotonic());
}

CommandController::~CommandController()
{
}

double CommandController::ui_command_handler(double eventtime)
{
    while (manual_control_sq.size())
    {
        std::string manual_control_cmd;
        manual_control_sq.pop(manual_control_cmd); //---1-2task-G-G--UI_control_task--
        if (manual_control_cmd != "")
        {
            LOG_D("manual control cmd : %s\n", manual_control_cmd.c_str());
            std::vector<std::string> script = {manual_control_cmd};
            Printer::GetInstance()->m_gcode->run_script(script);
        }
    }
    Printer::GetInstance()->m_manual_sq_require = false;
    return m_reactor->m_NEVER;
}

// double CommandController::serial_command_handler(double eventtime)
// {
//     extern SafeQueue<std::string> serial_control_sq;
//     while (serial_control_sq.size())
//     {
//         std::string serial_control_cmd;
//         serial_control_sq.pop(serial_control_cmd); //---1-2task-G-G--UI_control_task--
//         if (serial_control_cmd != "")
//         {
//             LOG_I("serial control cmd : %s\n" , serial_control_cmd.c_str());
//             std::vector<std::string> script = {serial_control_cmd};
//             Printer::GetInstance()->m_gcode->run_script(script);
//         }
//     }
//     Printer::GetInstance()->m_serial_sq_require = false;
//     return m_reactor->m_NEVER;
// }

double CommandController::highest_priority_cmd_handler(double eventtime)
{
    while (highest_priority_cmd_sq.size())
    {
        std::string highest_priority_cmd;
        highest_priority_cmd_sq.pop(highest_priority_cmd); //---1-2task-G-G--UI_control_task--
        if (highest_priority_cmd != "")
        {
            LOG_I("highest priority cmd : %s\n", highest_priority_cmd.c_str());
            std::vector<std::string> script = {highest_priority_cmd};
            Printer::GetInstance()->m_gcode->run_script(script);
        }
    }
    Printer::GetInstance()->m_highest_priority_sq_require = false;
    return m_reactor->m_NEVER;
}

double CommandController::fan_cmd_handler(double eventtime)
{
    while (fan_cmd_sq.size())
    {

        std::string cmd;
        fan_cmd_sq.pop(cmd); //---1-2task-G-G--UI_control_task--
        if (cmd != "")
        {
            std::vector<std::string> script = {cmd};
            Printer::GetInstance()->m_gcode->run_script(script);
        }
    }
    Printer::GetInstance()->m_fan_cmd_sq_require = false;
    return m_reactor->m_NEVER;
}