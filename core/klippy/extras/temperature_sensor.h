#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "pins.h"
#include "mcu_io.h"
#include "thermistor.h"
#include "temperature_sensors_base.h"

#include <iostream>
#include <functional>

class PrinterSensorGeneric
{
private:
    std::string m_name;
    TemperatureSensors *m_sensor;
    double m_min_temp;
    double m_max_temp;

    void temperature_callback(double read_time, double temp);

public:
    PrinterSensorGeneric(std::string section_name);
    ~PrinterSensorGeneric();
};
#endif