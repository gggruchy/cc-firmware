#include "thermistor.h"
#include "klippy.h"
#define KELVIN_TO_CELSIUS -273.15
#define INLINE_RESISTOR_OFFSET 3000

Thermistor::Thermistor(double pullup, double inline_resistor)
{
    m_pullup = pullup;
    m_inline_resistor = inline_resistor;
    m_c1 = m_c2 = m_c3 = 0;
}

Thermistor::~Thermistor()
{
}

void Thermistor::setup_coefficients(double t1, double r1, double t2, double r2, double t3, double r3, std::string name)
{
    double inv_t1 = 1.0 / (t1 - KELVIN_TO_CELSIUS);
    double inv_t2 = 1.0 / (t2 - KELVIN_TO_CELSIUS);
    double inv_t3 = 1.0 / (t3 - KELVIN_TO_CELSIUS);

    double ln_r1 = log(r1);
    double ln_r2 = log(r2);
    double ln_r3 = log(r3);

    double ln3_r1 = pow(ln_r1, 3);
    double ln3_r2 = pow(ln_r2, 3);
    double ln3_r3 = pow(ln_r3, 3);


    double inv_t12 = inv_t1 - inv_t2;
    double inv_t13 = inv_t1 - inv_t3;

    double ln_r12 = ln_r1 - ln_r2;
    double ln_r13 = ln_r1 - ln_r3;
    
    double ln3_r12 = ln3_r1 - ln3_r2;
    double ln3_r13 = ln3_r1 - ln3_r3;

    m_c3 =((inv_t12 - inv_t13 * ln_r12 / ln_r13) / (ln3_r12 - ln3_r13 * ln_r12 / ln_r13));

    if(m_c3 <= 0)
    {
        double beta = ln_r13 / inv_t13;
        setup_coefficients_beta(t1, r1, beta);
    }
    m_c2 = (inv_t12 - m_c3 * ln3_r12) / ln_r12;
    m_c1 = inv_t1 - m_c2 * ln_r1 - m_c3 * ln3_r1;
}

void Thermistor::setup_coefficients_beta(double t1, double r1, double beta)
{
    double inv_t1 = 1.0 / (t1 - KELVIN_TO_CELSIUS);
    double ln_r1 = log(r1);
    m_c3 = 0;
    m_c2 = 1.0 / beta;
    m_c1 = inv_t1 - m_c2 * ln_r1;
}

double Thermistor::calc_temp(std::string m_name, double adc)
{ 
    adc = std::max(0.00001, std::min(0.99999, adc));
    double r = m_pullup * adc / (1.0 - adc);
    
    // 计算热敏电阻的等效电阻
    double r_thermistor = r;
    if (fabs(m_inline_resistor) > 1e-15) {
        // 对于并联电阻，使用并联公式: 1/R_eq = 1/R1 + 1/R2
        // 因此 R_thermistor = (R_total * R_inline)/(R_inline - R_total)
        if (m_inline_resistor - r > 1e-15)
            r_thermistor = (r * m_inline_resistor) / (m_inline_resistor - r);
            printf("Thermistor::calc_temp adc:%f，r:%f，pullup:%f，inline_resistor:%f，r_thermistor:%f\n", adc, r, m_pullup, m_inline_resistor, r_thermistor);
        else
            r_thermistor = m_inline_resistor;
            printf("Thermistor::calc_temp adc:%f，r:%f，pullup:%f，inline_resistor:%f，r_thermistor:%f\n", adc, r, m_pullup, m_inline_resistor, r_thermistor);
        
        if (r > m_inline_resistor - INLINE_RESISTOR_OFFSET)
        {
            if (m_name == "extruder")
            {
                verify_heater_state_callback_call(VERIFY_HEATER_STATE_NTC_EXTRUDER_ERROR);
                Printer::GetInstance()->invoke_shutdown("");
            }
            else if (m_name == "heater_bed")
            {
                verify_heater_state_callback_call(VERIFY_HEATER_STATE_NTC_HOT_BED_ERROR);
                Printer::GetInstance()->invoke_shutdown("");
            }
        }
    }
    
    double ln_r = log(r_thermistor);
    double inv_t = m_c1 + m_c2 * ln_r + m_c3 * pow(ln_r, 3);
    double ret = 1.0 / inv_t + KELVIN_TO_CELSIUS;
    return ret;
}

double Thermistor::calc_adc(double temp)
{
    if(temp <= KELVIN_TO_CELSIUS)
    {
        return 1;
    }
    double inv_t = 1.0 / (temp - KELVIN_TO_CELSIUS);
    double ln_r = 0;
    if(fabs(m_c3) > 1e-15)
    {
        double y = (m_c1 - inv_t) / (2.0 * m_c3);
        double x = sqrt(pow((m_c2 / (3.0 * m_c3)), 3) + pow(y, 2));
        ln_r = pow(x - y, 1.0 / 3.0) - pow(x + y, 1.0 / 3.0);
    }
    else
    {
        ln_r = (inv_t - m_c1) / m_c2;
    }
    // 先计算热敏电阻的阻值
    double r_thermistor = exp(ln_r);
    
    // 计算总电阻
    double r = r_thermistor;
    if (fabs(m_inline_resistor) > 1e-15) {
        // 计算热敏电阻和并联电阻的等效电阻
        // 使用并联公式: 1/R_eq = 1/R1 + 1/R2
        // 因此 R_eq = (R1 * R2)/(R1 + R2)
        r = (r_thermistor * m_inline_resistor) / (r_thermistor + m_inline_resistor);
    }
    // 使用分压公式计算ADC值
    return r / (m_pullup + r);
}


