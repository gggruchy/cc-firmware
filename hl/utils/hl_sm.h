#ifndef HL_SM_H
#define HL_SM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define HL_SM_EVENT_ID_IDLE -1
#define HL_SM_EVENT_ID_ENTRY -2
#define HL_SM_EVENT_ID_EXIT -3
#define HL_SM_EVENT_ID_USER 0

    typedef void *hl_sm_t;
    typedef void (*hl_sm_state_t)(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);

    int hl_sm_create(hl_sm_t *sm, hl_sm_state_t init_state, void *entry_event_data, uint32_t entry_event_data_size, uint32_t event_queue_length, void *user_data);
    void hl_sm_destroy(hl_sm_t *sm);

    int hl_sm_send_event(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size);
    void hl_sm_dispatch_event(hl_sm_t sm);
    int hl_sm_trans_state(hl_sm_t sm, hl_sm_state_t state, const void *entry_event_data, uint32_t entry_event_data_size);

    hl_sm_state_t hl_sm_get_current_state(hl_sm_t sm);
    hl_sm_state_t hl_sm_get_prev_state(hl_sm_t sm);
    void hl_sm_set_user_data(hl_sm_t sm, void *user_data);
    void *hl_sm_get_user_data(hl_sm_t sm);
    int hl_sm_get_event_queue_length(hl_sm_t sm);
#ifdef __cplusplus
}
#endif

#endif