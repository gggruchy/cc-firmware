#ifndef SRV_CONTROL_H
#define SRV_CONTROL_H

typedef enum
{
    SRV_CONTROL_STEPPER_MOVE,
    SRV_CONTROL_STEPPER_HOME,
    SRV_CONTROL_STEPPER_DISABLE,
    SRV_CONTROL_FAN_SPEED,
    SRV_CONTROL_HEATER_TEMPERATURE,
} srv_control_srv_id_t;

typedef enum
{
    SRV_CONTROL_RET_OK,
    SRV_CONTROL_RET_ERROR,
} srv_control_ret_t;

typedef struct
{
    // distance in mm
    double x;
    double y;
    double z;
    double e;
    // speed in mm/min
    double f;
} srv_control_req_move_t;

typedef struct
{
    srv_control_ret_t ret;
} srv_control_res_move_t;

typedef struct
{
    int x;
    int y;
    int z;
} srv_control_req_home_t;

typedef struct
{
    srv_control_ret_t ret;
} srv_control_res_home_t;

typedef struct
{
    int heater_id;
    double temperature;
} srv_control_req_heater_t;

typedef struct
{
    int fan_id;
    double value; // 0.0-1.0
} srv_control_req_fan_t;

void srv_control_init(void);

#endif
