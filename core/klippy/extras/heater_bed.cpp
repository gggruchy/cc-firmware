#include "heater_bed.h"
#include "klippy.h"
#include "Define.h"

#include "config.h"

#define LOG_TAG "heater_bed"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

PrinterHeaterBed::PrinterHeaterBed(std::string section_name)
{
    Printer::GetInstance()->load_object("heaters");
    m_heater = Printer::GetInstance()->m_pheaters->setup_heater(section_name, "B");
    // Register commands
    Printer::GetInstance()->m_gcode->register_command("M140", std::bind(&PrinterHeaterBed::cmd_M140, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M190", std::bind(&PrinterHeaterBed::cmd_M190, this, std::placeholders::_1));
}

PrinterHeaterBed::~PrinterHeaterBed()
{
}

void PrinterHeaterBed::cmd_M140(GCodeCommand &gcmd)
{
    // Set Bed Temperature
    double temp = gcmd.get_double("S", 0.);
    Printer::GetInstance()->m_pheaters->set_temperature(m_heater, temp, false);
}

void PrinterHeaterBed::cmd_M190(GCodeCommand &gcmd)
{

    // Set Bed Temperature and Wait
    double temp = gcmd.get_double("S", 0.);
    Printer::GetInstance()->m_pheaters->set_temperature(m_heater, temp, true);

}