#include "utils_ehsm.h"
#include <map>
#include <semaphore.h>
#include <functional>
#include "debug.h"
#include "ui_api.h"
#include "toolhead.h"
#include "mcu.h"
#include "pins.h"
#include "msgproto.h"
#include "heaters.h"
#include "fan.h"
#include "gcode.h"
#include "gcode_move.h"
#include "force_move.h"
#include "extruder.h"
#include "heater_bed.h"
#include "canbus_ids.h"
#include "probe.h"
#include "tuning_tower.h"
#include "BLTouch.h"
#include "verify_heater.h"
#include "pid_calibrate.h"
#include "reactor.h"
#include "input_shaper.h"
#include "adxl345.h"
#include "bed_mesh.h"
#include "pause_resume.h"
#include "buttons.h"
#include "idle_timeout.h"
#include "heater_fan.h"
#include "webhooks.h"
#include "print_stats.h"
#include "virtual_sdcard.h"
#include "query_adc.h"
#include "query_endstops.h"
#include "statistics.h"
#include "fan_generic.h"
#include "resonance_tester.h"
#include "gcode_arcs.h"
#include "printer_para.h"
#include "../devices/serial_debug.h"
#include "net.h"
#include "wifi.h"
#include "auto_leveling.h"
#include "tmc2209.h"
#include "controller_fan.h"
#include "gpio.h"
#include "klippy.h"
#include "my_string.h"
#include "Define_config_path.h"
#define LOG_TAG "printer"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"


extern sem_t sem;
Printer *Printer::m_pInstance = nullptr;
std::mutex mutex_;

Printer::Printer()
{
    m_manual_sq_require = false;
    m_serial_sq_require = false;
    m_highest_priority_sq_require = false;
    m_communication_state = true;
}

Printer::~Printer()
{

    if (m_reactor != nullptr)
        delete m_reactor;

    if (m_gcode != nullptr)
        delete m_gcode;

    if (m_gcode_io != nullptr)
        delete m_gcode_io;

    if (m_webhooks != nullptr)
        delete m_webhooks;

    if (m_pconfig != nullptr)
        delete m_pconfig;

    if (m_print_stats != nullptr)
        delete m_print_stats;

    if (m_mcu != nullptr)
        delete m_mcu;

    if (m_ppins != nullptr)
        delete m_ppins;
    if (m_tool_head != nullptr)
        delete m_tool_head;
    if (m_stepper_enable != nullptr)
        delete m_stepper_enable;
    if (m_pheaters != nullptr)
        delete m_pheaters;
    if (m_printer_extruder != nullptr)
        delete m_printer_extruder;
    if (m_bed_heater != nullptr)
        delete m_bed_heater;
    if (m_printer_fan != nullptr)
        delete m_printer_fan;

    for (auto generic_fan : m_generic_fans)
    {
        if (generic_fan != nullptr)
            delete generic_fan;
    }
    for (auto heater_fan : m_heater_fans)
    {
        if (heater_fan != nullptr)
            delete heater_fan;
    }
    if (m_printer_homing != nullptr)
        delete m_printer_homing;
    if (m_gcode_move != nullptr)
        delete m_gcode_move;
    if (m_force_move != nullptr)
        delete m_force_move;
    if (m_cbid != nullptr)
        delete m_cbid;
    if (m_probe != nullptr)
        delete m_probe;
    if (m_tuning_tower != nullptr)
        delete m_tuning_tower;
    for (auto verify_heater : m_verify_heaters)
    {
        if (verify_heater.second != nullptr)
            verify_heater.second;
    }

    if (m_pid_calibrate != nullptr)
        delete m_pid_calibrate;
    if (m_input_shaper != nullptr)
        delete m_input_shaper;
    for (auto adxl345 : m_adxl345s)
    {
        if (adxl345.second != nullptr)
            adxl345.second;
    }
    if (m_bed_mesh != nullptr)
        delete m_bed_mesh;
    if (m_idle_timeout != nullptr)
        delete m_idle_timeout;
    if (m_virtual_sdcard != nullptr)
        delete m_virtual_sdcard;
    if (m_query_adc != nullptr)
        delete m_query_adc;

    if (m_query_endstops != nullptr)
        delete m_query_endstops;

    if (m_printer_sys_stats != nullptr)
        delete m_printer_sys_stats;

    if (m_printer_stats != nullptr)
        delete m_printer_stats;

    if (m_manual_probe != nullptr)
        delete m_manual_probe;

    if (m_pause_resume != nullptr)
        delete m_pause_resume;

    if (m_buttons != nullptr)
        delete m_buttons;
    if (m_resonance_tester != nullptr)
        delete m_resonance_tester;
    if (m_gcode_arcs != nullptr)
        delete m_gcode_arcs;
    if (m_printer_para != nullptr)
        delete m_printer_para;
}

