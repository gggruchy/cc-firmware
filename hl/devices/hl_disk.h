#ifndef HL_DISK_H
#define HL_DISK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "hl_callback.h"
#define HL_DISK_DIVIDE_SIZE_IGNORE -1
#define HL_DISK_DIVIDE_SIZE_LEAVE 0
#define HL_DISK_EVENT_DISK_NAME_MAX_LEN 128
    typedef enum
    {
        HL_DISK_TYPE_UNKNOWN = -1,
        HL_DISK_TYPE_USB,
        HL_DISK_TYPE_EMMC,
        HL_DISK_TYPE_NUMBERS,
    } hl_disk_type_t;

    typedef enum
    {
        HL_DISK_FS_TYPE_UNKNOWN = -1,
        HL_DISK_FS_TYPE_FAT,
        HL_DISK_FS_TYPE_EXFAT,
        HL_DISK_FS_TYPE_NTFS,
    } hl_disk_fs_type_t;
    typedef enum
    {
        HL_DISK_EVENT_MOUNTED = 0,
        HL_DISK_EVENT_UNMOUNTED,
    } hl_disk_event_id_t;
    typedef struct
    {
        hl_disk_event_id_t id;
        char devname[HL_DISK_EVENT_DISK_NAME_MAX_LEN];
    } hl_disk_event_t;

    void hl_disk_init(void);
    void hl_disk_set_default_partition(hl_disk_type_t type, int partition_index);
    int hl_disk_divide_emmc_partition(int partition_numbers, const int32_t *partition_size_in_mbytes);
    int hl_disk_identify_emmc_partition(int partition_numbers, const int32_t *partition_size_in_mbytes, const hl_disk_fs_type_t *fs_type, int *check_results);
    int hl_disk_format_partition(hl_disk_type_t type, int partition_index, hl_disk_fs_type_t fs_type);
    int hl_disk_format_default_partition(hl_disk_type_t type, hl_disk_fs_type_t fs_type);
    int hl_disk_get_mountpoint(hl_disk_type_t type, int partition_index, const char *name, char *path, uint32_t size);
    int hl_disk_get_default_mountpoint(hl_disk_type_t type, const char *name, char *path, uint32_t size);

    hl_disk_fs_type_t hl_disk_get_fstype(hl_disk_type_t type, int partition_index);
    hl_disk_fs_type_t hl_disk_get_default_fstype(hl_disk_type_t type);
    uint64_t hl_disk_get_size(hl_disk_type_t type, int partition_index);
    uint64_t hl_disk_get_default_size(hl_disk_type_t type);

    int hl_disk_is_mounted(hl_disk_type_t type, int partition_index);
    int hl_disk_default_is_mounted(hl_disk_type_t type);
    void hl_disk_register_event_callback(hl_callback_function_t function, void *user_data);

#ifdef __cplusplus
}
#endif

#endif