#include "hl_boot.h"
#include "hl_common.h"
#include "hl_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BOOTENV_GET "/etc/fw_printenv"
#define BOOTENV_SET "/etc/fw_setenv"

typedef struct
{
    char name[128];
    char value[1024];
} bootenv_t;

hl_list_t bootenv_list;

void hl_bootenv_init(void)
{
    char buf[4096];
    char line[1024];
    char *line_saveptr;
    char *saveptr;
    char *token;
    bootenv_t env;

    hl_list_create(&bootenv_list, sizeof(bootenv_t));

    FILE *fp = popen(BOOTENV_GET, "r");
    if (fp)
    {
        fread(buf, 1, sizeof(buf), fp);
        pclose(fp);
    }

    line_saveptr = buf;
    while (hl_get_line(line, sizeof(line), &line_saveptr))
    {
        token = strtok_r(line, "=", &saveptr);
        strncpy(env.name, token, sizeof(env.name));
        strncpy(env.value, saveptr, sizeof(env.value));
        hl_list_push_back(bootenv_list, &env);
    }
}

void hl_bootenv_set(const char *name, const char *value)
{
    int found = 0;

    if (hl_system("%s %s %s", BOOTENV_SET, name, value) == 0)
    {
        hl_list_node_t node = bootenv_list;
        while ((node = hl_list_get_next_node(node)) != bootenv_list)
        {
            bootenv_t *env = hl_list_get_data(node);
            if (strncmp(env->name, name, sizeof(env->name)) == 0)
            {
                found = 1;
                strncpy(env->value, value, sizeof(env->value));
            }
        }

        if (found == 0)
        {
            bootenv_t env;
            strncpy(env.name, name, sizeof(env.name));
            strncpy(env.value, value, sizeof(env.value));
            hl_list_push_back(bootenv_list, &env);
        }
    }
}

const char *hl_bootenv_get(const char *name)
{
    hl_list_node_t node = bootenv_list;
    while ((node = hl_list_get_next_node(node)) != bootenv_list)
    {
        bootenv_t *env = hl_list_get_data(node);
        if (strcmp(env->name, name) == 0)
            return env->value;
    }
}
int hl_get_chipid(char *buf, int len)
{
    FILE *fp = popen("cat /sys/class/sunxi_info/sys_info | grep sunxi_serial | awk '{print $3}'", "r");
    if (fp)
    {
        fread(buf, 1, len, fp);
        pclose(fp);
        // 去掉换行符
        buf[strcspn(buf, "\n")] = '\0';
    }
    else
    {
        return -1;
    }
    buf[len - 1] = '\0';
    return 0;
}

int hl_get_chiptemp(char *buf, int len)
{
    FILE *fp = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
    if (fp)
    {
        fread(buf, 1, len, fp);
        pclose(fp);
        // 去掉换行符
        buf[strcspn(buf, "\n")] = '\0';
    }
    else
    {
        return -1;
    }
    buf[len - 1] = '\0';
    return 0;
}