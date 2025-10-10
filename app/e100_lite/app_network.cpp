#include "app_network.h"
#include "hl_net.h"
#include "hl_wlan.h"
#include "hl_queue.h"
#include "hl_net.h"
#include "configfile.h"
#include "Define_config_path.h"
#define LOG_TAG "app_network"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define ITEM_NUMS 50
typedef enum
{
    // 设备相关
    UI_EVENT_ID_WLAN_REMOVE,
    UI_EVENT_ID_WLAN_APPEND,
    // 连接相关
    UI_EVENT_ID_WLAN_CONNECT_DISCONNECT,
    UI_EVENT_ID_WLAN_CONNECT_SUCCEEDED,
    UI_EVENT_ID_WLAN_CONNECT_FAILED,
    UI_EVENT_ID_WLAN_CONNECT_TIMEOUT, // 5
    UI_EVENT_ID_WLAN_CONNECT_CONNECT_FAILED,
    UI_EVENT_ID_WLAN_CONNECT_NOT_FOUND,
    UI_EVENT_ID_WLAN_WRONG_PASSWORD,
    // 扫描相关
    UI_EVENT_ID_WLAN_SCAN_SUCCEEDED,
    UI_EVENT_ID_WLAN_SCAN_FAILED,
} ui_event_id_t;

enum
{
    MSGBOX_TIP_CONNECT_FAIL = 0, 
    MSGBOX_TIP_PASSWD_ERROR,
    MSGBOX_TIP_SCAN_SUCCEEDED,
};
typedef struct
{
    ui_event_id_t id;
} ui_event_t;

typedef enum
{
    UI_WLAN_STATUS_DISCONNECTED,
    UI_WLAN_STATUS_CONNECTING,
    UI_WLAN_STATUS_CONNECTED,
} ui_wlan_status_t;

struct wifi_rotation_t
{
    int is_rotating;
    int index;
    hl_wlan_connection_t connection;
};

struct wifi_status_t
{
    struct wifi_rotation_t *wr;
    int is_connecting;
};

static struct wifi_rotation_t wifi_rotation = {0}; /* the index of wifi which choose by user */
static struct wifi_status_t wifi_status = {0};
static app_listitem_model_t *network_model = NULL;
static keyboard_t *keyboard = NULL;
static hl_queue_t ui_event_queue = NULL; // UI事件队列
static char wlan_psk[HL_WLAN_PSK_MAXLENGTH + 1];
static bool switch_state = false;
static bool wifi_password_state = false;
static bool forget_password_state = false;
static bool wifi_password_hidden = false;
static int wlan_scan_result_num = 0;
static bool wlan_scan_state = false;
static hl_wlan_connection_t wlan_scan_result[HL_WLAN_SCAN_RESULT_BUFFER_SIZE] = {0};
static hl_wlan_connection_t wlan_selete = {0}; // 选择的WIFI入口

static void network_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void network_listitem_update(void);
static void app_network_update(widget_t **widget_list);
extern ConfigParser *get_sysconf();
static bool app_network_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static int wifi_rotate_start(wifi_rotation_t *w);
static int wifi_rotate_stop(wifi_rotation_t *w);
static int start_wifi_connect(const char *ssid, const char *psk, hl_wlan_key_mgmt_t key_mgmt);
static int complete_wifi_connecting();

