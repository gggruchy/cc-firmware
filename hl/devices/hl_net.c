#include "hl_net.h"
#include "hl_wlan.h"
#include "hl_sm.h"
#include "hl_tpool.h"
#include "hl_common.h"
#include "hl_assert.h"
#include "hl_eth.h"
#include "hl_wlan.h"

#include "curl.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include "resolv.h"
#include <netinet/in.h>
#include <netdb.h>
#define NET_ROUTE_TABLE_SIZE 16
#define NTP_SERVER_NUM 4
#define MAX_URL_LEN 256
#define LOG_TAG "hl_net"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define LOG_NET_SM_EVENT()  \
            if (event_id != HL_SM_EVENT_ID_IDLE) {LOG_I("[%s] got event:%d\n", __FUNCTION__, event_id);}
static const char net_ntp_server_url[NTP_SERVER_NUM][MAX_URL_LEN] =
    {
        "time.apple.com", // 国外
        "ntp1.google.com",
        "ntp.aliyun.com", // 国内
        "ntp.tencent.com",
};
typedef struct
{
    char name[IFNAMSIZ];
    hl_net_interface_t interface;
    hl_net_status_t status;
    hl_net_mode_t mode;
    int enable;
    int bounded;
    int lan_connected;
    int wan_connected;

    int lan_disconnect_counter;
    int wan_disconnect_counter;
    char dns[2][16];
    hl_sm_t sm;
} net_t;
typedef struct
{
    char name[IFNAMSIZ];
    char destination[16];
    char gateway[16];
    char mask[16];
    uint32_t flags;
} net_route_t;
typedef enum
{
    NET_THREAD_EVENT_CARRIER_ON,
    NET_THREAD_EVENT_CARRIER_OFF,
    NET_THREAD_EVENT_WLAN_DISCONNECTED,
    NET_THREAD_EVENT_WLAN_CONNECTED,

} net_thread_event_t;
typedef struct
{
    net_thread_event_t event;
    char name[IFNAMSIZ];
} net_thread_msg_t;

typedef enum
{
    NET_SM_EVENT_ID_DHCP_ENABLE = HL_SM_EVENT_ID_USER,
    NET_SM_EVENT_ID_DHCP_DISABLE,
    NET_SM_EVENT_ID_DHCP_DEFCONFIG,
    NET_SM_EVENT_ID_DHCP_LEASEFAIL,
    NET_SM_EVENT_ID_DHCP_NAK,
    NET_SM_EVENT_ID_DHCP_RENEW,
    NET_SM_EVENT_ID_DHCP_BOUND,
    NET_SM_EVENT_ID_NET_ENABLE,
    NET_SM_EVENT_ID_NET_DISABLE,
    NET_SM_EVENT_ID_WLAN_CONNECTED,
    NET_SM_EVENT_ID_WLAN_DISCONNECTED,
} net_sm_event_t;
typedef struct
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t seq;
} icmp_echo_t;

typedef struct
{
    int detection_enable;
    int detection_done;
    int interface;
    char address[3][16];
    int address_count;
    int fd;
    uint16_t identifier;
    uint16_t seq;
} net_lan_detection_ctx_t;

static pthread_rwlock_t glock;
static net_t net_constants[2] = {0};
static hl_callback_t net_callback;
static hl_tpool_thread_t net_thread;
static hl_tpool_thread_t net_lan_connect_detction_thread;
static hl_tpool_thread_t net_wan_connect_detction_thread;
static int udhcpc_fifo;

static int ntpd_result = 0;     //记录时间同步是否成功 (0:失败,1:成功)

static const char *local_udhcpc_script_path;
static const char *local_udhcpc_tmp_path;
static const char *local_dns_resolv_conf_path;

static uint32_t local_lan_disconnect_threshold;
static uint64_t local_lan_detection_period;
static uint32_t local_wan_disconnect_threshold;
static uint64_t local_wan_detection_period;

const char **local_wan_detection_url_table;
uint32_t local_wan_detection_url_table_size;
FILE *nullfp = NULL;

static int net_init(hl_net_interface_t interface, const char *ifname);
static void net_deinit(hl_net_interface_t interface);

static void net_disable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);
static void net_enable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);

static int net_ioctl(int cmd, void *args);
static int net_get_route_table(net_route_t *route_table);

static int net_udhcpc_init(void);
static void net_udhcpc_poll_event(void);
static int net_udhcpc_start(hl_net_interface_t interface);
static int net_udhcpc_stop(hl_net_interface_t interface);
static void net_dns_update_address(hl_net_interface_t interface);

static uint16_t icmp_calculate_checksum(uint8_t *data, uint32_t size);
static int net_lan_connect_detection_send(net_lan_detection_ctx_t *ctx);
static int net_lan_connect_detection_recv(net_lan_detection_ctx_t *ctx);

static void net_routine(hl_tpool_thread_t thread, void *args);
static void net_lan_connect_detection_routine(hl_tpool_thread_t thread, void *args);
static void net_wan_connect_detection_routine(hl_tpool_thread_t thread, void *args);

static void wlan_callback(const void *data, void *user_data);
static void eth_callback(const void *data, void *user_data);

void hl_net_init(int wlan_enable, hl_net_mode_t wlan_mode,
                 int eth_enable, hl_net_mode_t eth_mode,
                 const char *udhcpc_script_path, const char *udhcpc_tmp_path,
                 const char *dns_resolv_conf_path,
                 uint32_t lan_disconnect_threshold, uint64_t lan_detection_period,
                 uint32_t wan_disconnect_threshold, uint64_t wan_detection_period,
                 const char **wan_detection_url_table, uint32_t wan_detection_url_table_size,
                 const char *wlan_entry_path)
{
    HL_ASSERT(udhcpc_script_path != NULL);
    HL_ASSERT(access(udhcpc_script_path, F_OK) == 0);
    HL_ASSERT(udhcpc_tmp_path != NULL);
    HL_ASSERT(strlen(udhcpc_tmp_path) > 0);
    HL_ASSERT(dns_resolv_conf_path != NULL);
    HL_ASSERT(wan_detection_url_table != NULL);
    HL_ASSERT(wan_detection_url_table_size > 0);
    HL_ASSERT((nullfp = fopen("/dev/null", "wb")) != NULL);

    net_constants[HL_NET_INTERFACE_WLAN].enable = wlan_enable;
    net_constants[HL_NET_INTERFACE_WLAN].mode = wlan_mode;
    net_constants[HL_NET_INTERFACE_ETH].enable = eth_enable;
    net_constants[HL_NET_INTERFACE_ETH].mode = eth_mode;
    local_udhcpc_script_path = udhcpc_script_path;
    local_udhcpc_tmp_path = udhcpc_tmp_path;
    local_dns_resolv_conf_path = dns_resolv_conf_path;
    local_lan_disconnect_threshold = lan_disconnect_threshold;
    local_lan_detection_period = lan_detection_period;
    local_wan_disconnect_threshold = wan_disconnect_threshold;
    local_wan_detection_period = wan_detection_period;
    local_wan_detection_url_table = wan_detection_url_table;
    local_wan_detection_url_table_size = wan_detection_url_table_size;

    net_constants[HL_NET_INTERFACE_WLAN].status = HL_NET_STATUS_NOT_EXISTS;
    net_constants[HL_NET_INTERFACE_ETH].status = HL_NET_STATUS_NOT_EXISTS;

    hl_netif_set_interface_enable(HL_NET_INTERFACE_WLAN, 1);
    hl_netif_set_interface_enable(HL_NET_INTERFACE_ETH, 1);

    hl_wlan_init(net_constants[HL_NET_INTERFACE_WLAN].enable, 8000, 30000, 8000, wlan_entry_path, 50, 1);
    // hl_eth_init();

    HL_ASSERT(net_udhcpc_init() == 0);
    HL_ASSERT(pthread_rwlock_init(&glock, NULL) == 0);
    HL_ASSERT(hl_callback_create(&net_callback) == 0);
    HL_ASSERT(hl_tpool_create_thread(&net_thread, net_routine, NULL, sizeof(net_thread_msg_t), 2, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(net_thread, 0) == 1);
    HL_ASSERT(hl_tpool_create_thread(&net_lan_connect_detction_thread, net_lan_connect_detection_routine, NULL, 0, 0, 0, 0) == 0);
    HL_ASSERT(hl_tpool_create_thread(&net_wan_connect_detction_thread, net_wan_connect_detection_routine, NULL, 0, 0, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(net_lan_connect_detction_thread, 0) == 1);
    HL_ASSERT(hl_tpool_wait_started(net_wan_connect_detction_thread, 0) == 1);
    hl_wlan_register_event_callback(wlan_callback, &net_constants[HL_NET_INTERFACE_WLAN]);
    // hl_eth_register_event_callback(eth_callback, &net_constants[HL_NET_INTERFACE_ETH]);
    // usleep(1000000);
    // hl_eth_trig_event();
}

int hl_net_lan_is_connected(void)
{
    int lan_connected;
    pthread_rwlock_rdlock(&glock);
    if (net_constants[HL_NET_INTERFACE_ETH].lan_connected || net_constants[HL_NET_INTERFACE_WLAN].lan_connected)
        lan_connected = 1;
    else
        lan_connected = 0;
    pthread_rwlock_unlock(&glock);
    return lan_connected;
}

int hl_net_wan_is_connected(void)
{
    int wan_connected;
    pthread_rwlock_rdlock(&glock);
    if (net_constants[HL_NET_INTERFACE_ETH].wan_connected || net_constants[HL_NET_INTERFACE_WLAN].wan_connected)
        wan_connected = 1;
    else
        wan_connected = 0;
    pthread_rwlock_unlock(&glock);
    return wan_connected;
}

void hl_net_register_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_register(net_callback, function, user_data);
}

void hl_net_unregister_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_unregister(net_callback, function, user_data);
}

