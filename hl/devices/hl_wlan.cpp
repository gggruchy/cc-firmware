#include "hl_wlan.h"
#include "hl_netlink_uevent.h"
#include "hl_sm.h"
#include "hl_tpool.h"
#include "hl_common.h"
#include "hl_assert.h"
#include "hl_ringbuffer.h"
// #include "usb_power.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <linux/if.h>

#include "configfile.h"

#include "wpa_client/wpa_ctrl.h"

#define LOG_TAG "hl_wlan"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#define LOG_WLAN_SM_EVENT()  \
            if (event_id != HL_SM_EVENT_ID_IDLE) {LOG_D("[%s] got event:%d\n", __FUNCTION__, event_id);}

#define WPA_CTRL_PATH "/var/run/wpa_supplicant"
#define WPA_SCAN_REQUEST_RETRY 16

typedef struct __attribute__((packed))
{
    char ssid[HL_WLAN_SSID_MAXLENGTH + 1];
    char psk[HL_WLAN_PSK_MAXLENGTH + 1];
    hl_wlan_key_mgmt_t key_mgmt;
} wlan_connection_entry_t;

typedef struct
{
    hl_sm_t scan_sm;
    hl_sm_t connect_sm;

    hl_wlan_connection_t scan_result[HL_WLAN_SCAN_RESULT_BUFFER_SIZE];
    int scan_result_numbers;
    int signal;

    hl_wlan_connection_t connection;
    hl_wlan_connection_t disconnect_connection;
    wlan_connection_entry_t connection_entry;

    uint64_t scanning_ticks;
    uint64_t connecting_ticks;
    uint64_t disconnect_ticks;
    uint64_t poll_ticks;

    struct wpa_ctrl *command;
    struct wpa_ctrl *monitor;
} wlan_t;

typedef struct
{
    int append;
    char interface[64];
} wlan_thread_msg_t;

typedef enum
{
    /* events for scan sm */
    WLAN_SM_EVENT_ID_SCAN_STARTED = HL_SM_EVENT_ID_USER,
    WLAN_SM_EVENT_ID_SCAN_FAILED,
    WLAN_SM_EVENT_ID_SCAN_RESULTS,
    WLAN_SM_EVENT_ID_SCAN_REQUEST, // 3

    /* events from wpa_supplicant for connect sm */
    WLAN_SM_EVENT_ID_CONNECT_CONNECTED = HL_SM_EVENT_ID_USER,
    WLAN_SM_EVENT_ID_CONNECT_DISCONNECTED,
    WLAN_SM_EVENT_ID_CONNECT_TEMP_DISABLED,
    WLAN_SM_EVENT_ID_CONNECT_WRONG_KEY,
    WLAN_SM_EVENT_ID_CONNECT_CONNECT_FAILED,
    WLAN_SM_EVENT_ID_CONNECT_NETWORK_NOT_FOUND,
    /* commands from outside for the connect sm */
    WLAN_SM_EVENT_ID_CONNECT_CONNECT_REQUEST,
    WLAN_SM_EVENT_ID_CONNECT_DISCONNECT_REQUEST, 
    WLAN_SM_EVENT_ID_CONNECT_DISABLE,
    WLAN_SM_EVENT_ID_CONNECT_ENABLE,
} wlan_sm_event_t;

static void netlink_uevent_callback(const void *data, void *user_data);
static void wlan_interface_scan(void);
static void wlan_interface_append(const char *interface);
static void wlan_interface_remove(const char *interface);

static int wlan_init(wlan_t *wlan, const char *interface);
static void wlan_deinit(wlan_t *wlan);

static int wpa_client_init(wlan_t *wlan, const char *interface);
static void wpa_client_deinit(wlan_t *wlan);
static void wpa_monitor_poll(wlan_t *wlan);
static int wpa_simple_cmd(wlan_t *wlan, char *reply, size_t *reply_len, const char *cmd);
static int wpa_cmd(wlan_t *wlan, char *reply, size_t *reply_len, const char *cmd, ...);
static int wpa_connect(wlan_t *wlan, const wlan_connection_entry_t *entry);
static int wpa_add_network(wlan_t *wlan, const wlan_connection_entry_t *entry);
static int wpa_enable_network(wlan_t *wlan, int network_id);
static int wpa_disable_network(wlan_t *wlan, int network_id);
static int wpa_enable_all_network(wlan_t *wlan);
static int wpa_disable_all_network(wlan_t *wlan);
static int wpa_remove_network(wlan_t *wlan, int network_id);
static int wpa_remove_all_network(wlan_t *wlan);
static int wpa_find_network_id(wlan_t *wlan, const char *ssid);
static int wpa_get_current_ssid(wlan_t *wlan, char *ssid, uint32_t ssid_len);
static int wpa_get_current_bssid(wlan_t *wlan, char *bssid, uint32_t bssid_len);
static int wpa_signal_poll(wlan_t *wlan, int *signal);

static int wpa_process_start(const char *interface);
static void wpa_process_stop(void);

static int wpa_get_ssid(char *ssid, char *ssid_unicode, uint32_t size);
static int wpa_get_key_mgmt(const char *flags);
static int wpa_get_signal(const char *signal_level);
static int wpa_has_token(char *token, const char *key);
static int wpa_scan_result_sort_by_ssid(const void *p1, const void *p2);
static int wpa_scan_result_sort_by_signal(const void *p1, const void *p2);
static int wpa_scan_result_update(char *reply, size_t reply_len, hl_wlan_connection_t *connections);

static int wlan_db_append_entry(wlan_connection_entry_t *connection);
static int wlan_db_find_entry(const char *ssid, wlan_connection_entry_t *connection);
static int wlan_db_is_exists(const char *ssid);
static int wlan_db_dump_to_network(wlan_t *wlan);

static void wlan_routine(hl_tpool_thread_t thread, void *args);

static void wlan_scan_idle(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);
static void wlan_scan_scanning(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);

static void wlan_connect_disable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);
static void wlan_connect_disconnected(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);
static void wlan_connect_connected(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);

static uint32_t local_scan_timeout;
static uint32_t local_connect_timeout;
static uint32_t local_reconnect_timeout;
static int local_wlan_enable;
static int local_wlan_onboard;
static int local_wlan_periodic_scan = 0;


static hl_wlan_status_t wlan_status;
static char wlan_interface[IFNAMSIZ];
static pthread_rwlock_t glock;

static hl_callback_t wlan_callback;
static hl_tpool_thread_t wlan_thread;

static wlan_t wlan_constant;

hl_ringbuffer_db_t db;
static pthread_rwlock_t db_lock;

extern ConfigParser *get_sysconf();

