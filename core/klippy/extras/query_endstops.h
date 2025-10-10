#ifndef QUERY_ENDSTOPS_H
#define QUERY_ENDSTOPS_H
#include <vector>
#include "mcu_io.h"
#include "gcode.h"
#include "webhooks.h"

class QueryEndstops{
    public:
        QueryEndstops(std::string section_name);
        ~QueryEndstops();

        std::vector<MCU_endstop*> m_endstops;
        std::vector<std::string> m_endstops_name;
        std::map<std::string, int> m_last_state;
        std::string m_cmd_QUERY_ENDSTOPS_help;

        void register_endstop(MCU_endstop* mcu_endstop, std::string name); 
        void get_status(double eventtime);   
        void _handle_web_request(WebRequest* web_request);
        void cmd_QUERY_ENDSTOPS(GCodeCommand& gcmd);
    private:
};
#endif