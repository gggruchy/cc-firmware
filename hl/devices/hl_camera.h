#ifndef HL_CAMERA_H
#define HL_CAMERA_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "hl_callback.h"
    typedef enum
    {
        HL_CAMERA_STATE_INITIALIZED = 0x01 << 0,
        HL_CAMERA_STATE_CAPTURED = 0x01 << 1,
        HL_CAMERA_STATE_FOUND_VIDEO = 0x01 << 2,
    } hl_camera_state_t;

    typedef enum
    {
        HL_CAMERA_EVENT_INITIALIZED,
        HL_CAMERA_EVENT_DEINITIALIZED,
        HL_CAMERA_EVENT_CAPTURE_ON,
        HL_CAMERA_EVENT_CAPTURE_OFF,
        HL_CAMERA_EVENT_FRAME,

    } hl_camera_event_id_t;

    typedef struct
    {
        hl_camera_event_id_t event;
        uint8_t *data;
        uint32_t data_length;
        uint32_t format;
    } hl_camera_event_t;

    void hl_camera_init(void);
    hl_camera_state_t hl_camera_get_state(void);
    uint32_t hl_camera_get_format(void);
    void hl_camera_get_size(int *width, int *height);

    void hl_camera_register(hl_callback_function_t func, void *user_data);
    void hl_camera_unregister(hl_callback_function_t func, void *user_data);
    int hl_camera_capture_on(void);
    void hl_camera_capture_off(void);
    void hl_camera_scan_enable(int enable);
    int hl_camera_get_exist_state(void);
    int camera_control_light(int light_state);
#ifdef __cplusplus
}
#endif
#endif