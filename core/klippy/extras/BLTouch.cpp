#include "BLTouch.h"
#include "klippy.h"
#include "Define.h"

BLTouchEndstopWrapper::BLTouchEndstopWrapper(std::string section_name)
{
    
    Printer::GetInstance()->register_event_handler("klippy:connect:BLTouchEndstopWrapper"+section_name, std::bind(&BLTouchEndstopWrapper::handle_connect, this));
    Printer::GetInstance()->register_event_handler("klippy:mcu_identify:BLTouchEndstopWrapper"+section_name, std::bind(&BLTouchEndstopWrapper::handle_mcu_identify, this));
    
    m_position_endstop = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_offset", DBL_MIN, 0.);
    m_stow_on_each_sample = Printer::GetInstance()->m_pconfig->GetBool(section_name, "stow_on_each_sample", true);
    m_probe_touch_mode = Printer::GetInstance()->m_pconfig->GetBool(section_name, "probe_with_touch_mode", false);
    m_mcu_pwm = (MCU_pwm*)Printer::GetInstance()->m_ppins->setup_pin("pwm", Printer::GetInstance()->m_pconfig->GetString(section_name, "control_pin", ""));
    m_mcu_pwm->setup_max_duration(0.);
    m_mcu_pwm->setup_cycle_time(BLTOUCH_SIGNAL_PERIOD);
    m_next_cmd_time = 0.;
    m_action_end_time = 0.;
    // self.finish_home_complete = self.wait_trigger_complete = None //---??---
    // Create an "endstop" object to handle the sensor pin
    std::string pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "sensor_pin", "");
    pinParams *pin_params = Printer::GetInstance()->m_ppins->lookup_pin(pin, true, true);
    m_mcu =  (MCU *)pin_params->chip;
    m_mcu_endstop = (MCU_endstop*)m_mcu->setup_pin("endstop", pin_params);
    // mcu->register_config_callback(self._build_config)
    // output mode
    m_omodes.push_back("5V");
    m_omodes.push_back("OD");
    m_output_mode = Printer::GetInstance()->m_pconfig->GetString(section_name, "set_output_mode", "");
    // Setup for sensor test
    m_next_test_time = 0.;
    m_pin_up_not_triggered = Printer::GetInstance()->m_pconfig->GetBool(section_name, "pin_up_reports_not_triggered", true);
    m_pin_up_touch_triggered = Printer::GetInstance()->m_pconfig->GetBool(section_name, "pin_up_touch_mode_reports_triggered", true);
    //Calculate pin move time
    m_pin_move_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pin_move_time", 0.680, DBL_MIN, DBL_MAX, 0.);
    // Register BLTOUCH_DEBUG command
    m_cmd_BLTOUCH_DEBUG_help = "Send a command to the bltouch for debugging";
    m_cmd_BLTOUCH_STORE_help = "Store an output mode in the BLTouch EEPROM";
    Printer::GetInstance()->m_gcode->register_command("BLTOUCH_DEBUG", std::bind(&BLTouchEndstopWrapper::cmd_BLTOUCH_DEBUG, this, std::placeholders::_1), false, m_cmd_BLTOUCH_DEBUG_help);
    Printer::GetInstance()->m_gcode->register_command("BLTOUCH_STORE", std::bind(&BLTouchEndstopWrapper::cmd_BLTOUCH_STORE, this, std::placeholders::_1), false, m_cmd_BLTOUCH_STORE_help);
    m_multi = "OFF";
    commands_init();
}

BLTouchEndstopWrapper::~BLTouchEndstopWrapper()
{
    
}

void BLTouchEndstopWrapper::commands_init()
{
    Commands["pin_down"] = 0.000650;
    Commands["touch_mode"] = 0.001165;
    Commands["pin_up"] = 0.001475;
    Commands["self_test"] = 0.001780;
    Commands["reset"] = 0.002190;
    Commands["set_5V_output_mode"] = 0.001988;
    Commands["set_OD_output_mode"] = 0.002091;
    Commands["output_mode_store"] = 0.001884;
}
        
void BLTouchEndstopWrapper::handle_mcu_identify()
{
    std::vector<std::vector<MCU_stepper*>> steppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < steppers.size(); i++)
    {
        std::vector<MCU_stepper*> steppers_temp = steppers[i];
        for (int j = 0; j < steppers_temp.size(); j++)
        {
            if (steppers_temp[j]->is_active_axis('z'))
            {
                m_mcu_endstop->add_stepper(steppers_temp[j]);
            }
        }
    }
}

void BLTouchEndstopWrapper::handle_connect()
{
    sync_mcu_print_time();
    m_next_cmd_time += 0.200;
    set_output_mode(m_output_mode);
    raise_probe();
    verify_raise_probe();
}

