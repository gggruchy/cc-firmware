#ifndef SIMPLEBUS_H
#define SIMPLEBUS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void (*simple_bus_subscribe_callback_t)(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len);
    typedef void (*simple_bus_service_callback_t)(const char *name, void *context, int request_id, void *args, void *response);

    void simple_bus_init(void);
    void simple_bus_deinit(void);

    // 订阅发布模型
    int simple_bus_subscribe(const char *name, void *context, simple_bus_subscribe_callback_t callback);
    int simple_bus_unsubscribe(const char *name, void *context, simple_bus_subscribe_callback_t callback);
    int simple_bus_publish_sync(const char *name, int msg_id, void *msg, uint32_t msg_len);
    int simple_bus_publish_async(const char *name, int msg_id, void *msg, uint32_t msg_len);

    // 请求服务模型
    int simple_bus_register_service(const char *name, void *context, simple_bus_service_callback_t callback);
    int simple_bus_unregister_service(const char *name, void *context, simple_bus_service_callback_t callback);
    int simple_bus_request(const char *name, int request_id, void *args, void *response);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif