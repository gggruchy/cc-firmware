#include "hl_disk.h"
#include "hl_netlink_uevent.h"
#include "hl_common.h"
#include "hl_tpool.h"
#include "hl_assert.h"
#include "hl_list.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>

#include "simplebus.h"
#include "srv_state.h"

#define LOG_TAG "hl_disk"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define DISK_PATH_MAX_LEN 1024
#define DISK_NAME_MAX_LEN 128
#define DISK_RETRY_MOUNT 5

typedef enum
{
    DISK_THREAD_MSG_ID_APPEND_ENTRY,
    DISK_THREAD_MSG_ID_REMOVE_ENTRY,
} disk_thread_msg_id_t;

typedef struct
{
    disk_thread_msg_id_t msg_id;
    char entry_name[256];
} disk_thread_msg_t;

typedef struct disk_entry_tag
{
    char devname[DISK_NAME_MAX_LEN];
    hl_disk_type_t disk_type;
    int partition_index;
    int mounted;
    hl_disk_fs_type_t fs_type;
    uint64_t size;
    int retry;
    int is_partition;
} disk_entry_t;

static int disk_default_partition_index[HL_DISK_TYPE_NUMBERS];
static hl_list_t disk_list;
static pthread_rwlock_t glock = PTHREAD_RWLOCK_INITIALIZER;
static hl_tpool_thread_t disk_thread;
static hl_callback_t disk_callback;

static void netlink_uevent_callback(const void *data, void *user_data);
static void disk_entry_scan(void);
static void disk_routine(hl_tpool_thread_t thread, void *args);

static int disk_entry_read_link(const char *devname, char *buf, uint32_t size);
static int disk_get_prev_name(const char *path, char *name, uint32_t size);

static hl_disk_type_t disk_entry_get_type(const char *devname);
static int disk_entry_get_partition_index(const char *devname);
static int disk_entry_is_partition(const char *devname);

static void disk_append_entry(const char *devname);
static void disk_remove_entry(const char *devname);
static int disk_entry_init(disk_entry_t *entry);
static void disk_entry_deinit(disk_entry_t *entry);
static int disk_mount(disk_entry_t *entry);
static disk_entry_t *disk_list_get_entry(hl_disk_type_t disk_type, int partition_index);

void hl_disk_init(void)
{
    HL_ASSERT(hl_list_create(&disk_list, sizeof(disk_entry_t)) == 0);
    HL_ASSERT(hl_callback_create(&disk_callback) == 0);
    HL_ASSERT(hl_tpool_create_thread(&disk_thread, disk_routine, NULL, sizeof(disk_thread_msg_t), 32, 0, 0) == 0);
    HL_ASSERT(hl_tpool_wait_started(disk_thread, 0) == 1);
}

void hl_disk_set_default_partition(hl_disk_type_t type, int partition_index)
{
    disk_default_partition_index[type] = partition_index;
}

int hl_disk_divide_emmc_partition(int partition_numbers, const int32_t *partition_size_in_mbytes)
{
    // 删除所有分区
    for (int i = 0; i < 4; i++)
    {
        if (partition_size_in_mbytes[i] == HL_DISK_DIVIDE_SIZE_IGNORE)
            continue;
        hl_system("echo -e 'd\n%d\nw\n' | fdisk /dev/mmcblk0", i + 1);
    }

    // 重建分区
    for (int i = 0; i < partition_numbers; i++)
    {
        if (partition_size_in_mbytes[i] == HL_DISK_DIVIDE_SIZE_IGNORE)
            continue;

        if (partition_size_in_mbytes[i] == HL_DISK_DIVIDE_SIZE_LEAVE)
            hl_system("echo -e 'n\np\n%d\n\n\n\nw\n' | fdisk /dev/mmcblk0", i + 1);
        else
            hl_system("echo -e 'n\np\n%d\n\n+%dM\nw\n' | fdisk /dev/mmcblk0", i + 1, partition_size_in_mbytes[i]);
    }
}

