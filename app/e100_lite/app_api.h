#ifndef APP_API_H
#define APP_API_H
#include "print_stats_c.h"
typedef enum
{
    UI_EVENT_ID_API_PRINT_START,
} ui_api_event_id_t;
typedef struct
{
    ui_api_event_id_t id;
    void *data;
} ui_api_event_t;
void ui_api_init(void);
int ui_api_event_post(ui_api_event_id_t id, void *data);
void ui_api_update(void);
int app_api_get_temperature(double *cur_bed_temp, double *tar_bed_temp, double *cur_extruder_temp, double *tar_extruder_temp, double *cur_box_temp, double *tar_box_temp);

int app_api_get_printer_fan_speed(int *fan_speed);
int app_api_get_auxiliary_fan_speed(int *fan_speed);
int app_api_get_box_fan_speed(int *fan_speed);

int app_api_get_rgb_light_status(int *light_status);

int app_api_get_printer_coordinate(double *coord, char *str, int lenofstr);
int app_api_get_zoffset(double *zoffset);

int app_api_get_print_stats(print_stats_t *print_status);
int app_api_get_printspeed(double *print_speed);

#define DOWNLOAD_FILE_TMP_PATH_TAIL ".cbdtmp"
#endif