void *Printer::lookup_object(std::string name)
{
    if (m_objects.find(name) != m_objects.end())
        return m_objects[name];
    return nullptr;
}

void Printer::add_object(std::string name, void *obj)
{
    if (m_objects.find(name) != m_objects.end())
        std::cout << "Printer object " << name << "already created" << std::endl;
    m_objects[name] = obj;
}
void *Printer::load_object(std::string section)
{
    if (m_objects.find(section) != m_objects.end())
        return m_objects[section];
    std::vector<std::string> module_parts = split(section, " ");
    if (module_parts.size() == 0)
        return nullptr;
    std::string module_name = module_parts[0];

    // std::cout << " find section:" << section << " module:" << module_name << std::endl;
    if (module_name == "heater_bed")
    {
        return m_objects[section] = m_bed_heater = new PrinterHeaterBed(module_name);
    }
    else if (module_name == "verify_heater")
    {
        return m_objects[section] = m_verify_heaters[section] = new HeaterCheck(section);
    }
    else if (module_name == "pid_calibrate")
    {
        return m_objects[section] = m_pid_calibrate = new PIDCalibrate(module_name);
    }
    else if (module_name == "gcode_move")
    {
        return m_objects[section] = m_gcode_move = new GCodeMove(module_name);
    }
    else if (module_name == "homing")
    {
        return m_objects[section] = m_printer_homing = new PrinterHoming(module_name);
    }
    else if (module_name == "idle_timeout")
    {
        return m_objects[section] = m_idle_timeout = new IdleTimeout(module_name);
    }
    else if (module_name == "system_stats")
    {
        return m_objects[section] = m_printer_sys_stats = new PrinterSysStats(module_name);
    }
    else if (module_name == "statistics")
    {
        return m_objects[section] = m_printer_stats = new PrinterStats(module_name);
    }
    else if (module_name == "manual_probe")
    {
        return m_objects[section] = m_manual_probe = new ManualProbe(module_name);
    }
    else if (module_name == "tuning_tower")
    {
        return m_objects[section] = m_tuning_tower = new TuningTower(module_name);
    }
    else if (module_name == "stepper_enable")
    {
        return m_objects[section] = m_stepper_enable = new PrinterStepperEnable(module_name);
    }
    else if (module_name == "force_move")
    {
        return m_objects[section] = m_force_move = new ForceMove(module_name);
    }
    else if (module_name == "query_endstops")
    {
        return m_objects[section] = m_query_endstops = new QueryEndstops(module_name);
    }
    else if (module_name == "heaters")
    {
        return m_objects[section] = m_pheaters = new PrinterHeaters(module_name);
    }
    else if (module_name == "fan")
    {
        PrinterFan *fan = new PrinterFan(section);
        std::string name = split(section, " ").back();
        if (name == "model")
            m_printer_fan = fan;
        m_printer_fans.push_back(fan);
        return m_objects[section] = fan;
    }
    else if (module_name == "fan_generic")
    {
        PrinterFanGeneric *generic_fan = new PrinterFanGeneric(section);
        m_generic_fans.push_back(generic_fan);
        return m_objects[section] = generic_fan;
    }
    else if (module_name == "heater_fan")
    {
        PrinterHeaterFan *heater_fan = new PrinterHeaterFan(section);
        m_heater_fans.push_back(heater_fan);
        return m_objects[section] = heater_fan;
    }
    else if (module_name == "query_adc")
    {
        return m_objects[section] = m_query_adc = new QueryADC(module_name);
    }
    else if (module_name == "canbus_ids")
    {
        return m_objects[section] = m_cbid = new PrinterCANBus(module_name);
    }
    else if (module_name == "probe")
    {
        ProbeEndstopWrapperBase *mcu_probe = new ProbeEndstopWrapper(module_name);
        return m_objects[section] = m_probe = new PrinterProbe(module_name, mcu_probe);
    }
    // else if (module_name == "bltouch")
    // {
    //     ProbeEndstopWrapperBase *mcu_probe = new BLTouchEndstopWrapper(module_name);
    //     return m_objects[section] = m_probe = new PrinterProbe(module_name, mcu_probe);
    // }
    else if (module_name == "input_shaper")
    {
        return m_objects[section] = m_input_shaper = new InputShaper(module_name);
    }
    else if (module_name == "adxl345")
    {
        ADXL345 *adxl345 = new ADXL345(section);
        return m_objects[section] = m_adxl345s[section] = adxl345;
    }
    else if (module_name == "bed_mesh_probe")
    {
        ProbeEndstopWrapperBase *bed_mesh_mcu_probe = new ProbeEndstopWrapper(module_name);
        return m_objects[section] = m_bed_mesh_probe = new PrinterProbe(module_name, bed_mesh_mcu_probe);
    }
    else if (module_name == "bed_mesh")
    {
        return m_objects[section] = m_bed_mesh = new BedMesh(module_name);
    }
    else if (module_name == "pause_resume")
    {
        return m_objects[section] = m_pause_resume = new PauseResume(module_name);
    }
    else if (module_name == "buttons")
    {
        return m_objects[section] = m_buttons = new PrinterButtons();
    }
    else if (module_name == "gcode_arcs")
    {
        return m_objects[section] = m_gcode_arcs = new ArcSupport(module_name);
    }
    else if (module_name == "resonance_tester")
    {
        return m_objects[section] = m_resonance_tester = new ResonanceTester(module_name);
    }
    else if (module_name == "virtual_sdcard")
    {
        return m_objects[section] = m_virtual_sdcard = new VirtualSD(module_name);
    }
    else if (module_name == "tmc2209")
    {
        return m_objects[section] = new TMC2209(section);
    }
    else if (module_name == "auto_leveling")
    {
        return m_objects[section] = m_auto_leveling = new AutoLeveling(module_name);
    }
    else if (module_name == "controller_fan")
    {
#if ENABLE_MANUTEST
        ControllerFan *controller_fan = m_controller_fan = new ControllerFan(section);
#else
        ControllerFan *controller_fan = new ControllerFan(section);
#endif
        controller_fans.push_back(controller_fan);
        return m_objects[section] = controller_fan;
    }
    else if (module_name == "safe_z_home")
    {
        return m_objects[section] = m_safe_z_homing = new SafeZHoming(section);
    }
    else if (module_name == "command_controller")
    {
        return m_objects[section] = m_command_controller = new CommandController(section);
    }
    else if (module_name == "strain_gauge")
    {
        return m_objects[section] = m_strain_gauge = new StrainGaugeWrapper(section);
    }
    else if (module_name == "filter")
    {
        return m_objects[section] = m_filter = new Filter(section);
    }
    else if (module_name == "hx711s")
    {
        return m_objects[section] = m_hx711s = new HX711S(section);
    }
    else if (module_name == "hx711_sample")
    {
        return m_objects[section] = m_hx711_sample = new HX711Sample(section);
    }
    else if (module_name == "dirzctl")
    {
        return m_objects[section] = m_dirzctl = new DirZCtl(section);
    }
    else if (module_name == "led")
    {
        PrinterPWMLED *m_led = new PrinterPWMLED(section);
        std::string name = split(section, " ").back();
        if (name == "led1")
        {
            m_printer_pwmled = m_led;
        }
        else if (name == "led2")
        {
            m_lightled = m_led;
        }
        return m_objects[section] = m_led;
    }
    else if (module_name == "typec_led")
    {
        m_usb_led = new SysfsLEDControl(section);
        return m_objects[section] = m_usb_led;
    }
    else if (module_name == "box_led")
    {
        m_box_led = new SysfsLEDControl(section);
        return m_objects[section] = m_box_led;
    }
    else if (module_name == "neopixel")
    {
        return m_objects[section] = m_neopixel = new NeoPixel(section);
    }
    else if (module_name == "break_save")
    {
        return m_objects[section] = m_break_save = new BreakSave(section);
    }
    else if (module_name == "temperature_sensor")
    {
        return m_objects[section] = m_temperature_sensor = new PrinterSensorGeneric(section);
    }
    else if (module_name == "change_filament")
    {
        return m_objects[section] = m_change_filament = new ChangeFilament(section);
    }
    // else if (module_name == "mcu")
    // {
    // }
    // else if (module_name == "stepper_x")
    // {
    // }
    // else if (module_name == "stepper_y")
    // {
    // }
    // else if (module_name == "stepper_z")
    // {
    // }
    // else if (startswith(module_name, "extruder ") )
    // {
    // }
    else
    {
        // std::cout << "no object find section:" << section << " module:" << module_name << std::endl;
    }

    return nullptr;
}
void *Printer::load_default_object()
{
    if (m_bed_heater == nullptr)
    {
        load_object("heater_bed");
    }
    // if (m_printer_sys_stats == nullptr)
    // {
    //     load_object("system_stats");
    // }
    // if (m_pid_calibrate == nullptr)
    // {
    //     load_object("pid_calibrate");
    // }

    // if (m_gcode_move == nullptr)
    // {
    //     load_object("gcode_move");
    // }
    // if (m_printer_homing == nullptr)
    // {
    //     load_object("homing");
    // }
    // if (m_idle_timeout == nullptr)
    // {
    //     load_object("idle_timeout");
    // }
    // if (m_printer_stats == nullptr)
    // {
    //     load_object("statistics");
    // }
    // if (m_manual_probe == nullptr)
    // {
    //     load_object("manual_probe");
    // }
    // if (m_tuning_tower == nullptr)
    // {
    //     load_object("tuning_tower");
    // }
    // if (m_stepper_enable == nullptr)
    // {
    //     load_object("stepper_enable");
    // }
    // if (m_force_move == nullptr)
    // {
    //     load_object("force_move");
    // }
    // if (m_query_endstops == nullptr)
    // {
    //     load_object("query_endstops");
    // }
    // if (m_pheaters == nullptr)
    // {
    //     load_object("heaters");
    // }
    if (m_printer_fan == nullptr)
    {
        load_object("fan");
    }
    if (m_query_adc == nullptr)
    {
        load_object("query_adc");
    }
    // if (m_cbid == nullptr)           //?
    // {
    //     load_object("canbus_ids");
    // }
    if (m_probe == nullptr)
    {
        load_object("probe"); // bltouch
    }
    if (m_input_shaper == nullptr)
    {
        load_object("input_shaper");
    }
    if (m_bed_mesh_probe == nullptr)
    {
        load_object("bed_mesh_probe");
    }
    if (m_bed_mesh == nullptr)
    {
        load_object("bed_mesh");
    }
    // if (m_pause_resume == nullptr)
    // {
    //     load_object("pause_resume");
    // }
    // if (m_buttons == nullptr)
    // {
    //     load_object("buttons");
    // }
    // if (m_gcode_arcs == nullptr)
    // {
    //     load_object("gcode_arcs");
    // }
    if (m_resonance_tester == nullptr)
    {
        load_object("resonance_tester");
    }
    // if (m_virtual_sdcard == nullptr)
    // {
    //     load_object("virtual_sdcard");
    // }
    if (m_auto_leveling == nullptr)
    {
        load_object("auto_leveling");
    }

    // if (m_adxl345s.size() == 0)
    // {
    //     load_object("adxl345 X");
    // }
    // if (m_generic_fans.size() == 0)
    // {
    //     load_object("fan_generic");
    // }
    // if (m_heater_fans.size() == 0)
    // {
    //     load_object("heater_fan hotend_cooling_fan");
    // }
    // if (m_verify_heaters.size() == 0)
    // {
    //     load_object("verify_heater extruder");
    // }
    // if (lookup_object("tmc2209 stepper_x") == nullptr)
    // {
    //     load_object("tmc2209 stepper_x");
    //     load_object("tmc2209 stepper_y");
    //     load_object("tmc2209 stepper_z");
    //     load_object("tmc2209 extruder");
    // }

    // if (lookup_object("controller_fan board_cooling_fan") == nullptr)
    // {
    //     load_object("controller_fan board_cooling_fan");
    // }

    // std::cout << " find section:" << section << " module:" << module_name << std::endl;
    return nullptr;
}