static void wlan_callback(const void *data, void *user_data)
{
    const hl_wlan_event_t *event = (const hl_wlan_event_t *)data;
    ui_event_t ui_event;
    LOG_D("[%s] event id:%d \n", __FUNCTION__, event->id);
    switch (event->id)
    {
        case HL_WLAN_EVENT_STATUS_CHANGED:
            {
            hl_wlan_status_t status = hl_wlan_get_status();
            switch (status)
            {
                case HL_WLAN_STATUS_CONNECTED:
                    ui_event.id = UI_EVENT_ID_WLAN_CONNECT_SUCCEEDED;
                    hl_queue_enqueue(ui_event_queue, &ui_event, 1);
                    break;
                case HL_WLAN_STATUS_DISABLE:
                case HL_WLAN_STATUS_DISCONNECTED:
                    ui_event.id = UI_EVENT_ID_WLAN_CONNECT_DISCONNECT;
                    hl_queue_enqueue(ui_event_queue, &ui_event, 1);
                    break;
                default:
                    break;
            }
            }
            break;
        case HL_WLAN_EVENT_SCAN_SUCCEEDED:
            LOG_I("wpa_scan get scan result success\n");
            ui_event.id = UI_EVENT_ID_WLAN_SCAN_SUCCEEDED;
            wlan_scan_state = true;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            break;
        case HL_WLAN_EVENT_SCAN_FAILED:
        case HL_WLAN_EVENT_SCAN_FAILED_REQUEST:
        case HL_WLAN_EVENT_SCAN_FAILED_TIMEOUT:
            ui_event.id = UI_EVENT_ID_WLAN_SCAN_FAILED;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            break;
        case HL_WLAN_EVENT_CONNECT_FAILED_WRONG_PASSWORD:
            LOG_D("[%s] HL_WLAN_EVENT_CONNECT_FAILED_WRONG_PASSWORD \n", __FUNCTION__);
            ui_event.id = UI_EVENT_ID_WLAN_WRONG_PASSWORD;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            break;
        case HL_WLAN_EVENT_CONNECT_FAILED_CONNECT_FAILED:
            LOG_D("[%s] HL_WLAN_EVENT_CONNECT_FAILED_CONNECT_FAILED \n", __FUNCTION__);
            ui_event.id = UI_EVENT_ID_WLAN_CONNECT_FAILED;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            break;
        case HL_WLAN_EVENT_CONNECT_FAILED_REQUEST:
            ui_event.id = UI_EVENT_ID_WLAN_CONNECT_FAILED;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            break;
        case HL_WLAN_EVENT_CONNECT_FAILED_TIMEOUT:
            LOG_D("[%s] HL_WLAN_EVENT_CONNECT_FAILED_TIMEOUT \n", __FUNCTION__);
            break;
        case HL_WLAN_EVENT_CONNECT_FAILED_NOT_FOUND: // CTRL0-EVENT-NETWORK-NOT-FOUND will be generated after scan if no network connected
            LOG_D("[%s] HL_WLAN_EVENT_CONNECT_FAILED_NOT_FOUND \n", __FUNCTION__);
            ui_event.id = UI_EVENT_ID_WLAN_CONNECT_NOT_FOUND;
            hl_queue_enqueue(ui_event_queue, &ui_event, 1);
            complete_wifi_connecting();
            break;
        default:
            LOG_W("[%s] event:%d won't be handled.\n", __FUNCTION__, event->id);
            break;
    }
}
static void wlan_event_update(widget_t **widget_list)
{
    ui_event_t ui_event;
    while (hl_queue_dequeue(ui_event_queue, &ui_event, 1))
    {
        switch (ui_event.id)
        {
        case UI_EVENT_ID_WLAN_SCAN_SUCCEEDED:
            if (wifi_status.is_connecting || wifi_password_state) 
            {
                LOG_D("wifi is connecting or passwd is inputing, so stop update the wifi list after scanning.\n");
                break;
            }
            wlan_scan_result_num = hl_wlan_get_scan_result(wlan_scan_result);
            network_listitem_update();
            break;
        case UI_EVENT_ID_WLAN_CONNECT_SUCCEEDED:
            wlan_scan_result_num = hl_wlan_get_scan_result(wlan_scan_result);
            network_listitem_update();
            complete_wifi_connecting();
            break;
        case UI_EVENT_ID_WLAN_CONNECT_DISCONNECT:
            wlan_scan_result_num = hl_wlan_get_scan_result(wlan_scan_result);
            network_listitem_update();
            break;
        case UI_EVENT_ID_WLAN_WRONG_PASSWORD:
            LOG_D("[%s] UI_EVENT_ID_WLAN_WRONG_PASSWORD \n", __FUNCTION__);
            if (wifi_status.is_connecting) /* only pop this message box while connecting wifi manually by user */
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_network_single_msgbox_callback, (void *)MSGBOX_TIP_PASSWD_ERROR);
            }
            complete_wifi_connecting();
            break;
        case UI_EVENT_ID_WLAN_CONNECT_FAILED:
            LOG_D("[%s] UI_EVENT_ID_WLAN_CONNECT_FAILED \n", __FUNCTION__);
            if (wifi_status.is_connecting) /* only pop this message box while connecting wifi manually by user */
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_network_single_msgbox_callback, (void *)MSGBOX_TIP_CONNECT_FAIL);
            }
            complete_wifi_connecting();
            break;
        case UI_EVENT_ID_WLAN_CONNECT_NOT_FOUND:
            LOG_D("[%s] UI_EVENT_ID_WLAN_NOT_FOUND \n", __FUNCTION__);
            if (wifi_status.is_connecting) /* only pop this message box while connecting wifi manually by user */
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_network_single_msgbox_callback, (void *)MSGBOX_TIP_CONNECT_FAIL);
            }
            complete_wifi_connecting();
            break;
        default:
            LOG_W("[%s] ui_event:%d won't be handled.\n", __FUNCTION__, ui_event.id);
            break;
        }
    }
}