int hl_netif_set_enable(hl_net_interface_t interface, int enable)
{
    net_t *net = &net_constants[interface];
    pthread_rwlock_wrlock(&glock);
    net->enable = enable;
    if (net->status != HL_NET_STATUS_NOT_EXISTS)
    {
        LOG_I("interface %s enable %d\n", net->name, enable);
        if (hl_sm_send_event(net->sm, enable ? NET_SM_EVENT_ID_NET_ENABLE : NET_SM_EVENT_ID_NET_DISABLE, NULL, 0) != 0)
        {
            pthread_rwlock_unlock(&glock);
            return -1;
        }
        if (net->interface == HL_NET_INTERFACE_WLAN)
            hl_wlan_set_enable(enable);
    }
    else
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_netif_set_mode(hl_net_interface_t interface, hl_net_mode_t mode)
{
    net_t *net = &net_constants[interface];
    pthread_rwlock_wrlock(&glock);
    net->mode = mode;
    if (net->status != HL_NET_STATUS_NOT_EXISTS)
    {
        if (hl_sm_send_event(net->sm, mode == HL_NET_MODE_DHCP ? NET_SM_EVENT_ID_DHCP_ENABLE : NET_SM_EVENT_ID_DHCP_DISABLE, NULL, 0) != 0)
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

hl_net_status_t hl_netif_get_state(hl_net_interface_t interface)
{
    hl_net_status_t state;
    net_t *net = &net_constants[interface];
    pthread_rwlock_rdlock(&glock);
    state = net->status;
    pthread_rwlock_unlock(&glock);
    return state;
}

int hl_netif_set_interface_enable(hl_net_interface_t interface, int enable)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    if (net_ioctl(SIOCGIFFLAGS, &ifr) != 0)
        return -1;
    if (enable)
        ifr.ifr_flags |= IFF_UP;
    else
        ifr.ifr_flags &= ~IFF_UP;
    if (net_ioctl(SIOCSIFFLAGS, &ifr) != 0)
        return -1;
    return 0;
}

int hl_netif_set_interface_enable2(const char *ifname, int enable)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (net_ioctl(SIOCGIFFLAGS, &ifr) != 0)
        return -1;
    if (enable)
        ifr.ifr_flags |= IFF_UP;
    else
        ifr.ifr_flags &= ~IFF_UP;
    if (net_ioctl(SIOCSIFFLAGS, &ifr) != 0)
        return -1;
    return 0;
}

int hl_netif_set_ip_address(hl_net_interface_t interface, const char *addr)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    sk_addr = (struct sockaddr_in *)&ifr.ifr_addr;
    inet_pton(AF_INET, addr, &sk_addr->sin_addr);
    LOG_I("hl_netif_set_ip_address name %s addr %s\n", net_constants[interface].name, addr);
    if (net_ioctl(SIOCSIFADDR, &ifr) != 0)
        return -1;
    return 0;
}

int hl_netif_get_ip_address(hl_net_interface_t interface, char *addr, uint32_t addr_len)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    if (net_ioctl(SIOCGIFADDR, &ifr) != 0)
        return -1;
    sk_addr = (struct sockaddr_in *)&ifr.ifr_addr;
    inet_ntop(AF_INET, &sk_addr->sin_addr, addr, addr_len);
    return 0;
}

int hl_netif_set_mac_address(hl_net_interface_t interface, const char *addr)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;
    sk_addr = (struct sockaddr_in *)&ifr.ifr_hwaddr;

    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    sscanf(addr, "%x:%x:%x:%x:%x:%x",
           ifr.ifr_hwaddr.sa_data[0],
           ifr.ifr_hwaddr.sa_data[1],
           ifr.ifr_hwaddr.sa_data[2],
           ifr.ifr_hwaddr.sa_data[3],
           ifr.ifr_hwaddr.sa_data[4],
           ifr.ifr_hwaddr.sa_data[5]);

    if (net_ioctl(SIOCSIFHWADDR, &ifr) != 0)
        return -1;
    return 0;
}

int hl_netif_get_mac_address(hl_net_interface_t interface, char *addr, uint32_t addr_len)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;
    sk_addr = (struct sockaddr_in *)&ifr.ifr_hwaddr;

    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    if (net_ioctl(SIOCGIFHWADDR, &ifr) != 0)
        return -1;

    snprintf(addr, addr_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             ifr.ifr_hwaddr.sa_data[0],
             ifr.ifr_hwaddr.sa_data[1],
             ifr.ifr_hwaddr.sa_data[2],
             ifr.ifr_hwaddr.sa_data[3],
             ifr.ifr_hwaddr.sa_data[4],
             ifr.ifr_hwaddr.sa_data[5]);

    return 0;
}

int hl_netif_set_netmask(hl_net_interface_t interface, const char *addr)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;

    ifr.ifr_netmask.sa_family = AF_INET;
    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);
    sk_addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    inet_pton(AF_INET, addr, &sk_addr->sin_addr);

    if (net_ioctl(SIOCSIFNETMASK, &ifr) != 0)
        return -1;

    return 0;
}

int hl_netif_get_netmask(hl_net_interface_t interface, char *addr, uint32_t addr_len)
{
    struct ifreq ifr;
    struct sockaddr_in *sk_addr;

    ifr.ifr_netmask.sa_family = AF_INET;
    strncpy(ifr.ifr_name, net_constants[interface].name, IFNAMSIZ);

    if (net_ioctl(SIOCGIFNETMASK, &ifr) != 0)
        return -1;

    sk_addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    inet_ntop(AF_INET, &sk_addr->sin_addr, addr, addr_len);

    return 0;
}

int hl_netif_set_dns_address(hl_net_interface_t interface, const char *dns1, const char *dns2)
{
    net_t *net = &net_constants[interface];
    hl_system("touch /tmp/resolv.conf.tmp");
    hl_system("grep -vE \"# %s\" %s > /tmp/resolv.conf.tmp", local_dns_resolv_conf_path, net->name);
    hl_system("cat /tmp/resolv.conf.tmp > %s", local_dns_resolv_conf_path);
    hl_system("rm /tmp/resolv.conf.tmp");
    if (dns1)
    {
        hl_system("echo \"nameserver %s # %s\" >> %s", dns1, net->name, local_dns_resolv_conf_path);
        strncpy(net->dns[0], dns1, sizeof(net->dns[0]));
    }
    else
        memset(net->dns[0], 0, sizeof(net->dns[0]));
    if (dns2)
    {
        hl_system("echo \"nameserver %s # %s\" >> %s", dns2, net->name, local_dns_resolv_conf_path);
        strncpy(net->dns[1], dns2, sizeof(net->dns[1]));
    }
    else
        memset(net->dns[1], 0, sizeof(net->dns[1]));

    return 0;
}

