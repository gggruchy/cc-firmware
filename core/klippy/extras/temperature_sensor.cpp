#include "temperature_sensor.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"

#include "simplebus.h"
#include "srv_state.h"

PrinterSensorGeneric::PrinterSensorGeneric(std::string section_name)
{
    m_name = split(section_name, " ").back();
    m_sensor = Printer::GetInstance()->m_pheaters->setup_sensor(section_name);
    m_min_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_temp", DBL_MIN, KELVIN_TO_CELSIUS);
    m_max_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_temp", DBL_MIN, DBL_MAX, DBL_MAX, m_min_temp);
    m_sensor->setup_minmax(m_min_temp, m_max_temp);
    m_sensor->setup_callback(std::bind(&PrinterSensorGeneric::temperature_callback, this, std::placeholders::_1, std::placeholders::_2));
}

PrinterSensorGeneric::~PrinterSensorGeneric()
{
    delete (m_sensor);
}

void PrinterSensorGeneric::temperature_callback(double read_time, double temp)
{
    srv_state_heater_msg_t heater_msg;
    if (m_name == "box")
        heater_msg.heater_id = HEATER_ID_BOX;
    else
        return;
    heater_msg.current_temperature = temp;
    heater_msg.target_temperature = 0;
    simple_bus_publish_async("heater", SRV_HEATER_MSG_ID_STATE, &heater_msg, sizeof(heater_msg));
}