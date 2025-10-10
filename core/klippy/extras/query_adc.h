#ifndef QUERY_ADC_H
#define QUERY_ADC_H

#include "mcu_io.h"
#include "gcode.h"

class QueryADC{
    private:

    public:
        QueryADC(std::string section_name);
        ~QueryADC();

        std::map<std::string, MCU_adc*> m_adc;
        std::string m_cmd_QUERY_ADC_help;
        void register_adc(std::string name, MCU_adc* mcu_adc);

        void cmd_QUERY_ADC(GCodeCommand& gcmd);
};
#endif