#ifndef HL_WLAN_H
#define HL_WLAN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if.h>

#include "hl_callback.h"

#define HL_WLAN_SSID_MINLENGTH 1
#define HL_WLAN_SSID_MAXLENGTH 32
#define HL_WLAN_PSK_MINLENGTH 8
#define HL_WLAN_PSK_MAXLENGTH 63
#define HL_WLAN_SCAN_RESULT_BUFFER_SIZE 64

    typedef enum
    {
        HL_WLAN_KEY_MGMT_NONE,
        HL_WLAN_KEY_MGMT_WPA_PSK,
        HL_WLAN_KEY_MGMT_WPA2_PSK,
        HL_WLAN_KEY_MGMT_WPA3_PSK,
        HL_WLAN_KEY_MGMT_WPA_EAP,
    } hl_wlan_key_mgmt_t;

    typedef enum
    {
        HL_WLAN_STATUS_NOT_EXISTS,   // 不存在
        HL_WLAN_STATUS_DISABLE,      // 禁用
        HL_WLAN_STATUS_DISCONNECTED, // 未连接
        HL_WLAN_STATUS_CONNECTED,    // 已连接
    } hl_wlan_status_t;
    typedef enum
    {
        HL_WLAN_EVENT_STATUS_CHANGED, // 状态变化
        HL_WLAN_EVENT_SIGNAL_CHANGED, // 信号变化

        HL_WLAN_EVENT_SCAN_SUCCEEDED,      // 扫描成功
        HL_WLAN_EVENT_SCAN_FAILED,         // 扫描失败
        HL_WLAN_EVENT_SCAN_FAILED_REQUEST, // 扫描失败,请求失败
        HL_WLAN_EVENT_SCAN_FAILED_TIMEOUT, // 扫描失败,扫描超时

        HL_WLAN_EVENT_CONNECT_FAILED_REQUEST,        // 连接失败，请求失败
        HL_WLAN_EVENT_CONNECT_FAILED_TIMEOUT,        // 连接失败，连接超时
        HL_WLAN_EVENT_CONNECT_FAILED_WRONG_PASSWORD, // 连接失败，密码错误
        HL_WLAN_EVENT_CONNECT_FAILED_CONNECT_FAILED, // 连接失败
        HL_WLAN_EVENT_CONNECT_FAILED_NOT_FOUND,      // 连接失败，查找失败
    } hl_wlan_event_id_t;

    typedef struct
    {
        hl_wlan_event_id_t id;
        char interface[IFNAMSIZ];
    } hl_wlan_event_t;

    typedef struct
    {
        char ssid[HL_WLAN_SSID_MAXLENGTH + 1];
        char bssid[18];
        uint32_t frequency;
        uint8_t signal; // 0-100
        hl_wlan_key_mgmt_t key_mgmt;
    } hl_wlan_connection_t;

    void hl_wlan_init(int wlan_enable, uint64_t scan_timeout, uint64_t connect_timeout, uint64_t reconnect_timeout,
                      const char *entry_file_path, uint32_t entry_max, int onboard);
    int hl_wlan_set_enable(int enable);
    int hl_wlan_connect(const char *ssid, const char *psk, hl_wlan_key_mgmt_t key_mgmt);
    int hl_wlan_disconnect(void);
    hl_wlan_status_t hl_wlan_get_status(void);
    int hl_wlan_get_interface(char *interface);
    int hl_wlan_get_connection(hl_wlan_connection_t *connection);
    int hl_wlan_is_in_db(const char *ssid, hl_wlan_key_mgmt_t *key_mgmt);
    int hl_wlan_scan(void);
    int hl_wlan_is_scanning(void);
    int hl_wlan_get_scan_result(hl_wlan_connection_t *scan_result);
    int hl_wlan_get_scan_result_raw(hl_wlan_connection_t *scan_result);
    int hl_wlan_get_scan_result_numbers(void);
    int hl_wlan_get_signal(void);
    void hl_wlan_register_event_callback(hl_callback_function_t function, void *user_data);
    void hl_wlan_unregister_event_callback(hl_callback_function_t function, void *user_data);
    int hl_wlan_get_enable(void);
    int wlan_db_remove_entry(const char *ssid);
    int hl_wlan_enable_periodic_scan(void);
    int hl_wlan_disable_periodic_scan(void);

#ifdef __cplusplus
}
#endif

#endif
