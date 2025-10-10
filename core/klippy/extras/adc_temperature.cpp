#include "adc_temperature.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"

PrinterADCtoTemperature::PrinterADCtoTemperature(std::string section_name, Thermistor* thermistor) :TemperatureSensors()
{            
    m_name =  split(section_name, " ").back();
    m_thermistor = thermistor;
    m_mcu_adc = (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", Printer::GetInstance()->m_pconfig->GetString(section_name, "sensor_pin", ""));
    m_mcu_adc->setup_adc_callback(ADC_TEMP_REPORT_TIME, std::bind(&PrinterADCtoTemperature::adc_callback, this, std::placeholders::_1, std::placeholders::_2));
    // query_adc = Printer::GetInstance()->load_object("query_adc");  //---??---PrinterADCtoTemperature
    // query_adc.register_adc(section_name, m_mcu_adc);
}

PrinterADCtoTemperature::~PrinterADCtoTemperature()
{
    if (m_mcu_adc != nullptr)
    {
        delete m_mcu_adc;
    }
}

void PrinterADCtoTemperature::setup_callback(std::function<void(double, double)> temperature_callback)
{
    m_temperature_callback = temperature_callback;
}

double PrinterADCtoTemperature::get_report_time_delta()
{
    return DEFAULT_ADC_TEMP_REPORT_TIME;
}

void PrinterADCtoTemperature::adc_callback(double read_time, double read_value)               //-----3-adc_callback-G-G-2022-08--------
{
    double temp = m_thermistor->calc_temp(m_name, read_value);
    m_temperature_callback(read_time + ADC_TEMP_SAMPLE_COUNT * ADC_TEMP_SAMPLE_TIME, temp);
}

void PrinterADCtoTemperature::setup_minmax(double min_temp, double max_temp)
{
    double minval = m_thermistor->calc_adc(min_temp);
    double maxval = m_thermistor->calc_adc(max_temp);
    m_mcu_adc->setup_minmax(ADC_TEMP_SAMPLE_TIME, ADC_TEMP_SAMPLE_COUNT, std::min(minval, maxval), std::max(minval, maxval), ADC_TEMP_RANGE_CHECK_COUNT);
}