static void rotate_animation_cb(void *var, int32_t v)
{
    lv_img_set_angle((lv_obj_t *)var, v);
}

static void app_network_callback_update(lv_timer_t *timer);
static lv_timer_t *app_network_callback_timer = NULL;
void app_network_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        forget_password_state = false;
        wifi_password_state = false;

        if (network_model == NULL)
        {
            network_model = app_listitem_model_create(WINDOW_ID_WIFI_LIST_TEMPLATE, widget_list[WINDOW_ID_NETWORK_CONTAINER_LIST]->obj_container[0], network_listitem_callback, NULL);
            for (int i = 0; i < ITEM_NUMS; i++)
                app_listitem_model_push_back(network_model);
        }
        hl_wlan_get_status() > HL_WLAN_STATUS_DISABLE ? switch_state = true : switch_state = false;
        if (switch_state)
        {
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_network_single_msgbox_callback, (void *)MSGBOX_TIP_SCAN_SUCCEEDED);
            hl_wlan_set_enable(1);
            hl_wlan_scan();
        }
        wlan_scan_result_num = hl_wlan_get_scan_result(wlan_scan_result);
        network_listitem_update();
        app_network_update(widget_list);
        if (ui_event_queue == NULL)
            hl_queue_create(&ui_event_queue, sizeof(ui_event_t), 8);
        hl_wlan_register_event_callback(wlan_callback, NULL);
        // start background periodic scanning
        hl_wlan_enable_periodic_scan();

        // 隐藏蒙板
        lv_obj_add_flag(widget_list[WIDGET_ID_NETWORK_CONTAINER_MASK]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        if (app_network_callback_timer == NULL)
        {
            app_network_callback_timer = lv_timer_create(app_network_callback_update, 500, (void *)widget_list);
            lv_timer_ready(app_network_callback_timer);
        }
        wlan_event_update(widget_list);
        app_network_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        app_listitem_model_destory(network_model);
        hl_wlan_unregister_event_callback(wlan_callback, NULL);
        hl_queue_destory(&ui_event_queue);
        network_model = NULL;
        // stop background periodic scanning
        hl_wlan_disable_periodic_scan();

        if (keyboard)
        {
            window_copy_destory((window_t *)(keyboard->user_data));
            keyboard_destroy(keyboard);
            keyboard = NULL;
        }
        if (app_network_callback_timer != NULL)
        {
            lv_timer_del(app_network_callback_timer);
            app_network_callback_timer = NULL;
        }
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WINDOW_ID_NETWORK_CONTAINER_LIST:
        {
            /* When wifi is connecting, disable connecting it again */
            if (wifi_rotation.is_rotating) { break;} 
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(network_model);
#if 1
            app_listitem_t *item;
            int item_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(network_model, item_index);
                if (item->win == win)
                {
                    LOG_I("LV_EVENT_CHILD_CLICKED wifi ssid: %s\n", wlan_scan_result[item_index].ssid);

                    /* wifi rotation related*/
                    int i = item_index;
                    wifi_rotation.index = i;
                    memcpy(&wifi_rotation.connection, &wlan_scan_result[i], sizeof(hl_wlan_connection_t));


                    hl_wlan_key_mgmt_t key_mgmt;
                    /* checke if the wifi network has already been connected */
                    if (hl_wlan_is_in_db(wlan_scan_result[i].ssid, &key_mgmt) == 0)
                    {
                        if (wlan_scan_result[i].key_mgmt == key_mgmt)
                        {
                            start_wifi_connect(wlan_scan_result[i].ssid, "", wlan_scan_result[i].key_mgmt);
                            break;
                        }
                        else /* when the wifi network with same ssid but different key_mgmt, remove it from db */
                        {
                            wlan_db_remove_entry(wlan_scan_result[i].ssid);
                        }
                    }
                    /* when the key_mgmt of the selected wifi is NONE, then just connect it without typing passwd */
                    if (wlan_scan_result[i].key_mgmt == HL_WLAN_KEY_MGMT_NONE)
                    {
                        start_wifi_connect(wlan_scan_result[i].ssid, "", wlan_scan_result[i].key_mgmt);
                        break;
                    }

                    memcpy(&wlan_selete, &wlan_scan_result[i], sizeof(hl_wlan_connection_t));
                    wifi_password_state = true;
                    app_network_update(widget_list);

                    if (keyboard)
                    {
                        window_copy_destory((window_t *)(keyboard->user_data));
                        keyboard_destroy(keyboard);
                        keyboard = NULL;
                    }

                    // 创建键盘，传入对象为键盘容器；
                    if ((keyboard = app_keyboard_create(widget_list[WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD])) == NULL)
                    {
                        LOG_E("keyboard null\n");
                    }
                    else
                    {
                        if (!keyboard_set_text(keyboard, (char *)"")) { LOG_E("keyboard set text failed!");}
                        lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], (char *)"");
                        lv_obj_set_width(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), 220);
                        lv_obj_align(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), LV_ALIGN_TOP_LEFT, 15, (38 - app_get_text_height(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]))) / 2);

                        lv_obj_clear_flag(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0], lv_color_hex(0xFF616161), LV_PART_MAIN);
                    }
                    break;
                }
            }