void Printer::read_config()
{
    printf("pritf read_config\n");
    // m_pconfig = new ConfigParser(cfg_path);
    m_printer_para = new Printer_para();
    add_object("configfile", m_pconfig); // Printer::GetInstance()->m_pconfig
    // create printer components
    m_ppins = new PrinterPins();
    add_object("pins", m_ppins);
    add_printer_mcu();
    std::vector<std::string> sections = m_pconfig->get_prefix_sections("");
    if (user_pconfig != nullptr)
    {
        // 用户配置文件内容覆盖默认配置文件内容
        std::vector<std::string> user_sections = user_pconfig->get_prefix_sections("");
        for (int i = 0; i < user_sections.size(); i++)
        {
            std::cout << "user section = " << user_sections[i] << std::endl;
            m_pconfig->Setuservalue(user_pconfig->GetSection(user_sections[i]));
        }
    }
    for (int i = 0; i < sections.size(); i++)
    {
        // std::cout << "section " << sections[i] << std::endl;
        load_object(sections[i]);
    }
    // load_default_object();
    m_tool_head = new ToolHead("printer"); //"toolhead"
    add_object("toolhead", m_tool_head);
    m_tool_head->load_kinematics("printer");
    add_printer_extruder();
}

SelectReactor *Printer::get_reactor()
{
    return m_reactor;
}