int hl_disk_identify_emmc_partition(int partition_numbers, const int32_t *partition_size_in_mbytes, const hl_disk_fs_type_t *fs_type, int *check_results)
{
    disk_entry_t entry;
    memset(check_results, 0, partition_numbers * sizeof(int));

    for (int i = 0; i < partition_numbers; i++)
    {
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.devname, sizeof(entry.devname), "mmcblk0p%d", i + 1);
        int retry = 0;
        // 尝试三次
        do
        {
            if (disk_entry_init(&entry) == 0)
            {

                // 误差小于64M且文件系统匹配
                if ((abs((entry.size / 1024 / 1024) - partition_size_in_mbytes[i]) < 64 || partition_size_in_mbytes[i] == HL_DISK_DIVIDE_SIZE_IGNORE) &&
                    entry.fs_type == fs_type[i])
                {
                    check_results[i] = 1;
                    disk_entry_deinit(&entry);
                    break;
                }
                else
                {
                    printf("abs %d --- type %d\n", abs((entry.size / 1024 / 1024) - partition_size_in_mbytes[i]), entry.fs_type == fs_type[i]);
                    printf("devname %s entry.size %d fs_type %d\n", entry.devname, entry.size / 1024 / 1024, entry.fs_type);
                    printf("entry.size %d fs_type %d\n", partition_size_in_mbytes[i], fs_type[i]);
                }
                disk_entry_deinit(&entry);
            }
        } while (retry++ < 3);
    }

    for (int i = 0; i < partition_numbers; i++)
    {
        if (check_results[i] == 0)
            return -1;
    }
    return 0;
}

int hl_disk_format_partition(hl_disk_type_t type, int partition_index, hl_disk_fs_type_t fs_type)
{
    DIR *dirp;
    struct dirent *dp;
    char syspath[DISK_PATH_MAX_LEN];
    const char add[] = "add";
    dirp = opendir("/sys/class/block");
    int ret = 0;
    if (dirp == NULL)
        return -1;

    while ((dp = readdir(dirp)) != NULL)
    {
        int is_partition = disk_entry_is_partition(dp->d_name);
        hl_disk_type_t disk_type = disk_entry_get_type(dp->d_name);
        if (disk_type != type)
            continue;

        int disk_partition_index;
        if (is_partition)
            disk_partition_index = disk_entry_get_partition_index(dp->d_name) - 1;
        else
            disk_partition_index = 0;

        if (disk_partition_index == partition_index)
        {
            // 先卸载后格式化
            hl_system("umount -f -l /mnt/%s", "exUDISK");
            if (fs_type == HL_DISK_FS_TYPE_EXFAT)
                ret = hl_system("mkfs.exfat /dev/%s", dp->d_name);
            else if (fs_type == HL_DISK_FS_TYPE_NTFS)
                ret = hl_system("mkfs.ntfs -f /dev/%s", dp->d_name);
            else if (fs_type == HL_DISK_FS_TYPE_FAT)
                ret = hl_system("mkfs.vfat /dev/%s", dp->d_name);
            break;
        }
    }
    closedir(dirp);
    return ret;
}

int hl_disk_format_default_partition(hl_disk_type_t type, hl_disk_fs_type_t fs_type)
{
    hl_disk_format_partition(type, disk_default_partition_index[type], fs_type);
}

int hl_disk_get_mountpoint(hl_disk_type_t type, int partition_index, const char *name, char *path, uint32_t size)
{
    if (type == HL_DISK_TYPE_EMMC)
    {
        if (name == NULL)
        {
#if CONFIG_BOARD == PROJECT_BOARD_E100
            snprintf(path, size, "/user-resource");
#else
            snprintf(path, size, "/board-resource");
#endif
        }
        else
        {
#if CONFIG_BOARD == PROJECT_BOARD_E100
            snprintf(path, size, "/user-resource/%s", name);
#else
            snprintf(path, size, "/board-resource/%s", name);
#endif
        }
        return 0;
    }
    pthread_rwlock_rdlock(&glock);
    disk_entry_t *entry = disk_list_get_entry(type, partition_index);
    if (entry && entry->mounted)
    {
        if (name == NULL)
            snprintf(path, size, "/mnt/%s", "exUDISK");
        else
            snprintf(path, size, "/mnt/%s/%s", "exUDISK", name);
        pthread_rwlock_unlock(&glock);
        return 0;
    }
    pthread_rwlock_unlock(&glock);
    *path = '\0';
    return -1;
}

int hl_disk_get_default_mountpoint(hl_disk_type_t type, const char *name, char *path, uint32_t size)
{
    return hl_disk_get_mountpoint(type, disk_default_partition_index[type], name, path, size);
}

hl_disk_fs_type_t hl_disk_get_fstype(hl_disk_type_t type, int partition_index)
{
    pthread_rwlock_rdlock(&glock);
    disk_entry_t *entry = disk_list_get_entry(type, partition_index);
    if (entry && entry->mounted)
    {
        pthread_rwlock_unlock(&glock);
        return entry->fs_type;
    }
    pthread_rwlock_unlock(&glock);
    return -1;
}

hl_disk_fs_type_t hl_disk_get_default_fstype(hl_disk_type_t type)
{
    return hl_disk_get_fstype(type, disk_default_partition_index[type]);
}

