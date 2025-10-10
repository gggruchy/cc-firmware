#include "app_api.h"
#include "hl_net_tool.h"
#include "hl_common.h"
#include "gpio.h"
#include "hl_disk.h"
#include "klippy.h"
#include "app.h"

#define LOG_TAG "ui_api"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#define NET_PRINT_CHECK_PERIOD 100

int app_api_get_temperature(double *cur_bed_temp, double *tar_bed_temp, double *cur_extruder_temp, double *tar_extruder_temp, double *cur_box_temp, double *tar_box_temp)
{
    srv_state_t *ss = app_get_srv_state();
    if (cur_bed_temp != NULL)
    {
        *cur_bed_temp = ss->heater_state[HEATER_ID_BED].current_temperature;
    }
    if (tar_bed_temp != NULL)
    {
        *tar_bed_temp = ss->heater_state[HEATER_ID_BED].target_temperature;
    }
    if (cur_extruder_temp != NULL)
    {
        *cur_extruder_temp = ss->heater_state[HEATER_ID_EXTRUDER].current_temperature;
    }
    if (tar_extruder_temp != NULL)
    {
        *tar_extruder_temp = ss->heater_state[HEATER_ID_EXTRUDER].target_temperature;
    }
    if (cur_box_temp != NULL)
    {
        *cur_box_temp = ss->heater_state[HEATER_ID_BOX].current_temperature;
    }
    return 0;
}

int app_api_get_printer_fan_speed(int *fan_speed)
{
    srv_state_t *ss = app_get_srv_state();
    if (fan_speed != NULL)
    {
        *fan_speed = ss->fan_state[FAN_ID_MODEL].value * 100.;
    }
    return 0;
}

int app_api_get_auxiliary_fan_speed(int *fan_speed)
{
    srv_state_t *ss = app_get_srv_state();
    if (fan_speed != NULL)
    {
        *fan_speed = ss->fan_state[FAN_ID_MODEL_HELPER].value * 100.;
    }
    return 0;
}

int app_api_get_box_fan_speed(int *fan_speed)
{
    srv_state_t *ss = app_get_srv_state();
    if (fan_speed != NULL)
    {
        *fan_speed = ss->fan_state[FAN_ID_BOX].value * 100.;
    }
    return 0;
}

// wait to do
int app_api_get_rgb_light_status(int *light_status)
{
    if (light_status != NULL)
    {
    }
    return 0;
}

int app_api_get_print_stats(print_stats_t *print_status)
{
    if (print_status != NULL)
    {
        ui_cb[get_print_status](print_status);
    }
    return 0;
}

int app_api_get_printspeed(double *print_speed)
{
    if (print_speed != NULL)
    {
        ui_cb[get_print_speed_cb](print_speed);
    }
    return 0;
}

int app_api_get_printer_coordinate(double *coord, char *str, int lenofstr)
{
    double *p = NULL;
    if (coord != NULL)
    {
        p = coord;
    }
    else
    {
        p = (double *)malloc(sizeof(double) * 3);
    }
    p[0] = Printer::GetInstance()->m_tool_head->m_commanded_pos[0];
    p[1] = Printer::GetInstance()->m_tool_head->m_commanded_pos[1];
    p[2] = Printer::GetInstance()->m_tool_head->m_commanded_pos[2];
    if (str != NULL)
    {
        snprintf(str, lenofstr, "%.2f,%.2f,%.2f", p[0], p[1], p[2]);
    }
    if (p)
    {
        free(p);
    }
    return 0;
}

int app_api_get_zoffset(double *zoffset)
{
    if (zoffset != NULL)
    {
        ui_cb[get_z_offset_cb](zoffset);
    }
    return 0;
}
