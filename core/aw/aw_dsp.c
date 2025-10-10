#include "aw_dsp.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define DSP_STATE_FILEPATH "/sys/class/remoteproc/remoteproc0/state"

static ssize_t simple_read(const char *filepath, char *content, size_t size)
{
    int fd = open(filepath, O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        printf("simple_read open failed %s\n", filepath);
        return -1;
    }
    ssize_t l = read(fd, content, size);
    close(fd);
    return l;
}

static ssize_t simple_write(const char *filepath, char *content, size_t size)
{
    int fd = open(filepath, O_WRONLY | O_NONBLOCK);
    if (fd == -1)
    {
        printf("simple_write open failed %s\n", filepath);
        return -1;
    }
    ssize_t l = write(fd, content, size);
    close(fd);
    return l;
}

int dsp_start(void)
{
    char cmd[] = "start\n";
    int l = 0;
    if ((l = simple_write(DSP_STATE_FILEPATH, cmd, sizeof(cmd) - 1)) < 0)
    {
        printf("simple_write failed %s\n", strerror(l));
        return DSP_STATE_UNKNOWN;
    }
    return 0;
}

int dsp_stop(void)
{
    char cmd[] = "stop\n";
    int l = 0;
    if ((l = simple_write(DSP_STATE_FILEPATH, cmd, sizeof(cmd) - 1)) < 0)
    {
        printf("simple_write failed %s\n", strerror(l));
        return DSP_STATE_UNKNOWN;
    }
    return 0;
}

int dsp_restart(void)
{
    if (dsp_get_state() == DSP_STATE_RUNNING)
        dsp_stop();
    return dsp_start();
}

int dsp_get_state(void)
{
    char state[64];
    int l = 0;
    if ((l = simple_read(DSP_STATE_FILEPATH, state, sizeof(state) - 1)) < 0)
    {
        printf("simple_write failed %s\n", strerror(l));
        return DSP_STATE_UNKNOWN;
    }
    state[l] = '\0';
    printf("dsp state=[%s]\n", state);
    
    if (strncmp(state, "running\n", sizeof(state)) == 0)
    {
        printf("DSP_STATE_RUNNING\n");
        return DSP_STATE_RUNNING;
    }
    else if (strncmp(state, "offline\n", sizeof(state)) == 0)
    {
        printf("DSP_STATE_OFFLINE\n");
        return DSP_STATE_OFFLINE;
    }

    printf("DSP_STATE_UNKNOWN\n");
    return DSP_STATE_UNKNOWN;
}