int hl_netif_get_dns_address(hl_net_interface_t interface, char *dns1, char *dns2, uint32_t addr_len)
{
    net_t *net = &net_constants[interface];
    pthread_rwlock_rdlock(&glock);
    if (dns1)
        strncpy(dns1, net->dns[0], addr_len);
    else
        memset(dns1, 0, addr_len);
    if (dns2)
        strncpy(dns2, net->dns[1], addr_len);
    else
        memset(dns1, 0, addr_len);
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_netif_set_default_gateway(hl_net_interface_t interface, const char *addr)
{
    struct rtentry rt;
    struct sockaddr_in *sockinfo;
    memset(&rt, 0, sizeof(rt));

    char netmask[16];
    hl_netif_get_netmask(interface, netmask, sizeof(netmask));

    in_addr_t gateway = inet_addr(addr);
    in_addr_t mask = inet_addr(netmask);
    in_addr_t dest = gateway & mask;

    // 先删除该端口所有网关
    hl_netif_delete_gateway(interface);

    // 先添加本地网络
    sockinfo = (struct sockaddr_in *)&rt.rt_gateway;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = INADDR_ANY;

    sockinfo = (struct sockaddr_in *)&rt.rt_dst;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = dest;

    sockinfo = (struct sockaddr_in *)&rt.rt_genmask;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = mask;

    rt.rt_flags = RTF_UP;
    rt.rt_dev = net_constants[interface].name;

    if (net_ioctl(SIOCADDRT, &rt) != 0)
        return -1;

    // 添加网关
    sockinfo = (struct sockaddr_in *)&rt.rt_gateway;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = inet_addr(addr);

    sockinfo = (struct sockaddr_in *)&rt.rt_dst;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = INADDR_ANY;

    sockinfo = (struct sockaddr_in *)&rt.rt_genmask;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = INADDR_ANY;

    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    rt.rt_dev = net_constants[interface].name;

    if (net_ioctl(SIOCADDRT, &rt) != 0)
        return -1;
    return 0;
}

int hl_netif_get_default_gateway(hl_net_interface_t interface, char *addr, uint32_t addr_len)
{
    net_route_t route_table[NET_ROUTE_TABLE_SIZE];
    int route_table_size = net_get_route_table(route_table);
    for (int i = 0; i < route_table_size; i++)
    {
        if (strcmp(net_constants[interface].name, route_table[i].name) == 0 &&
            route_table[i].flags & RTF_GATEWAY)
        {
            strncpy(addr, route_table[i].gateway, addr_len);
            return 0;
        }
    }
    return -1;
}

int hl_netif_delete_default_gateway(hl_net_interface_t interface)
{
    struct rtentry rt;
    struct sockaddr_in *sockinfo;
    char addr[16];

    if (hl_netif_get_default_gateway(interface, addr, sizeof(addr)) != 0)
    {
        LOG_I("can't find default gateway\n");
        return -1;
    }
    LOG_I("default gateway: %s\n", addr);
    memset(&rt, 0, sizeof(rt));
    sockinfo = (struct sockaddr_in *)&rt.rt_gateway;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = inet_addr(addr);

    sockinfo = (struct sockaddr_in *)&rt.rt_dst;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = INADDR_ANY;

    sockinfo = (struct sockaddr_in *)&rt.rt_genmask;
    sockinfo->sin_family = AF_INET;
    sockinfo->sin_addr.s_addr = INADDR_ANY;

    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    rt.rt_dev = net_constants[interface].name;

    if (net_ioctl(SIOCDELRT, &rt) != 0)
        return -1;
    return 0;
}

int hl_netif_delete_gateway(hl_net_interface_t interface)
{
#if 1
    struct rtentry rt;
    struct sockaddr_in *sockinfo;
    char addr[16];

    net_route_t route_table[NET_ROUTE_TABLE_SIZE];

    int route_table_size = net_get_route_table(route_table);
    for (int i = 0; i < route_table_size; i++)
    {
        if (strcmp(net_constants[interface].name, route_table[i].name) == 0)
        {
            memset(&rt, 0, sizeof(rt));
            sockinfo = (struct sockaddr_in *)&rt.rt_gateway;
            sockinfo->sin_family = AF_INET;
            sockinfo->sin_addr.s_addr = INADDR_ANY;

            sockinfo = (struct sockaddr_in *)&rt.rt_dst;
            sockinfo->sin_family = AF_INET;
            sockinfo->sin_addr.s_addr = inet_addr(route_table[i].destination);

            sockinfo = (struct sockaddr_in *)&rt.rt_genmask;
            sockinfo->sin_family = AF_INET;
            sockinfo->sin_addr.s_addr = inet_addr(route_table[i].mask);

            rt.rt_flags = RTF_UP;
            rt.rt_dev = route_table[i].name;

            if (net_ioctl(SIOCDELRT, &rt) != 0)
                continue;
        }
    }
#endif
    return 0;
}

int hl_netif_lan_is_connected(hl_net_interface_t interface)
{
    int lan_connected;
    net_t *net = &net_constants[interface];
    pthread_rwlock_rdlock(&glock);
    lan_connected = net->lan_connected;
    pthread_rwlock_unlock(&glock);
    return lan_connected;
}

int hl_netif_wan_is_connected(hl_net_interface_t interface)
{
    int wan_connected;
    net_t *net = &net_constants[interface];
    pthread_rwlock_rdlock(&glock);
    wan_connected = net->wan_connected;
    pthread_rwlock_unlock(&glock);
    return wan_connected;
}

int hl_netif_dhcp_is_bounded(hl_net_interface_t interface)
{
    int bounded;
    net_t *net = &net_constants[interface];
    pthread_rwlock_rdlock(&glock);
    bounded = net->bounded;
    pthread_rwlock_unlock(&glock);
    return bounded;
}

static void net_routine(hl_tpool_thread_t thread, void *args)
{
    net_thread_msg_t thread_msg;
    int exists[2] = {0};

    for (;;)
    {
        if (hl_tpool_thread_recv_msg_try(thread, &thread_msg) == 0)
        {
            int net_index = -1;
            int changed = 0;

            if (strstr(thread_msg.name, "eth"))
                net_index = HL_NET_INTERFACE_ETH;
            else if (strstr(thread_msg.name, "wlan"))
                net_index = HL_NET_INTERFACE_WLAN;
            else
                continue;

            LOG_I("#########thread msg %s event %d status %d#########\n", thread_msg.name, thread_msg.event, net_constants[net_index].status);

            pthread_rwlock_wrlock(&glock);
            if (thread_msg.event == NET_THREAD_EVENT_CARRIER_ON && net_constants[net_index].status == HL_NET_STATUS_NOT_EXISTS)
            {
                if (net_init(net_index, thread_msg.name) == 0)
                    exists[net_index] = 1;
                changed = 1;
            }
            else if (thread_msg.event == NET_THREAD_EVENT_CARRIER_ON)
            {
                LOG_D("[%s] send NET_SM_EVENT_ID_NET_ENABLE to sm.\n", __FUNCTION__);
                hl_sm_send_event(net_constants[net_index].sm, NET_SM_EVENT_ID_NET_ENABLE, NULL, 0);
            }
            else if (thread_msg.event == NET_THREAD_EVENT_CARRIER_OFF && net_constants[net_index].status != HL_NET_STATUS_NOT_EXISTS && strcmp(net_constants[net_index].name, thread_msg.name) == 0)
            {
                net_deinit(net_index);
                exists[net_index] = 0;
                changed = 1;
            }
            else if (thread_msg.event == NET_THREAD_EVENT_WLAN_DISCONNECTED && net_constants[net_index].status != HL_NET_STATUS_NOT_EXISTS && strcmp(net_constants[net_index].name, thread_msg.name) == 0)
            {
                // LOG_I("hl_sm_send_event NET_SM_EVENT_ID_WLAN_DISCONNECTED1\n");
                hl_sm_send_event(net_constants[net_index].sm, NET_SM_EVENT_ID_WLAN_DISCONNECTED, NULL, 0);
                // LOG_I("hl_sm_send_event NET_SM_EVENT_ID_WLAN_DISCONNECTED2\n");
            }
            else if (thread_msg.event == NET_THREAD_EVENT_WLAN_CONNECTED && net_constants[net_index].status != HL_NET_STATUS_NOT_EXISTS && strcmp(net_constants[net_index].name, thread_msg.name) == 0)
            {
                // LOG_I("hl_sm_send_event NET_SM_EVENT_ID_WLAN_CONNECTED1\n");
                hl_sm_send_event(net_constants[net_index].sm, NET_SM_EVENT_ID_WLAN_CONNECTED, NULL, 0);
                // LOG_I("hl_sm_send_event NET_SM_EVENT_ID_WLAN_CONNECTED2\n");
            }
            pthread_rwlock_unlock(&glock);

            if (changed)
            {
                // LOG_I("hl_callback_call HL_NET_EVENT_STATUS_CHANGED\n");
                hl_net_event_t event;
                event.id = HL_NET_EVENT_STATUS_CHANGED;
                event.interface = net_index;
                hl_callback_call(net_callback, &event);
            }
        }

        // poll for udhcpc
        if (exists[HL_NET_INTERFACE_ETH] || exists[HL_NET_INTERFACE_WLAN])
        {
            // LOG_I("net_udhcpc_poll_event1\n");
            net_udhcpc_poll_event();
            // LOG_I("net_udhcpc_poll_event2\n");
        }
        if (exists[HL_NET_INTERFACE_ETH])
        {
            // LOG_I("hl_sm_dispatch_event eth1\n");
            hl_sm_dispatch_event(net_constants[HL_NET_INTERFACE_ETH].sm);
            // LOG_I("hl_sm_dispatch_event eth2\n");
        }
        if (exists[HL_NET_INTERFACE_WLAN])
        {
            // LOG_I("hl_sm_dispatch_event wlan1\n");
            hl_sm_dispatch_event(net_constants[HL_NET_INTERFACE_WLAN].sm);
            // LOG_I("hl_sm_dispatch_event wlan2\n");
        }

        usleep(500000);
    }
}

static void wlan_callback(const void *data, void *user_data)
{
    const hl_wlan_event_t *event = (const hl_wlan_event_t *)data;
    net_thread_msg_t msg;
    // LOG_I("wlan_callback: id %d interface %s\n", event->id, event->interface);

    if (event->id == HL_WLAN_EVENT_STATUS_CHANGED)
    {
        strncpy(msg.name, event->interface, sizeof(msg.name));
        hl_wlan_status_t status = hl_wlan_get_status();
        LOG_I("wlan_callback: status %d\n", status);

        if (status == HL_WLAN_STATUS_NOT_EXISTS)
            msg.event = NET_THREAD_EVENT_CARRIER_OFF;
        else
            msg.event = NET_THREAD_EVENT_CARRIER_ON;
        hl_tpool_send_msg(net_thread, &msg);

        if (status == HL_WLAN_STATUS_DISCONNECTED)
        {
            msg.event = NET_THREAD_EVENT_WLAN_DISCONNECTED;
            // LOG_I("hl_tpool_send_msg: NET_THREAD_EVENT_WLAN_DISCONNECTED1\n");
            hl_tpool_send_msg(net_thread, &msg);
            // LOG_I("hl_tpool_send_msg: NET_THREAD_EVENT_WLAN_DISCONNECTED2\n");
        }
        else if (status == HL_WLAN_STATUS_CONNECTED)
        {
            msg.event = NET_THREAD_EVENT_WLAN_CONNECTED;
            // LOG_I("hl_tpool_send_msg: NET_THREAD_EVENT_WLAN_CONNECTED1\n");
            hl_tpool_send_msg(net_thread, &msg);
            // LOG_I("hl_tpool_send_msg: NET_THREAD_EVENT_WLAN_CONNECTED2\n");
        }
    }
}

static void eth_callback(const void *data, void *user_data)
{
    const hl_eth_event_t *event = (const hl_eth_event_t *)data;
    net_thread_msg_t msg;

    strncpy(msg.name, event->interface, sizeof(msg.name));
    if (event->id == HL_ETH_EVENT_CARRIER_ON)
        msg.event = NET_THREAD_EVENT_CARRIER_ON;
    else if (event->id == HL_ETH_EVENT_CARRIER_OFF)
        msg.event = NET_THREAD_EVENT_CARRIER_OFF;
    LOG_I("eth_callback\n");
    hl_tpool_send_msg(net_thread, &msg);
}

static int net_init(hl_net_interface_t interface, const char *ifname)
{
    net_t *net = &net_constants[interface];
    strncpy(net->name, ifname, sizeof(net->name));
    net->interface = interface;
    hl_netif_set_interface_enable(interface, 1);
    LOG_I("net_init %s enable %d\n", net->name, net->enable);
    if (hl_sm_create(&net->sm, net->enable ? net_enable : net_disable, NULL, 0, 8, net) != 0)
        return -1;
    return 0;
}

static void net_deinit(hl_net_interface_t interface)
{
    net_t *net = &net_constants[interface];
    LOG_I("net_deinit %s\n", net->name);
    net_udhcpc_stop(net->interface);
    hl_sm_destroy(&net->sm);
    net->status = HL_NET_STATUS_NOT_EXISTS;
}

static void net_enable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    net_t *net = hl_sm_get_user_data(sm);
    hl_net_event_t event;

    LOG_NET_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        break;
    case HL_SM_EVENT_ID_ENTRY:
        pthread_rwlock_wrlock(&glock);
        net->status = HL_NET_STATUS_ENABLE;
        pthread_rwlock_unlock(&glock);
        event.id = HL_NET_EVENT_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        hl_sm_send_event(net->sm, net->mode == HL_NET_MODE_DHCP ? NET_SM_EVENT_ID_DHCP_ENABLE : NET_SM_EVENT_ID_DHCP_DISABLE, NULL, 0);
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case NET_SM_EVENT_ID_DHCP_ENABLE:
        pthread_rwlock_wrlock(&glock);
        net->lan_connected = 0;
        net->wan_connected = 0;
        pthread_rwlock_unlock(&glock);
        event.id = HL_NET_EVENT_LAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        event.id = HL_NET_EVENT_WAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        net_udhcpc_start(net->interface);
        break;
    case NET_SM_EVENT_ID_DHCP_DISABLE:
        pthread_rwlock_wrlock(&glock);
        net->lan_connected = 0;
        net->wan_connected = 0;
        pthread_rwlock_unlock(&glock);
        event.id = HL_NET_EVENT_LAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        event.id = HL_NET_EVENT_WAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        net_udhcpc_stop(net->interface);
        break;
    case NET_SM_EVENT_ID_DHCP_DEFCONFIG:
        break;
    case NET_SM_EVENT_ID_DHCP_LEASEFAIL:
    case NET_SM_EVENT_ID_DHCP_NAK:
        pthread_rwlock_wrlock(&glock);
        net->bounded = 0;
        pthread_rwlock_unlock(&glock);

        event.id = HL_NET_EVENT_DHCP_FAILED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        break;
    case NET_SM_EVENT_ID_DHCP_RENEW:
    case NET_SM_EVENT_ID_DHCP_BOUND:
        LOG_I("dhcp success\n");
        net_dns_update_address(net->interface);

        pthread_rwlock_wrlock(&glock);
        net->bounded = 1;
        pthread_rwlock_unlock(&glock);

        event.id = HL_NET_EVENT_DHCP_SUCCEEDED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        break;
    case NET_SM_EVENT_ID_NET_DISABLE:
        hl_sm_trans_state(sm, net_disable, NULL, 0);
        break;
    case NET_SM_EVENT_ID_WLAN_CONNECTED:
        if (net->interface == HL_NET_INTERFACE_WLAN)
        {
            if (net->mode == HL_NET_MODE_DHCP)
                hl_sm_send_event(net->sm, NET_SM_EVENT_ID_DHCP_ENABLE, NULL, 0);
        }
        break;
    case NET_SM_EVENT_ID_WLAN_DISCONNECTED:
        if (net->interface == HL_NET_INTERFACE_WLAN)
        {
            pthread_rwlock_wrlock(&glock);
            net->lan_connected = 0;
            net->wan_connected = 0;
            pthread_rwlock_unlock(&glock);

            LOG_I("net_enable hl_callback_call1\n");
            event.id = HL_NET_EVENT_LAN_STATUS_CHANGED;
            event.interface = net->interface;
            hl_callback_call(net_callback, &event);
            LOG_I("net_enable hl_callback_call2\n");

            LOG_I("net_enable hl_callback_call1\n");
            event.id = HL_NET_EVENT_WAN_STATUS_CHANGED;
            event.interface = net->interface;
            hl_callback_call(net_callback, &event);
            LOG_I("net_enable hl_callback_call2\n");

            if (net->mode == HL_NET_MODE_DHCP)
            {
                hl_sm_send_event(net->sm, NET_SM_EVENT_ID_DHCP_DISABLE, NULL, 0);
                LOG_I("net_enable hl_sm_send_event NET_SM_EVENT_ID_DHCP_DISABLE\n");
            }
        }
        break;
    }
}

