#ifndef __DEFINE_H__
#define __DEFINE_H__
#include "configfile.h"
// #include "Define_default.h"
#include "Define_reference.h"

// extern klipper_print_config klipper_printer_para;
// //mcu
// #define SERIAL klipper_printer_para.mcu.serial
// //printer
// #define KINEMATICS klipper_printer_para.printer.kinematics
// #define MAX_VELOCITY klipper_printer_para.printer.max_velocity
// #define MAX_ACCEL klipper_printer_para.printer.max_accel
// #define MAX_Z_VELOCITY klipper_printer_para.printer.max_z_velocity
// #define MAX_Z_ACCEL klipper_printer_para.printer.max_z_accel
// //heater bed
// #define HEATER_BED_NAME "bed"
// #define HEATER_BED_HEATER_PIN klipper_printer_para.heater_bed.heater_pin
// #define HEATER_BED_SENSOR_PIN klipper_printer_para.heater_bed.sensor_pin
// #define HEATER_BED_SENSOR_TYPE klipper_printer_para.heater_bed.sensor_type
// #define HEATER_BED_CONTROL klipper_printer_para.heater_bed.control
// #define HEATER_BED_MIN_TEMP klipper_printer_para.heater_bed.min_temp
// #define HEATER_BED_MAX_TEMP klipper_printer_para.heater_bed.max_temp
// //fan
// #define FAN_PIN klipper_printer_para.fan.pin
// #define FAN_SPEED 77.0
// //stepper X
// #define STEPPER_X_STEP_PIN klipper_printer_para.stepper_x.step_pin
// #define STEPPER_X_DIR_PIN klipper_printer_para.stepper_x.dir_pin
// #define STEPPER_X_ENABLE_PIN klipper_printer_para.stepper_x.enable_pin
// #define STEPPER_X_ENDSTOP_PIN klipper_printer_para.stepper_x.endstop_pin
// #define STEPPER_X_MICROSTEPS klipper_printer_para.stepper_x.microsteps
// #define STEPPER_X_GEAR_RATIO ""
// #define STEPPER_X_FULL_STEPS_PER_ROTATION klipper_printer_para.stepper_x.full_steps_per_rotation
// #define STEPPER_X_STEP_DISTANCE -1
// #define STEPPER_X_ROTATION_DISTANCE klipper_printer_para.stepper_x.rotation_distance
// #define STEPPER_X_POSITION_ENDSTOP klipper_printer_para.stepper_x.position_endstop
// #define STEPPER_X_POSITION_MIN klipper_printer_para.stepper_x.position_min
// #define STEPPER_X_POSITION_MAX klipper_printer_para.stepper_x.position_max
// #define STEPPER_X_HOMING_SPEED klipper_printer_para.stepper_x.homing_speed
// #define STEPPER_X_SECOND_HOMING_SPEED klipper_printer_para.stepper_x.second_homing_speed
// #define STEPPER_X_HOMING_RETRACT_DIST klipper_printer_para.stepper_x.homing_retract_dist
// #define STEPPER_X_HOMING_POSITIVE_DIR klipper_printer_para.stepper_x.homing_positive_dir
// //stepper Y
// #define STEPPER_Y_STEP_PIN klipper_printer_para.stepper_y.step_pin
// #define STEPPER_Y_DIR_PIN klipper_printer_para.stepper_y.dir_pin
// #define STEPPER_Y_ENABLE_PIN klipper_printer_para.stepper_y.enable_pin
// #define STEPPER_Y_ENDSTOP_PIN klipper_printer_para.stepper_y.endstop_pin
// #define STEPPER_Y_MICROSTEPS klipper_printer_para.stepper_y.microsteps
// #define STEPPER_Y_GEAR_RATIO ""
// #define STEPPER_Y_FULL_STEPS_PER_ROTATION klipper_printer_para.stepper_y.full_steps_per_rotation
// #define STEPPER_Y_STEP_DISTANCE -1
// #define STEPPER_Y_ROTATION_DISTANCE klipper_printer_para.stepper_y.rotation_distance
// #define STEPPER_Y_POSITION_ENDSTOP klipper_printer_para.stepper_y.position_endstop
// #define STEPPER_Y_POSITION_MIN klipper_printer_para.stepper_y.position_min
// #define STEPPER_Y_POSITION_MAX klipper_printer_para.stepper_y.position_max
// #define STEPPER_Y_HOMING_SPEED klipper_printer_para.stepper_y.homing_speed
// #define STEPPER_Y_SECOND_HOMING_SPEED klipper_printer_para.stepper_y.second_homing_speed
// #define STEPPER_Y_HOMING_RETRACT_DIST klipper_printer_para.stepper_y.homing_retract_dist
// #define STEPPER_Y_HOMING_POSITIVE_DIR klipper_printer_para.stepper_y.homing_positive_dir
// //stepper Z
// #define STEPPER_Z_STEP_PIN klipper_printer_para.stepper_z.step_pin
// #define STEPPER_Z_DIR_PIN klipper_printer_para.stepper_z.dir_pin
// #define STEPPER_Z_ENABLE_PIN klipper_printer_para.stepper_z.enable_pin
// #define STEPPER_Z_ENDSTOP_PIN klipper_printer_para.stepper_z.endstop_pin
// #define STEPPER_Z_MICROSTEPS klipper_printer_para.stepper_z.microsteps
// #define STEPPER_Z_GEAR_RATIO klipper_printer_para.stepper_z.gear_ratio
// #define STEPPER_Z_FULL_STEPS_PER_ROTATION klipper_printer_para.stepper_z.full_steps_per_rotation
// #define STEPPER_Z_STEP_DISTANCE -1
// #define STEPPER_Z_ROTATION_DISTANCE klipper_printer_para.stepper_z.rotation_distance
// #define STEPPER_Z_POSITION_ENDSTOP klipper_printer_para.stepper_z.position_endstop
// #define STEPPER_Z_POSITION_MIN klipper_printer_para.stepper_z.position_min
// #define STEPPER_Z_POSITION_MAX klipper_printer_para.stepper_z.position_max
// #define STEPPER_Z_HOMING_SPEED klipper_printer_para.stepper_z.homing_speed
// #define STEPPER_Z_SECOND_HOMING_SPEED klipper_printer_para.stepper_z.second_homing_speed
// #define STEPPER_Z_HOMING_RETRACT_DIST klipper_printer_para.stepper_z.homing_retract_dist
// #define STEPPER_Z_HOMING_POSITIVE_DIR klipper_printer_para.stepper_z.homing_positive_dir

