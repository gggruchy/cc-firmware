#ifndef DISPLAY_STATUS_H
#define DISPLAY_STATUS_H

#include <string>
#include "idle_timeout.h"


typedef struct display_status_tag{
    double progress;
    std::string message;

}display_status_t;

class DisplayStatus{
    private:

    public:
        DisplayStatus(std::string section_name);
        ~DisplayStatus();

        double m_expire_progress;
        double m_progress;
        std::string m_message;

        display_status_t get_status(double eventtime);
        void cmd_M73(GCodeCommand& gcmd);
        void cmd_M117(GCodeCommand& gcmd);
};
#endif