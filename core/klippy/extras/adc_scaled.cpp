#include "adc_scaled.h"
#include "klippy.h"


#define SAMPLE_TIME 0.001
#define SAMPLE_COUNT 8
#define REPORT_TIME 0.300
#define RANGE_CHECK_COUNT 4

MCU_scaled_adc::MCU_scaled_adc(PrinterADCScaled* main, pinParams * pin_params)
{
    m_main = main;
    m_last_state = {0., 0.};
    m_mcu_adc = new MCU_adc(main->m_mcu, pin_params);
    qname = main->m_name + ":" + pin_params->pin;
    Printer::GetInstance()->m_query_adc->register_adc(qname, m_mcu_adc);
}
        
void MCU_scaled_adc::_handle_callback(double read_time, double read_value)
{
    double max_adc = m_main->m_last_vref[1];
    double min_adc = m_main->m_last_vssa[1];
    double scaled_val = (read_value - min_adc) / (max_adc - min_adc);
    m_last_state = {scaled_val, read_time};
    m_callback(read_time, scaled_val);
}
    
void MCU_scaled_adc::setup_adc_callback(double report_time, std::function<void(double, double)> callback)
{
    m_callback = callback;
    m_mcu_adc->setup_adc_callback(report_time, std::bind(&MCU_scaled_adc::_handle_callback, this, std::placeholders::_1, std::placeholders::_2));
}
    
std::vector<double> MCU_scaled_adc::get_last_value()
{
    return m_last_state;
}
    

PrinterADCScaled::PrinterADCScaled(std::string section_name)
{
    m_name = section_name;
    m_last_vref = {0., 0.};
    m_last_vssa = {0., 0.};
    // Configure vref and vssa pins
    m_mcu_vref = _config_pin(section_name, "vref", std::bind(&PrinterADCScaled::vref_callback, this, std::placeholders::_1, std::placeholders::_2));
    m_mcu_vssa = _config_pin(section_name, "vssa", std::bind(&PrinterADCScaled::vssa_callback, this, std::placeholders::_1, std::placeholders::_2));
    double smooth_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "smooth_time", 2., DBL_MIN, DBL_MAX, 0.);
    m_inv_smooth_time = 1. / smooth_time;
    m_mcu = m_mcu_vref->get_mcu();
    if (m_mcu != m_mcu_vssa->get_mcu())
    {
        // raise config.error("vref and vssa must be on same mcu")
    }
}

PrinterADCScaled::~PrinterADCScaled()
{

}
    
MCU_adc* PrinterADCScaled::_config_pin(std::string section_name, std::string name, std::function<void(double, double)> callback)
{
    std::string pin_name = Printer::GetInstance()->m_pconfig->GetString(section_name, name + "_pin", "");
    MCU_adc* mcu_adc = (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", pin_name);
    mcu_adc->setup_adc_callback(REPORT_TIME, callback);
    mcu_adc->setup_minmax(SAMPLE_TIME, SAMPLE_COUNT, 0., 1.,RANGE_CHECK_COUNT);
    QueryADC * query_adc = Printer::GetInstance()->m_query_adc;
    query_adc->register_adc(section_name + ":" + name, mcu_adc);
    return mcu_adc;
}
    
MCU_scaled_adc* PrinterADCScaled::setup_pin(std::string pin_type, pinParams *pin_params)
{
    if (pin_type != "adc")
    {
        // raise m_printer.config_error("adc_scaled only supports adc pins")
    } 
    return new MCU_scaled_adc(this, pin_params);
}
        
std::vector<double> PrinterADCScaled::calc_smooth(double read_time, double read_value, std::vector<double> last)
{
    double last_time = last[0];
    double last_value = last[1];
    double time_diff = read_time - last_time;
    double value_diff = read_value - last_value;
    double adj_time = std::min(time_diff * m_inv_smooth_time, 1.);
    double smoothed_value = last_value + value_diff * adj_time;
    std::vector<double> ret = {read_time, smoothed_value};
    return ret;
}
        
void PrinterADCScaled::vref_callback(double read_time, double read_value)
{
    m_last_vref = calc_smooth(read_time, read_value, m_last_vref);
}
        
void PrinterADCScaled::vssa_callback(double read_time, double read_value)
{
    m_last_vssa = calc_smooth(read_time, read_value, m_last_vssa);
}
        