int hl_disk_is_mounted(hl_disk_type_t type, int partition_index)
{
    pthread_rwlock_rdlock(&glock);
    disk_entry_t *entry = disk_list_get_entry(type, partition_index);
    if (entry)
    {
        pthread_rwlock_unlock(&glock);
        return entry->mounted;
    }
    pthread_rwlock_unlock(&glock);
    return 0;
}

int hl_disk_default_is_mounted(hl_disk_type_t type)
{
    if (type == HL_DISK_TYPE_EMMC)
    {
        return 1;
    }
    return hl_disk_is_mounted(type, disk_default_partition_index[type]);
}

uint64_t hl_disk_get_size(hl_disk_type_t type, int partition_index)
{
    pthread_rwlock_rdlock(&glock);
    disk_entry_t *entry = disk_list_get_entry(type, partition_index);
    if (entry && entry->mounted)
    {
        pthread_rwlock_unlock(&glock);
        return entry->size;
    }
    pthread_rwlock_unlock(&glock);
    return -1;
}

uint64_t hl_disk_get_default_size(hl_disk_type_t type)
{
    return hl_disk_get_size(type, disk_default_partition_index[type]);
}

static void disk_routine(hl_tpool_thread_t thread, void *args)
{
    disk_thread_msg_t thread_msg;
    static int count = 0, entry_init = 0;

    hl_netlink_uevent_register_callback(netlink_uevent_callback, NULL);
    disk_entry_scan();

    for (;;)
    {
        if (hl_tpool_thread_recv_msg_try(thread, &thread_msg) == 0)
        {
            if (thread_msg.msg_id == DISK_THREAD_MSG_ID_APPEND_ENTRY)
            {
                disk_append_entry(thread_msg.entry_name);
                entry_init++;
            }
            else if (thread_msg.msg_id == DISK_THREAD_MSG_ID_REMOVE_ENTRY)
            {
                disk_remove_entry(thread_msg.entry_name);
            }
        }

        if (count < 5 && entry_init)
        {
            count++;
            // LOG_I("disk %s not ready, retry %d\n", thread_msg.entry_name, count);
            usleep(10);
        }
        else
        {
            hl_list_node_t node, tmp_node;
            node = disk_list;
            node = hl_list_get_next_node(disk_list);
            tmp_node = hl_list_get_next_node(node);
            if ( node != disk_list)
            {
                disk_entry_t *entry = hl_list_get_data(node);
                // LOG_I("disk %s mounted %d，is_partition = %d，retry = %d\n", entry->devname, entry->mounted, entry->is_partition, entry->retry);
                // 持续挂载未挂载的磁盘
                if (entry->mounted == 0)
                {
                    if (entry->is_partition)
                    {
                        if (entry->retry < DISK_RETRY_MOUNT)
                            disk_entry_init(entry);
                    }
                    else if (disk_entry_init(entry) != 0)
                    {
                        disk_remove_entry(entry->devname);
                    }
                }
                node = tmp_node;
                tmp_node = hl_list_get_next_node(node);
            }
            count = 0;
            if (entry_init > 0)
                entry_init--;
        }
        usleep(100000);
    }
}

static void disk_entry_scan(void)
{
    DIR *dirp;
    struct dirent *dp;
    char syspath[DISK_PATH_MAX_LEN];
    const char add[] = "add";
    dirp = opendir("/sys/class/block");

    if (dirp == NULL)
        return;

    while ((dp = readdir(dirp)) != NULL)
    {
        hl_disk_type_t type = disk_entry_get_type(dp->d_name);
        if (type != HL_DISK_TYPE_UNKNOWN)
        {
            snprintf(syspath, sizeof(syspath), "/sys/class/block/%s/uevent", dp->d_name);
            printf("echo to syspath %s\n", syspath);
            hl_echo(syspath, add, sizeof(add));
        }
    }
    closedir(dirp);
}

static void disk_append_entry(const char *devname)
{
    hl_disk_type_t disk_type = disk_entry_get_type(devname);
    hl_list_node_t node;
    if (disk_type == HL_DISK_TYPE_UNKNOWN)
        return;

    // 1)查找是否已经挂载了磁盘
    pthread_rwlock_rdlock(&glock);
    node = disk_list;
    while ((node = hl_list_get_next_node(node)) != disk_list)
    {
        disk_entry_t *entry = hl_list_get_data(node);
        // if (strcmp(entry->devname, devname) == 0)
        // {
        //     pthread_rwlock_unlock(&glock);
        //     return;
        // }
        if (entry->mounted)
        {
            pthread_rwlock_unlock(&glock);
            return;
        }
    }
    pthread_rwlock_unlock(&glock);

    // 2) 初始化推入队列
    disk_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.is_partition = disk_entry_is_partition(devname);
    strncpy(entry.devname, devname, sizeof(entry.devname));
    if (entry.is_partition)
        entry.partition_index = disk_entry_get_partition_index(devname) - 1;
    else
        entry.partition_index = 0;
    entry.disk_type = disk_type;

    pthread_rwlock_wrlock(&glock);
    hl_list_push_back(disk_list, &entry);
    pthread_rwlock_unlock(&glock);
}

