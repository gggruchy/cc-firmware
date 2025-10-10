#ifndef UTILS_PARAMS_H
#define UTILS_PARAMS_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "utils.h"

    typedef struct utils_params_header_tag
    {
        int magic;
        int size;
        uint16_t crc;
    } utils_params_header_t;

    int utils_params_read(void *params, uint32_t size, const char *path);
    int utils_params_write(void *params, uint32_t size, const char *path);
    
#ifdef __cplusplus
}
#endif
#endif