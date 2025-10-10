#ifndef SDCARD_LOOP_H
#define SDCARE_LOOP_H
#include <string>
#include <vector>
#include "gcode.h"

class SDCardLoop{
    private:


    public:
        SDCardLoop(std::string section_name);
        ~SDCardLoop();

        std::string m_cmd_SDCARD_LOOP_BEGIN_help;
        std::string m_cmd_SDCARD_LOOP_DESIST_help;
        std::string m_cmd_SDCARD_LOOP_END_help;
        std::vector<std::vector<int>> m_loop_stack;

    public:
        void cmd_SDCARD_LOOP_BEGIN(GCodeCommand& gcmd);    
        void cmd_SDCARD_LOOP_END(GCodeCommand& gcmd); 
        void cmd_SDCARD_LOOP_DESIST(GCodeCommand& gcmd);
        bool loop_begin(int count);
        bool loop_end();
        bool loop_desist();
};
#endif 