// //extruder
// #define HEATER_EXTRUDER_NAME klipper_printer_para.extruder.axis_name
// #define EXTRUDER_STEP_PIN klipper_printer_para.extruder.step_pin
// #define EXTRUDER_DIR_PIN klipper_printer_para.extruder.dir_pin
// #define EXTRUDER_ENABLE_PIN klipper_printer_para.extruder.enable_pin
// #define EXTRUDER_HEATER_PIN klipper_printer_para.extruder.heater_pin
// #define EXTRUDER_SENSOR_PIN klipper_printer_para.extruder.sensor_pin
// #define EXTRUDER_MICROSTEPS klipper_printer_para.extruder.microsteps
// #define EXTRUDER_GEAR_RATIO klipper_printer_para.extruder.gear_ratio
// #define EXTRUDER_FULL_STEPS_PER_ROTATION klipper_printer_para.extruder.full_steps_per_rotation
// #define EXTRUDER_STEP_DISTANCE -1
// #define EXTRUDER_ROTATION_DISTANCE klipper_printer_para.extruder.rotation_distance
// #define EXTRUDER_NOZZLE_DIAMETER klipper_printer_para.extruder.nozzle_diameter
// #define EXTRUDER_FILAMENT_DIAMETER klipper_printer_para.extruder.filament_diameter
// #define EXTRUDER_SENSOR_TYPE klipper_printer_para.extruder.sensor_type
// #define EXTRUDER_CONTROL klipper_printer_para.extruder.control
// #define EXTRUDER_PID_KP klipper_printer_para.extruder.pid_Kp
// #define EXTRUDER_PID_KI klipper_printer_para.extruder.pid_Ki
// #define EXTRUDER_PID_KD klipper_printer_para.extruder.pid_Kd
// #define EXTRUDER_MIN_TEMP klipper_printer_para.extruder.min_temp
// #define EXTRUDER_MAX_TEMP klipper_printer_para.extruder.max_temp
// #define NOZZLE_DIAMTER 0.4
// #define FILAMENT_DIAMETER 1.75
// #define MAX_CROSS_SECTION 0.64
// #define MAX_E_DIST 50.0
// #define INSTANT_CORNER_V 1.0