void hl_wlan_init(int wlan_enable, uint64_t scan_timeout, uint64_t connect_timeout, uint64_t reconnect_timeout,
                  const char *entry_file_path, uint32_t entry_max, int onboard)
{
    HL_ASSERT(entry_file_path != NULL);
    HL_ASSERT(strlen(entry_file_path) > 0);
    HL_ASSERT(entry_max > 0);

    local_scan_timeout = scan_timeout;
    local_connect_timeout = connect_timeout;
    local_reconnect_timeout = reconnect_timeout;
    local_wlan_enable = wlan_enable;
    local_wlan_onboard = onboard;

    wlan_status = HL_WLAN_STATUS_NOT_EXISTS;

    HL_ASSERT(hl_ringbuffer_db_open(entry_file_path, &db, sizeof(wlan_connection_entry_t), entry_max) == 0);
    HL_ASSERT(pthread_rwlock_init(&glock, NULL) == 0);
    HL_ASSERT(pthread_rwlock_init(&db_lock, NULL) == 0);
    HL_ASSERT(hl_callback_create(&wlan_callback) == 0);
    HL_ASSERT(hl_tpool_create_thread(&wlan_thread, wlan_routine, &wlan_constant, sizeof(wlan_thread_msg_t), 2, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(wlan_thread, 0) == 1);

    hl_netlink_uevent_register_callback(netlink_uevent_callback, NULL);
}

int hl_wlan_set_enable(int enable)
{
    // TODO: 设置后由线程转发事件并记录
    pthread_rwlock_rdlock(&glock);
    local_wlan_enable = enable;
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        if (hl_sm_send_event(wlan_constant.connect_sm, enable ? WLAN_SM_EVENT_ID_CONNECT_ENABLE : WLAN_SM_EVENT_ID_CONNECT_DISABLE, NULL, 0) != 0)
        {
            pthread_rwlock_unlock(&glock);
            return -1;
        }
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_connect(const char *ssid, const char *psk, hl_wlan_key_mgmt_t key_mgmt)
{
    int ssid_len = strlen(ssid);
    int psk_len = strlen(psk);
    wlan_connection_entry_t entry;

    if (ssid_len < HL_WLAN_SSID_MINLENGTH || ssid_len > HL_WLAN_SSID_MAXLENGTH)
    {
        LOG_I("ssid overrange: %s\n", ssid);
        return -1;
    }
    else
    {
        strncpy(entry.ssid, ssid, sizeof(entry.ssid));
    }

    if (key_mgmt != HL_WLAN_KEY_MGMT_NONE && key_mgmt != HL_WLAN_KEY_MGMT_WPA_PSK && key_mgmt != HL_WLAN_KEY_MGMT_WPA2_PSK && key_mgmt != HL_WLAN_KEY_MGMT_WPA3_PSK)
    {
        LOG_I("key_mgmt not supported: %d\n", key_mgmt);
        return -1;
    }
    else
    {
        entry.key_mgmt = key_mgmt;
    }

    if (key_mgmt == HL_WLAN_KEY_MGMT_NONE)
    {
        if (psk_len > 0)
        {
            LOG_I("key_mgnt is none but psk not empty\n");
            return -1;
        }
        else
        {
            memset(entry.psk, 0, sizeof(entry.psk));
        }
    }
    else if (key_mgmt != HL_WLAN_KEY_MGMT_NONE)
    {
        // 查询持久化记录
        if (psk_len == 0)
        {
            if (wlan_db_find_entry(ssid, &entry) != 0)
            {
                LOG_I("can't find persistent\n");
                return -1;
            }
            LOG_I("hl_wlan_connect:find persistent ssid %s psk %s\n", entry.ssid, entry.psk);
        }
        else if (psk_len < HL_WLAN_PSK_MINLENGTH || psk_len > HL_WLAN_PSK_MAXLENGTH)
        {
            LOG_I("psk overrange: %s\n", psk);
            return -1;
        }
        else
        {
            strncpy(entry.psk, psk, sizeof(entry.psk));
        }
    }

    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        if (hl_sm_send_event(wlan_constant.connect_sm, WLAN_SM_EVENT_ID_CONNECT_CONNECT_REQUEST, &entry, sizeof(entry)) != 0)
        {
            pthread_rwlock_unlock(&glock);
            return -1;
        }
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_disconnect(void)
{
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        if (hl_sm_send_event(wlan_constant.connect_sm, WLAN_SM_EVENT_ID_CONNECT_DISCONNECT_REQUEST, NULL, 0) != 0)
        {
            pthread_rwlock_unlock(&glock);
            return -1;
        }
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

hl_wlan_status_t hl_wlan_get_status(void)
{
    hl_wlan_status_t state;
    pthread_rwlock_rdlock(&glock);
    state = wlan_status;
    pthread_rwlock_unlock(&glock);
    return state;
}

int hl_wlan_get_interface(char *interface)
{
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        strcpy(interface, wlan_interface);
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_get_connection(hl_wlan_connection_t *connection)
{
    pthread_rwlock_rdlock(&glock);
    if (wlan_status == HL_WLAN_STATUS_CONNECTED)
    {
        memcpy(connection, &wlan_constant.connection, sizeof(*connection));
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_is_in_db(const char *ssid, hl_wlan_key_mgmt_t *key_mgmt)
{
    int rv;
    wlan_connection_entry_t entry;
    rv = wlan_db_find_entry(ssid, &entry);
    *key_mgmt = entry.key_mgmt;

    return rv;
}

int hl_wlan_scan(void)
{
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        if (hl_sm_get_current_state(wlan_constant.scan_sm) != wlan_scan_idle)
        {
            LOG_I("wlan scan already in progress\n");
            pthread_rwlock_unlock(&glock);
            return -1;
        }

        if (hl_sm_send_event(wlan_constant.scan_sm, WLAN_SM_EVENT_ID_SCAN_REQUEST, NULL, 0) != 0)
        {
            LOG_I("wlan scan send request failed\n");
            pthread_rwlock_unlock(&glock);
            return -1;
        }
    }
    else
    {
        LOG_I("wlan not exists\n");
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_is_scanning(void)
{
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        if (hl_sm_get_current_state(wlan_constant.scan_sm) == wlan_scan_scanning)
        {
            pthread_rwlock_unlock(&glock);
            return 1;
        }
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_wlan_get_scan_result(hl_wlan_connection_t *scan_result)
{
    hl_wlan_connection_t *tmp[HL_WLAN_SCAN_RESULT_BUFFER_SIZE];
    int sorted_numbers = 0;
    LOG_I("hl_wlan_get_scan_result\n");
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        LOG_I("wlan_constant.scan_result_numbers %d\n", wlan_constant.scan_result_numbers);
        for (int i = 0; i < wlan_constant.scan_result_numbers; i++)
        {
            tmp[i] = &wlan_constant.scan_result[i];
        }

        qsort(tmp, wlan_constant.scan_result_numbers, sizeof(hl_wlan_connection_t *), wpa_scan_result_sort_by_ssid);

        for (int i = 0; i < wlan_constant.scan_result_numbers; i++)
        {
            if (strlen(tmp[i]->ssid) == 0)
            {
                LOG_I("skip no ssid wifi %d\n", i);
                break;
            }

            if (sorted_numbers)
            {
                // 合并同名WIFI
                if (strncmp(tmp[sorted_numbers - 1]->ssid, tmp[i]->ssid, HL_WLAN_SSID_MAXLENGTH + 1) == 0)
                {
                    LOG_D("same ssid wifi %s case\n", tmp[i]->ssid);
                    if (tmp[i]->signal > tmp[sorted_numbers - 1]->signal)
                        tmp[sorted_numbers - 1] = tmp[i];
                }
                else
                {
                    tmp[sorted_numbers] = tmp[i];
                    sorted_numbers++;
                }
            }
            else
            {
                tmp[sorted_numbers] = tmp[i];
                sorted_numbers++;
            }
        }
        qsort(tmp, sorted_numbers, sizeof(hl_wlan_connection_t *), wpa_scan_result_sort_by_signal);
        LOG_I("ssid sort done\n");

        if (wlan_status == HL_WLAN_STATUS_CONNECTED)
        {
            hl_wlan_connection_t *cuurent_connect = &wlan_constant.connection;
            LOG_I("place connected ssid first %s\n", cuurent_connect->ssid);

            int index = 0;
            memcpy(&scan_result[index++], cuurent_connect, sizeof(hl_wlan_connection_t));
            for (int i = 0; i < sorted_numbers; i++)
            {
                if (strcmp(tmp[i]->ssid, cuurent_connect->ssid))
                {
                    memcpy(&scan_result[index++], tmp[i], sizeof(hl_wlan_connection_t));
                }
                // 当前已经连接的WIFI不在扫描列表里
                if (index == HL_WLAN_SCAN_RESULT_BUFFER_SIZE)
                {
                    LOG_W("current ssid not at list\n");
                    break;
                }
            }
        }
        else
        {
            for (int i = 0; i < sorted_numbers; i++)
            {
                memcpy(&scan_result[i], tmp[i], sizeof(scan_result[i]));
            }
        }
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return 0;
    }
    pthread_rwlock_unlock(&glock);

#define PRINT_SORTED_RESULT
#ifdef PRINT_SORTED_RESULT
    LOG_D("sorted ssids(%d):\n", sorted_numbers);
    for (int i = 0; i < sorted_numbers; i++)
    {
        LOG_D("sssid(%2d): %s\n", i+1, scan_result[i].ssid);
    }

#endif
    LOG_I("[%s] scan result number: %d\n", __FUNCTION__, sorted_numbers);

    return sorted_numbers;
}

int hl_wlan_get_scan_result_raw(hl_wlan_connection_t *scan_result)
{
    int numbers;
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        numbers = wlan_constant.scan_result_numbers;
        if (numbers > 0)
            memcpy(scan_result, wlan_constant.scan_result, sizeof(hl_wlan_connection_t) * numbers);
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);

    return numbers;
}

int hl_wlan_get_scan_result_numbers(void)
{
    int numbers;
    pthread_rwlock_rdlock(&glock);
    if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS)
    {
        numbers = wlan_constant.scan_result_numbers;
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return numbers;
}

int hl_wlan_get_signal(void)
{
    int signal;
    pthread_rwlock_rdlock(&glock);
    signal = wlan_constant.signal;
    pthread_rwlock_unlock(&glock);
    return signal;
}

void hl_wlan_register_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_register(wlan_callback, function, user_data);
}

void hl_wlan_unregister_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_unregister(wlan_callback, function, user_data);
}


int hl_wlan_get_enable(void)
{
    return local_wlan_enable;
}

int hl_wlan_enable_periodic_scan(void)
{
    local_wlan_periodic_scan = 1;
    LOG_D("Enable wlan periodic scan.\n");
}

int hl_wlan_disable_periodic_scan(void)
{
    local_wlan_periodic_scan = 0;
    LOG_D("Disable wlan periodic scan.\n");
}

static void netlink_uevent_callback(const void *data, void *user_data)
{
    hl_netlink_uevent_msg_t *uevent_msg = (hl_netlink_uevent_msg_t *)data;
    if (strlen(uevent_msg->devtype) == 0 || strcmp(uevent_msg->devtype, "wlan") != 0)
        return;
    if (strlen(uevent_msg->interface) == 0)
    {
        LOG_I("wlan can't found interface\n");
        return;
    }
    if (strcmp(uevent_msg->action, "add") == 0)
        wlan_interface_append(uevent_msg->interface);
    else if (strcmp(uevent_msg->action, "remove") == 0)
        wlan_interface_remove(uevent_msg->interface);
}

static void wlan_interface_scan(void)
{
    DIR *dirp;
    struct dirent *dp;
    char syspath[PATH_MAX];
    const char add[] = "add";
    dirp = opendir("/sys/class/net");
    if (dirp == NULL)
    {
        LOG_I("opendir failed: %s\n", strerror(errno));
        return;
    }

    while ((dp = readdir(dirp)) != NULL)
    {
        if (strstr(dp->d_name, "wlan"))
        {
            snprintf(syspath, sizeof(syspath), "/sys/class/net/%s/uevent", dp->d_name);
            hl_echo(syspath, add, sizeof(add));
            break;
        }
    }
    closedir(dirp);
}

static void wlan_interface_append(const char *interface)
{
    wlan_thread_msg_t msg;
    msg.append = 1;
    strncpy(msg.interface, interface, sizeof(msg.interface));
    LOG_I("wlan_interface_append\n");
    hl_tpool_send_msg(wlan_thread, &msg);
}

static void wlan_interface_remove(const char *interface)
{
    wlan_thread_msg_t msg;
    msg.append = 0;
    strncpy(msg.interface, interface, sizeof(msg.interface));
    LOG_I("wlan_interface_remove\n");
    hl_tpool_send_msg(wlan_thread, &msg);
}

static int wpa_client_init(wlan_t *wlan, const char *interface)
{
    char ctrl_path[PATH_MAX];

    if (access(WPA_CTRL_PATH, F_OK) != 0)
    {
        LOG_I("wpa_supplicant don't exist\n");
        if (wpa_process_start(interface) != 0)
            return -1;
    }
    else
    {
        LOG_I("wpa_supplicant already exist\n");
        wpa_process_stop();
        if (wpa_process_start(interface) != 0)
            return -1;
    }

    snprintf(ctrl_path, sizeof(ctrl_path), "%s/%s", WPA_CTRL_PATH, interface);

    if ((wlan->monitor = wpa_ctrl_open(ctrl_path)) == NULL)
    {
        wpa_process_stop();
        return -1;
    }

    if (wpa_ctrl_attach(wlan->monitor) != 0)
    {
        wpa_ctrl_close(wlan->monitor);
        wpa_process_stop();
        return -1;
    }

    if ((wlan->command = wpa_ctrl_open(ctrl_path)) == NULL)
    {
        wpa_ctrl_close(wlan->monitor);
        wpa_process_stop();
        return -1;
    }

    return 0;
}

static void wpa_client_deinit(wlan_t *wlan)
{
    wpa_ctrl_close(wlan->command);
    wpa_ctrl_close(wlan->monitor);
    wpa_process_stop();
}

static void wpa_monitor_poll(wlan_t *wlan)
{
    char reply[4096];
    size_t reply_len = sizeof(reply);
    if (wpa_ctrl_pending(wlan->monitor) == 1)
    {
        wpa_ctrl_recv(wlan->monitor, reply, &reply_len);
        reply[reply_len] = '\0';
        LOG_D("wpa monitor:%s\n", reply);
        if (strstr(reply, WPA_EVENT_SCAN_STARTED))
        {
            hl_sm_send_event(wlan->scan_sm, WLAN_SM_EVENT_ID_SCAN_STARTED, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_SCAN_FAILED))
        {
            hl_sm_send_event(wlan->scan_sm, WLAN_SM_EVENT_ID_SCAN_FAILED, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_SCAN_RESULTS))
        {
            hl_sm_send_event(wlan->scan_sm, WLAN_SM_EVENT_ID_SCAN_RESULTS, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_CONNECTED))
        {
            char bssid[18];
            wpa_get_current_bssid(wlan, bssid, sizeof(bssid));
            pthread_rwlock_wrlock(&glock);
            for (int i = 0; i < wlan->scan_result_numbers; i++)
            {
                if (strcmp(wlan->scan_result[i].bssid, bssid) == 0)
                {
                    memcpy(&wlan->connection, &wlan->scan_result[i], sizeof(wlan->connection));
                    break;
                }
            }
            wlan->signal = wlan->connection.signal;
            pthread_rwlock_unlock(&glock);

            hl_sm_send_event(wlan->connect_sm, WLAN_SM_EVENT_ID_CONNECT_CONNECTED, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_DISCONNECTED))
        {
            hl_sm_send_event(wlan->connect_sm, WLAN_SM_EVENT_ID_CONNECT_DISCONNECTED, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_TEMP_DISABLED) && strstr(reply, "reason=WRONG_KEY"))
        {
            char ssid[256];
            char ssid_unicode[HL_WLAN_SSID_MAXLENGTH + 1];
            const char *p1 = strstr(reply, "ssid=");
            char *p2 = ssid;
            if (p1 != NULL)
            {
                p1 += sizeof("ssid=");
                while (*p1 != '"' && p2 != ssid + sizeof(ssid) - 1)
                    *p2++ = *p1++;
                *p2 = '\0';
            }
            wpa_get_ssid(ssid, ssid_unicode, sizeof(ssid_unicode));
            hl_sm_send_event(wlan->connect_sm, WLAN_SM_EVENT_ID_CONNECT_WRONG_KEY, ssid_unicode, sizeof(ssid_unicode));
        }
        else if (strstr(reply, WPA_EVENT_TEMP_DISABLED) && strstr(reply, "reason=CONN_FAILED"))
        {
            hl_sm_send_event(wlan->connect_sm, WLAN_SM_EVENT_ID_CONNECT_CONNECT_FAILED, NULL, 0);
        }
        else if (strstr(reply, WPA_EVENT_NETWORK_NOT_FOUND))
        {
            hl_sm_send_event(wlan->connect_sm, WLAN_SM_EVENT_ID_CONNECT_NETWORK_NOT_FOUND, NULL, 0);
        }
    }
}

static int wpa_simple_cmd(wlan_t *wlan, char *reply, size_t *reply_len, const char *cmd)
{
    LOG_D("wpa cmd send: %s\n", cmd);
    int ret = wpa_ctrl_request(wlan->command, cmd, strlen(cmd), reply, reply_len, NULL);
    LOG_D("wpa cmd recv(%d): %s\n", (int)(*reply_len), reply); 
    return ret;
}

static int wpa_cmd(wlan_t *wlan, char *reply, size_t *reply_len, const char *cmd, ...)
{
    int ret;
    char cmdbuf[4096];
    va_list ap;
    va_start(ap, cmd);
    vsnprintf(cmdbuf, sizeof(cmdbuf), cmd, ap);
    va_end(ap);

    LOG_D("wpa cmd send: %s\n", cmdbuf);
    ret = wpa_ctrl_request(wlan->command, cmdbuf, strlen(cmdbuf), reply, reply_len, NULL);
    LOG_D("wpa cmd recv(%d): %s\n", (int)(*reply_len), reply); 
    return ret;
}

static int wpa_connect(wlan_t *wlan, const wlan_connection_entry_t *entry)
{
    char reply[128];
    size_t reply_len;

    int network_id = wpa_add_network(wlan, entry);
    if (network_id == -1)
    {
        LOG_E("wpa_add_network failed\n");
        return -1;
    }

    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "SELECT_NETWORK %d", network_id) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_E("select_network failed\n");
        return -1;
    }
    LOG_I("wpa_connect %s %s %d\n", entry->ssid, entry->psk, network_id);
    return 0;
}

static int wpa_add_network(wlan_t *wlan, const wlan_connection_entry_t *entry)
{
    char reply[128];
    size_t reply_len;
    int network_id;

    // LOG_I("wpa_add_network %s %s\n", entry->ssid, entry->psk);

    const char *key_mgmt;
    if (entry->key_mgmt == HL_WLAN_KEY_MGMT_NONE)
        key_mgmt = "NONE";
    else if (entry->key_mgmt == HL_WLAN_KEY_MGMT_WPA_PSK || entry->key_mgmt == HL_WLAN_KEY_MGMT_WPA2_PSK || entry->key_mgmt == HL_WLAN_KEY_MGMT_WPA3_PSK)
        key_mgmt = "WPA-PSK";
    else if (entry->key_mgmt == HL_WLAN_KEY_MGMT_WPA_EAP)
        key_mgmt = "WPA-EAP";
    else
        return -1;

    network_id = wpa_find_network_id(wlan, entry->ssid);
    if (network_id == -1)
    {
        reply_len = sizeof(reply);
        if (wpa_simple_cmd(wlan, reply, &reply_len, "ADD_NETWORK") == 0)
        {
            network_id = strtoul(reply, NULL, 10);
        }
        else
        {
            LOG_I("add_network failed\n");
            return -1;
        }
    }
    else
    {
        LOG_I("find network id %d\n", network_id);
    }

    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d ssid \"%s\"", network_id, entry->ssid) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("set_network ssid failed\n");
        return -1;
    }

    if (entry->key_mgmt != HL_WLAN_KEY_MGMT_NONE)
    {
        reply_len = sizeof(reply);
        if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d psk \"%s\"", network_id, entry->psk) || strncmp(reply, "OK", reply_len - 1))
        {
            LOG_I("set_network psk failed\n");
            return -1;
        }
    }

    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d key_mgmt %s", network_id, key_mgmt) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("set_network key_mgmt failed\n");
        return -1;
    }

    // 记录关机前连接的网络，将此网络设置为高优先级连接
    if (strncmp(entry->ssid, get_sysconf()->GetString("system", "ssid_connected", "").c_str(), sizeof(entry->ssid)) == 0)
    {
        LOG_I("ssid : %s - %s %d\n", entry->ssid, get_sysconf()->GetString("system", "ssid_connected", "").c_str(), 5);
        reply_len = sizeof(reply);
        if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d priority 5", network_id) || strncmp(reply, "OK", reply_len - 1))
        {
            LOG_I("set_network priority failed\n");
            return -1;
        }
    }
    else
    {
        LOG_I("ssid : %s - %s %d\n", entry->ssid, get_sysconf()->GetString("system", "ssid_connected", "").c_str(), 1);

        reply_len = sizeof(reply);
        if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d priority 1", network_id) || strncmp(reply, "OK", reply_len - 1))
        {
            LOG_I("set_network priority failed\n");
            return -1;
        }
    }


    if(entry->key_mgmt == HL_WLAN_KEY_MGMT_WPA3_PSK)
    {
        reply_len = sizeof(reply);
        if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d pairwise CCMP", network_id) || strncmp(reply, "OK", reply_len - 1))
        {
            LOG_I("set_network pairwise CCMP failed\n");
            return -1;
        }
    
        reply_len = sizeof(reply);
        if (wpa_cmd(wlan, reply, &reply_len, "SET_NETWORK %d proto RSN", network_id) || strncmp(reply, "OK", reply_len - 1))
        {
            LOG_I("set_network proto RSN failed\n");
            return -1;
        }
    }

    return network_id;
}

