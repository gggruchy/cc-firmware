#ifndef __HOMING_H__
#define __HOMING_H__

#include "mcu_io.h"
#include <unordered_map>
#include "gcode.h"
#include "print_rail.h"
#define HomingLOG() // std--::cout << "######### File: " << __FILE__ << "  line:" << __LINE__ << "  function:" << __FUNCTION__ << std::endl
struct HomingInfo
{
    double speed;
    double position_endstop;
    double retract_speed;
    double retract_dist;
    bool positive_dir = false;
    double second_homing_speed;
};

class Homing
{
public:
    Homing();
    ~Homing();

    std::map<std::string, double> m_kin_spos;
    std::vector<int> m_changed_axes;

    void set_axes(std::vector<int> axes);
    std::vector<int> get_axes();
    std::map<std::string, double> get_stepper_trigger_positions();
    void set_homed_position(std::vector<double> pos);
    std::vector<double> fill_coord(std::vector<double> coord);
    bool home_rails(std::vector<PrinterRail *> rails, std::vector<double> forcepos, std::vector<double> movepos, int axis);

private:
};

// struct StepperPosition
// {
//     MCU_stepper stepper;
//     std::string endstop_name;
//     std::string stepper_name;
//     std::vector<double> start_pos;
//     std::vector<double> halt_pos;
//     std::vector<double> trig_pos;
//     StepperPosition(MCU_stepper _stepper, std::string _endstop_name)
//     {
//         stepper = _stepper;
//         endstop_name = _endstop_name;
//         stepper_name = stepper.getName();
//         start_pos = stepper.get_mcu_position();
//     }
// };

class HomingMove
{
private:
public:
    HomingMove(std::vector<MCU_endstop *> endstops);
    ~HomingMove();
    std::vector<MCU_endstop *> m_endstops;
    double calc_endstop_rate(MCU_endstop *mcu_endstop, std::vector<double> movepos, double speed);
    std::vector<double> homing_move(std::vector<double> movepos, double homing_speed, bool probe_pos = false,
                                    bool triggered = true, bool check_triggered = true);
    double homing_z_move(std::vector<double> movepos, double homing_speed, bool probe_pos = false,
                                      bool triggered = true, bool check_triggered = true);
    std::vector<double> G29_z_move(std::vector<double> movepos, double homing_speed, bool probe_pos = false,
                                      bool triggered = true, bool check_triggered = true);                                  
    std::vector<MCU_endstop *> get_mcu_endstops();
    std::string check_no_movement();
    std::map<std::string, int64_t> m_end_mcu_pos;
    std::map<std::string, int64_t> m_start_mcu_pos;
    bool is_succ;
};

class PrinterHoming
{
private:
public:
    PrinterHoming(std::string section_name);
    ~PrinterHoming();

    void manual_home(std::vector<MCU_endstop *> endstops, std::vector<double> pos, double speed, bool triggered, bool check_triggered);
    std::vector<double> probing_move(MCU_endstop *mcu_probe_endstop, std::vector<double> pos, double speed);
    void cmd_G28(GCodeCommand &gcmd);
};

enum
{
    HOMING_STATE_X_BEGIN = 0,
    HOMING_STATE_Y_BEGIN = 1,
    HOMING_STATE_Z_BEGIN = 2,
    HOMING_STATE_SEED_LIMIT = 3,
    HOMING_STATE_OUT_LIMIT = 4,
    HOMING_STATE_X_FALT = 5,
    HOMING_STATE_Y_FALT = 6,
    HOMING_STATE_Z_FALT = 7,
    HOMING_STATE_X_SUCC = 8,
    HOMING_STATE_Y_SUCC = 9,
    HOMING_STATE_Z_SUCC = 10,
    HOMING_STATE_START = 11,
    HOMING_STATE_COMPLETE = 12,
    WARNING_MOVE_HOME_FIRST = 13,
};
typedef void (*homing_state_callback_t)(int state);
int homing_register_state_callback(homing_state_callback_t state_callback);
int homing_state_callback_call(int state);
#endif
