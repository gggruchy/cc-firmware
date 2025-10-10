#ifndef HL_NETLINK_UEVENT_H
#define HL_NETLINK_UEVENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "hl_callback.h"

    typedef struct
    {
        char action[64];
        char devname[64];
        char devtype[64];
        char product[64];
        char interface[64];
    } hl_netlink_uevent_msg_t;

    void hl_netlink_uevent_init(void);
    void hl_netlink_uevent_register_callback(hl_callback_function_t function, void *user_data);
    void hl_netlink_uevent_unregister_callback(hl_callback_function_t function, void *user_data);
#ifdef __cplusplus
}
#endif

#endif