static int wpa_enable_network(wlan_t *wlan, int network_id)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "ENABLE_NETWORK %d", network_id) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("enable_network %d failed\n", network_id);
        return -1;
    }
    return 0;
}

static int wpa_disable_network(wlan_t *wlan, int network_id)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "DISABLE_NETWORK %d", network_id) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("disable_network %d failed\n", network_id);
        return -1;
    }
    return 0;
}

static int wpa_enable_all_network(wlan_t *wlan)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "ENABLE_NETWORK all") || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("enable_network all failed\n");
        return -1;
    }
    return 0;
}

static int wpa_disable_all_network(wlan_t *wlan)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "DISABLE_NETWORK all") || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("disable_network all failed\n");
        return -1;
    }
    return 0;
}

static int wpa_remove_network(wlan_t *wlan, int network_id)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_cmd(wlan, reply, &reply_len, "REMOVE_NETWORK %d", network_id) || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("remove_network %d failed\n", network_id);
        return -1;
    }
    return 0;
}

static int wpa_remove_all_network(wlan_t *wlan)
{
    char reply[128];
    size_t reply_len;
    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "REMOVE_NETWORK all") || strncmp(reply, "OK", reply_len - 1))
    {
        LOG_I("remove_network all failed\n");
        return -1;
    }
    return 0;
}

