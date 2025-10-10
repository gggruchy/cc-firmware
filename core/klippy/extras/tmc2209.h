#ifndef TMC2209_H
#define TMC2209_H

#include "tmc2208.h"
#include "tmc2130.h"
#include "tmc.h"
#include "tmc_uart.h"
#include <string>
#include <map>

class TMC2209
{
private:
public:
    TMC2209(std::string section_name);
    ~TMC2209();
    std::function<int()> get_microsteps;
    std::function<int()> get_phase;
    // int(*get_microsteps)() ;
    // int(*get_phase)() ;
    FieldHelper *m_fields;
    MCU_TMC_uart *m_mcu_tmc;
    TMCVirtualPinHelper *tmc_virPin_helper;
    TMCCurrentHelper *current_helper;
    TMCCommandHelper *cmdhelper;
    TMCMicrostepHelper *mh;
};

#endif