static void net_disable(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    net_t *net = hl_sm_get_user_data(sm);
    hl_net_event_t event;

    LOG_NET_SM_EVENT();
    switch (event_id)
    {
    case HL_SM_EVENT_ID_IDLE:
        pthread_rwlock_wrlock(&glock);
        net->status = HL_NET_STATUS_DISABLE;
        pthread_rwlock_unlock(&glock);

        event.id = HL_NET_EVENT_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        break;
    case HL_SM_EVENT_ID_ENTRY:
        hl_netif_delete_gateway(net->interface);
        pthread_rwlock_wrlock(&glock);
        net->lan_connected = 0;
        net->wan_connected = 0;
        pthread_rwlock_unlock(&glock);
        event.id = HL_NET_EVENT_LAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        event.id = HL_NET_EVENT_WAN_STATUS_CHANGED;
        event.interface = net->interface;
        hl_callback_call(net_callback, &event);
        break;
    case HL_SM_EVENT_ID_EXIT:
        break;
    case NET_SM_EVENT_ID_NET_ENABLE:
        hl_sm_trans_state(sm, net_enable, NULL, 0);
        break;
    }
}

static int net_ioctl(int cmd, void *args)
{
    int sk;
    sk = socket(AF_INET, SOCK_DGRAM, 0);
    if (sk == -1)
    {
        LOG_I("socket failed %s\n", strerror(errno));
        return -1;
    }

    if (ioctl(sk, cmd, args) != 0)
    {
        // LOG_I("ioctl cmd %d failed %s\n", cmd, strerror(errno));
        close(sk);
        return -1;
    }
    close(sk);
    return 0;
}