static int wpa_find_network_id(wlan_t *wlan, const char *ssid)
{
    char reply[1024];
    size_t reply_len;
    char line[512];
    int line_idx = 0;
    int pos_network_id = -1, pos_ssid = -1;
    char *saveptr;
    char *token;
    char *line_saveptr;

    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "LIST_NETWORKS") != 0)
        return -1;

    line_saveptr = reply;

    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (line_idx)
        {
            token = strtok_r(line, "\t", &saveptr);
            int pos = 0;
            char networks_ssid[HL_WLAN_SSID_MAXLENGTH + 1] = {0};
            int network_id = -1;
            while (token != NULL)
            {
                if (pos == pos_network_id)
                    network_id = strtol(token, NULL, 10);
                else if (pos == pos_ssid)
                    wpa_get_ssid(token, networks_ssid, sizeof(networks_ssid));
                pos++;
                token = strtok_r(NULL, "\t", &saveptr);
            }
            if (strcmp(networks_ssid, ssid) == 0)
                return network_id;
        }
        else
        {
            token = strtok_r(line, "/", &saveptr);
            int pos = 0;
            while (token != NULL)
            {
                if (wpa_has_token(token, "network id"))
                    pos_network_id = pos;
                else if (wpa_has_token(token, "ssid"))
                    pos_ssid = pos;
                pos++;
                token = strtok_r(NULL, "/", &saveptr);
            }
        }
        line_idx++;
    }

    return -1;
}