// #define TMC2209_STEPPER_X_UART_PIN klipper_printer_para.tmc2209_stepper_x.uart_pin
// #define TMC2209_STEPPER_X_DIAG_PIN klipper_printer_para.tmc2209_stepper_x.diag_pin
// #define TMC2209_STEPPER_X_RUN_CURRENT klipper_printer_para.tmc2209_stepper_x.run_current
// #define TMC2209_STEPPER_X_HOLD_CURRENT klipper_printer_para.tmc2209_stepper_x.hold_current
// #define TMC2209_STEPPER_X_STEALTHCHOP_THRESHOLD klipper_printer_para.tmc2209_stepper_x.stealthchop_threshold

// #define TMC2209_STEPPER_Y_UART_PIN klipper_printer_para.tmc2209_stepper_y.uart_pin
// #define TMC2209_STEPPER_Y_DIAG_PIN klipper_printer_para.tmc2209_stepper_y.diag_pin
// #define TMC2209_STEPPER_Y_RUN_CURRENT klipper_printer_para.tmc2209_stepper_y.run_current
// #define TMC2209_STEPPER_Y_HOLD_CURRENT klipper_printer_para.tmc2209_stepper_y.hold_current
// #define TMC2209_STEPPER_Y_STEALTHCHOP_THRESHOLD klipper_printer_para.tmc2209_stepper_y.stealthchop_threshold

// #define TMC2209_STEPPER_Z_UART_PIN klipper_printer_para.tmc2209_stepper_z.uart_pin
// #define TMC2209_STEPPER_Z_DIAG_PIN klipper_printer_para.tmc2209_stepper_z.diag_pin
// #define TMC2209_STEPPER_Z_RUN_CURRENT klipper_printer_para.tmc2209_stepper_z.run_current
// #define TMC2209_STEPPER_Z_HOLD_CURRENT klipper_printer_para.tmc2209_stepper_z.hold_current
// #define TMC2209_STEPPER_Z_STEALTHCHOP_THRESHOLD klipper_printer_para.tmc2209_stepper_z.stealthchop_threshold

// #define TMC2209_STEPPER_EXTRUDER_UART_PIN klipper_printer_para.tmc2209_stepper_extruder.uart_pin
// #define TMC2209_STEPPER_EXTRUDER_DIAG_PIN klipper_printer_para.tmc2209_stepper_extruder.diag_pin
// #define TMC2209_STEPPER_EXTRUDER_RUN_CURRENT klipper_printer_para.tmc2209_stepper_extruder.run_current
// #define TMC2209_STEPPER_EXTRUDER_HOLD_CURRENT klipper_printer_para.tmc2209_stepper_extruder.hold_current
// #define TMC2209_STEPPER_EXTRUDER_STEALTHCHOP_THRESHOLD klipper_printer_para.tmc2209_stepper_extruder.stealthchop_threshold

// #define TMC2260_STEPPER_X_CS_PIN klipper_printer_para.tmc2660_stepper_x.cs_pin
// #define TMC2260_STEPPER_X_SPI_BUS klipper_printer_para.tmc2660_stepper_x.spi_bus
// #define TMC2260_STEPPER_X_INTERPOLATE klipper_printer_para.tmc2660_stepper_x.interpolate
// #define TMC2260_STEPPER_X_RUN_CURRENT klipper_printer_para.tmc2660_stepper_x.run_current
// #define TMC2260_STEPPER_X_SENSE_RESISTOR klipper_printer_para.tmc2660_stepper_x.sense_resistor
// #define TMC2260_STEPPER_X_IDLE_CURRENT_PERCENT klipper_printer_para.tmc2660_stepper_x.idle_current_percent

