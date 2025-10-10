#include "verify_heater.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"
#define LOG_TAG "verify_heater"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

HeaterCheck::HeaterCheck(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:connect:HeaterCheck" + section_name, std::bind(&HeaterCheck::handle_connect, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:HeaterCheck" + section_name, std::bind(&HeaterCheck::handle_shutdown, this));
    m_heater_name = split(section_name, " ").back();
    m_heater = nullptr;
    m_hysteresis = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "hysteresis", 5., 0.);
    m_max_error = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_error", 120., 0.);
    m_heating_gain = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "heating_gain", 2., DBL_MIN, DBL_MAX, 0.);
    double default_gain_time = 20.;
    if (m_heater_name == "heater_bed")
        default_gain_time = 60.;
    m_check_gain_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "check_gain_time", default_gain_time, 1.);
    m_approaching_target = false;
    m_starting_approach = false;
    m_last_target = 0.;
    m_goal_temp = 0.;
    m_error = 0.;
    m_goal_systime = Printer::GetInstance()->m_reactor->m_NEVER;
    // m_check_timer = None //---??---
}

HeaterCheck::~HeaterCheck()
{
}

void HeaterCheck::handle_connect()
{
    if (Printer::GetInstance()->get_start_args("debugoutput") != "")
    {
        // Disable verify_heater if outputting to a debug file
        return;
    }
    m_heater = Printer::GetInstance()->m_pheaters->lookup_heater(m_heater_name);
    std::cout << "Starting heater checks for " << m_heater_name << std::endl;
    // logging.info("Starting heater checks for %s", m_heater_name) //---??---
    // reactor = m_printer.get_reactor()
    // m_check_timer = reactor.register_timer(m_check_event, reactor.NOW)
    m_check_timer = Printer::GetInstance()->m_reactor->register_timer("heater_check_timer", std::bind(&HeaterCheck::check_event, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);
}

void HeaterCheck::handle_shutdown()
{
    if(m_check_timer != nullptr)
    {
        Printer::GetInstance()->m_reactor->update_timer(m_check_timer, Printer::GetInstance()->m_reactor->m_NOW);
    }
}

double HeaterCheck::check_event(double eventtime)
{
#if !ENABLE_MANUTEST /* disable warning report for temperature on extruder */
    std::vector<double> ret_temp = m_heater->get_temp(eventtime);
    double temp = ret_temp[0];
    double target = ret_temp[1];
    if (temp < -20)
    {
        if (m_heater_name == "extruder")
        {
            verify_heater_state_callback_call(VERIFY_HEATER_STATE_NTC_EXTRUDER_ERROR);
        }
        else if (m_heater_name == "heater_bed")
        {
            verify_heater_state_callback_call(VERIFY_HEATER_STATE_NTC_HOT_BED_ERROR);
        }
        Printer::GetInstance()->invoke_shutdown("");
        if (is_verify_heater_arrayempty() != false)
            return Printer::GetInstance()->m_reactor->m_NEVER;
    }
    if (temp >= target - m_hysteresis || target <= 0.)
    {
        // Temperature near target - reset checks
        if (m_approaching_target && target)
        {
            LOG_I("Heater %s within range of %.3f\n", m_heater_name.c_str(), target);
            // logging.info("Heater %s within range of %.3f",
            //                 m_heater_name, target)  //---??---
        }
        m_approaching_target = m_starting_approach = false;
        if (temp <= target + m_hysteresis)
            m_error = 0.;
        m_last_target = target;
        return eventtime + 1.;
    }
    m_error += (target - m_hysteresis) - temp;
    if (!m_approaching_target)
    {
        if (target != m_last_target)
        {
            LOG_I("Heater %s approaching new target of %.3f\n", m_heater_name.c_str(), target);
            // Target changed - reset checks
            // logging.info("Heater %s approaching new target of %.3f",
            //                 m_heater_name, target)  //---??---
            m_approaching_target = m_starting_approach = true;
            m_goal_temp = temp + m_heating_gain;
            m_goal_systime = eventtime + m_check_gain_time;
        }

        else if (m_error >= m_max_error)
        {
            // Failure due to inability to maintain target temperature
            return heater_fault();
        }
    }
    else if (temp >= m_goal_temp)
    {
        // Temperature approaching target - reset checks
        m_starting_approach = false;
        m_error = 0.;
        m_goal_temp = temp + m_heating_gain;
        m_goal_systime = eventtime + m_check_gain_time;
    }
    else if (eventtime >= m_goal_systime)
    {
        // Temperature is no longer approaching target
        m_approaching_target = false;
        LOG_I("Heater %s no longer approaching target %.3f\n", m_heater_name.c_str(), target);
        // logging.info("Heater %s no longer approaching target %.3f",
        //                 m_heater_name, target)  //---??---
    }
    else if (m_starting_approach)
    {
        m_goal_temp = std::min(m_goal_temp, temp + m_heating_gain);
    }
    m_last_target = target;
    return eventtime + 1.;
#endif
}

double HeaterCheck::heater_fault()
{
    std::string msg = "Heater " + m_heater_name + " not heating at expected rate";
    LOG_E("Heater %s not heating at expected rate\n", m_heater_name.c_str());
    if (m_heater_name == "extruder")
    {
        verify_heater_state_callback_call(VERIFY_HEATER_STATE_EXTRUDER_ERROR);
    }
    else if (m_heater_name == "heater_bed")
    {
        verify_heater_state_callback_call(VERIFY_HEATER_STATE_HOT_BED_ERROR);
    }
    // logging.error(msg);　// ---??---
    Printer::GetInstance()->invoke_shutdown(msg);
    if (is_verify_heater_arrayempty() != false)
        return Printer::GetInstance()->m_reactor->m_NEVER;
}

#define VERIFY_HEATER_STATE_CALLBACK_SIZE 16
static verify_heater_state_callback_t verify_heater_state_callback[VERIFY_HEATER_STATE_CALLBACK_SIZE];

int verify_heater_register_state_callback(homing_state_callback_t state_callback)
{
    for (int i = 0; i < VERIFY_HEATER_STATE_CALLBACK_SIZE; i++)
    {
        if (verify_heater_state_callback[i] == NULL)
        {
            verify_heater_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int verify_heater_state_callback_call(int state)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < VERIFY_HEATER_STATE_CALLBACK_SIZE; i++)
    {
        if (verify_heater_state_callback[i] != NULL)
        {
            verify_heater_state_callback[i](state);
        }
    }
    return 0;
}

bool is_verify_heater_arrayempty(void)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < VERIFY_HEATER_STATE_CALLBACK_SIZE; i++)
    {
        if (verify_heater_state_callback[i] != NULL)
        {
            return true;
        }
    }
    return false;
}

void check_heater_init(hl_queue_t *queue, verify_heater_state_callback_t state_callback)
{
    hl_queue_create(queue, sizeof(ui_event_verify_heater_state_id_t), 8);
    verify_heater_register_state_callback(state_callback);
}
