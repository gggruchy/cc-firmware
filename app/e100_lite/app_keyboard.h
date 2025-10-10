#ifndef APP_KEYBOARD_H
#define APP_KEYBOARD_H
#include "app.h"
#include "keyboard.h"

/**
 * @brief 创建数字键盘
 *
 * @param parent
 * @param keyboard_type 1:带小数点  0：不带小数点
 * @return keyboard_t*
 */
keyboard_t *app_digital_keyboard_create(widget_t *parent, int keyboard_type);
keyboard_t *app_keyboard_create(widget_t *parent);
void app_keyboard_update(keyboard_t *keyboard);

#endif