// #define TMC2260_STEPPER_Y_CS_PIN klipper_printer_para.tmc2660_stepper_y.cs_pin
// #define TMC2260_STEPPER_Y_SPI_BUS klipper_printer_para.tmc2660_stepper_y.spi_bus
// #define TMC2260_STEPPER_Y_INTERPOLATE klipper_printer_para.tmc2660_stepper_y.interpolate
// #define TMC2260_STEPPER_Y_RUN_CURRENT klipper_printer_para.tmc2660_stepper_y.run_current
// #define TMC2260_STEPPER_Y_SENSE_RESISTOR klipper_printer_para.tmc2660_stepper_y.sense_resistor
// #define TMC2260_STEPPER_Y_IDLE_CURRENT_PERCENT klipper_printer_para.tmc2660_stepper_y.idle_current_percent

// #define TMC2260_STEPPER_Z_CS_PIN klipper_printer_para.tmc2660_stepper_z.cs_pin
// #define TMC2260_STEPPER_Z_SPI_BUS klipper_printer_para.tmc2660_stepper_z.spi_bus
// #define TMC2260_STEPPER_Z_INTERPOLATE klipper_printer_para.tmc2660_stepper_z.interpolate
// #define TMC2260_STEPPER_Z_RUN_CURRENT klipper_printer_para.tmc2660_stepper_z.run_current
// #define TMC2260_STEPPER_Z_SENSE_RESISTOR klipper_printer_para.tmc2660_stepper_z.sense_resistor
// #define TMC2260_STEPPER_Z_IDLE_CURRENT_PERCENT klipper_printer_para.tmc2660_stepper_z.idle_current_percent

// #define TMC2260_STEPPER_EXTRUDER_CS_PIN klipper_printer_para.tmc2660_stepper_extruder.cs_pin
// #define TMC2260_STEPPER_EXTRUDER_SPI_BUS klipper_printer_para.tmc2660_stepper_extruder.spi_bus
// #define TMC2260_STEPPER_EXTRUDER_INTERPOLATE klipper_printer_para.tmc2660_stepper_extruder.interpolate
// #define TMC2260_STEPPER_EXTRUDER_RUN_CURRENT klipper_printer_para.tmc2660_stepper_extruder.run_current
// #define TMC2260_STEPPER_EXTRUDER_SENSE_RESISTOR klipper_printer_para.tmc2660_stepper_extruder.sense_resistor

// adc
#define ADC_MAX 4095
#define SAMPLE_TIME 8
#define MAX_STEPPER_ERROR 0.000025

// adc_temp
#define ADC_TEMP_SAMPLE_TIME 0.001
#define ADC_TEMP_SAMPLE_COUNT 8
#define ADC_TEMP_REPORT_TIME 0.500
#define ADC_TEMP_RANGE_CHECK_COUNT 4
#define KELVIN_TO_CELSIUS -273.15
#define MAX_HEAT_TIME 5.0
#define AMBIENT_TEMP 25.0
#define PID_PARAM_BASE 255.0

#define NEVER 10e16

// endstop
#define REASON_ENDSTOP_HIT 1
#define REASON_COMMS_TIMEOUT 2
#define REASON_HOST_REQUEST 3
#define REASON_PAST_END_TIME 4
#define HOMING_START_DELAY 0.001
#define ENDSTOP_SAMPLE_TIME 0.000015
#define ENDSTOP_SAMPLE_COUNT 4

// gcode
#define GCODE_COMMAND_QUEUE_SIZE 1024
#define DO_NOT_MOVE 0x7fffffff
#define DO_NOT_MOVE_F 10000000.0 // 10000m

// clock
#define CLOCK 0
#define SEND_TIME 0.0f
#define RECEIVE_TIME 0.0f
#define RTT_AGE 0.0000010f / (60.0f * 60.0f)
#define DECAY 1.0f / 100.0f
#define TRANSMIT_EXTRA 0.001f
#define CLOCK_FREQ 200000000.0f //---PARA-G-G-2022---180000000

// MoveQueue
#define LOOKAHEAD_FLUSH_TIME 0.250 //----G-G-2022-08-11----

// ToolHead
#define MIN_KIN_TIME 0.100
#define MOVE_BATCH_TIME 0.500   // 至少0.5S给MCU发一个MOVE包
#define DRIP_SEGMENT_TIME 0.050 // 归0的时候至少0.05S给MCU发一个MOVE包
#define DRIP_TIME 0.100
#define SDS_CHECK_TIME 0.001 // 至少留下1mS数据等下个命令来的时候发送，实测会被修改为3ms  step+dir+step filter in stepcompress.c

#define MANUAL_CONFIG 1

#endif