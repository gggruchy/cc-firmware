#ifndef HEATER_BED_H
#define HEATER_BED_H
#include "heaters.h"

class PrinterHeaterBed
{
private:
    
public:
    PrinterHeaterBed(std::string section_name);
    ~PrinterHeaterBed();

    Heater* m_heater;
    void cmd_M140(GCodeCommand &gcmd);
    void cmd_M190(GCodeCommand &gcmd);
};




#endif