static void disk_remove_entry(const char *devname)
{
    hl_disk_type_t disk_type = disk_entry_get_type(devname);
    hl_list_node_t node, pos;
    if (disk_type == HL_DISK_TYPE_UNKNOWN)
        return;
    pthread_rwlock_wrlock(&glock);
    node = disk_list;
    while ((node = hl_list_get_next_node(node)) != disk_list)
    {
        disk_entry_t *entry = hl_list_get_data(node);
        if (strcmp(entry->devname, devname) == 0)
        {
            if (entry->mounted)
                disk_entry_deinit(entry);
            hl_list_remove(disk_list, node);
            pthread_rwlock_unlock(&glock);
            return;
        }
    }
    pthread_rwlock_unlock(&glock);
}

static int disk_entry_init(disk_entry_t *entry)
{
    // 1. 建立挂载目录
    struct stat statbuf;
    if (stat("/mnt/exUDISK", &statbuf) == 0)
    {
        if (!S_ISDIR(statbuf.st_mode))
        {
            LOG_W("file /mnt/exUDISK is exist\n");
            // It is a file, try to remove it
            hl_system("rm /mnt/exUDISK");
        }
    }
    char path[DISK_PATH_MAX_LEN];
    hl_system("mkdir -p /mnt/%s", "exUDISK");
    hl_system("umount -f -l /mnt/%s", "exUDISK");

    // 2. 尝试挂载
    int mountresult = 0;
    // 先尝试NTFS挂载
    mountresult = hl_system("mount.ntfs-3g /dev/%s /mnt/%s", entry->devname, "exUDISK");
    if (mountresult != 0)
        mountresult = hl_system("mount -t vfat,exfat -o iocharset=utf8 /dev/%s /mnt/%s", entry->devname, "exUDISK");

    if (mountresult == 0)
    {
        // 判断是否真正挂载成功
        FILE *fp = fopen("/proc/mounts", "r");
        char linebuf[1024];
        snprintf(path, sizeof(path), "/dev/%s", entry->devname);
        if (fp)
        {
            while (fgets(linebuf, sizeof(linebuf), fp) != NULL)
            {
                if ((strstr(linebuf, path) != NULL) && (strstr(linebuf, "rw") == NULL))
                {
                    fclose(fp);
                    goto umount;
                }
            }
            fclose(fp);
        }
        else
        {
            goto umount;
        }
        entry->mounted = 1;
        entry->retry = 0;
        // 读取文件格式信息
        struct statfs st;
        snprintf(path, sizeof(path), "/mnt/%s", "exUDISK");
        if (statfs(path, &st) == 0)
        {
            if (st.f_type == 0x4d44)
                entry->fs_type = HL_DISK_FS_TYPE_FAT;
            else if (st.f_type == 0x2011BAB0)
                entry->fs_type = HL_DISK_FS_TYPE_EXFAT;
            else if (st.f_type == 0x5346544e || st.f_type == 0x65735546)
                entry->fs_type = HL_DISK_FS_TYPE_NTFS;
            else
                entry->fs_type = HL_DISK_FS_TYPE_UNKNOWN;
            entry->size = st.f_bsize * st.f_blocks;
        }
        else
        {
            entry->fs_type = HL_DISK_FS_TYPE_UNKNOWN;
            entry->size = 0;
        }
        LOG_I("disk %s mounted\n", entry->devname);
        hl_disk_event_t event_data;
        event_data.id = HL_DISK_EVENT_MOUNTED;
        strncpy(event_data.devname, entry->devname, sizeof(event_data.devname));
        hl_callback_call(disk_callback, &event_data);
    }
    else
    {
        goto remove;
    }
    return 0;
umount:
    hl_system("umount -f -l /mnt/%s", "exUDISK");
remove:
    hl_system("rmdir /mnt/%s", "exUDISK");
    entry->retry++;
    return -1;
}

