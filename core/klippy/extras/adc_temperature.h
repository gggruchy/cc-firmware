#ifndef ADCTEMPERATURE_H
#define ADCTEMPERATURE_H

#include "pins.h"
#include "mcu_io.h"
#include "thermistor.h"
#include "temperature_sensors_base.h"

#include <iostream>
#include <functional>


class PrinterADCtoTemperature : public TemperatureSensors
{
private:

public:
    std::string m_name;
    MCU_adc* m_mcu_adc;
    Thermistor* m_thermistor;
    std::function<void(double, double)> m_temperature_callback;
public:
    PrinterADCtoTemperature(std::string section_name, Thermistor* thermistor);
    ~PrinterADCtoTemperature();
    void adc_callback(double read_time, double read_value);
    double get_report_time_delta();
    void setup_callback(std::function<void(double, double)> temperature_callback);
    void setup_minmax(double min_temp, double max_temp);
};



#endif
