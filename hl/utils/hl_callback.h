#ifndef HL_CALLBACK_H
#define HL_CALLBACK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void *hl_callback_t;
    typedef void (*hl_callback_function_t)(const void *data, void *user_data);

    int hl_callback_create(hl_callback_t *cb);
    void hl_callback_destory(hl_callback_t *cb);
    void hl_callback_call(hl_callback_t cb, const void *data);
    void hl_callback_register(hl_callback_t cb, hl_callback_function_t function, void *user_data);
    void hl_callback_unregister(hl_callback_t cb, hl_callback_function_t function, void *user_data);

#ifdef __cplusplus
}
#endif

#endif