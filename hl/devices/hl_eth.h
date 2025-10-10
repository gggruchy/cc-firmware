#ifndef HL_ETH_H
#define HL_ETH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "hl_callback.h"

    typedef enum
    {
        HL_ETH_EVENT_CARRIER_ON,
        HL_ETH_EVENT_CARRIER_OFF,
    } hl_eth_event_id_t;
    typedef struct
    {
        hl_eth_event_id_t id;
        char interface[64];
    } hl_eth_event_t;

    int hl_eth_init(void);
    void hl_eth_trig_event(void);
    void hl_eth_register_event_callback(hl_callback_function_t function, void *user_data);
    void hl_eth_unregister_event_callback(hl_callback_function_t function, void *user_data);

#ifdef __cplusplus
}
#endif

#endif