static void disk_entry_deinit(disk_entry_t *entry)
{
    hl_system("umount -f -l /mnt/%s", "exUDISK");
    hl_system("rmdir /mnt/%s", "exUDISK");
    entry->mounted = 0;
    LOG_I("disk %s unmounted\n", entry->devname);
    hl_disk_event_t event_data;
    event_data.id = HL_DISK_EVENT_UNMOUNTED;
    strncpy(event_data.devname, entry->devname, sizeof(event_data.devname));
    hl_callback_call(disk_callback, &event_data);
}

static int disk_entry_is_partition(const char *devname)
{
    char linkpath[DISK_PATH_MAX_LEN];
    char prevname[DISK_NAME_MAX_LEN];

    if (disk_entry_read_link(devname, linkpath, sizeof(linkpath)) != 0)
    {
        printf("disk_entry_read_link failed %s\n", devname);
        return -1;
    }

    if (disk_get_prev_name(linkpath, prevname, sizeof(prevname) != 0))
    {
        printf("disk_get_prev_name failed %s\n", devname);
        return -1;
    }

    if (strcmp(prevname, "block") == 0)
        return 0;
    return 1;
}

static hl_disk_type_t disk_entry_get_type(const char *devname)
{
    // 仅识别USB
#if 0
    if (strstr(devname, "mmcblk") == devname)
    {
        if (strstr(devname, "rpmb") || strstr(devname, "boot"))
            return HL_DISK_TYPE_UNKNOWN;
        return HL_DISK_TYPE_EMMC;
    }
    else
#endif
    if (strstr(devname, "sd") == devname)
        return HL_DISK_TYPE_USB;
    else
        return HL_DISK_TYPE_UNKNOWN;
}

static int disk_entry_get_partition_index(const char *devname)
{
    const char *p = devname + strlen(devname) - 1;
    while (*p != '\0')
    {
        if (isdigit(*p))
            p--;
        else
            break;
    }
    if (*p)
        return atoi(p + 1);
    return -1;
}

static int disk_entry_read_link(const char *devname, char *buf, uint32_t size)
{
    char syspath[DISK_PATH_MAX_LEN];
    struct stat st;
    snprintf(syspath, sizeof(syspath), "/sys/class/block/%s", devname);
    stat(syspath, &st);
    if (S_ISDIR(st.st_mode))
    {
        int len = readlink(syspath, buf, size);
        if (len > 0)
            buf[len] = '\0';
        else
            return -1;
    }
    else
        return -1;
    return 0;
}

static int disk_get_prev_name(const char *path, char *name, uint32_t size)
{
    const char *p = path + strlen(path) - 1;
    const char *pstart = NULL;
    const char *pend = NULL;

    if (*p == '\0')
        return -1;

    while (*p != '\0')
    {
        if (*p != '/')
            p--;
        else
        {
            if (pend == NULL)
            {
                pend = p - 1;
            }
            else if (pstart == NULL)
            {
                pstart = p + 1;
                break;
            }
            p--;
        }
    }

    // printf("pend %c pstart %c\n", *pend, *pstart);

    if (pend && pstart)
    {
        strncpy(name, pstart, pend - pstart + 1);
        name[pend - pstart + 1] = '\0';
        return 0;
    }

    return -1;
}

static disk_entry_t *disk_list_get_entry(hl_disk_type_t disk_type, int partition_index)
{
    hl_list_node_t node;
    node = disk_list;
    while ((node = hl_list_get_next_node(node)) != disk_list)
    {
        disk_entry_t *entry = hl_list_get_data(node);
        if (entry->disk_type == disk_type && entry->partition_index == partition_index)
            return entry;
    }
    return NULL;
}

static void netlink_uevent_callback(const void *data, void *user_data)
{
    hl_netlink_uevent_msg_t *uevent_msg = (hl_netlink_uevent_msg_t *)data;

    if (strstr(uevent_msg->devtype, "disk") || strstr(uevent_msg->devtype, "partition"))
    {
        disk_thread_msg_t msg;
        if (strcmp(uevent_msg->action, "add") == 0)
        {
            msg.msg_id = DISK_THREAD_MSG_ID_APPEND_ENTRY;
            strncpy(msg.entry_name, uevent_msg->devname, sizeof(msg.entry_name));
            hl_tpool_send_msg(disk_thread, &msg);
        }
        else if (strcmp(uevent_msg->action, "remove") == 0)
        {
            msg.msg_id = DISK_THREAD_MSG_ID_REMOVE_ENTRY;
            strncpy(msg.entry_name, uevent_msg->devname, sizeof(msg.entry_name));
            hl_tpool_send_msg(disk_thread, &msg);
        }
    }
}

void hl_disk_register_event_callback(hl_callback_function_t function, void *user_data)
{
    hl_callback_register(disk_callback, function, user_data);
}