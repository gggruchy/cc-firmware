#ifndef TEMPERATURE_SENSOR_BASE_H
#define TEMPERATURE_SENSOR_BASE_H
#include <functional>

class TemperatureSensors
{
private:
    
public:
    TemperatureSensors(){};
    ~TemperatureSensors(){};
    virtual void setup_callback(std::function<void(double, double)> temperature_callback) = 0;
    virtual void setup_minmax(double min_temp, double max_temp) = 0;
    virtual void adc_callback(double read_time, double read_value) = 0;
    virtual double get_report_time_delta() = 0;
    
};
#endif