static int net_get_route_table(net_route_t *route_table)
{
    FILE *fp;
    char buf[1024];
    char line[512];
    int len;
    if ((fp = fopen("/proc/net/route", "r")) == NULL)
    {
        LOG_I("open /proc/net/route failed %s\n", strerror(errno));
        return -1;
    }
    len = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    char *saveptr;
    char *token;
    char *line_saveptr;
    int line_idx = 0;
    int pos_iface = -1, pos_dest = -1, pos_gateway = -1, pos_flags = -1, pos_mask = -1;
    int numbers = 0;
    buf[len] = '\0';
    line_saveptr = buf;

    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (strlen(line) == 0)
            continue;
        if (line_idx && numbers < NET_ROUTE_TABLE_SIZE)
        {
            token = strtok_r(line, "\t", &saveptr);
            int pos = 0;
            while (token != NULL)
            {
                if (pos == pos_iface)
                {
                    strncpy(route_table[numbers].name, token, sizeof(route_table[numbers].name));
                }
                else if (pos == pos_dest)
                {
                    in_addr_t addr = strtoull(token, NULL, 16);
                    inet_ntop(AF_INET, &addr, route_table[numbers].destination, sizeof(route_table[numbers].destination));
                }
                else if (pos == pos_gateway)
                {
                    in_addr_t addr = strtoull(token, NULL, 16);
                    inet_ntop(AF_INET, &addr, route_table[numbers].gateway, sizeof(route_table[numbers].gateway));
                }
                else if (pos == pos_flags)
                    route_table[numbers].flags = strtoul(token, NULL, 16);
                else if (pos == pos_mask)
                {
                    in_addr_t addr = strtoull(token, NULL, 16);
                    inet_ntop(AF_INET, &addr, route_table[numbers].mask, sizeof(route_table[numbers].mask));
                }
                pos++;
                token = strtok_r(NULL, "\t", &saveptr);
            }
            numbers++;
        }
        else
        {
            token = strtok_r(line, "\t", &saveptr);
            int pos = 0;
            while (token != NULL)
            {
                if (strstr(token, "Iface"))
                    pos_iface = pos;
                else if (strstr(token, "Destination"))
                    pos_dest = pos;
                else if (strstr(token, "Gateway"))
                    pos_gateway = pos;
                else if (strstr(token, "Flags"))
                    pos_flags = pos;
                else if (strstr(token, "Mask"))
                    pos_mask = pos;
                pos++;
                token = strtok_r(NULL, "\t", &saveptr);
            }
        }
        line_idx++;
    }
    return numbers;
}

static int net_udhcpc_init(void)
{
    int err;

    if (access(local_udhcpc_tmp_path, F_OK) == 0)
        remove(local_udhcpc_tmp_path);

    if ((err = mkfifo(local_udhcpc_tmp_path, 0666)) != 0 && errno != EEXIST)
    {
        LOG_I("mkfifo failed: %s\n", strerror(errno));
        return -1;
    }
    udhcpc_fifo = open(local_udhcpc_tmp_path, O_RDONLY | O_NONBLOCK);
    if (udhcpc_fifo < 0)
    {
        LOG_I("open fifo failed: %s\n", strerror(err));
        return -1;
    }
    return 0;
}

static void net_udhcpc_poll_event(void)
{
    char buf[64];
    char line[16];

    int len;
    char *token;
    char *saveptr;
    char *line_saveptr;
    net_t *net;
    net_sm_event_t event;

    if ((len = read(udhcpc_fifo, buf, sizeof(buf))) > 0)
    {
        line_saveptr = buf;
        while (hl_get_line(line, sizeof(line), &line_saveptr))
        {
            if (strlen(line) == 0)
                continue;
            pthread_rwlock_wrlock(&glock);
            token = strtok_r(line, ":", &saveptr);
            if (token)
            {
                if (net_constants[HL_NET_INTERFACE_ETH].status != HL_NET_STATUS_NOT_EXISTS && strcmp(token, net_constants[HL_NET_INTERFACE_ETH].name) == 0)
                    net = &net_constants[HL_NET_INTERFACE_ETH];
                else if (net_constants[HL_NET_INTERFACE_WLAN].status != HL_NET_STATUS_NOT_EXISTS && strcmp(token, net_constants[HL_NET_INTERFACE_WLAN].name) == 0)
                    net = &net_constants[HL_NET_INTERFACE_WLAN];
                else
                    continue;
                if (strcmp(saveptr, "deconfig") == 0)
                    event = NET_SM_EVENT_ID_DHCP_DEFCONFIG;
                else if (strcmp(saveptr, "leasefail") == 0)
                    event = NET_SM_EVENT_ID_DHCP_LEASEFAIL;
                else if (strcmp(saveptr, "nak") == 0)
                    event = NET_SM_EVENT_ID_DHCP_NAK;
                else if (strcmp(saveptr, "renew") == 0)
                    event = NET_SM_EVENT_ID_DHCP_RENEW;
                else if (strcmp(saveptr, "bound") == 0)
                    event = NET_SM_EVENT_ID_DHCP_BOUND;
                else
                {
                    LOG_I("udhcpc unknown action %s\n", saveptr);
                    continue;
                }
            }
            else
            {
                LOG_I("net_udhcpc_poll_event: %s\n", line);
            }
            hl_sm_send_event(net->sm, event, NULL, 0);
            pthread_rwlock_unlock(&glock);
        }
    }
}

static int net_udhcpc_start(hl_net_interface_t interface)
{
    net_t *net = &net_constants[interface];
    char pidfile[PATH_MAX];
    uint64_t start_ticks = hl_get_tick_ms();

    snprintf(pidfile, PATH_MAX, "/tmp/%s_udhcpc.pid", net->name);
    if (access(pidfile, F_OK) == 0)
    {
        LOG_I("net_udhcpc_start process already exist\n");
        net_udhcpc_stop(interface);
    }

#if CONFIG_PROJECT == PROJECT_ELEGO_E100
    if (hl_system("udhcpc -i %s -b -p %s -s %s -x hostname:Centauri-Carbon", net->name, pidfile, local_udhcpc_script_path) != 0)
#else
    if (hl_system("udhcpc -i %s -b -p %s -s %s", net->name, pidfile, local_udhcpc_script_path) != 0)
#endif
    {
        LOG_I("net_start_udhcpc failed\n");
        return -1;
    }

    // wait for pidfile
    start_ticks = hl_get_tick_ms();
    while (access(pidfile, F_OK) != 0 && hl_tick_is_overtime(start_ticks, hl_get_tick_ms(), 5000) == 0)
    {
        usleep(1000000);
    }
    if (access(pidfile, F_OK) != 0)
    {
        LOG_I("net_start_udhcpc timeout\n");
        return -1;
    }
    res_init(); /* for some unkonwn reason, add this line, then ota via wifi succeed */
    return 0;
}

