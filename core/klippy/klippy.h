#ifndef __KLIPPY_H__
#define __KLIPPY_H__

#include <map>
#include <semaphore.h>
#include <functional>

#include "utils_ehsm.h"
#include "debug.h"
#include "ui_api.h"
#include "toolhead.h"
#include "mcu.h"
#include "pins.h"
#include "msgproto.h"
#include "heaters.h"
#include "fan.h"
// #include "dspMemOps.h"
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
#include "safe_z_home.h"
#include "command_controller.h"
#include "strain_gauge.h"
#include "filter.h"
#include "led.h"
#include "neopixel.h"
#include "temperature_sensor.h"
#include "break_save.h"
#include "hx711_sample.h"
#include "change_filament.h"

extern "C"
{
#include "../devices/hl_net.h"
}

#include "configfile.h"

class SelectReactor;
class GCodeDispatch;
class GCodeIO;
class WebHooks;
class ConfigParser;
class PrintStats;
class MCU;
class CommandController;

class PrinterPins;
class ToolHead;
class PrinterStepperEnable;
class PrinterHeaters;
class PrinterExtruder;
class PrinterHeaterBed;
class PrinterFan;

class PrinterHoming;
class GCodeMove;
class ForceMove;
class PrinterCANBus;
class PrinterProbe;
class TuningTower;
class PIDCalibrate;
class InputShaper;

class BedMesh;
class PrinterProbe; // 床网调平专用探针
class IdleTimeout;
class VirtualSD;
class QueryADC;
class QueryEndstops;
class PrinterSysStats;
class PrinterStats;
class ManualProbe;
class PauseResume;
class PrinterButtons;
class ResonanceTester;
class ArcSupport;
class Printer_para;
class AutoLeveling;

class HomingMove;
class Homing;

class PrinterRail;

class PrinterFanGeneric;
class PrinterHeaterFan;
class HeaterCheck;
class ADXL345;

#define IS_FILEOUTPUT 0
#define START_REASON ""

// 风扇id 和配置文件的顺序有关
enum {
    MODEL_FAN,
    MODEL_HELPER_FAN,
    BOX_FAN,
};

class MCUException : public std::exception {
public:
    MCUException(const std::string& type, const std::string& message) : m_type(type), m_message(message) {}

    const char* what() const throw() {
        return m_message.c_str();
    }

    std::string getType() const {
        return m_type;
    }

private:
    std::string m_type;
    std::string m_message;
};

class Printer
{
private:
    Printer();
    // static Printer m_pInstance;
    static Printer *m_pInstance;

public:
    ~Printer();
    std::string state_message;
    bool m_in_shutdown_state;
    bool m_communication_state;
    bool m_restart_flag = false;
    int exceptional_temp_status = 0;
    bool break_save_state = false;
    std::vector<std::string> serial_data_error;
    std::map<std::string, std::function<void()>> event_handlers;
    std::map<std::string, std::function<void(bool)>> event_bool_handlers;
    std::map<std::string, std::function<void(double)>> event_double_handlers;
    std::map<std::string, std::function<void(double, double, double)>> event_double3_handlers;
    std::map<std::string, std::function<void(HomingMove *)>> event_homing_move_handlers;
    std::map<std::string, std::function<void(Homing *, std::vector<PrinterRail *>)>> event_homing_handlers;
    std::map<std::string, void *> m_objects;
    SelectReactor *get_reactor();
    std::string get_state_message();
    bool is_shutdown();
    int get_exceptional_temp_status();
    void set_exceptional_temp_status(int exceptional_temp_status);
    bool get_break_save_state();
    void set_break_save_state(bool break_save_state);
    std::vector<std::string> get_serial_data_error_state();
    void set_serial_data_error_state(std::string serial_data_error_state);
    void set_state(std::string msg);
    void add_object(std::string name, void *obj);
    void *lookup_object(std::string name);
    void *lookup_objects();
    void *load_object(std::string section);
    void *load_default_object();
    void read_config();
    void get_versions();
    void connect();
    void run();
    void invoke_shutdown(std::string msg);
    void set_rollover_info();
    void invoke_async_shutdown(std::string msg);
    void register_event_handler(std::string event, std::function<void()> callback);
    void register_event_bool_handler(std::string event, std::function<void(bool)> callback);
    void register_event_double_handler(std::string event, std::function<void(double)> callback);
    void register_event_double3_handler(std::string event, std::function<void(double, double, double)> callback);
    void register_event_homing_move_handler(std::string event, std::function<void(HomingMove *)> callback);
    void register_event_homing_handler(std::string event, std::function<void(Homing *, std::vector<PrinterRail *>)> callback);
    int get_printer_sem_state();
    void send_event(std::string event);
    void send_event(std::string event, bool force);
    void send_event(std::string event, double print_time);
    void send_event(std::string event, double curtime, double print_time, double est_print_time);
    void send_event(std::string event, HomingMove *hmove);
    void send_event(std::string event, Homing *homing_state, std::vector<PrinterRail *> rails);
    void request_exit(std::string result);
    std::string get_start_args(std::string arg = "");
    void manual_control_signal();
    void serial_control_signal();
    void highest_priority_control_signal();
    void fan_cmd_control_signal();

public:
    // static Printer * GetInstance()
    // {
    // 	static Printer m_pInstance;
    // 	return &m_pInstance;
    // }
    // 线程安全？？
    static Printer *GetInstance()
    {   
        // 懒汉式
        if (m_pInstance == NULL)
            m_pInstance = new Printer();
        return m_pInstance;
    }
    std::atomic_bool m_manual_sq_require;
    std::atomic_bool m_serial_sq_require;
    std::atomic_bool m_highest_priority_sq_require;
    std::atomic_bool m_fan_cmd_sq_require;
    SelectReactor *m_reactor = nullptr;
    GCodeDispatch *m_gcode = nullptr;
    GCodeIO *m_gcode_io = nullptr;
    WebHooks *m_webhooks = nullptr;
    ConfigParser *m_pconfig = nullptr;
    ConfigParser *m_unmodifiable_cfg = nullptr;
    ConfigParser *user_pconfig = nullptr;
    PrintStats *m_print_stats = nullptr;
    MCU *m_mcu = nullptr;
    std::map<std::string, MCU *> m_mcu_map;
    PrinterPins *m_ppins = nullptr;
    ToolHead *m_tool_head;
    PrinterStepperEnable *m_stepper_enable = nullptr;
    PrinterHeaters *m_pheaters = nullptr;
    PrinterExtruder *m_printer_extruder = nullptr;
    PrinterHeaterBed *m_bed_heater = nullptr;
    PrinterFan *m_printer_fan; // 特指模型风扇
    std::vector<PrinterFan *> m_printer_fans;

