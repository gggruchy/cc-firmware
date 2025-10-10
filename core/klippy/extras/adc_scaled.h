#ifndef ADC_SCALED_H
#define ADC_SCALED_H
#include <string>
#include <map>
#include "mcu_io.h"
#include "query_adc.h"

class PrinterADCScaled;

class MCU_scaled_adc{
    private:

    public:
        MCU_scaled_adc(PrinterADCScaled* main, pinParams * pin_params);
        ~MCU_scaled_adc();

        PrinterADCScaled* m_main;
        std::vector<double> m_last_state;
        MCU_adc *m_mcu_adc;
        std::string qname;
        std::function<void(double, double)> m_callback;

        void _handle_callback(double read_time, double read_value);
        void setup_adc_callback(double report_time, std::function<void(double, double)> callback);
        std::vector<double> get_last_value();
};

class PrinterADCScaled{
    private:

    public:
        PrinterADCScaled(std::string section_name);
        ~PrinterADCScaled();

        std::string m_name;
        std::vector<double> m_last_vref;
        std::vector<double> m_last_vssa;
        MCU_adc* m_mcu_vref;
        MCU_adc* m_mcu_vssa;
        double m_inv_smooth_time;
        MCU* m_mcu;

        MCU_adc* _config_pin(std::string section_name, std::string name, std::function<void(double, double)> callback);
        MCU_scaled_adc* setup_pin(std::string pin_type, pinParams *pin_params);
        std::vector<double> calc_smooth(double read_time, double read_value, std::vector<double> last);
        void vref_callback(double read_time, double read_value);     
        void vssa_callback(double read_time, double read_value);

};

#endif