static int net_udhcpc_stop(hl_net_interface_t interface)
{
    net_t *net = &net_constants[interface];
    uint64_t start_ticks = hl_get_tick_ms();
    char pidfile[PATH_MAX];
    char pid[64] = {0};

    snprintf(pidfile, PATH_MAX, "/tmp/%s_udhcpc.pid", net->name);
    if (access(pidfile, F_OK) != 0)
    {
        LOG_I("net_stop_udhcpc process not exist\n");
        return 0;
    }

    FILE *fp = fopen(pidfile, "rb");
    if (fp == NULL)
    {
        LOG_I("net_stop_udhcpc open pidfile failed %s\n", strerror(errno));
        return -1;
    }
    int pid_len = fread(pid, 1, sizeof(pid) - 1, fp);
    pid[pid_len - 1] = '\0';
    fclose(fp);

    if (hl_system("kill %s", pid) != 0)
    {
        LOG_I("net_stop_udhcpc failed pid %s\n", pid);
        return -1;
    }

    // wait exit
    start_ticks = hl_get_tick_ms();
    while (access(pidfile, F_OK) == 0 && hl_tick_is_overtime(start_ticks, hl_get_tick_ms(), 5000) == 0)
    {
        usleep(1000000);
    }
    if (access(pidfile, F_OK) == 0)
    {
        LOG_I("net_stop_udhcpc timeout\n");
        return -1;
    }

    return 0;
}

static int net_dns_get_address(char *line, char *dns_addr, uint32_t addr_len)
{
    char *p;
    char *saveptr;
    char *token;
    p = dns_addr;

    if ((token = strtok_r(line, " ", &saveptr)) == NULL)
        return -1;

    while (token != NULL)
    {
        while (*token == ' ')
            token++;
        if (strcmp(token, "nameserver") == 0)
        {
            if ((token = strtok_r(NULL, " ", &saveptr)) == NULL)
                return -1;

            while (*token == ' ')
                token++;

            while (*token != '\0' && *token != ' ' && (p - dns_addr < addr_len))
                *p++ = *token++;
            *p = '\0';
            // LOG_I("dns_addr %s\n", dns_addr);
            return 0;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    return -1;
}

static void net_dns_update_address(hl_net_interface_t interface)
{
    net_t *net = &net_constants[interface];
    FILE *fp;
    int dns_count = 0;
    char buf[1024];
    char line[128];
    char *line_saveptr;

    if ((fp = fopen(local_dns_resolv_conf_path, "rb+")) == NULL)
    {
        LOG_I("fopen failed %s\n", strerror(errno));
        return;
    }
    if (fread(buf, 1, sizeof(buf), fp) < 0)
    {
        LOG_I("fread failed %s\n", strerror(errno));
        return;
    }
    fclose(fp);
    line_saveptr = buf;
    pthread_rwlock_rdlock(&glock);
    memset(net->dns, 0, sizeof(net->dns));
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        if (strstr(line, net->name))
        {
            if (dns_count < sizeof(net->dns) / sizeof(net->dns[0]))
            {
                if (net_dns_get_address(line, net->dns[dns_count], sizeof(net->dns[dns_count])) == 0)
                    dns_count++;
            }
            else
                break;
        }
    }
    for (int i = 0; i < dns_count; i++)
    {
        LOG_I("dns %d %s\n", i, net->dns[i]);
    }
    pthread_rwlock_unlock(&glock);
}

static uint16_t icmp_calculate_checksum(uint8_t *data, uint32_t size)
{
    uint32_t checksum = 0;
    unsigned char *end = data + size;

    if (size % 2 == 1)
    {
        end = data + size - 1;
        checksum += (*end) << 8;
    }

    while (data < end)
    {
        checksum += data[0] << 8;
        checksum += data[1];
        data += 2;
    }

    uint32_t carray = checksum >> 16;
    while (carray)
    {
        checksum = (checksum & 0xffff) + carray;
        carray = checksum >> 16;
    }
    checksum = ~checksum;
    return checksum & 0xffff;
}

static int net_lan_connect_detection_send(net_lan_detection_ctx_t *ctx)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    icmp_echo_t echo;
    net_t *net = (net_t *)&net_constants[ctx->interface];

    ctx->fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (ctx->fd < 0)
    {
        LOG_I("socket error %s\n", strerror(errno));
        return -1;
    }
    if (setsockopt(ctx->fd, SOL_SOCKET, SO_BINDTODEVICE, net->name, IFNAMSIZ) != 0)
    {
        LOG_I("setsockopt error %s\n", strerror(errno));
        close(ctx->fd);
        return -1;
    }

    memset(&echo, 0, sizeof(echo));
    srand(hl_get_tick_ms());
    ctx->identifier = rand();
    ctx->seq = rand();

    echo.type = 8;
    echo.code = 0;
    echo.identifier = htons(ctx->identifier);
    echo.seq = htons(ctx->seq);
    echo.checksum = htons(icmp_calculate_checksum((uint8_t *)&echo, sizeof(echo)));
    for (int i = 0; i < ctx->address_count; i++)
    {
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        inet_pton(AF_INET, ctx->address[i], &addr.sin_addr);
        if (sendto(ctx->fd, &echo, sizeof(echo), 0, (struct sockaddr *)&addr, addr_len) < 0)
            LOG_I("sendto failed %s\n", strerror(errno));
    }
    return 0;
}

static int net_lan_connect_detection_recv(net_lan_detection_ctx_t *ctx)
{
    uint8_t buffer[1500];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    icmp_echo_t *from;
    uint16_t checksum;
    int len = recvfrom(ctx->fd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr *)&addr, &addr_len);
    if (len < 0)
    {
        LOG_I("recvfrom error %s\n", strerror(errno));
        return -1;
    }

    // 校验
    from = (icmp_echo_t *)(buffer + 20);
    if (from->type != 0 || from->code != 0)
        return -1;
    if (ntohs(from->identifier) != ctx->identifier || ntohs(from->seq) != ctx->seq)
        return -1;
    checksum = ntohs(from->checksum);
    from->checksum = 0;
    if (icmp_calculate_checksum((uint8_t *)from, len - 20) != checksum)
        return -1;

    ctx->detection_done = 1;
    return 0;
}

