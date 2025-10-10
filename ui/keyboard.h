#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef struct keyboard_tag keyboard_t;
typedef bool (*keyboard_accept_hook_t)(keyboard_t *keyboard, char c);
typedef void (*keyboard_edited_callback_t)(keyboard_t *keyboard, const char *text);
typedef void (*keyboard_exit_callback_t)(keyboard_t *keyboard, const char *text);

#define KEYBOARD_HOOK_MAX_SIZE 16
#define KEYBOARD_CALLBACK_MAX_SIZE 16
struct keyboard_tag
{
    char *text;                                                             //字符缓存区
    int size;                                                               //缓存区长度
    int current_input_index;                                                //当前字符串的插入位置
    keyboard_accept_hook_t accept_hook[KEYBOARD_HOOK_MAX_SIZE];             //接受钩子函数，提交字符后触发该函数由判断是否要将该字符push进入缓存区
    keyboard_edited_callback_t edited_callback[KEYBOARD_CALLBACK_MAX_SIZE]; //编辑回调函数,当编辑缓存区后触发
    keyboard_exit_callback_t exit_callback[KEYBOARD_CALLBACK_MAX_SIZE];     //编辑回调函数,当编辑缓存区后触发
    void *user_data;
    void *user_callback;
};

/**
 * @brief 创建键盘
 *
 * @param fmt 初始化字段
 * @param ...
 * @return keyboard_t*
 */
keyboard_t *keyboard_create();

/**
 * @brief 销毁键盘
 *
 * @param keyboard
 */
void keyboard_destroy(keyboard_t *keyboard);

/**
 * @brief 设置键盘字符串缓存
 *
 * @param keyboard
 * @param fmt
 * @param ...
 */
bool keyboard_set_text(keyboard_t *keyboard, char *fmt, ...);

/**
 * @brief 返回键盘字符串缓存，失败返回空字符""
 *
 * @param keyboard
 * @return const char*
 */
const char *keyboard_get_text(keyboard_t *keyboard);

/**
 * @brief 提交键盘输入字符
 *
 * @param keyboard
 * @param c 输入字符
 */
void keyboard_commit_character(keyboard_t *keyboard, char c);

void keyboard_register_accept_hook(keyboard_t *keyboard, keyboard_accept_hook_t accept_hook);
void keyboard_unregister_accept_hook(keyboard_t *keyboard, keyboard_accept_hook_t accept_hook);
void keyboard_register_edited_callback(keyboard_t *keyboard, keyboard_edited_callback_t edited_callback);
void keyboard_unregister_edited_callback(keyboard_t *keyboard, keyboard_edited_callback_t edited_callback);
void keyboard_register_exit_callback(keyboard_t *keyboard, keyboard_exit_callback_t edited_callback);
void keyboard_unregister_exit_callback(keyboard_t *keyboard, keyboard_exit_callback_t edited_callback);

#endif