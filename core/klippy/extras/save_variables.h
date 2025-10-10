#ifndef SAVE_VARIAVLES_H
#define SAVE_VARIAVLES_H
#include <string>
#include "gcode.h"

class SaveVariables{
    private:

    public:
        SaveVariables();
        ~SaveVariables();

        std::string m_filename;
        std::string m_cmd_SAVE_VARIABLE_help;
    public:
        void load_variables();
        void cmd_SAVE_VARIABLE(GCodeCommand& gcmd);
        void get_status(double eventtime);
};

#endif