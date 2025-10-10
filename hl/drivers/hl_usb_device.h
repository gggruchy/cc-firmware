#ifndef HL_USB_DEVICE_H
#define HL_USB_DEVICE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "hl_callback.h"

    void hl_usb_device_init(const char *module_directory);

#ifdef __cplusplus
}
#endif

#endif