bool Printer::is_shutdown()
{
    return m_in_shutdown_state;
}

int Printer::get_exceptional_temp_status()
{
    return exceptional_temp_status;
}

void Printer::set_exceptional_temp_status(int exceptional_temp_status)
{
    this->exceptional_temp_status = exceptional_temp_status;
}

bool Printer::get_break_save_state()
{
    return break_save_state;
}

void Printer::set_break_save_state(bool break_save_state)
{
    this->break_save_state = break_save_state;
    Printer::GetInstance()->send_event("klippy:break_connection");
}

std::vector<std::string> Printer::get_serial_data_error_state()
{
    return serial_data_error;
}

void Printer::set_serial_data_error_state(std::string serial_data_error_state)
{
    serial_data_error.push_back(serial_data_error_state);
}

void Printer::invoke_shutdown(std::string msg)
{
    if (m_in_shutdown_state)
    {
        return;
    }
    m_gcode_io->single_command("M104 S0");
    m_gcode_io->single_command("M140 S0");
    m_gcode_io->single_command("M107");
    m_in_shutdown_state = true;
    send_event("klippy:shutdown");
    send_event("klippy:shutdown", false);
    send_event("klippy:break_connection");
}

void Printer::invoke_async_shutdown(std::string msg)
{
    invoke_shutdown(msg);
}

