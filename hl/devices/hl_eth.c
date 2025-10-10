#include "hl_eth.h"
#include "hl_tpool.h"
#include "hl_callback.h"
#include "hl_common.h"
#include "hl_assert.h"
#include "hl_net.h"

//TODO
#if 0
#include "netlink/netlink.h"
#include "netlink/socket.h"
#include "netlink/msg.h"
#include "netlink/cache.h"
#include "netlink/addr.h"
#include "netlink/object-api.h"
#include "netlink/route/link.h"

#include <net/if.h>

#define LOG_TAG "hl_eth"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

static struct nl_sock *sk;
static struct nl_cache_mngr *mngr;
static struct nl_cache *cache;
static hl_tpool_thread_t th;
static hl_callback_t callback;
static pthread_mutex_t lock;

static void eth_carrier_poll_thread(hl_tpool_thread_t thread, void *args);
static void eth_link_change_callback(struct nl_cache *cache, struct nl_object *obj, int action, void *args);
static void eth_link_up_callback(struct nl_cache *cache, struct nl_object *obj, int action, void *args);
static void eth_foreach_for_trig(struct nl_object *obj, void *args);
static void eth_foreach_for_up(struct nl_object *obj, void *args);

int hl_eth_init(void)
{
    HL_ASSERT(pthread_mutex_init(&lock, NULL) == 0);
    HL_ASSERT((sk = nl_socket_alloc()) != NULL);
    HL_ASSERT(nl_cache_mngr_alloc(sk, NETLINK_ROUTE, NL_AUTO_PROVIDE, &mngr) == 0);
    HL_ASSERT(rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache) == 0);
    HL_ASSERT(nl_cache_mngr_add_cache(mngr, cache, eth_link_change_callback, NULL) == 0);

    HL_ASSERT(hl_callback_create(&callback) == 0);
    HL_ASSERT(hl_tpool_create_thread(&th, eth_carrier_poll_thread, NULL, 0, 0, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(th, 0) == 1);
}

void hl_eth_trig_event(void)
{
    nl_cache_foreach(cache, eth_foreach_for_trig, NULL);
}

void hl_eth_register_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_register(callback, function, user_data);
}

void hl_eth_unregister_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_unregister(callback, function, user_data);
}

static void eth_carrier_poll_thread(hl_tpool_thread_t thread, void *args)
{
    for (;;)
    {
        nl_cache_foreach(cache, eth_foreach_for_up, NULL);
        nl_cache_mngr_poll(mngr, 1000);
    }
}

static void eth_link_change_callback(struct nl_cache *cache, struct nl_object *obj, int action, void *args)
{
    struct rtnl_link *rtnl_link = (struct rtnl_link *)obj;
    hl_eth_event_t event;

    pthread_mutex_lock(&lock);
    if (strstr(rtnl_link_get_name(rtnl_link), "eth"))
    {
        strncpy(event.interface, rtnl_link_get_name(rtnl_link), sizeof(event.interface));
        if (rtnl_link_get_carrier(rtnl_link))
            event.id = HL_ETH_EVENT_CARRIER_ON;
        else
            event.id = HL_ETH_EVENT_CARRIER_OFF;
        pthread_mutex_unlock(&lock);
        hl_callback_call(callback, &event);
        return;
    }
    pthread_mutex_unlock(&lock);
}

static void eth_link_up_callback(struct nl_cache *cache, struct nl_object *obj, int action, void *args)
{
    struct rtnl_link *rtnl_link = (struct rtnl_link *)obj;
    hl_eth_event_t event;
    pthread_mutex_lock(&lock);
    if (strstr(rtnl_link_get_name(rtnl_link), "eth"))
    {
        uint32_t flgas = rtnl_link_get_flags(rtnl_link);
        if ((flgas & IFF_UP) == 0)
            hl_netif_set_interface_enable2(rtnl_link_get_name(rtnl_link), 1);
    }
    pthread_mutex_unlock(&lock);
}

static void eth_foreach_for_trig(struct nl_object *obj, void *args)
{
    eth_link_change_callback(NULL, obj, 0, args);
}

static void eth_foreach_for_up(struct nl_object *obj, void *args)
{
    eth_link_up_callback(NULL, obj, 0, args);
}
#endif