static void net_lan_connect_detection_routine(hl_tpool_thread_t thread, void *args)
{
    struct pollfd fds[HL_NET_INTERFACE_NUMBERS];
    int nfds = 0;
    uint64_t start_ticks;
    net_lan_detection_ctx_t ctx[HL_NET_INTERFACE_NUMBERS];
    char dns[2][16];

    for (;;)
    {
        LOG_D("detection routine start\n");
        memset(ctx, 0, sizeof(ctx));
        // 获取PING地址
        pthread_rwlock_rdlock(&glock);
        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            // 网卡使能后获取准备PING的地址
            if (net_constants[i].status == HL_NET_STATUS_ENABLE)
            {
                ctx[i].interface = i;
                if (hl_netif_get_default_gateway(i, ctx[i].address[ctx[i].address_count], sizeof(ctx[i].address[0])) == 0)
                    ctx[i].address_count++;
                else
                {
                    // 系统无默认网关下尝试Ping第一台主机
                    char ipaddr[16] = {0};
                    char maskaddr[16] = {0};
                    if (hl_netif_get_ip_address(i, ipaddr, sizeof(ipaddr)) == 0)
                    {
                        hl_netif_get_netmask(i, maskaddr, sizeof(maskaddr));
                        in_addr_t __ipaddr = inet_addr(ipaddr);
                        in_addr_t __maskaddr = inet_addr(maskaddr);
                        in_addr_t __gateway = (__ipaddr & __maskaddr) | inet_addr("0.0.0.1");
                        inet_ntop(AF_INET, &__gateway, ctx[i].address[ctx[i].address_count], sizeof(ctx[i].address[ctx[i].address_count]));
                        // LOG_I("ping test use generate gateway %s\n", ctx[i].address[ctx[i].address_count]);
                        ctx[i].address_count++;
                    }
                }

                hl_netif_get_dns_address(i, dns[0], dns[1], sizeof(dns[0]));
                for (int j = 0; j < sizeof(dns) / sizeof(dns[0]); j++)
                {
                    if (strlen(dns[j]) == 0)
                        continue;
                    // 查找是否为重复的IP
                    int dup = 0;
                    for (int k = 0; k < ctx[i].address_count; k++)
                    {
                        if (strcmp(dns[j], ctx[i].address[k]) == 0)
                        {
                            dup = 1;
                            break;
                        }
                    }
                    if (dup == 0)
                    {
                        strcpy(ctx[i].address[ctx[i].address_count], dns[j]);
                        ctx[i].address_count++;
                    }
                }
                if (ctx[i].address_count > 0)
                    ctx[i].detection_enable = 1;
            }
        }
        pthread_rwlock_unlock(&glock);

        // 构建ICMP包并发包
        nfds = 0;
        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            if (ctx[i].detection_enable && net_lan_connect_detection_send(&ctx[i]) == 0)
            {
                fds[nfds].fd = ctx[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
            else
            {
                ctx[i].detection_enable = 0;
            }
        }

        // 轮询
        start_ticks = hl_get_tick_ms();
        if (nfds > 0)
        {
            while (hl_tick_is_overtime(start_ticks, hl_get_tick_ms(), local_lan_detection_period) == 0)
            {
                int ret = poll(fds, nfds, 100);
                if (ret == -1)
                {
                    LOG_I("poll failed: %s\n", strerror(errno));
                    break;
                }
                else if (ret > 0)
                {
                    for (int i = 0; i < nfds; i++)
                    {
                        if (fds[i].revents & POLLIN)
                        {
                            // 查找哪个接口的描述符收到数据则处理
                            for (int j = 0; j < HL_NET_INTERFACE_NUMBERS; j++)
                            {
                                if (ctx[j].detection_enable && ctx[j].fd == fds[i].fd)
                                    net_lan_connect_detection_recv(&ctx[j]);
                            }
                        }
                    }

                    // 查询是否所有接口都已收到包则提前退出轮询
                    int all_done = 1;
                    for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
                    {
                        if (ctx[i].detection_enable && ctx[i].detection_done == 0)
                        {
                            all_done = 0;
                            break;
                        }
                    }
                    if (all_done)
                        break;
                }
                usleep(900);
            }
            // 关闭
            for (int i = 0; i < nfds; i++)
                close(fds[i].fd);
        }

        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            int changed = 0;
            pthread_rwlock_wrlock(&glock);
            if (ctx[i].detection_enable && ctx[i].detection_done == 1)
            {
                if (net_constants[i].lan_connected == 0)
                {
                    net_constants[i].lan_connected = 1;
                    LOG_I("%s lan connected\n", net_constants[i].name);
                    changed = 1;
                }
                net_constants[i].lan_disconnect_counter = 0;
            }
            else if (net_constants[i].lan_connected)
            {
                if (net_constants[i].lan_disconnect_counter++ >= local_lan_disconnect_threshold)
                {
                    net_constants[i].lan_connected = 0;
                    LOG_I("%s lan disconnected\n", net_constants[i].name);
                    changed = 1;
                }
            }
            pthread_rwlock_unlock(&glock);

            if (changed)
            {
                hl_net_event_t event;
                event.id = HL_NET_EVENT_LAN_STATUS_CHANGED;
                event.interface = i;
                hl_callback_call(net_callback, &event);
            }
        }
        LOG_D("detection routine finish\n");
        uint64_t tick = hl_get_tick_ms() - start_ticks;
        if (local_lan_detection_period > tick)
            usleep((local_lan_detection_period - tick) * 1000);
    }
}

typedef struct
{
    hl_net_interface_t interface;
    CURL **handles;
    int handle_numbers;
    char curl_interface_name[IFNAMSIZ + 3];
    int conneted;
    int enable;
} wan_detection_ctx_t;

void net_wan_connect_detection_init(wan_detection_ctx_t *ctx, CURLM *multi_handle, hl_net_interface_t itf, const char *ifname)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->handles = (CURL **)malloc(sizeof(CURL *) * local_wan_detection_url_table_size);
    if (ctx->handles == NULL)
        return;
    ctx->interface = itf;
    snprintf(ctx->curl_interface_name, sizeof(ctx->curl_interface_name), "if!%s", ifname);
    for (int i = 0; i < local_wan_detection_url_table_size; i++)
    {
        ctx->handles[ctx->handle_numbers] = curl_easy_init();
        if (ctx->handles[ctx->handle_numbers] == NULL)
            continue;
        curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_URL, local_wan_detection_url_table[i]);
        curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_NOSIGNAL, 1L);
        // curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_WRITEDATA, nullfp);
        curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_INTERFACE, ctx->curl_interface_name);
        curl_easy_setopt(ctx->handles[ctx->handle_numbers], CURLOPT_QUICK_EXIT, 1L);
        if (curl_multi_add_handle(multi_handle, ctx->handles[ctx->handle_numbers]) != CURLM_OK)
        {
            curl_easy_cleanup(ctx->handles[ctx->handle_numbers]);
            continue;
        }
        ctx->handle_numbers++;
    }
}

void net_wan_connect_detection_deinit(wan_detection_ctx_t *ctx, CURLM *multi_handle)
{
    for (int i = 0; i < ctx->handle_numbers; i++)
    {
        curl_multi_remove_handle(multi_handle, ctx->handles[i]);
        curl_easy_cleanup(ctx->handles[i]);
    }
    if (ctx->handles)
        free(ctx->handles);
}

static void net_wan_connect_detection_routine(hl_tpool_thread_t thread, void *args)
{
    uint64_t start_ticks;
    CURLM *multi_handle = NULL;
    CURLMsg *m;
    CURLMcode mc;
    int still_running;
    wan_detection_ctx_t ctx[HL_NET_INTERFACE_NUMBERS] = {0};
    HL_ASSERT((multi_handle = curl_multi_init()) != NULL);

    for (;;)
    {
        LOG_D("net_wan_connect_detection_routine start\n");
        // 初始化
        pthread_rwlock_rdlock(&glock);
        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            if (net_constants[i].status == HL_NET_STATUS_ENABLE)
            {
                net_wan_connect_detection_init(&ctx[i], multi_handle, net_constants[i].interface, net_constants[i].name);
                ctx[i].enable = 1;
            }
            else
            {
                ctx[i].enable = 0;
                ctx[i].conneted = 0;
            }
        }
        pthread_rwlock_unlock(&glock);

        // 发起网络请求
        start_ticks = hl_get_tick_ms();

        int handle = 0;
        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            if (ctx[i].handle_numbers != 0)
            {
                handle = 1;
                break;
            }
        }

        if (handle)
        {
            do
            {
                LOG_D("curl_multi_perform start\n");
                if ((mc = curl_multi_perform(multi_handle, &still_running)) != CURLM_OK)
                {
                    LOG_I("curl_multi_perform failed %d\n", mc);
                    still_running = 0;
                    break;
                }
                LOG_D("curl_multi_wait start\n");
                if ((mc = curl_multi_wait(multi_handle, NULL, 0, 1000, NULL)) != CURLM_OK)
                {
                    LOG_I("curl_multi_wait failed %d\n", mc);
                    still_running = 0;
                    break;
                }
                LOG_D("curl_multi_wait finish\n");
                LOG_D("curl_multi_wait still_running %d\n", still_running);
                do
                {
                    int msgq = 0;
                    m = curl_multi_info_read(multi_handle, &msgq);
                    if (m == NULL)
                    {
                        LOG_D("curl_multi_info_read failed\n");
                    }
                    if (m && (m->msg == CURLMSG_DONE))
                    {
                        CURL *e = m->easy_handle;
                        long response_code;
                        curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &response_code);
                        if (response_code != 204 && response_code != 200)
                            continue;

                        // 查询哪一张网卡收到了消息
                        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
                        {
                            if (ctx[i].enable == 0)
                                continue;
                            for (int j = 0; j < ctx[i].handle_numbers; j++)
                            {
                                if (ctx[i].handles[j] == e)
                                {
                                    ctx[i].conneted = 1;
                                    break;
                                }
                            }
                        }

                        // 查询是否网卡都已经收到消息
                        int all_done = 1;
                        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
                        {
                            if (ctx[i].enable && ctx[i].conneted == 0)
                            {
                                all_done = 0;
                                break;
                            }
                        }
                        if (all_done)
                        {
                            LOG_D("all done\n");
                            still_running = 0;
                            break;
                        }
                    }
                    usleep(900);
                    LOG_D("hl_net: waiting for all interfaces to connect, still running : %d\n", still_running);
                } while (m && hl_tick_is_overtime(start_ticks, hl_get_tick_ms(), local_wan_detection_period) == 0);
                if (hl_tick_is_overtime(start_ticks, hl_get_tick_ms(), local_wan_detection_period))
                {
                    // LOG_W("local wan detection timeout\n");
                    still_running = 0;
                }
                usleep(900);
            } while (still_running);
        }

        // 销毁
        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            if (ctx[i].enable)
                net_wan_connect_detection_deinit(&ctx[i], multi_handle);
        }

        for (int i = 0; i < HL_NET_INTERFACE_NUMBERS; i++)
        {
            int changed = 0;
            pthread_rwlock_wrlock(&glock);
            if (ctx[i].conneted == 1 && net_constants[i].wan_connected == 0)
            {
                net_constants[i].wan_connected = 1;
                changed = 1;
                net_constants[i].wan_disconnect_counter = 0;
                LOG_I("%s wan connected\n", net_constants[i].name);
                if (get_ntpd_result() == 0)
                    hl_net_do_ntp(net_ntp_server_url[2], 3000);
            }
            else if (ctx[i].conneted == 0 && net_constants[i].wan_connected == 1)
            {
                if (net_constants[i].wan_disconnect_counter++ >= local_wan_disconnect_threshold)
                {

/* As the net work framwork currently is unstable
 * and frequently incorrectly detect wlan0 wan disconnection.
 * To avoid affecting the OTA process,
 * ignore all factors that could disconnect wlan0 wan.
 * This need to be improved in the near future.*/
// #define TEMPORARY_DEBUG
#ifdef TEMPORARY_DEBUG
                    LOG_I("Currently disable all factor that could disconnect %s wan.\n", net_constants[i].name);
#else
                    net_constants[i].wan_disconnect_counter = 0;
                    net_constants[i].wan_connected = 0;
                    changed = 1;
                    LOG_I("%s wan disconnected\n", net_constants[i].name);
#endif
                }
            }
            pthread_rwlock_unlock(&glock);

            if (changed)
            {
                hl_net_event_t event;
                event.id = HL_NET_EVENT_WAN_STATUS_CHANGED;
                event.interface = i;
                hl_callback_call(net_callback, &event);
            }

            // LOG_I("net_constants[%d].wan_disconnect_counter:%d ctx[%d].conneted:%d\n", i, net_constants[i].wan_disconnect_counter, i, ctx[i].conneted);

            // 为避免网络状态频繁变换,当检测到时[conneted]状态,网络断开计数置0
            if (ctx[i].conneted == 1)
                net_constants[i].wan_disconnect_counter = 0;

        }
        LOG_D("net_wan_connect_detection_routine finish\n");
        uint64_t ticks = hl_get_tick_ms() - start_ticks;
        if (local_wan_detection_period > ticks)
            usleep((local_wan_detection_period - ticks) * 1000);
    }
}