#else
            for (int i = 0; i < wlan_scan_result_num; i++)
            {
                char *ssid_this = wlan_scan_result[i].ssid;
                char *ssid_selected = lv_label_get_text(win->widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[2]);
                if (ssid_this == NULL || ssid_selected == NULL) 
                {
                    LOG_E("null pointer!!\n");
                    break;
                }

                /* try to compare those two ssid and find the corresponding ssid in the scan results */
                int found = 0;
                if (strcmp(ssid_this, ssid_selected) == 0)
                {
                    found = 1;
                }
                else 
                {
                    int slen = strlen(ssid_selected);
                    /* when the ssid is 0123456789abcdef, it will displayed like 0123456789abc... in the UI */
                    if (slen >= 13 &&  /* 13 is a tested specific number here */
                        ssid_selected[slen-1] == '.' && 
                        ssid_selected[slen-2] == '.' && 
                        ssid_selected[slen-3] == '.')
                    {
                        if (strncmp(ssid_this, ssid_selected, slen-3) == 0)
                        {
                            LOG_W("try to handle this specific ssid: %s\n", ssid_selected);
                            found = 1;
                        }
                    }
                }

                /* handle the wifi connection */
                if (found)
                {
                    /* wifi rotation related*/
                    wifi_rotation.index = i;
                    memcpy(&wifi_rotation.connection, &wlan_scan_result[i], sizeof(hl_wlan_connection_t));


                    hl_wlan_key_mgmt_t key_mgmt;
                    /* checke if the wifi network has already been connected */
                    if (hl_wlan_is_in_db(wlan_scan_result[i].ssid, &key_mgmt) == 0)
                    {
                        if (wlan_scan_result[i].key_mgmt == key_mgmt)
                        {
                            start_wifi_connect(wlan_scan_result[i].ssid, "", wlan_scan_result[i].key_mgmt);
                            break;
                        }
                        else /* when the wifi network with same ssid but different key_mgmt, remove it from db */
                        {
                            wlan_db_remove_entry(wlan_scan_result[i].ssid);
                        }
                    }
                    /* when the key_mgmt of the selected wifi is NONE, then just connect it without typing passwd */
                    if (wlan_scan_result[i].key_mgmt == HL_WLAN_KEY_MGMT_NONE)
                    {
                        start_wifi_connect(wlan_scan_result[i].ssid, "", wlan_scan_result[i].key_mgmt);
                        break;
                    }

                    memcpy(&wlan_selete, &wlan_scan_result[i], sizeof(hl_wlan_connection_t));
                    app_listitem_t *item;
                    int item_index;
                    for (item_index = 0; item_index < model_size; item_index++)
                    {
                        item = app_listitem_model_get_item(network_model, item_index);
                        if (item->win == win)
                            break;
                    }
                    wifi_password_state = true;
                    app_network_update(widget_list);

                    if (keyboard)
                    {
                        window_copy_destory((window_t *)(keyboard->user_data));
                        keyboard_destroy(keyboard);
                        keyboard = NULL;
                    }

                    // 创建键盘，传入对象为键盘容器；
                    if ((keyboard = app_keyboard_create(widget_list[WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD])) == NULL)
                    {
                        LOG_E("keyboard null\n");
                    }
                    else
                    {
                        if (!keyboard_set_text(keyboard, (char *)"")) { LOG_E("keyboard set text failed!");}
                        lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], (char *)"");
                        lv_obj_set_width(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), 220);
                        lv_obj_align(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), LV_ALIGN_TOP_LEFT, 15, (38 - app_get_text_height(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]))) / 2);

                        lv_obj_clear_flag(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0], lv_color_hex(0xFF616161), LV_PART_MAIN);
                    }
                }
            }
