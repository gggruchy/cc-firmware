#include "hl_usb_device.h"
#include "hl_netlink_uevent.h"
#include "hl_common.h"
#include "hl_assert.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define LOG_TAG "hl_usb_device"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define MODULE_TRACK_LIST_SIZE 4

typedef struct
{
    uint16_t vid;
    uint16_t pid;
    const char *module_name;
} usb_info_t;

typedef struct
{
    uint16_t vid;
    uint16_t pid;
} usb_modeswitch_info_t;

typedef struct
{
    const char *module_name;
    uint8_t ref_count;
} module_track_t;

static const usb_info_t usb_info[] = {
    {0x0bda, 0xb82b, "8821cu"},//双频单模
    {0x0bda, 0xb820, "8821cu"},
    {0x0bda, 0xC821, "8821cu"},
    {0x0bda, 0xC820, "8821cu"},
    {0x0bda, 0xC82A, "8821cu"},
    {0x0bda, 0xC82B, "8821cu"},
    {0x0bda, 0xC811, "8821cu"},
    {0x0bda, 0x8811, "8821cu"},
    {0x0bda, 0x8731, "8821cu"},
    {0x0bda, 0xC80C, "8821cu"},

    {0x0bda, 0xD723, "8723du"},
    {0x7392, 0xd611, "8723du"},//双频双模

	{0xa69c, 0x8d80, "aic8800"},//aic8800
};

static const usb_modeswitch_info_t usb_modeswitch_info[] = {
    {0x0bda, 0x1a2b},
    {0x0bda, 0xa192},
};

static const char *local_module_directory;

static module_track_t module_track_list[MODULE_TRACK_LIST_SIZE] = {0};
int track_list_count = 0;

static void netlink_uevent_callback(const void *data, void *user_data);
static void usb_device_insmod(const usb_info_t *info);
static void usb_device_rmmod(const usb_info_t *info);

static void module_track_init(void);
static int module_track_on(const char *module_name);
static int module_track_off(const char *module_name);
static int module_is_exists(const char *module_name);

void hl_usb_device_init(const char *module_directory)
{
    HL_ASSERT(module_directory);
    HL_ASSERT(strlen(module_directory) > 0);
    local_module_directory = module_directory;
    module_track_init();
    hl_netlink_uevent_register_callback(netlink_uevent_callback, NULL);
}

static void netlink_uevent_callback(const void *data, void *user_data)
{
    hl_netlink_uevent_msg_t *uevent_msg = (hl_netlink_uevent_msg_t *)data;
    const usb_info_t *info;
    uint16_t vid, pid;
    int i;

    if (strlen(uevent_msg->devtype) == 0 || strcmp(uevent_msg->devtype, "usb_device") != 0)
        return;

    if (strlen(uevent_msg->product) == 0 || hl_parse_usb_id(uevent_msg->product, &vid, &pid) != 0)
        return;

    // TODO:Cold Pulg
    for (i = 0; i < sizeof(usb_modeswitch_info) / sizeof(usb_modeswitch_info[0]); i++)
    {
        if (vid == usb_modeswitch_info[i].vid && pid == usb_modeswitch_info[i].pid)
        {
            LOG_I("usb_device %04x:%04x need usb_modeswitch\n", vid, pid);
            if (hl_system("usb_modeswitch -K -v %04x -p %04x", vid, pid) != 0)
                LOG_I("usb_device %04x:%04x usb_modeswitch failed\n", vid, pid);
            return;
        }
    }

    for (i = 0; i < sizeof(usb_info) / sizeof(usb_info[0]); i++)
    {
        if (vid == usb_info[i].vid && pid == usb_info[i].pid)
        {
            info = &usb_info[i];
            break;
        }
    }
    if (i == sizeof(usb_info) / sizeof(usb_info[0]))
        return;

    if (strcmp(uevent_msg->action, "add") == 0)
        usb_device_insmod(info);
    else if (strcmp(uevent_msg->action, "remove") == 0)
        usb_device_rmmod(info);
}