void Printer::connect()
{
    m_reactor = new SelectReactor(true); // add_early_printer_objects
    m_gcode = new GCodeDispatch();
    m_gcode_io = new GCodeIO();
    m_print_stats = new PrintStats("");

    add_object("gcode", m_gcode);
    add_object("gcode_io", m_gcode_io);

    // m_webhooks = new WebHooks();
    read_config();

    // load_object("adxl345");
    load_object("gcode_arcs");
    load_object("pause_resume");
    send_event("klippy:mcu_identify");
    send_event("klippy:connect");
    send_event("klippy:ready");

    load_object("command_controller");

    Printer::GetInstance()->m_reactor->update_timer(m_tool_head->m_stats_timer, get_monotonic() + 1.0);
}

std::string Printer::get_start_args(std::string arg)
{
    if (arg == "start_reason")
        return START_REASON;
    if (arg == "is_fileoutput")
    {
        if (IS_FILEOUTPUT == 0)
        {
            return "";
        }
        return "IS_FILEOUTPUT";
    }
    return "";
}

void Printer::register_event_handler(std::string event, std::function<void()> callback)
{
    auto ret = event_handlers.find(event);
    if (ret != event_handlers.end())
    {
        std::cout << "redo register_event_handler event = " << event << std::endl;
        return;
    }
    event_handlers[event] = callback;
}