#endif
        }
        break;
        }
        break;
    case LV_EVENT_CHILD_VALUE_CHANGE:
        switch (widget->header.index)
        {
        case WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD:
        {
            const char *str = keyboard_get_text(keyboard);
            if (str)
            {
                strncpy(wlan_psk, str, sizeof(wlan_psk));
                if(strlen(str) >= 8)
                {
                    lv_obj_add_flag(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_text_color(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                }
                else
                {
                    lv_obj_clear_flag(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_text_color(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM]->obj_container[0], lv_color_hex(0xFF616161), LV_PART_MAIN);
                }
            }
            else
            {
                LOG_E("keyboard get test failed\n");
            }

            if (wifi_password_hidden)
            {
                if (strlen(wlan_psk) > 0)
                {
                    char password_hidden_string[256];
                    memset(password_hidden_string, 0, sizeof(password_hidden_string));
                    for (int i = 0; i <= strlen(wlan_psk) - 1; i++)
                        strncat((char *)password_hidden_string, (const char *)"*", sizeof(password_hidden_string) - 1);
                    lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], password_hidden_string);
                }
                else
                    lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], "");
            }
            else
            {
                if (strlen(wlan_psk) > 0)
                    lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], wlan_psk);
                else
                    lv_textarea_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], "");
            }
            lv_obj_set_width(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), 220);
            lv_obj_align(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]), LV_ALIGN_TOP_LEFT, 19, (38 - app_get_text_height(lv_textarea_get_label(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0]))) / 2);

            lv_textarea_set_cursor_pos(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], keyboard->current_input_index);
            lv_obj_add_state(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_TEXTAREA]->obj_container[0], LV_STATE_FOCUSED);
            break;
        }
        }
        break;
    case LV_EVENT_CHILD_DESTROYED:
        switch (widget->header.index)
        {
        case WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD:
            // const char *str = keyboard_get_text(keyboard);
            // if (str && wlan_scan_result[click_wifi_item].ssid)
            // {
            //     strncpy(wlan_psk, str, sizeof(wlan_psk));
            //     if (strlen(wlan_psk) >= 8 && strlen(wlan_psk) <= 64)
            //         wifi_handle_flag = 3;
            //     else
            //         app_msgbox_push(WINDOW_ID_MSGBOX_DOUBLE_BUTTON, true, wifi_password_tip_msgbox_callback, NULL);
            // }
            keyboard = NULL;
            wifi_password_state = false;
            app_network_update(widget_list);
            break;
        }
        start_wifi_connect(wlan_selete.ssid, wlan_psk, wlan_selete.key_mgmt);
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WINDOW_ID_NETWORK_BTN_CONNECT_WIFI:
        case WINDOW_ID_NETWORK_BTN_FORGET_RETURN:
            if (widget->header.index == WINDOW_ID_NETWORK_BTN_CONNECT_WIFI)
                forget_password_state = true;
            if (widget->header.index == WINDOW_ID_NETWORK_BTN_FORGET_RETURN)
                forget_password_state = false;
            break;
        case WINDOW_ID_NETWORK_BTN_SWITCH:
            if (get_app_msgbox_is_busy()) //触发弹窗后禁止点击开关
            {
                break;
            }
            switch_state = !switch_state;
            if (switch_state)
            {
                wlan_scan_state = false;
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_network_single_msgbox_callback, (void *)MSGBOX_TIP_SCAN_SUCCEEDED);
                hl_wlan_set_enable(1);
                hl_wlan_scan();
            }
            else
            {
                hl_wlan_set_enable(0);
            }
            get_sysconf()->SetInt("system", "wifi", switch_state);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_network_update(widget_list);
            break;
        case WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_RETURN:
            wifi_password_state = false;
            app_network_update(widget_list);
            if (keyboard)
            {
                window_copy_destory((window_t *)(keyboard->user_data));
                keyboard_destroy(keyboard);
                keyboard = NULL;
            }
            break;
        case WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_CONFIRM:
        {
            wifi_password_state = false;
            app_network_update(widget_list);
            
            // 取消蒙板隐藏,禁止点击
             lv_obj_clear_flag(widget_list[WIDGET_ID_NETWORK_CONTAINER_MASK]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            
            start_wifi_connect(wlan_selete.ssid, wlan_psk, wlan_selete.key_mgmt);
            if (keyboard)
            {
                window_copy_destory((window_t *)(keyboard->user_data));
                keyboard_destroy(keyboard);
                keyboard = NULL;
            }

            // 隐藏蒙板
             lv_obj_add_flag(widget_list[WIDGET_ID_NETWORK_CONTAINER_MASK]->obj_container[0],LV_OBJ_FLAG_HIDDEN);

        }
            break;
        case WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_HIDDEN:
            wifi_password_hidden = !wifi_password_hidden;
            app_network_update(widget_list);
            lv_event_send(widget_list[WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD]->obj_container[0], (lv_event_code_t)LV_EVENT_CHILD_VALUE_CHANGE, NULL);
            break;
        case WINDOW_ID_NETWORK_BTN_FORGET_PASSWORD:
            hl_wlan_connection_t wlan_curr = {0}; 
            if (hl_wlan_get_connection(&wlan_curr) != -1)
            {
                wlan_db_remove_entry(wlan_curr.ssid);
                hl_wlan_disconnect();
            }
            else
            {
                LOG_E("[%s] ERROR: get wlan connection.\n", __FUNCTION__);
            }
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void app_network_callback_update(lv_timer_t *timer)
{
    widget_t **widget_list = (widget_t **)timer->user_data;
    wlan_event_update(widget_list);
    app_network_update(widget_list);

    if(keyboard != NULL)
    {
        app_keyboard_update(keyboard);
    }
}

static void app_network_update(widget_t **widget_list)
{
    hl_wlan_connection_t wlan_cur_connection = {0};
    if (wifi_password_state == false)
    {
        widget_t **top_widget_list;
        top_widget_list = window_get_top_widget_list();
        lv_obj_clear_flag(top_widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(widget_list[WINDOW_ID_NETWORK_CONTAINER_CONNECT_CONTAINER]->obj_container[2], "Wi-Fi");
        lv_label_set_text(widget_list[WINDOW_ID_NETWORK_CONTAINER_WIFI_LIST_CONTAINER]->obj_container[2], "Wi-Fi");
        lv_label_set_long_mode(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], LV_LABEL_LONG_DOT);
        lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_MAIN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_PASSWORD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (forget_password_state)
        {
            lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_CONNECT_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_FORGET_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_CONNECT_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_FORGET_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }

        if (switch_state)
        {
            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_BTN_SWITCH]->obj_container[1], ui_get_image_src(131));
            if (!get_app_msgbox_is_busy() && wlan_scan_state)
            {
                lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_LIST]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            }
        }

        else
        {
            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_BTN_SWITCH]->obj_container[1], ui_get_image_src(132));
            lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_LIST]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
        if (hl_wlan_get_status() == HL_WLAN_STATUS_CONNECTED && hl_wlan_get_connection(&wlan_cur_connection) != -1)
        {
            char ipaddr[16] = {0};
            hl_netif_get_ip_address(HL_NET_INTERFACE_WLAN, ipaddr, sizeof(ipaddr));
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], wlan_cur_connection.ssid);
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_BTN_FORGET_WIFI_NAME]->obj_container[2], wlan_cur_connection.ssid);
            lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_x(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], 32);
            lv_obj_set_width(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], 120);
            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_IMAGE_CONNECT_WIFI]->obj_container[0], ui_get_image_src(121));
            lv_obj_set_style_text_color(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);

            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_CONTAINER_BTN_CONNECT_IP]->obj_container[1], ui_get_image_src(196));
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_CONTAINER_BTN_CONNECT_IP]->obj_container[2], ipaddr);
        }
        else
        {
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], "");
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_BTN_FORGET_WIFI_NAME]->obj_container[2], "");
            lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

            lv_obj_set_x(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], 16);
            lv_obj_set_width(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], 120);
            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_IMAGE_CONNECT_WIFI]->obj_container[0], ui_get_image_src(203));
            lv_obj_set_style_text_color(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], lv_color_hex(0xFFC8C8C8), LV_PART_MAIN);

            lv_img_set_src(widget_list[WINDOW_ID_NETWORK_CONTAINER_BTN_CONNECT_IP]->obj_container[1], ui_get_image_src(197));
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_BTN_CONNECT_WIFI]->obj_container[2], "");
            lv_label_set_text(widget_list[WINDOW_ID_NETWORK_CONTAINER_BTN_CONNECT_IP]->obj_container[2], "");
        }
    }
    else
    {
        widget_t **top_widget_list;
        top_widget_list = window_get_top_widget_list();
        lv_obj_add_flag(top_widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_MAIN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WINDOW_ID_NETWORK_CONTAINER_PASSWORD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_NAME]->obj_container[1], ui_get_image_src(207));
        lv_label_set_text(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_NAME]->obj_container[2],wlan_selete.ssid);
        app_set_widget_item2center_align(widget_list, WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_NAME, 10);

        if (wifi_password_hidden)
            lv_img_set_src(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_HIDDEN]->obj_container[1], ui_get_image_src(206));
        else
            lv_img_set_src(widget_list[WIDGET_ID_NETWORK_BTN_WIFI_PASSWORD_HIDDEN]->obj_container[1], ui_get_image_src(205));
    }
}

