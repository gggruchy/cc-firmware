#include "app.h"
#include "keyboard.h"

#define LOG_TAG "app_keyboard"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

enum
{
    KEYBOARD_MODE_SYMBOL,    //符号键盘
    KEYBOARD_MODE_DIGITAL,   //数字键盘
    KEYBOARD_MODE_LOWERCASE, //小写键盘
    KEYBOARD_MODE_UPPERCASE, //大写键盘
};

//键盘字符输入长度限定
#define DIGITAL_KEYBOARD_STRLEN_MAX 64
#define KEYBOARD_STRLEN_MAX 64

static void app_digital_keyboard_callback(widget_t **widget_list, widget_t *widget, void *e);
static void app_keyboard_callback(widget_t **widget_list, widget_t *widget, void *e);
static void digital_keyboard_update_symbol(widget_t **widget_list);
static bool digital_keyboard_handle_click(keyboard_t *keyboard, widget_t *widget);
static void keyboard_update_symbol(widget_t** widget_list, int mode, keyboard_t* keyboard);
static bool keyboard_handle_click(keyboard_t *keyboard, widget_t *widget, int *mode);

static int digital_keyboard_type; // 1:带小数点  0：不带小数点

keyboard_t *app_digital_keyboard_create(widget_t *parent, int keyboard_type)
{
    keyboard_t *keyboard = NULL;
    window_t *win = NULL;
    digital_keyboard_type = keyboard_type;
    if ((keyboard = keyboard_create()) == NULL)
    {
        LOG_E("keyboard_create failed");
        return NULL;
    }
    if ((win = window_copy(WINDOW_ID_DIGITAL_KEYBOARD, app_digital_keyboard_callback, parent->obj_container[0], keyboard)) == NULL)
    {
        LOG_E("window_copy failed");
        return NULL;
    }
    keyboard->user_data = win;
    return keyboard;
}

keyboard_t *app_keyboard_create(widget_t *parent)
{
    keyboard_t *keyboard = NULL;
    window_t *win = NULL;
    if ((keyboard = keyboard_create()) == NULL)
    {
        LOG_E("keyboard_create failed");
        return NULL;
    }
    if ((win = window_copy(WINDOW_ID_KEYBOARD, app_keyboard_callback, parent->obj_container[0], keyboard)) == NULL)
    {
        LOG_E("window_copy failed");
        return NULL;
    }
    keyboard->user_data = win;
    LOG_I("[%s] keyboard addr:0x%p", __FUNCTION__, keyboard);

    return keyboard;
}