void BLTouchEndstopWrapper::sync_mcu_print_time()
{
    double curtime = get_monotonic();
    double est_time = m_mcu_pwm->get_mcu()->estimated_print_time(curtime);
    std::cout << "est_time = " << est_time << std::endl; 
    std::cout << "m_next_cmd_time = " << m_next_cmd_time << std::endl;
    m_next_cmd_time = std::max(m_next_cmd_time, est_time + BLTOUCH_MIN_CMD_TIME);
}

void BLTouchEndstopWrapper::sync_print_time()
{
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    if (m_next_cmd_time > print_time)
    {
        Printer::GetInstance()->m_tool_head->dwell(m_next_cmd_time - print_time);
    }
    else
    {
        m_next_cmd_time = print_time;
    }
}

void BLTouchEndstopWrapper::send_cmd(std::string cmd, double duration)
{
    //Translate duration to ticks to avoid any secondary mcu clock skew
    uint64_t cmd_clock = m_mcu->print_time_to_clock(m_next_cmd_time);
    double pulse = int((duration - BLTOUCH_MIN_CMD_TIME) / BLTOUCH_SIGNAL_PERIOD) * BLTOUCH_SIGNAL_PERIOD;
    cmd_clock += m_mcu->seconds_to_clock(std::max(BLTOUCH_MIN_CMD_TIME, pulse));
    double end_time = m_mcu->clock_to_print_time(cmd_clock);
    // Schedule command followed by PWM disable
    // std::cout << "Commands " << cmd << " = " << Commands[cmd] << std::endl;
    m_mcu_pwm->set_pwm(m_next_cmd_time, Commands[cmd] / BLTOUCH_SIGNAL_PERIOD);
    m_mcu_pwm->set_pwm(end_time, 0.);
    // Update time tracking
    m_action_end_time = m_next_cmd_time + duration;
    m_next_cmd_time = std::max(m_action_end_time, end_time + BLTOUCH_MIN_CMD_TIME);
}

int BLTouchEndstopWrapper::verify_state(bool triggered)
{
    // Perform endstop check to verify bltouch reports desired state
    m_mcu_endstop->home_start(m_action_end_time, BLTOUCH_ENDSTOP_SAMPLE_TIME,BLTOUCH_ENDSTOP_SAMPLE_COUNT, 
                                BLTOUCH_ENDSTOP_REST_TIME, triggered);
                                sleep(2);
    return m_mcu_endstop->home_wait(m_action_end_time + 0.100);
}
        
void BLTouchEndstopWrapper::raise_probe()
{
    sync_mcu_print_time();
    if (!m_pin_up_not_triggered)
    {
        send_cmd("reset");
    }
    send_cmd("pin_up", m_pin_move_time);
}
        
void BLTouchEndstopWrapper::verify_raise_probe()
{
    if (!m_pin_up_not_triggered)
    {
        // No way to verify raise attempt
        return;
    }
    for (int retry = 0; retry < 3; retry++)
    {
        //DisTraceMsg();
        int success = verify_state(false);
        if (success)
        {
            //The "probe raised" test completed successfully
            break;
        }
        if (retry >= 2)
        {
            std::cout << "BLTouch failed to raise probe" << std::endl;
        }
        std::string msg = "Failed to verify BLTouch probe is raised; retrying.";
        Printer::GetInstance()->m_gcode->respond_info(msg);
        sync_mcu_print_time();
        send_cmd("reset", BLTOUCH_ENDSTOP_REST_TIME);
        send_cmd("pin_up", m_pin_move_time);
    }
}

void BLTouchEndstopWrapper::lower_probe()
{
    test_sensor();
    sync_print_time();
    send_cmd("pin_down", m_pin_move_time);
    if (m_probe_touch_mode)
        send_cmd("touch_mode");
}
        
void BLTouchEndstopWrapper::test_sensor()
{
    if (!m_pin_up_touch_triggered)
    {
        // Nothing to test
        return;
    }
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    if (print_time < m_next_test_time)
    {
        m_next_test_time = print_time + BLTOUCH_TEST_TIME;
        return;
    }
    // Raise the bltouch probe and test if probe is raised
    sync_print_time();
    for (int retry = 0; retry < 3; retry++)
    {
        send_cmd("pin_up", m_pin_move_time);
        send_cmd("touch_mode");
        int success = verify_state(true);
        sync_print_time();
        if (success)
        {
            // The "bltouch connection" test completed successfully
            m_next_test_time = print_time + BLTOUCH_TEST_TIME;
            return;
        }
        std::string msg = "BLTouch failed to verify sensor state";
        if (retry >= 2)
            std::cout << msg << std::endl;
        Printer::GetInstance()->m_gcode->respond_info(msg + "; retrying.");
        send_cmd("reset", BLTOUCH_RETRY_RESET_TIME);
    }
}

