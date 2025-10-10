#ifndef __THERMISTOR__H__
#define __THERMISTOR__H__

#include <iostream>
#include <math.h>
#include <map>

struct Sensor
{
    double t1;
    double r1;
    double t2;
    double r2;
    double t3;
    double r3;
};

class Thermistor
{
private:
    double m_pullup;
    double m_inline_resistor;
    double m_c2;
    double m_c1;
    double m_c3;

public:
    Thermistor(double pullup, double inline_resistor);
    ~Thermistor();
    void setup_coefficients(double t1, double r1, double t2, double r2, double t3, double r3, std::string name = "");
    void setup_coefficients_beta(double t1, double r1, double beta);
    double calc_temp(std::string m_name, double adc);
    double calc_adc(double temp);
};

class CustomThermistor{
    private:

    public:
        CustomThermistor(std::string section_name);
        ~CustomThermistor();

        std::map<std::string, double> m_params;
        std::string m_name;
        // PrinterADCtoTemperature* create(std::string section_name);

};

void add_custom_thermistor_sensors(std::string section_name);
void add_thermistor_sensors(std::string section_name);

#endif