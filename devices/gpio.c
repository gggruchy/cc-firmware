#include "gpio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

/* sysfs interface */
static int sysfs_gpio_export(unsigned char index);
static int sysfs_gpio_unexport(unsigned char index);
static int sysfs_gpio_set_direction(unsigned char index, unsigned char direction);
static int sysfs_gpio_set_value(unsigned char index, unsigned char value);
static int sysfs_gpio_get_direction(unsigned char index);
static int sysfs_gpio_get_value(unsigned char index);

int gpio_init(int index)
{
    if (sysfs_gpio_export(index) == -1)
    {
        return -1;
    }
    return 0;
}

int gpio_deinit(int index)
{
    if (sysfs_gpio_unexport(index) == -1)
        return -1;
    return 0;
}

int gpio_set_direction(int index, gpio_direction_t direction)
{
    return sysfs_gpio_set_direction(index, direction);
}

int gpio_set_value(int index, gpio_state_t value)
{
    return sysfs_gpio_set_value(index, value);
}

int gpio_get_direction(int index)
{
    return sysfs_gpio_get_direction(index);
}

int gpio_get_value(int index)
{
    return sysfs_gpio_get_value(index);
}

int gpio_is_init(int index)
{
    char path[255];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", index);
    DIR *dp = opendir(path);
    if (dp == NULL)
    {
        return -1;
    }
    closedir(dp);
    return 0;
}
static int sysfs_gpio_export(unsigned char index)
{
    char path[255];
    char __index[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", index);
    snprintf(__index, sizeof(__index), "%d", index);

    DIR *dp = opendir(path);
    if (dp == NULL)
    {
        int export_fd = open("/sys/class/gpio/export", O_WRONLY);
        if (write(export_fd, __index, sizeof(__index)) == -1)
        {
            close(export_fd);
            return -1;
        }
        close(export_fd);
        dp = opendir(path);
        if (dp == NULL)
        {
            return -1;
        }
    }
    closedir(dp);
    return 0;
}

static int sysfs_gpio_unexport(unsigned char index)
{
    char path[255];
    char __index[8];
    snprintf(path, sizeof(path), "/sys/class/gpio/pwm%d", index);
    snprintf(__index, sizeof(__index), "%d", index);

    DIR *dp = opendir(path);
    if (dp != NULL)
    {
        int unexport_fd = open("/sys/class/gpio/unexport", O_WRONLY);
        if (unexport_fd == -1)
        {
            closedir(dp);
            return -1;
        }

        if (write(unexport_fd, __index, sizeof(__index)) == -1)
        {
            close(unexport_fd);
            closedir(dp);
            return -1;
        }

        close(unexport_fd);
        closedir(dp);
    }
    return 0;
}

static int sysfs_gpio_set_direction(unsigned char index, unsigned char direction)
{
    char path[255];
    char __direction[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", index);
    snprintf(__direction, sizeof(__direction), "%s", direction ? "out" : "in");

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        return -1;
    }

    if (write(fd, __direction, strlen(__direction)) == -1)
    {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int sysfs_gpio_set_value(unsigned char index, unsigned char value)
{
    char path[255];
    char __value[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", index);
    snprintf(__value, sizeof(__value), "%s", value ? "1" : "0");

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        return -1;
    }

    if (write(fd, __value, strlen(__value)) == -1)
    {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int sysfs_gpio_get_direction(unsigned char index)
{
    char path[255];
    char __direction[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", index);

    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    int rlen = 0;
    if ((rlen = read(fd, __direction, sizeof(__direction))) == -1)
    {
        close(fd);
        return -1;
    }
    __direction[rlen - 1] = '\0';
    close(fd);
    return strncmp(__direction, "in", sizeof(__direction));
}

static int sysfs_gpio_get_value(unsigned char index)
{
    char path[255];
    char __value[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", index);

    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    int rlen = 0;
    if ((rlen = read(fd, __value, sizeof(__value))) == -1)
    {
        close(fd);
        return -1;
    }
    __value[rlen - 1] = '\0';
    close(fd);
    return strncmp(__value, "0", sizeof(__value));
}