    std::vector<PrinterFanGeneric *> m_generic_fans;
    std::vector<PrinterHeaterFan *> m_heater_fans;
    std::vector<ControllerFan *> controller_fans;
    PrinterHoming *m_printer_homing = nullptr;
    GCodeMove *m_gcode_move = nullptr;
    ForceMove *m_force_move = nullptr;
    PrinterCANBus *m_cbid = nullptr;
    PrinterProbe *m_probe = nullptr;
    TuningTower *m_tuning_tower = nullptr;
    std::map<std::string, HeaterCheck *> m_verify_heaters;
    // HeaterCheck *m_verify_heater = nullptr;
    PIDCalibrate *m_pid_calibrate = nullptr;
    InputShaper *m_input_shaper = nullptr;
    std::map<std::string, ADXL345 *> m_adxl345s;
    BedMesh *m_bed_mesh = nullptr;
    PrinterProbe *m_bed_mesh_probe = nullptr; // 床网调平专用探针
    IdleTimeout *m_idle_timeout = nullptr;
    VirtualSD *m_virtual_sdcard = nullptr;
    QueryADC *m_query_adc = nullptr;
    QueryEndstops *m_query_endstops = nullptr;
    PrinterSysStats *m_printer_sys_stats = nullptr;
    PrinterStats *m_printer_stats = nullptr;
    ManualProbe *m_manual_probe = nullptr;
    PauseResume *m_pause_resume = nullptr;
    PrinterButtons *m_buttons = nullptr;
    ResonanceTester *m_resonance_tester = nullptr;
    ArcSupport *m_gcode_arcs = nullptr;
    Printer_para *m_printer_para = nullptr;
    AutoLeveling *m_auto_leveling = nullptr;
#if ENABLE_MANUTEST
    ControllerFan *m_controller_fan = nullptr;
#endif
    SafeZHoming *m_safe_z_homing = nullptr;
    CommandController *m_command_controller = nullptr;
    StrainGaugeWrapper *m_strain_gauge = nullptr;
    Filter *m_filter = nullptr;
    DirZCtl *m_dirzctl = nullptr;
    HX711S *m_hx711s = nullptr;
    HX711Sample *m_hx711_sample = nullptr;
    PrinterPWMLED *m_printer_pwmled = nullptr;
    PrinterPWMLED *m_lightled = nullptr;
    SysfsLEDControl *m_usb_led = nullptr;
    SysfsLEDControl *m_box_led = nullptr;
    NeoPixel *m_neopixel = nullptr;
    BreakSave *m_break_save = nullptr;
    PrinterSensorGeneric *m_temperature_sensor = nullptr;
    ChangeFilament* m_change_filament = nullptr;
};
#endif