static void usb_device_insmod(const usb_info_t *info)
{
	int ret = 0;
    if (module_track_on(info->module_name) == 0)
    {
        LOG_I("usb_device %04x:%04x already track on\n", info->vid, info->pid);
        return;
    }

	if((info->vid == 0xa69c) && (info->pid == 0x8d80)){
		ret = hl_system("insmod %s/%s.ko", local_module_directory, "aic_load_fw");
		ret |= hl_system("insmod %s/%s.ko", local_module_directory, "aic8800_fdrv");
		if(ret != 0){
			LOG_I("usb_device %04x:%04x insmod %s failed\n", info->vid, info->pid, info->module_name);
		}
	}else{
		if (hl_system("insmod %s/%s.ko", local_module_directory, info->module_name) != 0)
			LOG_I("usb_device %04x:%04x insmod %s failed\n", info->vid, info->pid, info->module_name);
	}
}

static void usb_device_rmmod(const usb_info_t *info)
{
    int track_off = module_track_off(info->module_name);
    if (track_off == 0)
    {
        LOG_I("usb_device %04x:%04x keep track on\n", info->vid, info->pid);
        return;
    }
    else if (track_off < 0)
    {
        LOG_I("usb_device %04x:%04x not found\n", info->vid, info->pid);
        return;
    }
    if (hl_system("rmmod %s", info->module_name) != 0)
        LOG_I("usb_device %04x:%04x rmmod %s failed\n", info->vid, info->pid, info->module_name);
}

static void module_track_init(void)
{
    FILE *fp;
    char line[128];
    char *p;
    uint16_t vid, pid;
    int retry = 0;
    do
    {
        fp = popen("lsusb", "r");
        if (fp == NULL)
        {
            LOG_I("popen failed %s\n", strerror(errno));
            usleep(100000);
        }
    } while (fp != NULL && retry++ < 10);

    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            if ((p = strstr(line, "ID ")) == NULL)
            {
                LOG_I("lsusb not found ID: %s\n", line);
                continue;
            }

            p += sizeof("ID ") - 1;
            if (hl_parse_usb_id2(p, &vid, &pid) != 0)
            {
                LOG_I("parse lsusb id failed: %s\n", p);
                continue;
            }

            for (int i = 0; i < sizeof(usb_modeswitch_info) / sizeof(usb_modeswitch_info[0]); i++)
            {
                if (vid == usb_modeswitch_info[i].vid && pid == usb_modeswitch_info[i].pid)
                {
                    LOG_I("usb_device %04x:%04x need usb_modeswitch\n", vid, pid);
                    if (hl_system("usb_modeswitch -KW -v %04x -p %04x", vid, pid) != 0)
                        LOG_I("usb_device %04x:%04x usb_modeswitch failed\n", vid, pid);
                    continue;
                }
            }

            for (int i = 0; i < sizeof(usb_info) / sizeof(usb_info[0]); i++)
            {
                if (usb_info[i].vid == vid && usb_info[i].pid == pid)
                {
                    if (module_is_exists(usb_info[i].module_name) == 0)
                        usb_device_insmod(&usb_info[i]);
                    else
                        module_track_on(usb_info[i].module_name);
                    continue;
                }
            }
        }
        pclose(fp);
    }
}

static int module_track_on(const char *module_name)
{
    for (int i = 0; i < track_list_count; i++)
    {
        if (module_track_list[i].module_name == module_name)
        {
            module_track_list[i].ref_count++;
            return 0;
        }
    }
    if (track_list_count < sizeof(module_track_list) / sizeof(module_track_list[0]))
    {
        module_track_list[track_list_count].module_name = module_name;
        module_track_list[track_list_count].ref_count++;
        track_list_count++;
        return 1;
    }
    return 0;
}

static int module_track_off(const char *module_name)
{
    for (int i = 0; i < track_list_count; i++)
    {
        if (module_track_list[i].module_name == module_name)
        {
            module_track_list[i].ref_count--;
            if (module_track_list[i].ref_count == 0)
            {
                for (int j = i; j < track_list_count - 1; j++)
                    module_track_list[j] = module_track_list[j + 1];
                track_list_count--;
                return 1;
            }
            else
                return 0;
        }
    }
    return -1;
}

static int module_is_exists(const char *module_name)
{
    FILE *fp = popen("lsmod", "r");
    char line[128];
    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            if (strstr(line, module_name))
            {
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}