static int wpa_get_current_ssid(wlan_t *wlan, char *ssid, uint32_t ssid_len)
{
    char reply[1024];
    size_t reply_len;
    char *saveptr;
    char *token;
    char *line_saveptr;
    char line[512];

    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "STATUS") != 0)
        return -1;

    line_saveptr = reply;
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (strstr(line, "ssid=") == line)
        {
            wpa_get_ssid(line + sizeof("ssid=") - 1, ssid, ssid_len);
            return 0;
        }
    }
    return -1;
}

static int wpa_get_current_bssid(wlan_t *wlan, char *bssid, uint32_t bssid_len)
{
    char reply[1024];
    size_t reply_len;
    char *saveptr;
    char *token;
    char *line_saveptr;
    char line[512];

    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "STATUS") != 0)
        return -1;

    line_saveptr = reply;
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (strstr(line, "bssid=") == line)
        {
            strncpy(bssid, line + sizeof("bssid=") - 1, bssid_len);
            return 0;
        }
    }
    return -1;
}

static int wpa_signal_poll(wlan_t *wlan, int *signal)
{
    char reply[1024];
    size_t reply_len;
    char *saveptr;
    char *token;
    char *line_saveptr;
    char line[512];
    reply_len = sizeof(reply);
    if (wpa_simple_cmd(wlan, reply, &reply_len, "SIGNAL_POLL") != 0)
        return -1;

    line_saveptr = reply;
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (strstr(line, "RSSI=") == line)
        {
            *signal = wpa_get_signal(line + sizeof("RSSI=") - 1);
            return 0;
        }
    }
    return -1;
}

static int wpa_process_start(const char *interface)
{
    int retry = 0;
    hl_system("ifconfig %s up", interface);
    if (hl_system("wpa_supplicant -D nl80211,wext -C %s -B -i %s", WPA_CTRL_PATH, interface) != 0)
    {
        LOG_I("wpa_process_start failed\n");
        return -1;
    }
    while (access(WPA_CTRL_PATH, F_OK) != 0)
    {
        LOG_I("wpa_process_start failed retry %d\n", retry);
        usleep(100000);
        if (retry++ > 5)
            return -1;
    }
    return 0;
}

static void wpa_process_stop(void)
{
    int retry = 0;
    while (access(WPA_CTRL_PATH, F_OK) == 0)
    {
        hl_system("killall wpa_supplicant");
        usleep(100000);
        if (retry++ > 5)
            return;
    }
}

static int wpa_get_signal(const char *signal_level)
{
#define MIN_RSSI -100
#define MAX_RSSI -55
    int level = strtol(signal_level, NULL, 10);

    if (level < 0)
    {
        if (level < MIN_RSSI)
            level = MAX_RSSI;
        else if (level > MAX_RSSI)
            level = MAX_RSSI;
        level = (level - MIN_RSSI) * 100 / (MAX_RSSI - MIN_RSSI);
    }
    return level;
}

static int wpa_get_key_mgmt(const char *flags)
{
    if (strstr(flags, "WPA-PSK"))
        return HL_WLAN_KEY_MGMT_WPA_PSK;
    else if (strstr(flags, "WPA2-PSK"))
        return HL_WLAN_KEY_MGMT_WPA2_PSK;
    else if (strstr(flags, "WPA2--CCMP"))
        return HL_WLAN_KEY_MGMT_WPA3_PSK;
    else if (strstr(flags, "WPA-EAP"))
        return HL_WLAN_KEY_MGMT_WPA_EAP;
    else
        return HL_WLAN_KEY_MGMT_NONE;
}

static int wpa_get_ssid(char *ssid, char *ssid_unicode, uint32_t size)
{
    size_t src_len = strlen(ssid);
    size_t dst_len = 0;
    char hex[3] = {0};
    for (int i = 0; i < src_len;)
    {
        if (src_len - i >= 4 && ssid[i] == '\\' && ssid[i + 1] == 'x')
        {
            hex[0] = ssid[i + 2];
            hex[1] = ssid[i + 3];
            ssid_unicode[dst_len++] = strtoul(hex, NULL, 16);
            i += 4;
        }
        else
        {
            ssid_unicode[dst_len++] = ssid[i];
            i++;
        }
    }
    ssid_unicode[dst_len] = '\0';
    return 0;
}

static int wpa_has_token(char *token, const char *key)
{
    while (*token && *token == ' ')
        token++;
    return strstr(token, key) == token;
}

static int wpa_scan_result_sort_by_ssid(const void *p1, const void *p2)
{
    hl_wlan_connection_t **pc1 = (hl_wlan_connection_t **)p1;
    hl_wlan_connection_t **pc2 = (hl_wlan_connection_t **)p2;
    return strcmp((*pc1)->ssid, (*pc2)->ssid) > 0 ? -1 : 1;
}

static int wpa_scan_result_sort_by_signal(const void *p1, const void *p2)
{
    hl_wlan_connection_t **pc1 = (hl_wlan_connection_t **)p1;
    hl_wlan_connection_t **pc2 = (hl_wlan_connection_t **)p2;
    return (*pc1)->signal - (*pc2)->signal > 0 ? -1 : 1;
}