PrinterADCtoTemperature* PrinterThermistor(std::string section_name, std::map<std::string, double> params)
{
    double pullup = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pullup_resistor", 4700., DBL_MIN, DBL_MAX, 0.);
    double inline_resistor = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "inline_resistor", 0., 0.);
    Thermistor* thermistor = new Thermistor(pullup, inline_resistor);
    if (params.find("beta") != params.end())
    {
        thermistor->setup_coefficients_beta(params["t1"], params["r1"], params["beta"]);
    }
    else
    {
        thermistor->setup_coefficients(params["t1"], params["r1"], params["t2"], params["r2"],
            params["t3"], params["r3"], section_name);
    }
    PrinterADCtoTemperature *ADCtoTemp = new PrinterADCtoTemperature(section_name, thermistor);
    return ADCtoTemp;
}

CustomThermistor::CustomThermistor(std::string section_name)
{
    m_name = Printer::GetInstance()->m_pconfig->GetString(section_name, "custom_thermistor_name", "");
    double t1 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "temperature1", DBL_MIN, KELVIN_TO_CELSIUS);
    double r1 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "resistance1", DBL_MIN, 0.);
    double beta = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "beta", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    if (beta != DBL_MIN)
    {
        m_params["t1"] = t1;
        m_params["r1"] = r1;
        m_params["beta"] = beta;
        return;
    } 
    double t2 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "temperature2", DBL_MIN, KELVIN_TO_CELSIUS);
    double r2 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "resistance2", DBL_MIN, 0.);
    double t3 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "temperature3", DBL_MIN, KELVIN_TO_CELSIUS);
    double r3 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "resistance3", DBL_MIN, 0.);
    m_params["t1"] = t1;
    m_params["r1"] = r1;
    m_params["t2"] = t2;
    m_params["r2"] = r2;
    m_params["t3"] = t3;
    m_params["r3"] = r3;
}
CustomThermistor::~CustomThermistor()
{

}

// PrinterADCtoTemperature* CustomThermistor::create(std::string section_name)
// {
//     return PrinterThermistor(section_name, m_params);
// }
        


void add_thermistor_sensors(std::string section_name)
{
    std::string sensor_type = Printer::GetInstance()->m_pconfig->GetString(section_name, "sensor_type");
    if (sensor_type == "EPCOS 100K B57560G104F")
    {
        std::map<std::string, double> params1 = {
        {"t1", 25.}, {"r1", 100000.}, {"t2", 150.}, {"r2", 1641.9},{"t3", 250.}, {"r3", 226.15}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("EPCOS 100K B57560G104F", PrinterThermistor(section_name, params1));
    }
    else if (sensor_type == "EPCOS 100K B57560G104F")
    {
        std::map<std::string, double> params1 = {
        {"t1", 25.}, {"r1", 100000.}, {"t2", 150.}, {"r2", 1641.9},{"t3", 250.}, {"r3", 226.15}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("EPCOS 100K B57560G104F", PrinterThermistor(section_name, params1));
    
    }
    else if (sensor_type == "ATC Semitec 104GT-2")
    {
        std::map<std::string, double> params2 = {
        {"t1", 20.}, {"r1", 126800.}, {"t2", 150.}, {"r2", 1360.},{"t3", 300.}, {"r3", 80.65}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("ATC Semitec 104GT-2", PrinterThermistor(section_name, params2));
    }
    else if (sensor_type == "SliceEngineering 450")
    {
        std::map<std::string, double> params3 = {
        {"t1", 25.}, {"r1", 500000.}, {"t2", 200.}, {"r2", 3734.},{"t3", 400.}, {"r3", 240.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("SliceEngineering 450", PrinterThermistor(section_name, params3));
    
    }
    else if (sensor_type == "TDK NTCG104LH104JT1")
    {
        std::map<std::string, double> params4 = {
        {"t1", 25.}, {"r1", 100000.}, {"t2", 50.}, {"r2", 31230.},{"t3", 125.}, {"r3", 2066.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("TDK NTCG104LH104JT1", PrinterThermistor(section_name, params4));
    
    }
    else if (sensor_type == "NTC 100K beta 3950")
    {
        std::map<std::string, double> params5 = {
        {"t1", 25.}, {"r1", 100000.}, {"beta", 3950.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("NTC 100K beta 3950", PrinterThermistor(section_name, params5));
    
    }
    else if (sensor_type == "Honeywell 100K 135-104LAG-J01")
    {
        std::map<std::string, double> params6 = {
        {"t1", 25.}, {"r1", 100000.}, {"beta", 3974.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("Honeywell 100K 135-104LAG-J01", PrinterThermistor(section_name, params6));
    
    }
    else if (sensor_type == "NTC 100K MGB18-104F39050L32")
    {
        std::map<std::string, double> params7 = {
        {"t1", 25.}, {"r1", 100000.}, {"beta", 4100.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("NTC 100K MGB18-104F39050L32", PrinterThermistor(section_name, params7));

    }
    else if (sensor_type == "NTC 100K beta 4300")
    {
        std::map<std::string, double> params8 = {
        {"t1", 25.}, {"r1", 100000.}, {"beta", 4300.}};
        Printer::GetInstance()->m_pheaters->add_sensor_factory("NTC 100K beta 4300", PrinterThermistor(section_name, params8));
    }
}

void add_custom_thermistor_sensors(std::string section_name)
{
    CustomThermistor custom_thermistor(section_name);
    // Printer::GetInstance()->m_pheaters->add_sensor_factory("custom_thermistor_name", custom_thermistor.create(section_name));
}