void app_digital_keyboard_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    keyboard_t *keyboard = NULL;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        digital_keyboard_update_symbol(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED_REPEAT:
    case LV_EVENT_CLICKED:
        if (widget_list[0] && widget_list[0]->win && widget_list[0]->win->user_data)
            keyboard = (keyboard_t *)widget_list[0]->win->user_data;
        if (keyboard)
        {
            bool exit = digital_keyboard_handle_click(keyboard, widget);
            if (exit == true)
            {
                lv_event_send(widget_list[0]->obj_container[0]->parent, (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
                keyboard_destroy(keyboard);
                window_copy_destory(widget_list[0]->win);
            }
            else
            {
                lv_event_send(widget_list[0]->obj_container[0]->parent, (lv_event_code_t)LV_EVENT_CHILD_VALUE_CHANGE, NULL);
                if (keyboard->text != NULL)
                {
                }
            }
        }

        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

void app_keyboard_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    keyboard_t *keyboard = NULL;
    static int current_keyboard_mode;
    int keyboard_mode;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        current_keyboard_mode = KEYBOARD_MODE_LOWERCASE;
        keyboard_update_symbol(widget_list, current_keyboard_mode, NULL);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED_REPEAT:
    case LV_EVENT_CLICKED:
        //获取键盘
        if (widget_list[0] && widget_list[0]->win && widget_list[0]->win->user_data)
            keyboard = (keyboard_t *)widget_list[0]->win->user_data;
        if (keyboard)
        {
            keyboard_mode = current_keyboard_mode;

            bool exit = keyboard_handle_click(keyboard, widget, &keyboard_mode);

            if (exit == true)
            {
                lv_event_send(widget_list[0]->obj_container[0]->parent, (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
                keyboard_destroy(keyboard);
                window_copy_destory(widget_list[0]->win);
                ui_update_window_immediately();
            }
            else
            {
                lv_event_send(widget_list[0]->obj_container[0]->parent, (lv_event_code_t)LV_EVENT_CHILD_VALUE_CHANGE, NULL);
                //模式更改则刷新键盘字符
                if (keyboard_mode != current_keyboard_mode)
                {
                    current_keyboard_mode = keyboard_mode;
                    keyboard_update_symbol(widget_list, current_keyboard_mode, keyboard);
                }
            }
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

void app_keyboard_update(keyboard_t *keyboard)
{
    window_t *win = (window_t *)keyboard->user_data;
    widget_t **widget_list = win->widget_list;
    if (keyboard->text != NULL)
    {
        if (strlen(keyboard->text) >= 8)
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[0], lv_color_hex(0xFF474747), 0);
            lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[1],ui_get_image_src(211));
            lv_obj_add_flag(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[0], lv_color_hex(0xFF2D2D2D), 0);
            lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[1],ui_get_image_src(245));
            lv_obj_clear_flag(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void digital_keyboard_update_symbol(widget_t **widget_list)
{
    // if (digital_keyboard_type == 1)
    //     lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_DOT]->obj_container[2], ".");
    // else
    //     lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_DOT]->obj_container[2], "00");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_0]->obj_container[2], "0");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_1]->obj_container[2], "1");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_2]->obj_container[2], "2");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_3]->obj_container[2], "3");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_4]->obj_container[2], "4");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_5]->obj_container[2], "5");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_6]->obj_container[2], "6");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_7]->obj_container[2], "7");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_8]->obj_container[2], "8");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_9]->obj_container[2], "9");
    lv_label_set_text(widget_list[WIDGET_ID_DIGITAL_KEYBOARD_BTN_ENTER]->obj_container[2], "OK");
}

static bool digital_keyboard_handle_click(keyboard_t *keyboard, widget_t *widget)
{
    uint16_t index;
    const char *put = "";
    if (keyboard == NULL)
        return false;
    index = widget->header.index;
    switch (index)
    {
    // case WIDGET_ID_DIGITAL_KEYBOARD_BTN_DOT:
    //     if (digital_keyboard_type == 1)
    //         put = ".";
    //     else
    //         put = "00";
    //     break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_0:
        put = "0";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_1:
        put = "1";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_2:
        put = "2";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_3:
        put = "3";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_4:
        put = "4";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_5:
        put = "5";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_6:
        put = "6";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_7:
        put = "7";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_8:
        put = "8";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_9:
        put = "9";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_DELETE:
        put = "\x7f";
        break;
    case WIDGET_ID_DIGITAL_KEYBOARD_BTN_ENTER:
        return true;
        break;
    }
    for (int i = 0; i < strlen(put); i++)
        if ((keyboard->text != NULL && strlen(keyboard->text) < DIGITAL_KEYBOARD_STRLEN_MAX) || put[i] == '\x7f')
            keyboard_commit_character(keyboard, put[i]);
    return false;
}

static void keyboard_update_symbol(widget_t** widget_list, int mode, keyboard_t* keyboard)
{
    switch (mode)
    {
    case KEYBOARD_MODE_SYMBOL:
        lv_obj_add_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_ALPHABET]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_SYMBOL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_00]->obj_container[2], "[");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_01]->obj_container[2], "]");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_02]->obj_container[2], "{");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_03]->obj_container[2], "}");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_04]->obj_container[2], "#");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_05]->obj_container[2], "%");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_06]->obj_container[2], "^");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_07]->obj_container[2], "*");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_08]->obj_container[2], "+");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_09]->obj_container[2], "=");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_10]->obj_container[2], "_");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_11]->obj_container[2], "-");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_12]->obj_container[2], "\\");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_13]->obj_container[2], "|");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_14]->obj_container[2], "(");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_15]->obj_container[2], ")");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_16]->obj_container[2], "\"");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_17]->obj_container[2], "@");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_18]->obj_container[2], "&");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_19]->obj_container[2], "~");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_20]->obj_container[2], "123");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_21]->obj_container[2], "--");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_22]->obj_container[2], ",");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_23]->obj_container[2], "...");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_24]->obj_container[2], "?");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_25]->obj_container[2], "!");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_26]->obj_container[2], "'");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_27]->obj_container[2], "");
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_27]->obj_container[1], ui_get_image_src(210));
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_MODE]->obj_container[2], "abc");
        break;
    case KEYBOARD_MODE_DIGITAL:
        lv_obj_add_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_ALPHABET]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_SYMBOL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_00]->obj_container[2], "1");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_01]->obj_container[2], "2");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_02]->obj_container[2], "3");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_03]->obj_container[2], "4");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_04]->obj_container[2], "5");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_05]->obj_container[2], "6");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_06]->obj_container[2], "7");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_07]->obj_container[2], "8");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_08]->obj_container[2], "9");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_09]->obj_container[2], "0");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_10]->obj_container[2], "-");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_11]->obj_container[2], "/");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_12]->obj_container[2], ":");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_13]->obj_container[2], ";");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_14]->obj_container[2], "(");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_15]->obj_container[2], ")");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_16]->obj_container[2], "$");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_17]->obj_container[2], "@");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_18]->obj_container[2], "<");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_19]->obj_container[2], ">");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_20]->obj_container[2], "#+=");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_21]->obj_container[2], ".");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_22]->obj_container[2], ",");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_23]->obj_container[2], "'");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_24]->obj_container[2], "?");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_25]->obj_container[2], "!");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_26]->obj_container[2], ".");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_27]->obj_container[2], "");
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_SYMBOL_27]->obj_container[1], ui_get_image_src(210));

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_MODE]->obj_container[2], "abc");
        break;
    case KEYBOARD_MODE_LOWERCASE:
        lv_obj_clear_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_ALPHABET]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_SYMBOL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_00]->obj_container[2], "q");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_01]->obj_container[2], "w");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_02]->obj_container[2], "e");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_03]->obj_container[2], "r");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_04]->obj_container[2], "t");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_05]->obj_container[2], "y");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_06]->obj_container[2], "u");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_07]->obj_container[2], "i");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_08]->obj_container[2], "o");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_09]->obj_container[2], "p");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_10]->obj_container[2], "a");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_11]->obj_container[2], "s");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_12]->obj_container[2], "d");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_13]->obj_container[2], "f");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_14]->obj_container[2], "g");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_15]->obj_container[2], "h");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_16]->obj_container[2], "j");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_17]->obj_container[2], "k");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_18]->obj_container[2], "l");

        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_20]->obj_container[1], ui_get_image_src(209));
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_20]->obj_container[2], "");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_21]->obj_container[2], "z");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_22]->obj_container[2], "x");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_23]->obj_container[2], "c");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_24]->obj_container[2], "v");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_25]->obj_container[2], "b");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_26]->obj_container[2], "n");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_27]->obj_container[2], "m");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_28]->obj_container[2], "");
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_28]->obj_container[1], ui_get_image_src(210));
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_MODE]->obj_container[2], "123");

        break;
    case KEYBOARD_MODE_UPPERCASE:
        lv_obj_clear_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_ALPHABET]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_KEYBOARD_CONTAINER_SYMBOL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_00]->obj_container[2], "Q");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_01]->obj_container[2], "W");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_02]->obj_container[2], "E");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_03]->obj_container[2], "R");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_04]->obj_container[2], "T");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_05]->obj_container[2], "Y");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_06]->obj_container[2], "U");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_07]->obj_container[2], "I");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_08]->obj_container[2], "O");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_09]->obj_container[2], "P");

        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_10]->obj_container[2], "A");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_11]->obj_container[2], "S");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_12]->obj_container[2], "D");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_13]->obj_container[2], "F");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_14]->obj_container[2], "G");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_15]->obj_container[2], "H");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_16]->obj_container[2], "J");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_17]->obj_container[2], "K");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_18]->obj_container[2], "L");

        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_20]->obj_container[1], ui_get_image_src(208));
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_20]->obj_container[2], "");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_21]->obj_container[2], "Z");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_22]->obj_container[2], "X");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_23]->obj_container[2], "C");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_24]->obj_container[2], "V");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_25]->obj_container[2], "B");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_26]->obj_container[2], "N");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_27]->obj_container[2], "M");
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_28]->obj_container[2], "");
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ALPHABET_28]->obj_container[1], ui_get_image_src(210));
        lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_MODE]->obj_container[2], "123");
        break;
    }

    lv_label_set_text(widget_list[WIDGET_ID_KEYBOARD_BTN_BLANK]->obj_container[2], "space");
    if (keyboard != NULL && strlen(keyboard->text) >= 8)
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[1], ui_get_image_src(211));
    else
        lv_img_set_src(widget_list[WIDGET_ID_KEYBOARD_BTN_ENTER]->obj_container[1], ui_get_image_src(245));
}