static int wpa_scan_result_update(char *reply, size_t reply_len, hl_wlan_connection_t *connections)
{
    char line[512];
    int line_idx = 0;
    int pos_bssid = -1, pos_frequency = -1, pos_signal = -1, pos_flags = -1, pos_ssid = -1;
    char *saveptr;
    char *token;
    char *line_saveptr;
    int numbers = 0;
    line_idx = 0;
    line_saveptr = reply;
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (line_idx && numbers < HL_WLAN_SCAN_RESULT_BUFFER_SIZE)
        {
            token = strtok_r(line, "\t", &saveptr);
            int pos = 0;
            memset(&connections[numbers], 0, sizeof(connections[numbers]));
            while (token != NULL)
            {
                if (pos == pos_bssid)
                    strncpy(connections[numbers].bssid, token, sizeof(connections[numbers].bssid));
                else if (pos == pos_frequency)
                    connections[numbers].frequency = strtoul(token, NULL, 10);
                else if (pos == pos_signal)
                    connections[numbers].signal = wpa_get_signal(token);
                else if (pos == pos_flags)
                    connections[numbers].key_mgmt = (hl_wlan_key_mgmt_t)wpa_get_key_mgmt(token);
                else if (pos == pos_ssid)
                    wpa_get_ssid(token, connections[numbers].ssid, sizeof(connections[numbers].ssid));
                else
                {
                    LOG_I("unknown pos %d:%s\n", pos, token);
                }
                pos++;
                token = strtok_r(NULL, "\t", &saveptr);
            }
            numbers++;
        }
        else
        {
            token = strtok_r(line, "/", &saveptr);
            int pos = 0;
            while (token != NULL)
            {
                if (wpa_has_token(token, "bssid"))
                    pos_bssid = pos;
                else if (wpa_has_token(token, "frequency"))
                    pos_frequency = pos;
                else if (wpa_has_token(token, "signal level"))
                    pos_signal = pos;
                else if (wpa_has_token(token, "flags"))
                    pos_flags = pos;
                else if (wpa_has_token(token, "ssid"))
                    pos_ssid = pos;
                pos++;
                token = strtok_r(NULL, "/", &saveptr);
            }
        }
        line_idx++;
    }

#define PRINT_WIFI_RESULT
#ifdef PRINT_WIFI_RESULT
    LOG_D("result ssids(%d):\n", numbers);
    for (int i = 0; i < numbers; i++)
    {
        LOG_D("rssid(%2d): %s\n", i+1, connections[i].ssid);
    }

#endif

    return numbers;
}

static int wlan_init(wlan_t *wlan, const char *interface)
{
    memset(wlan, 0, sizeof(*wlan));

    if (hl_sm_create(&wlan->scan_sm, wlan_scan_idle, NULL, 0, 64, wlan) != 0)
        return -1;
    if (hl_sm_create(&wlan->connect_sm, local_wlan_enable ? wlan_connect_disconnected : wlan_connect_disable, NULL, 0, 64, wlan) != 0)
    {
        hl_sm_destroy(&wlan->scan_sm);
        return -1;
    }

    if (wpa_client_init(wlan, interface) != 0)
    {
        hl_sm_destroy(&wlan->connect_sm);
        hl_sm_destroy(&wlan->scan_sm);
        LOG_I("wpa_client init failed\n");
        return -1;
    }
    strncpy(wlan_interface, interface, sizeof(wlan_interface));
    return 0;
}

static void wlan_deinit(wlan_t *wlan)
{
    wpa_client_deinit(wlan);
    hl_sm_destroy(&wlan->connect_sm);
    hl_sm_destroy(&wlan->scan_sm);
}

typedef struct
{
    const char *ssid;
    int index;
    int found;
} db_find_ctx_t;

int db_find_callback(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args)
{
    wlan_connection_entry_t *connection = (wlan_connection_entry_t *)data;
    db_find_ctx_t *ctx = (db_find_ctx_t *)args;
    if (strncmp(connection->ssid, ctx->ssid, sizeof(connection->ssid)) == 0)
    {
        ctx->index = index;
        ctx->found = 1;
        return 1;
    }
    return 0;
}

int db_append_callback(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args)
{
    wlan_connection_entry_t *connection = (wlan_connection_entry_t *)data;
    wlan_t *wlan = (wlan_t *)args;
    LOG_I("append ssid %s psk %s\n", connection->ssid, connection->psk);
    wpa_add_network(wlan, connection);
    return 0;
}

static int wlan_db_append_entry(wlan_connection_entry_t *connection)
{
    LOG_I("wlan_db_append_entry %s %s\n", connection->ssid, connection->psk);
    hl_ringbuffer_t rb = hl_ringbuffer_db_get(db);
    db_find_ctx_t ctx;
    ctx.found = 0;
    ctx.ssid = connection->ssid;
    pthread_rwlock_wrlock(&db_lock);
    hl_ringbuffer_foreach_reverse(rb, db_find_callback, &ctx);
    if (ctx.found)
    {
        wlan_connection_entry_t tmp;
        hl_ringbuffer_get(rb, ctx.index, &tmp);
        if (strcmp(tmp.ssid, connection->ssid) == 0 && strcmp(tmp.psk, connection->psk) == 0 && tmp.key_mgmt == connection->key_mgmt)
            hl_ringbuffer_del(rb, ctx.index);
    }
    hl_ringbuffer_push(rb, connection);
    hl_ringbuffer_db_sync(db);
    pthread_rwlock_unlock(&db_lock);
    return 0;
}

int wlan_db_remove_entry(const char *ssid)
{
    HL_ASSERT(ssid != NULL);

    LOG_I("wlan_db_remove_entry: %s\n", ssid);
    hl_ringbuffer_t rb = hl_ringbuffer_db_get(db);
    db_find_ctx_t ctx;
    ctx.found = 0;
    ctx.ssid = ssid;
    pthread_rwlock_wrlock(&db_lock);
    hl_ringbuffer_foreach_reverse(rb, db_find_callback, &ctx);
    if (ctx.found)
    {
        hl_ringbuffer_del(rb, ctx.index);
        hl_ringbuffer_db_sync(db);
        pthread_rwlock_unlock(&db_lock);

        return 0;
    }
    pthread_rwlock_unlock(&db_lock);
    return -1;
}

static int wlan_db_find_entry(const char *ssid, wlan_connection_entry_t *connection)
{
    hl_ringbuffer_t rb = hl_ringbuffer_db_get(db);
    db_find_ctx_t ctx;
    ctx.found = 0;
    ctx.ssid = ssid;
    pthread_rwlock_rdlock(&db_lock);
    hl_ringbuffer_foreach_reverse(rb, db_find_callback, &ctx);

    if (ctx.found)
    {
        hl_ringbuffer_get(rb, ctx.index, connection);
        pthread_rwlock_unlock(&db_lock);
        return 0;
    }
    pthread_rwlock_unlock(&db_lock);
    return -1;
}

static int wlan_db_is_exists(const char *ssid)
{
    hl_ringbuffer_t rb = hl_ringbuffer_db_get(db);
    db_find_ctx_t ctx;
    ctx.found = 0;
    ctx.ssid = ssid;
    pthread_rwlock_rdlock(&db_lock);
    hl_ringbuffer_foreach_reverse(rb, db_find_callback, &ctx);
    pthread_rwlock_unlock(&db_lock);
    return ctx.found;
}

static int wlan_db_dump_to_network(wlan_t *wlan)
{
    hl_ringbuffer_t rb = hl_ringbuffer_db_get(db);
    pthread_rwlock_rdlock(&db_lock);
    hl_ringbuffer_foreach_reverse(rb, db_append_callback, wlan);
    pthread_rwlock_unlock(&db_lock);
    return 0;
}