void Printer::register_event_bool_handler(std::string event, std::function<void(bool)> callback)
{
    auto ret = event_bool_handlers.find(event);
    if (ret != event_bool_handlers.end())
    {
        std::cout << "redo register_event_bool_handler event = " << event << std::endl;
        return;
    }
    event_bool_handlers[event] = callback;
}
void Printer::register_event_double_handler(std::string event, std::function<void(double)> callback)
{
    auto ret = event_double_handlers.find(event);
    if (ret != event_double_handlers.end())
    {
        std::cout << "redo register_event_double_handler event = " << event << std::endl;
        return;
    }
    event_double_handlers[event] = callback;
}

void Printer::register_event_double3_handler(std::string event, std::function<void(double, double, double)> callback)
{
    auto ret = event_double3_handlers.find(event);
    if (ret != event_double3_handlers.end())
    {
        std::cout << "redo register_event_double3_handler event = " << event << std::endl;
        return;
    }
    event_double3_handlers[event] = callback;
}

void Printer::register_event_homing_move_handler(std::string event, std::function<void(HomingMove *)> callback)
{
    auto ret = event_homing_move_handlers.find(event);
    if (ret != event_homing_move_handlers.end())
    {
        std::cout << "redo register_event_homing_move_handler event = " << event << std::endl;
        return;
    }
    event_homing_move_handlers[event] = callback;
}

void Printer::register_event_homing_handler(std::string event, std::function<void(Homing *, std::vector<PrinterRail *>)> callback)
{
    auto ret = event_homing_handlers.find(event);
    if (ret != event_homing_handlers.end())
    {
        std::cout << "redo register_event_homing_handler event = " << event << std::endl;
        return;
    }
    event_homing_handlers[event] = callback;
}

int Printer::get_printer_sem_state()
{
    int value = 0;
    sem_getvalue(&sem, &value) != 0;
    return value;
}