static bool keyboard_handle_click(keyboard_t *keyboard, widget_t *widget, int *mode)
{
    uint16_t index;
    const char *put = "";
    if (keyboard == NULL)
        return false;
    index = widget->header.index;

    switch (*mode)
    {
    case KEYBOARD_MODE_LOWERCASE: //小写键盘
        switch (index)
        {
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_00:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_01:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_02:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_03:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_04:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_05:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_06:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_07:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_08:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_09:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_10:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_11:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_12:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_13:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_14:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_15:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_16:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_17:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_18:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_21:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_22:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_23:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_24:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_25:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_26:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_27:
            put = lv_label_get_text(widget->obj_container[2]);
            break;
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_20: // SHIFT
            *mode = KEYBOARD_MODE_UPPERCASE;
            break;
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_28: //'Del'
            put = "\x7f";
            break;
        }
        break;
    case KEYBOARD_MODE_UPPERCASE: //大写键盘
        switch (index)
        {
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_00:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_01:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_02:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_03:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_04:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_05:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_06:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_07:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_08:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_09:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_10:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_11:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_12:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_13:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_14:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_15:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_16:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_17:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_18:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_21:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_22:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_23:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_24:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_25:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_26:
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_27:
            put = lv_label_get_text(widget->obj_container[2]);
            break;
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_20: // SHIFT
            *mode = KEYBOARD_MODE_LOWERCASE;
            break;
        case WIDGET_ID_KEYBOARD_BTN_ALPHABET_28: //'Del'
            put = "\x7f";
            break;
        }
        break;
    case KEYBOARD_MODE_SYMBOL:
        switch (index)
        {
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_00:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_01:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_02:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_03:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_04:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_05:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_06:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_07:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_08:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_09:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_10:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_11:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_12:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_13:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_14:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_15:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_16:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_17:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_18:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_19:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_21:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_22:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_23:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_24:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_25:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_26:
            put = lv_label_get_text(widget->obj_container[2]);
            break;
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_20: //'123'
            *mode = KEYBOARD_MODE_DIGITAL;
            break;
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_27: //'Del'
            put = "\x7f";
            break;
        }
        break;
    case KEYBOARD_MODE_DIGITAL:
        switch (index)
        {
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_00:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_01:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_02:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_03:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_04:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_05:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_06:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_07:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_08:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_09:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_10:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_11:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_12:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_13:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_14:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_15:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_16:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_17:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_18:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_19:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_21:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_22:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_23:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_24:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_25:
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_26:
            put = lv_label_get_text(widget->obj_container[2]);
            break;
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_20: //'abc'
            *mode = KEYBOARD_MODE_SYMBOL;
            break;
        case WIDGET_ID_KEYBOARD_BTN_SYMBOL_27: //'Del'
            put = "\x7f";
            break;
        }
        break;
    }

    switch (index)
    {
    case WIDGET_ID_KEYBOARD_BTN_MODE:
        if (*mode == KEYBOARD_MODE_LOWERCASE || *mode == KEYBOARD_MODE_UPPERCASE)
            *mode = KEYBOARD_MODE_DIGITAL;
        else
            *mode = KEYBOARD_MODE_LOWERCASE;
        break;
    case WIDGET_ID_KEYBOARD_BTN_BLANK:
        put = " ";
        break;
    case WIDGET_ID_KEYBOARD_BTN_ENTER:
        return true;
        break;
    }

    if (strcmp(put, ""))
    {
        for (int i = 0; i < strlen(put); i++)
            if ((keyboard->text != NULL && strlen(keyboard->text) < KEYBOARD_STRLEN_MAX) || put[i] == '\x7f')
                keyboard_commit_character(keyboard, put[i]);
    }
    return false;
}