static void wlan_routine(hl_tpool_thread_t thread, void *args)
{
    wlan_thread_msg_t thread_msg;
    wlan_t *wlan = (wlan_t *)args;
    int is_exists = 0;
    hl_wlan_event_t event;
    uint64_t ticks = hl_get_tick_ms();
    int failed_count = 0;
    for (;;)
    {
        if (hl_tpool_thread_recv_msg_try(thread, &thread_msg) == 0)
        {
            if (thread_msg.append)
            {
                pthread_rwlock_wrlock(&glock);
                if (wlan_status == HL_WLAN_STATUS_NOT_EXISTS)
                {
                    if (wlan_init(wlan, thread_msg.interface) == 0)
                    {
                        LOG_I("wlan init\n");
                        is_exists = 1;
                    }
                    else
                    {
                        LOG_I("wlan init failed\n");
                    }
                }
                else
                {
                    LOG_I("wlan already exists\n");
                }
                pthread_rwlock_unlock(&glock);
            }
            else
            {
                pthread_rwlock_wrlock(&glock);
                if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS && strncmp(wlan_interface, thread_msg.interface, sizeof(wlan_interface)) == 0)
                {
                    wlan_deinit(wlan);
                    is_exists = 0;
                    LOG_I("wlan deinit\n");
                }
                wlan_status = HL_WLAN_STATUS_NOT_EXISTS;
                pthread_rwlock_unlock(&glock);

                event.id = HL_WLAN_EVENT_STATUS_CHANGED;
                strncpy(event.interface, wlan_interface, sizeof(event.interface));
                hl_callback_call(wlan_callback, &event);
            }
        }

        if (is_exists == 0)
        {
            // 超过3S主动扫描
            if (hl_tick_is_overtime(ticks, hl_get_tick_ms(), 3000))
            {
                ticks = hl_get_tick_ms();
                wlan_interface_scan();
                // failed_count++;
                // if (failed_count > 2)
                // {
                //     failed_count = 0;
                //     // 板载WIFI重启
                //     if (local_wlan_onboard)
                //     {
                //         usb_wifi_power_ctrl(0, 0);
                //         usleep(100000);
                //         usb_wifi_power_ctrl(0, 1);
                //     }
                // }
            }
            usleep(50000);
            continue;
        }

        wpa_monitor_poll(wlan);
        hl_sm_dispatch_event(wlan->scan_sm);
        hl_sm_dispatch_event(wlan->connect_sm);

        usleep(50000);
    }
}

static void wlan_scan_idle(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    wlan_t* wlan = (wlan_t*)hl_sm_get_user_data(sm);
    char reply[4096];
    size_t reply_len;
    int retry = 0;
    hl_wlan_event_t event;
    static uint64_t scan_time_ticks = 0;

    LOG_WLAN_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        if (hl_tick_is_overtime(scan_time_ticks, hl_get_tick_ms(), 20000) &&
            local_wlan_periodic_scan == 1) // 定时扫描()
        {
            if (wlan_status != HL_WLAN_STATUS_NOT_EXISTS && wlan_status != HL_WLAN_STATUS_DISABLE)
            {
                hl_wlan_scan();
            }
            scan_time_ticks = hl_get_tick_ms();
        }
        break;
    case HL_SM_EVENT_ID_ENTRY:
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case WLAN_SM_EVENT_ID_SCAN_REQUEST:
        LOG_I("scan requested\n");
        do
        {
            reply_len = sizeof(reply);
            if (wpa_simple_cmd(wlan, reply, &reply_len, "SCAN") == 0)
            {
                if (strncmp(reply, "OK", reply_len - 1) == 0)
                    break;
                else if (strncmp(reply, "FAIL-BUSY", reply_len - 1) == 0)
                    break;
            }
        } while (++retry < WPA_SCAN_REQUEST_RETRY);

        if (retry == WPA_SCAN_REQUEST_RETRY)
        {
            event.id = HL_WLAN_EVENT_SCAN_FAILED_REQUEST;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
        }
        else
        {
            hl_sm_trans_state(sm, wlan_scan_scanning, NULL, 0);
        }
        break;
    case WLAN_SM_EVENT_ID_SCAN_RESULTS:
        reply_len = sizeof(reply);
        if (wpa_simple_cmd(wlan, reply, &reply_len, "SCAN_RESULTS") == 0)
        {
            pthread_rwlock_wrlock(&glock);
            wlan->scan_result_numbers = wpa_scan_result_update(reply, reply_len, wlan->scan_result);
            pthread_rwlock_unlock(&glock);

            event.id = HL_WLAN_EVENT_SCAN_SUCCEEDED;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
        }
        break;
    }
}

static void wlan_scan_scanning(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    wlan_t* wlan = (wlan_t*)hl_sm_get_user_data(sm);
    char reply[4096];
    size_t reply_len;
    hl_wlan_event_t event;

    LOG_WLAN_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        if (hl_tick_is_overtime(wlan->scanning_ticks, hl_get_tick_ms(), local_scan_timeout))
        {
            event.id = HL_WLAN_EVENT_SCAN_FAILED_TIMEOUT;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
            hl_sm_trans_state(sm, wlan_scan_idle, NULL, 0);
        }
        break;
    case HL_SM_EVENT_ID_ENTRY:
        wlan->scanning_ticks = hl_get_tick_ms();
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case WLAN_SM_EVENT_ID_SCAN_FAILED:
        event.id = HL_WLAN_EVENT_SCAN_FAILED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);
        hl_sm_trans_state(sm, wlan_scan_idle, NULL, 0);
        break;
    case WLAN_SM_EVENT_ID_SCAN_RESULTS:
        reply_len = sizeof(reply);
        if (wpa_simple_cmd(wlan, reply, &reply_len, "SCAN_RESULTS") == 0)
        {
            pthread_rwlock_wrlock(&glock);
            wlan->scan_result_numbers = wpa_scan_result_update(reply, reply_len, wlan->scan_result);
            pthread_rwlock_unlock(&glock);

            event.id = HL_WLAN_EVENT_SCAN_SUCCEEDED;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
        }
        else
        {
            LOG_I("wpa_scan get scan result failed\n");
            event.id = HL_WLAN_EVENT_SCAN_FAILED;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
        }
        hl_sm_trans_state(sm, wlan_scan_idle, NULL, 0);
        break;
    }
}

static void wlan_connect_disable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    wlan_t* wlan = (wlan_t*)hl_sm_get_user_data(sm);
    hl_wlan_event_t event;

    LOG_WLAN_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        break;
    case HL_SM_EVENT_ID_ENTRY:
        wpa_disable_all_network(wlan);

        pthread_rwlock_wrlock(&glock);
        wlan_status = HL_WLAN_STATUS_DISABLE;
        pthread_rwlock_unlock(&glock);

        event.id = HL_WLAN_EVENT_STATUS_CHANGED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case WLAN_SM_EVENT_ID_CONNECT_ENABLE:
        LOG_I("#########WLAN_SM_EVENT_ID_CONNECT_ENABLE#########\n");
        hl_sm_trans_state(sm, wlan_connect_disconnected, NULL, 0);
        break;
    }
}

