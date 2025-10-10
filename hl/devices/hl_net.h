#ifndef HL_NET_H
#define HL_NET_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "hl_callback.h"

    typedef enum
    {
        HL_NET_INTERFACE_WLAN = 0,
        HL_NET_INTERFACE_ETH,
        HL_NET_INTERFACE_NUMBERS,
    } hl_net_interface_t;

    typedef enum
    {
        HL_NET_STATUS_NOT_EXISTS,
        HL_NET_STATUS_DISABLE,
        HL_NET_STATUS_ENABLE,
    } hl_net_status_t;

    typedef enum
    {
        HL_NET_MODE_STATIC,
        HL_NET_MODE_DHCP,
    } hl_net_mode_t;

    typedef enum
    {
        HL_NET_EVENT_STATUS_CHANGED,
        HL_NET_EVENT_DHCP_FAILED,
        HL_NET_EVENT_DHCP_SUCCEEDED,
        HL_NET_EVENT_LAN_STATUS_CHANGED,
        HL_NET_EVENT_WAN_STATUS_CHANGED,
    } hl_net_event_id_t;

    typedef struct
    {
        hl_net_event_id_t id;
        hl_net_interface_t interface;
    } hl_net_event_t;

    // NET
    void hl_net_init(int wlan_enable, hl_net_mode_t wlan_mode,
                     int eth_enable, hl_net_mode_t eth_mode,
                     const char *udhcpc_script_path, const char *udhcpc_tmp_path,
                     const char *dns_resolv_conf_path,
                     uint32_t lan_disconnect_threshold, uint64_t lan_detection_period,
                     uint32_t wan_disconnect_threshold, uint64_t wan_detection_period,
                     const char **wan_detection_url_table, uint32_t wan_detection_url_table_size,
                     const char *wlan_entry_path);
    int hl_net_lan_is_connected(void);
    int hl_net_wan_is_connected(void);
    void hl_net_register_event_callback(hl_callback_function_t function, void *user_data);
    void hl_net_unregister_event_callback(hl_callback_function_t function, void *user_data);

    // NETIF
    int hl_netif_set_enable(hl_net_interface_t interface, int enable);
    int hl_netif_set_mode(hl_net_interface_t interface, hl_net_mode_t mode);
    hl_net_status_t hl_netif_get_state(hl_net_interface_t interface);

    int hl_netif_set_interface_enable(hl_net_interface_t interface, int enable);
    int hl_netif_set_interface_enable2(const char *ifname, int enable);
    int hl_netif_set_ip_address(hl_net_interface_t interface, const char *addr);
    int hl_netif_get_ip_address(hl_net_interface_t interface, char *addr, uint32_t addr_len);

    int hl_netif_set_netmask(hl_net_interface_t interface, const char *addr);
    int hl_netif_get_netmask(hl_net_interface_t interface, char *addr, uint32_t addr_len);

    int hl_netif_set_default_gateway(hl_net_interface_t interface, const char *addr);
    int hl_netif_get_default_gateway(hl_net_interface_t interface, char *addr, uint32_t addr_len);
    int hl_netif_delete_default_gateway(hl_net_interface_t interface);
    int hl_netif_delete_gateway(hl_net_interface_t interface);

    int hl_netif_set_mac_address(hl_net_interface_t interface, const char *addr);
    int hl_netif_get_mac_address(hl_net_interface_t interface, char *addr, uint32_t addr_len);

    int hl_netif_set_dns_address(hl_net_interface_t interface, const char *dns1, const char *dns2);
    int hl_netif_get_dns_address(hl_net_interface_t interface, char *dns1, char *dns2, uint32_t addr_len);

    int hl_netif_lan_is_connected(hl_net_interface_t interface);
    int hl_netif_wan_is_connected(hl_net_interface_t interface);
    int hl_netif_dhcp_is_bounded(hl_net_interface_t interface);
    int hl_net_do_ntp(const char *ntp_server, int timeout_ms);
    
    void set_ntpd_result(int result);
    int get_ntpd_result(void);

#ifdef __cplusplus
}
#endif

#endif