void Printer::send_event(std::string event)
{
    std::map<std::string, std::function<void()>>::iterator mcu_iter = event_handlers.begin();
    std::vector<std::string> error_type;
    while (mcu_iter != event_handlers.end())
    {
        if (startswith(mcu_iter->first, event) && endswith(mcu_iter->first, "MCU"))
        {
            // std::cout << "mcu_iter->first = " << mcu_iter->first << std::endl;
            event_handlers[mcu_iter->first]();
        }
        mcu_iter++;
        // error_type = Printer::GetInstance()->get_serial_data_error_state();
        // if (!error_type.empty())
        // {
        //     if (get_printer_sem_state())
        //     {
        //         sem_post(&sem);
        //     }
        // }
    }

    std::map<std::string, std::function<void()>>::iterator iter = event_handlers.begin();
    while (iter != event_handlers.end())
    {
        if (startswith(iter->first, event) && !endswith(iter->first, "MCU"))
        {
            // std::cout << "iter->first = " << iter->first << std::endl;
            event_handlers[iter->first]();
        }
        iter++;
        // error_type = Printer::GetInstance()->get_serial_data_error_state();
        // if (!error_type.empty())
        // {
        //     if (get_printer_sem_state())
        //     {
        //         sem_post(&sem);
        //     }
        // }
    }
}

void Printer::send_event(std::string event, bool force)
{
    std::map<std::string, std::function<void(bool)>>::iterator iter = event_bool_handlers.begin();
    while (iter != event_bool_handlers.end())
    {
        if (startswith(iter->first, event))
        {
            event_bool_handlers[iter->first](force);
        }
        iter++;
    }
}

void Printer::send_event(std::string event, double print_time)
{
    std::map<std::string, std::function<void(double)>>::iterator iter = event_double_handlers.begin();
    while (iter != event_double_handlers.end())
    {
        if (startswith(iter->first, event))
        {
            event_double_handlers[iter->first](print_time);
        }
        iter++;
    }
}

void Printer::send_event(std::string event, double curtime, double print_time, double est_print_time)
{
    std::map<std::string, std::function<void(double, double, double)>>::iterator iter = event_double3_handlers.begin();
    while (iter != event_double3_handlers.end())
    {
        if (startswith(iter->first, event))
        {
            event_double3_handlers[iter->first](curtime, print_time, est_print_time);
        }
        iter++;
    }
}

void Printer::send_event(std::string event, HomingMove *hmove)
{
    std::map<std::string, std::function<void(HomingMove *)>>::iterator iter = event_homing_move_handlers.begin();
    while (iter != event_homing_move_handlers.end())
    {
        if (startswith(iter->first, event))
        {
            event_homing_move_handlers[iter->first](hmove);
        }
        iter++;
    }
}

void Printer::send_event(std::string event, Homing *homing_state, std::vector<PrinterRail *> rails)
{
    std::map<std::string, std::function<void(Homing *, std::vector<PrinterRail *>)>>::iterator iter = event_homing_handlers.begin();
    while (iter != event_homing_handlers.end())
    {
        if (startswith(iter->first, event))
        {
            event_homing_handlers[iter->first](homing_state, rails);
        }
        iter++;
    }
}

void Printer::manual_control_signal()
{
    m_manual_sq_require = true;
}

void Printer::serial_control_signal()
{
    m_serial_sq_require = true;
}

void Printer::highest_priority_control_signal()
{
    m_highest_priority_sq_require = true;
}

void Printer::fan_cmd_control_signal()
{
    m_fan_cmd_sq_require = true;
}