static void network_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        lv_obj_add_flag(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_CONNECTING]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(network_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(network_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void network_listitem_update(void)
{
    for (int i = 0; i < ITEM_NUMS; i++)
    {
        /* update wifi rotation icon */
        if (wifi_rotation.is_rotating 
            && strcmp(wlan_scan_result[i].ssid, wifi_rotation.connection.ssid) == 0)
        {
            if (wifi_rotation.index != i)
            {
                wifi_rotate_stop(&wifi_rotation);
                wifi_rotation.index = i;
                wifi_rotate_start(&wifi_rotation);
            }
        }
        app_listitem_t *item = app_listitem_model_get_item(network_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;
        if (i < wlan_scan_result_num)
            lv_obj_clear_flag(widget_list[WIDGET_ID_WIFI_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(widget_list[WIDGET_ID_WIFI_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_pos(widget_list[WIDGET_ID_WIFI_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], 0, 40 * i);
        lv_label_set_long_mode(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[2], LV_LABEL_LONG_DOT);
        lv_label_set_text_fmt(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[2], "%s", wlan_scan_result[i].ssid);
        if (wlan_scan_result[i].signal <= 100 / 3)
        {
            lv_img_set_src(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_SIGNAL]->obj_container[0], ui_get_image_src(200));
        }
        else if (wlan_scan_result[i].signal <= 200 / 3)
        {
            lv_img_set_src(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_SIGNAL]->obj_container[0], ui_get_image_src(201));
        }
        else
        {
            lv_img_set_src(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_SIGNAL]->obj_container[0], ui_get_image_src(202));
        }

        if (wlan_scan_result_num > 0 && i == 0 && hl_wlan_get_status() == HL_WLAN_STATUS_CONNECTED) // 已连接
            lv_obj_set_style_bg_color(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
        else
            lv_obj_set_style_bg_color(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);

        if (wlan_scan_result[i].key_mgmt == HL_WLAN_KEY_MGMT_NONE)
        {
            // hide the lock icon
            lv_obj_add_flag(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_CONNECTING]->obj_container[0],146);
        }
        else
        {
            // show up the lock icon
            lv_obj_clear_flag(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_BTN_INFO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(widget_list[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_CONNECTING]->obj_container[0],111);
        }
    }
}

static void animation_rotation(lv_obj_t *rotate_obj)
{
    lv_anim_t rotate_anim;
    // 设置图片旋转动画
    lv_anim_init(&rotate_anim);
    lv_anim_set_values(&rotate_anim, 0, 3590);
    lv_anim_set_time(&rotate_anim, 1000);
    lv_anim_set_exec_cb(&rotate_anim, rotate_animation_cb);
    lv_anim_set_path_cb(&rotate_anim, lv_anim_path_linear);
    lv_anim_set_var(&rotate_anim, rotate_obj);
    lv_anim_set_repeat_count(&rotate_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&rotate_anim);
    lv_obj_clear_flag(rotate_obj,LV_OBJ_FLAG_HIDDEN);
}

static bool app_network_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_CONNECT_FAIL)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(288));
        else if (tip_index == MSGBOX_TIP_PASSWD_ERROR)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(287));
        }
        else if (tip_index == MSGBOX_TIP_SCAN_SUCCEEDED)
        {
            wlan_scan_state = false;
            lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER]->obj_container[0], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER]->obj_container[1], ui_get_image_src(367));
            lv_obj_set_align(widget_list[WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER]->obj_container[1], LV_ALIGN_TOP_MID);
            animation_rotation(widget_list[WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER]->obj_container[1]);
        }
        
        start_tick = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            if (tip_index == MSGBOX_TIP_CONNECT_FAIL || tip_index == MSGBOX_TIP_PASSWD_ERROR)
            {
                complete_wifi_connecting();
                return true;
            }
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_CONNECT_FAIL || tip_index == MSGBOX_TIP_PASSWD_ERROR)
        {
            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                complete_wifi_connecting();
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_SCAN_SUCCEEDED)
        {
            if (utils_get_current_tick() - start_tick > 7 * 1000 || wlan_scan_state)
            {
                return true;
            }
        }
        
        break;
    }
    return false;
}



