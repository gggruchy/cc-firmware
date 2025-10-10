#ifndef HL_RINGBUFFER_H
#define HL_RINGBUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#define WLAN_ENTRY_FILE_PATH "/board-resource/wlan_entry"
    typedef void *hl_ringbuffer_t;
    typedef void *hl_ringbuffer_db_t;

    int hl_ringbuffer_create(hl_ringbuffer_t *ringbuffer, uint32_t size, uint32_t length);
    void hl_ringbuffer_destory(hl_ringbuffer_t *ringbuffer);
    void hl_ringbuffer_reset(hl_ringbuffer_t ringbuffer);

    int hl_ringbuffer_set(hl_ringbuffer_t ringbuffer, uint32_t index, const void *data);
    int hl_ringbuffer_get(hl_ringbuffer_t ringbuffer, uint32_t index, void *data);
    int hl_ringbuffer_del(hl_ringbuffer_t ringbuffer, uint32_t index);
    void hl_ringbuffer_push(hl_ringbuffer_t ringbuffer, const void *data);

    void hl_ringbuffer_foreach(hl_ringbuffer_t ringbuffer, int (*callback)(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args), void *args);
    void hl_ringbuffer_foreach_reverse(hl_ringbuffer_t ringbuffer, int (*callback)(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args), void *args);

    uint32_t hl_ringbuffer_get_available_length(hl_ringbuffer_t ringbuffer);
    uint32_t hl_ringbuffer_get_length(hl_ringbuffer_t ringbuffer);
    uint32_t hl_ringbuffer_get_size(hl_ringbuffer_t ringbuffer);

    int hl_ringbuffer_db_open(const char *db_file, hl_ringbuffer_db_t *ringbuffer_db, uint32_t size, uint32_t length);
    void hl_ringbuffer_db_close(hl_ringbuffer_db_t *ringbuffer_db);
    void hl_ringbuffer_db_sync(hl_ringbuffer_db_t ringbuffer_db);
    hl_ringbuffer_t hl_ringbuffer_db_get(hl_ringbuffer_db_t ringbuffer_db);

#ifdef __cplusplus
}
#endif

#endif