static void wlan_connect_disconnected(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    wlan_t* wlan = (wlan_t*)hl_sm_get_user_data(sm);
    hl_wlan_event_t event;
    wlan_connection_entry_t *connection_entry;
    int network_id;

    LOG_WLAN_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        if (wlan->connecting_ticks > 0 && hl_tick_is_overtime(wlan->connecting_ticks, hl_get_tick_ms(), local_connect_timeout))
        {
            wlan->connecting_ticks = 0;
            event.id = HL_WLAN_EVENT_CONNECT_FAILED_TIMEOUT;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);

            // 删除密码记录
            // if (strlen(wlan->connection_entry.ssid) > 0)
            //     wlan_db_remove_entry(wlan->connection_entry.ssid);

            // 超时后自动开启自动重连功能
            wlan_db_dump_to_network(wlan);
            wpa_enable_all_network(wlan);
        }
        break;
    case HL_SM_EVENT_ID_ENTRY:
        pthread_rwlock_wrlock(&glock);
        wlan_status = HL_WLAN_STATUS_DISCONNECTED;
        pthread_rwlock_unlock(&glock);

        event.id = HL_WLAN_EVENT_STATUS_CHANGED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);

        if (event_data != NULL)
        {
            hl_sm_send_event(sm, WLAN_SM_EVENT_ID_CONNECT_CONNECT_REQUEST, event_data, event_data_size);
        }
        else
        {
            wpa_remove_all_network(wlan);
            wlan_db_dump_to_network(wlan);
            wpa_enable_all_network(wlan);
        }
        wlan->connecting_ticks = 0;
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case WLAN_SM_EVENT_ID_CONNECT_CONNECTED:
        hl_sm_trans_state(sm, wlan_connect_connected, NULL, 0);
        break;
    case WLAN_SM_EVENT_ID_CONNECT_DISABLE:
        hl_sm_trans_state(sm, wlan_connect_disable, NULL, 0);
        break;
    case WLAN_SM_EVENT_ID_CONNECT_CONNECT_REQUEST:
        connection_entry = (wlan_connection_entry_t *)event_data;
        wpa_remove_all_network(wlan);
        if (wpa_connect(wlan, connection_entry) == 0)
        {
            memcpy(&wlan->connection_entry, connection_entry, sizeof(wlan->connection_entry));
            wlan->connecting_ticks = hl_get_tick_ms();
        }
        else
        {
            event.id = HL_WLAN_EVENT_CONNECT_FAILED_REQUEST;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
        }
        break;
    case WLAN_SM_EVENT_ID_CONNECT_TEMP_DISABLED:
    case WLAN_SM_EVENT_ID_CONNECT_WRONG_KEY:
    {
        event.id = HL_WLAN_EVENT_CONNECT_FAILED_WRONG_PASSWORD;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);

        // here delete the ssid from db when connection failed because of wrong passwd
        const char* ssid = (const char*)event_data;
        if (strlen(ssid) > 0)
        {
            LOG_I("wifi association failed. remove ssid:%s\n", ssid);
            wlan_db_remove_entry(ssid);
        }
        // 复位连接超时
        wlan->connecting_ticks = 0;
        // 从网络列表中删除连接并开启自动连接
        wpa_remove_all_network(wlan);
        wlan_db_dump_to_network(wlan);
        wpa_enable_all_network(wlan);
    }
        break;
    case WLAN_SM_EVENT_ID_CONNECT_CONNECT_FAILED:
        event.id = HL_WLAN_EVENT_CONNECT_FAILED_CONNECT_FAILED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);
        break;
    case WLAN_SM_EVENT_ID_CONNECT_NETWORK_NOT_FOUND:
        if (wlan->connecting_ticks == 0)
        {
            event.id = HL_WLAN_EVENT_CONNECT_FAILED_NOT_FOUND;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);
            // 设置超时时间
            wlan->connecting_ticks = hl_get_tick_ms();
        }
        break;
    }
}

static void wlan_connect_connected(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    wlan_t* wlan = (wlan_t*)hl_sm_get_user_data(sm);
    hl_wlan_event_t event;
    wlan_connection_entry_t *connection_entry;
    char ssid[HL_WLAN_SSID_MAXLENGTH + 1];

    LOG_WLAN_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        // 防止网络波动
        if (wlan->disconnect_ticks > 0 && hl_tick_is_overtime(wlan->disconnect_ticks, hl_get_tick_ms(), local_reconnect_timeout))
        {
            wlan->disconnect_ticks = 0;
            hl_sm_trans_state(sm, wlan_connect_disconnected, NULL, 0);
        }

        if (hl_tick_is_overtime(wlan->poll_ticks, hl_get_tick_ms(), 3000))
        {
            int signal;
            int changed = 0;
            wpa_signal_poll(wlan, &signal);
            pthread_rwlock_wrlock(&glock);
            if (signal != wlan->signal)
            {
                changed = 1;
                wlan->signal = signal;
            }
            pthread_rwlock_unlock(&glock);

            if (changed)
            {
                event.id = HL_WLAN_EVENT_SIGNAL_CHANGED;
                strncpy(event.interface, wlan_interface, sizeof(event.interface));
                hl_callback_call(wlan_callback, &event);
            }
            wlan->poll_ticks = hl_get_tick_ms();
        }
        break;
    case HL_SM_EVENT_ID_ENTRY:
        pthread_rwlock_wrlock(&glock);
        wlan_status = HL_WLAN_STATUS_CONNECTED;
        pthread_rwlock_unlock(&glock);
        event.id = HL_WLAN_EVENT_STATUS_CHANGED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);
        event.id = HL_WLAN_EVENT_SIGNAL_CHANGED;
        strncpy(event.interface, wlan_interface, sizeof(event.interface));
        hl_callback_call(wlan_callback, &event);

        wlan->disconnect_ticks = 0;
        wlan->poll_ticks = hl_get_tick_ms();

        if (wpa_get_current_ssid(wlan, ssid, sizeof(ssid)) == 0 && strcmp(ssid, wlan->connection_entry.ssid) == 0)
        {
            wlan_db_append_entry(&wlan->connection_entry);
        }
        else
        {
            LOG_I("current ssid not request ssid\n");
        }
        LOG_I("wlan connected bssid %s ssid %s\n", wlan->connection.bssid, wlan->connection.ssid);
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case WLAN_SM_EVENT_ID_CONNECT_CONNECTED:
        // 重连
        wlan->disconnect_ticks = 0;
        if (strcmp(wlan->disconnect_connection.bssid, wlan->connection.bssid) == 0)
        {
            LOG_I("reconnected same bssid\n");
        }
        else if (strcmp(wlan->disconnect_connection.ssid, wlan->connection.ssid) == 0)
        {
            LOG_I("reconnected same ssid\n");
        }
        else
        {
            pthread_rwlock_wrlock(&glock);
            wlan_status = HL_WLAN_STATUS_DISCONNECTED;
            pthread_rwlock_unlock(&glock);
            event.id = HL_WLAN_EVENT_STATUS_CHANGED;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);

            pthread_rwlock_wrlock(&glock);
            wlan_status = HL_WLAN_STATUS_CONNECTED;
            pthread_rwlock_unlock(&glock);
            event.id = HL_WLAN_EVENT_STATUS_CHANGED;
            strncpy(event.interface, wlan_interface, sizeof(event.interface));
            hl_callback_call(wlan_callback, &event);

            if (wpa_get_current_ssid(wlan, ssid, sizeof(ssid)) == 0 && strcmp(ssid, wlan->connection_entry.ssid) == 0)
                wlan_db_append_entry(&wlan->connection_entry);
        }
        break;
    case WLAN_SM_EVENT_ID_CONNECT_DISCONNECTED:
        wlan->disconnect_ticks = hl_get_tick_ms();
        wlan->disconnect_connection = wlan->connection;
        break;
    case WLAN_SM_EVENT_ID_CONNECT_DISABLE:
        hl_sm_trans_state(sm, wlan_connect_disable, NULL, 0);
        break;
    case WLAN_SM_EVENT_ID_CONNECT_CONNECT_REQUEST:
        hl_sm_trans_state(sm, wlan_connect_disconnected, event_data, event_data_size);
        break;
    case WLAN_SM_EVENT_ID_CONNECT_DISCONNECT_REQUEST:
        hl_sm_trans_state(sm, wlan_connect_disconnected, NULL, 0);
        // wlan_db_remove_entry(wlan->connection.ssid);
        break;
    }
}
