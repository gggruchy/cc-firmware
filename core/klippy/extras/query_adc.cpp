#include "query_adc.h"
#include "klippy.h"

QueryADC::QueryADC(std::string section_name)
{
    m_cmd_QUERY_ADC_help = "Report the last value of an analog pin";
    Printer::GetInstance()->m_gcode->register_command("QUERY_ADC", std::bind(&QueryADC::cmd_QUERY_ADC, this, std::placeholders::_1), false, m_cmd_QUERY_ADC_help);
}

QueryADC::~QueryADC()
{

}

void QueryADC::register_adc(std::string name, MCU_adc* mcu_adc)
{
    m_adc[name] = mcu_adc;
}
        
    
void QueryADC::cmd_QUERY_ADC(GCodeCommand& gcmd)
{
    std::string name = gcmd.get_string("NAME", "");
    if (m_adc.find(name) == m_adc.end())
    {
        // objs = [""%s"" % (n,) for n in sorted(self.adc.keys())]
        // msg = "Available ADC objects: %s" % (", ".join(objs),)
        // gcmd.respond_info(msg)
        return;
    }
    
    std::pair<double, double> ret = m_adc[name]->get_last_value();

    double value = ret.first;
    double timestamp = ret.second;
    std::string msg = "ADC object " + name + " has value " + std::to_string(value) + " timestamp " + std::to_string(timestamp);
    double pullup = gcmd.get_double("PULLUP", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    if (pullup != DBL_MIN)
    {
        double v = std::max(.00001, std::min(.99999, value));
        double r = pullup * v / (1.0 - v);
        msg += "\n resistance " + std::to_string(r) + " (with " + std::to_string(pullup) + " pullup)";
    }
    // gcmd.respond_info(msg)
}
        