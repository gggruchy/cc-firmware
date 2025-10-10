#ifndef PROBE_ENDSTOP_WRAPPER_BASE_H
#define PROBE_ENDSTOP_WRAPPER_BASE_H
#include "mcu_io.h"
#include "gcode.h"
#include "homing.h"

class ProbeEndstopWrapperBase{
    private:

    public:
        ProbeEndstopWrapperBase(){};
        ~ProbeEndstopWrapperBase(){};

    public:
        std::string m_multi;
        float m_position_endstop;
        bool m_stow_on_each_sample;
        MCU_endstop *m_mcu_endstop;
    
        virtual void handle_mcu_identify() = 0;
        virtual void raise_probe() = 0;
        virtual void lower_probe() = 0;
        virtual void multi_probe_begin() = 0;
        virtual void multi_probe_end() = 0;
        virtual void probe_prepare(HomingMove* hmove) = 0;
        virtual void probe_finish(HomingMove* hmove) = 0;
        virtual double get_position_endstop() = 0;

};
#endif