/**
 * @Breif Start rotate wifi icon to notify users that wifi is connecting
 *
 * @Param index: index of wifi network in the wifi list
 *
 * @Returns
 */
static int wifi_rotate_start(wifi_rotation_t *w)
{
    LOG_D("wifi rotate: start!!!!!\n");
    int index;
    app_listitem_t *item;
    window_t *win;
    widget_t **widget_list_temp;
    lv_anim_t rotate_anim;
    lv_obj_t *rotate_obj;

    if (w == NULL) { goto null_point; }
    if (w->is_rotating == 1)
    {
        LOG_D("wifi rotate: already started\n");
        return 0;
    }

    index = w->index;
    item = app_listitem_model_get_item(network_model, index);
    if (item == NULL) { goto null_point; }
    win = item->win;
    widget_list_temp = win->widget_list;

    rotate_obj = widget_list_temp[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_CONNECTING]->obj_container[0];
    // 设置图片旋转动画
    lv_anim_init(&rotate_anim);
    lv_anim_set_values(&rotate_anim, 0, 3590);
    lv_anim_set_time(&rotate_anim, 1000);
    lv_anim_set_exec_cb(&rotate_anim, rotate_animation_cb);
    lv_anim_set_path_cb(&rotate_anim, lv_anim_path_linear);
    lv_anim_set_var(&rotate_anim, rotate_obj);
    lv_anim_set_repeat_count(&rotate_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&rotate_anim);
    lv_obj_clear_flag(rotate_obj,LV_OBJ_FLAG_HIDDEN);
    w->is_rotating = 1;

    return 0;

null_point:
    LOG_E("wifi start: null pointer\n");
    return -1;
}