void BLTouchEndstopWrapper::multi_probe_begin()
{
    if (m_stow_on_each_sample)
        return;
    m_multi = "FIRST";
}

void BLTouchEndstopWrapper::multi_probe_end()
{
    if (m_stow_on_each_sample)
        return;
    sync_print_time();
    raise_probe();
    verify_raise_probe();
    sync_print_time();
    m_multi = "OFF";
}
        
void BLTouchEndstopWrapper::probe_prepare(HomingMove* hmove)
{
    if (m_multi == "OFF" || m_multi == "FIRST")
    {
        lower_probe();
        if (m_multi == "FIRST")
            m_multi = "ON";
    }
    sync_print_time();
}
        
void BLTouchEndstopWrapper::home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered)
{
    double rest_time_temp = std::min(rest_time, BLTOUCH_ENDSTOP_REST_TIME);
    // self.finish_home_complete = self.mcu_endstop.home_start(print_time, sample_time, sample_count, rest_time, triggered);
    // // Schedule wait_for_trigger callback
    // r = self.printer.get_reactor()
    // self.wait_trigger_complete = r.register_callback(self.wait_for_trigger)
    // return self.finish_home_complete //---??---
}
        
void BLTouchEndstopWrapper::wait_for_trigger(double eventtime)
{
    // self.finish_home_complete.wait() //---??---
    if (m_multi == "OFF")
        raise_probe();
}
        
void BLTouchEndstopWrapper::probe_finish(HomingMove* hmove)
{
    //DisTraceMsg();
    // self.wait_trigger_complete.wait() //---??---
    if (m_multi == "OFF")
        verify_raise_probe();
    sync_print_time();
    if (hmove->check_no_movement() != "")
        std::cout << "BLTouch failed to deploy" << std::endl;
}
        
double BLTouchEndstopWrapper::get_position_endstop()
{
    return m_position_endstop;
}
       
void BLTouchEndstopWrapper::set_output_mode(std::string mode)
{
    //If this is inadvertently/purposely issued for a
    //BLTOUCH pre V3.0 and clones:
    //  No reaction at all.
    //BLTOUCH V3.0 and V3.1:
    //  This will set the mode.
    if (mode == "")
        return;
    sync_mcu_print_time();
    if (mode == "5V")
        send_cmd("set_5V_output_mode");
    if (mode == "OD")
        send_cmd("set_OD_output_mode");
}
        
void BLTouchEndstopWrapper::store_output_mode(std::string mode)
{
    // If this command is inadvertently/purposely issued for a
    // BLTOUCH pre V3.0 and clones:
    //   No reaction at all to this sequence apart from a pin-down/pin-up
    // BLTOUCH V3.0:
    //   This will set the mode (twice) and sadly, a pin-up is needed at
    //   the end, because of the pin-down
    // BLTOUCH V3.1:
    //   This will set the mode and store it in the eeprom.
    //   The pin-up is not needed but does not hurt
    sync_print_time();
    send_cmd("pin_down");
    if (mode == "5V")
        send_cmd("set_5V_output_mode");
    else
        send_cmd("set_OD_output_mode");
    send_cmd("output_mode_store");
    if (mode == "5V")
        send_cmd("set_5V_output_mode");
    else
        send_cmd("set_OD_output_mode");
    send_cmd("pin_up");
}

void BLTouchEndstopWrapper::cmd_BLTOUCH_DEBUG(GCodeCommand& gcmd)
{
    std::string cmd = gcmd.get_string("COMMAND", "");
    if (cmd == "" || Commands.find(cmd) == Commands.end())
    {
        // gcmd.respond_info("BLTouch commands: %s" % (
        //     ", ".join(sorted([c for c in Commands if c is not None]))))  //---??---
        return;
    }

    gcmd.m_respond_info("Sending BLTOUCH_DEBUG COMMAND=" + cmd, true); //---??---
    sync_print_time();
    send_cmd(cmd, m_pin_move_time);
    sync_print_time();
}

void BLTouchEndstopWrapper::cmd_BLTOUCH_STORE(GCodeCommand& gcmd)
{
    std::string cmd = gcmd.get_string("MODE", "");
    if (cmd == "" || cmd == "5V" || cmd == "OD")
    {
        // gcmd.respond_info("BLTouch output modes: 5V, OD"); //---??---
        return;
    }
    // gcmd.respond_info("Storing BLTouch output mode: %s" % (cmd,)) //---??---
    sync_print_time();
    store_output_mode(cmd);
    sync_print_time();
}
        