void *process_task_printer(void *arg)
{
    try
    {
        mutex_.lock();
        Printer::GetInstance();
        mutex_.unlock();

        LOG_D("process_task_printer starting...\n");
        Printer::GetInstance()->connect();

        // LOG_D("Starting reactor reliability tests...\n");
        // LOG_D("Initial timer count: %zu\n", Printer::GetInstance()->m_reactor->m_timers.size());

        // // 测试任务1: 快速周期性定时器
        // LOG_D("Creating fast periodic timer (100ms interval)...\n");
        // auto fast_timer = Printer::GetInstance()->m_reactor->register_timer("fast_timer", [](double eventtime) -> double
        //                                                                     {
        //                                                                         static int count = 0;
        //                                                                         count++;
        //                                                                         if (count % 1000 == 0)
        //                                                                         { // 降低输出频率
        //                                                                             LOG_D("[Fast Timer] Triggered %d times\n", count);
        //                                                                         }
        //                                                                         return eventtime + 0.1; // 改为100ms触发一次
        //                                                                     },
        //                                                                     get_monotonic());

        // // 测试任务2: 中等周期定时器
        // LOG_D("Creating medium timer (1s interval)...\n");
        // auto medium_timer = Printer::GetInstance()->m_reactor->register_timer("medium_timer", [reactor = Printer::GetInstance()->m_reactor](double eventtime) -> double
        //                                                                       {
        //         static int count = 0;
        //         count++;
        //         LOG_D("[Medium Timer] Triggered %d times\n", count);
        //         if (count >= 10) {
        //             LOG_D("[Medium Timer] Reached 10 counts, self-deleting\n");
        //             return reactor->m_NEVER;
        //         }
        //         return eventtime + 1.0; }, get_monotonic());

        // // 测试任务3: 慢速周期定时器
        // LOG_D("Creating slow timer (5s interval)...\n");
        // auto slow_timer = Printer::GetInstance()->m_reactor->register_timer("slow_timer", [reactor = Printer::GetInstance()->m_reactor, &fast_timer](double eventtime) -> double
        //                                                                     {
        //         static int count = 0;
        //         count++;
        //         LOG_D("[Slow Timer] Triggered %d times\n", count);
        //         if (count == 5) {
        //             LOG_D("[Slow Timer] Attempting to delete fast timer\n");
        //             reactor->delay_unregister_timer(fast_timer);
        //         }
        //         return eventtime + 5.0; }, get_monotonic());

        // LOG_D("All test timers created. Total timers: %zu\n",
        //       Printer::GetInstance()->m_reactor->m_timers.size());

        sem_post(&sem);
        usleep(1000 * 1000);

        // // 状态监控定时器 - 每10秒输出一次
        // auto monitor_timer = Printer::GetInstance()->m_reactor->register_timer("monitor_timer", [reactor = Printer::GetInstance()->m_reactor](double eventtime) -> double
        //                                                                        {
        //         LOG_D("Timer status - Active timers: %zu\n", reactor->m_timers.size());
        //         return eventtime + 10.0; }, get_monotonic());

        while (1)
        {
            double poll_time = Printer::GetInstance()->m_reactor->_check_timers(get_monotonic(), true);

            // ui command
            if (Printer::GetInstance()->m_manual_sq_require)
            {
                Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_command_controller->ui_command_timer, Printer::GetInstance()->m_reactor->m_NOW);
            }

            if (Printer::GetInstance()->m_fan_cmd_sq_require)
            {
                Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_command_controller->fan_cmd_timer, Printer::GetInstance()->m_reactor->m_NOW);
            }

            // serial_debug
            if (Printer::GetInstance()->m_serial_sq_require)
            {
                Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_command_controller->serial_command_timer, Printer::GetInstance()->m_reactor->m_NOW);
            }

            // highest_priority_cmd
            if (Printer::GetInstance()->m_highest_priority_sq_require)
            {
                Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_command_controller->highest_priority_cmd_timer, Printer::GetInstance()->m_reactor->m_NOW);
            }

            usleep(1000);
        } /* end of while(1) */
    }
    catch (const MCUException &e)
    {
        LOG_E("Exception in process_task_printer: %s\n", e.getType().c_str());
        Printer::GetInstance()->set_serial_data_error_state(e.getType());
        Printer::GetInstance()->m_communication_state = false;
        if (e.getType() == "")
        {
        }
        else if (e.getType() == "strain_gauge_mcu")
        {
        }
        else if (e.getType() == "stm32")
        {
        }
        int value = 0;
        sem_getvalue(&sem, &value);
        if (value == 0)
        {
            sem_post(&sem);
        }
    }
    while (1)
    {
    }
}
