#ifndef API_H
#define API_H
#include <queue>
#include <string>
#include "safe_queue.h"
#include "gcode.h"
// #include "debug.h"

#define MANUAL_COMMAND_MAX_LENGTH 2048

typedef void (*api_cb)(void *arg);
extern api_cb ui_cb[100];
extern SafeQueue<std::string> manual_control_sq;
extern SafeQueue<std::string> highest_priority_cmd_sq;
extern SafeQueue<std::string> fan_cmd_sq;
extern volatile bool print_gcode;
extern std::string print_file_path;

enum
{
    //-------print---------
    print_start_cb = 0,
    print_stop_cb,
    manual_control_cb,
    highest_priority_cmd_cb,
    set_print_speed_cb,
    get_print_speed_cb,
    get_print_actual_speed,
    get_print_size_cb,
    get_print_state_cb,
    get_alread_print_size_cb,
    get_print_pos_cb,
    get_alread_print_time_cb,
    get_total_layer_cb,
    get_current_layer_cb,
    command_queue_clear_cb,
    //-------heat--------
    extruder1_heat_cb,
    extruder2_heat_cb,
    bed_heat_cb,
    get_extruder1_curent_temp_cb,
    get_extruder2_curent_temp_cb,
    get_bed_curent_temp_cb,
    get_box_curent_temp_cb,
    get_extruder1_target_temp_cb,
    get_extruder2_target_temp_cb,
    get_bed_target_temp_cb,
    get_box_target_temp_cb,
    //--------fan--------
    printer_fan_cb,
    set_fan_speed_cb,
    fan0_cb,
    fan1_cb,
    fan2_cb,
    get_printer_fan_speed_cb,
    get_fan0_speed_cb,
    get_fan1_speed_cb,
    get_fan2_speed_cb,
    set_printer_fan_speed_cb,
    set_fan0_speed_cb,
    set_fan1_speed_cb,
    set_fan2_speed_cb,
    //--------info--------
    get_pos_info_cb,
    get_is_print_complete_cb,
    set_cur_Zslect_cb,

    check_resonance_test_cb,
    check_pid_calibrate_cb,
    check_flow_calibration_cb,
    set_resonance_test_cb,
    set_pid_calibrate_cb,
    set_flow_calibration_cb,

    get_stepper_x_state,
    get_stepper_y_state,
    get_stepper_z_state,
    get_stepper_e_state,

    get_start_record_time,
    set_start_record_time,

    get_print_time,

    get_pa_val_cb,
    save_pa_cb,
    get_line_length_cb,
    get_line_space_cb,
    set_line_length_cb,
    set_line_space_cb,
    write_ini_cb,

    get_z_offset_cb,
    set_z_offset_cb,
    save_z_offset_cb,

    position_calibration_cb,
    position_calibration_save_x_cb,
    position_calibration_save_y_cb,
    position_calibration_save_cb,
    position_calibration_read,

    get_printing_wait_cb,
    set_printing_wait_cb,

    stop_move_cb,
    stop_extrude_cb,

    get_target_spot_x,
    get_target_spot_y,
    reset_acceleration_cb,

    get_print_status, // 网络管理使用,获取打印状态信息进行上报
    set_silent_mode_cb,
    clear_print_stats_cb,
};

void register_api();

void api_get_resonance_freq_cb(void *arg1 = NULL, void *arg2 = NULL, void *arg3 = NULL, void *arg4 = NULL);
void api_set_resonance_freq_cb(void *arg1 = NULL, void *arg2 = NULL, void *arg3 = NULL, void *arg4 = NULL);
// void api_get_resonance_x_freq_cb(void *arg1 = NULL, void *arg2 = NULL);
// void api_get_resonance_y_freq_cb(void *arg1 = NULL, void *arg2 = NULL);
void api_get_pid_value_cb(void *arg1 = NULL, void *arg2 = NULL, void *arg3 = NULL);

void get_adxl_hal_exception_flag(void *arg1, void *arg2);
void set_adxl_hal_exception_flag(void *arg1, void *arg2);
void get_adxl_name(char **adxl_names);

#endif