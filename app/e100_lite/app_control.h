#ifndef __APP_CONTROL_H__
#define __APP_CONTROL_H__
#include "app.h"

#define PRINTING_RAGE_SPEED 160
#define PRINTING_EXERCISE_SPEED 130
#define PRINTING_EQUILIBRIUM_SPEED 100
#define PRINTING_SILENCE_SPEED 50

enum
{
    RAGE_PRINT_SPEED = 0,    // 狂暴模式
    EXERCISE_PRINT_SPEED,    // 运动模式
    EQUILIBRIUM_PRINT_SPEED, // 均衡模式
    SILENCE_PRINT_SPEED,     // 静音模式
};

void app_control_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_fan_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_feed_callback(widget_t **widget_list, widget_t *widget, void *e);
void set_filament_out_in_printing_flag(bool flag);
void extrude_filament(int feed_type);
int get_feed_type(void);

typedef void (*feed_type_state_callback_t)(int state);
int feed_type_register_state_callback(feed_type_state_callback_t state_callback);
int feed_type_unregister_state_callback(feed_type_state_callback_t state_callback);
int feed_type_state_callback_call(int state);

#endif