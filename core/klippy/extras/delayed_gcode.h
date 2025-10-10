#ifndef DELAYED_GCODE_H
#define DELAYED_GCODE_H

#include <string>
#include "reactor.h"
#include "gcode.h"

class DelayedGcode{
    private:

    public:
        DelayedGcode(std::string section_name);
        ~DelayedGcode();

        std::string m_name;
        double m_duration;
        ReactorTimerPtr m_timer_handler = nullptr;
        bool m_inside_timer;
        bool m_repeat;
        std::string m_cmd_UPDATE_DELAYED_GCODE_help;

        void _handle_ready();
        double _gcode_timer_event(double eventtime);
        void cmd_UPDATE_DELAYED_GCODE(GCodeCommand& gcmd);
};
#endif