int hl_net_do_ntp(const char *ntp_server, int timeout_ms)
{
    uint64_t ntpd_tick = 0;
    struct tm tm;
    int ret = 0;
    do
    {
        if (is_process_running(ntp_server) == -1 && ntpd_tick == 0)
        {
            ntpd_tick = hl_get_tick_ms();
            LOG_I("run ntpd %s\n", ntp_server);
            hl_system("ntpd -p %s -qN", ntp_server);
        }
        usleep(500000);
    } while (is_process_running(ntp_server) > 0 && (hl_get_tick_ms() - ntpd_tick) < timeout_ms);
    ret = is_process_running(ntp_server) > 0 ? -1 : 0;
    
    tm = hl_get_localtime_time_from_utc_second(hl_get_utc_second());
    LOG_I("%d-%d %d:%d:%d ntpd %s -> ntpd_tick %llu \n", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ret < 0 ? "failed" : "success", hl_get_tick_ms() - ntpd_tick);
    if (ret < 0 || tm.tm_year + 1900 < 2024)
    {
        return -1;
    }
    if(get_ntpd_result() == 0 && ret >= 0)
        set_ntpd_result(1);
    return 0;
}
// #define NTP_TIMESTAMP_DELTA 2208988800ull
// #define NTP_TIMEOUT_SEC 5 // 设置超时为 5 秒

// // NTP packet structure
// typedef struct
// {
//     uint8_t li_vn_mode;        // Leap indicator, version number and mode
//     uint8_t stratum;           // Stratum level of the local clock
//     uint8_t poll;              // Polling interval
//     uint8_t precision;         // Precision of the local clock
//     uint32_t rootDelay;        // Total round trip delay time
//     uint32_t rootDispersion;   // Max error allowed from primary clock
//     uint32_t refId;            // Reference clock identifier
//     uint32_t refTimeStamp[2];  // Reference time-stamp
//     uint32_t origTimeStamp[2]; // Originate time-stamp
//     uint32_t recvTimeStamp[2]; // Receive time-stamp
//     uint32_t txTimeStamp[2];   // Transmit time-stamp
// } ntp_packet;

// int ntp_get_time(const char *ntp_server, struct timespec *ts, int timeout_ms)
// {
//     int sockfd;
//     struct sockaddr_in server_addr;
//     ntp_packet packet;
//     int n;
//     fd_set readfds;
//     struct timeval timeout;

//     // Create a UDP socket
//     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (sockfd < 0)
//     {
//         LOG_E("socket error\n");
//         return -1;
//     }

//     // Prepare the NTP server address
//     memset(&server_addr, 0, sizeof(server_addr));
//     struct hostent *he = gethostbyname(ntp_server);
//     if (he == NULL)
//     {
//         LOG_E("gethostbyname error\n");
//         close(sockfd);
//         return -1;
//     }

//     // 使用gethostbyname返回的第一个IP地址
//     memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(123); // NTP端口

//     // Prepare the NTP packet
//     memset(&packet, 0, sizeof(ntp_packet));
//     packet.li_vn_mode = (0 << 6) | (4 << 3) | (3); // LI = 0, VN = 4, Mode = 3

//     // Send the packet to the NTP server
//     if (sendto(sockfd, &packet, sizeof(ntp_packet), 0,
//                (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
//     {
//         LOG_E("sendto failed\n");
//         close(sockfd);
//         return -1;
//     }

//     // Initialize the timeout
//     timeout.tv_sec = timeout_ms / 1000;
//     timeout.tv_usec = 0;

//     // Set up the file descriptor set
//     FD_ZERO(&readfds);
//     FD_SET(sockfd, &readfds);

//     // Wait for a response with timeout
//     uint64_t start_tick = hl_get_tick_ms();
//     n = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
//     LOG_I("get ntp spend %lld \n", hl_get_tick_ms() - start_tick);
//     if (n < 0)
//     {
//         LOG_E("select error\n");
//         close(sockfd);
//         return -1;
//     }
//     else if (n == 0)
//     {
//         // Timeout occurred
//         LOG_E("NTP request timed out\n");
//         close(sockfd);
//         return -2; // Return a specific error code for timeout
//     }
//     // Receive the response
//     if (FD_ISSET(sockfd, &readfds))
//     {
//         n = recv(sockfd, &packet, sizeof(ntp_packet), 0);
//         if (n < 0)
//         {
//             LOG_E("recv failed\n");
//             close(sockfd);
//             return -1;
//         }
//     }
//     else
//     {
//         LOG_E("NTP请求在文件描述符中未设置，但select成功\n");
//         close(sockfd);
//         return -2;
//     }
//     // Close the socket
//     close(sockfd);

//     // Convert the received time to a timespec structure
//     ts->tv_sec = ntohl(packet.txTimeStamp[0]) - NTP_TIMESTAMP_DELTA;
//     ts->tv_nsec = 0; // You can further decode fractional seconds if needed

//     return 0;
// }

// int hl_net_do_ntp(const char *ntp_server, int timeout_ms)
// {
//     struct timespec ts;
//     int ret;

//     // Attempt to get the NTP time
//     ret = ntp_get_time(ntp_server, &ts, timeout_ms);
//     if (ret < 0)
//     {
//         if (ret == -2)
//         { // Timeout error
//             LOG_I("NTP request timed out\n");
//         }
//         else
//         {
//             LOG_I("NTP request failed\n");
//         }
//         return -1;
//     }

//     // Log the successful time retrieval
//     LOG_I("NTP time retrieved: %ld.%09ld\n", ts.tv_sec, ts.tv_nsec);

//     // Convert seconds to local time
//     struct tm *tm_info = localtime(&ts.tv_sec);
//     char time_str[100];

//     // Format the time into a string
//     strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

//     // Log the complete time with nanoseconds
//     LOG_I("Complete NTP time: %s.%09ld\n", time_str, ts.tv_nsec);

//     if (get_ntpd_result() == 0 && ret >= 0)
//         set_ntpd_result(1);
//     // Check if the year is valid (greater than or equal to 2024)
//     if (tm_info->tm_year + 1900 < 2024)
//     {
//         return -1;
//     }
//     return 0;
// }

// 设置时间同步的结果
void set_ntpd_result(int result)
{
    ntpd_result = result;
}

// 获取时间同步的结果
int get_ntpd_result(void)
{
    return ntpd_result;
}