static int wifi_rotate_stop(wifi_rotation_t *w)
{
    LOG_D("wifi rotate: stop!!!!!\n");
    int index;
    app_listitem_t *item;
    window_t *win;
    widget_t **widget_list_temp;
    lv_anim_t rotate_anim;
    lv_obj_t *rotate_obj;

    if (w == NULL) { goto null_point; }
    if (w->is_rotating == 0)
    {
        LOG_D("wifi rotate: already stopped\n");
        return 0;
    }

    index = w->index;
    item = app_listitem_model_get_item(network_model, index);
    if (item == NULL) { goto null_point; }
    win = item->win;
    widget_list_temp = win->widget_list;
    rotate_obj = widget_list_temp[WINDOW_ID_WIFI_LIST_TEMPLATE_IMAGE_CONNECTING]->obj_container[0];
    //删除动画并隐藏
    lv_anim_del(rotate_obj, &rotate_animation_cb);   
    lv_img_set_angle(rotate_obj, 0);
    lv_obj_add_flag(rotate_obj,LV_OBJ_FLAG_HIDDEN);
    w->is_rotating = 0;

    return 0;

null_point:
    LOG_E("wifi stop: null pointer\n");
    return -1;
}

static int start_wifi_connect(const char *ssid, const char *psk, hl_wlan_key_mgmt_t key_mgmt)
{
    if (wifi_status.is_connecting) 
    {
        LOG_W("wifi has already been connecting.\n");
        return 1;
    }

    wifi_status.is_connecting = 1;
    wifi_rotate_start(&wifi_rotation);
    hl_wlan_disable_periodic_scan();
    hl_wlan_connect(ssid, psk, key_mgmt);

    return 0;
}

static int complete_wifi_connecting()
{
    wifi_status.is_connecting = 0;
    wifi_rotate_stop(&wifi_rotation);
    hl_wlan_enable_periodic_scan();
}
