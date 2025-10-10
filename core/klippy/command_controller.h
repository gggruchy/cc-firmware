#ifndef COMMAND_CONTROLLER_H
#define COMMAND_CONTROLLER_H
#include "klippy.h"

class CommandController
{
private:
    SelectReactor *m_reactor;

public:
    CommandController(std::string section_name);
    ~CommandController();
    double ui_command_handler(double eventtime);
    double serial_command_handler(double eventtime);
    double highest_priority_cmd_handler(double eventtime);
    double fan_cmd_handler(double eventtime);

    ReactorTimerPtr ui_command_timer;
    ReactorTimerPtr serial_command_timer;
    ReactorTimerPtr highest_priority_cmd_timer;
    ReactorTimerPtr fan_cmd_timer;
};

#endif