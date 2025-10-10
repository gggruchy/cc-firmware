#include "uart_combus.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "pollreactor.h"
#include "pyhelper.h"

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#define LOG_TAG "uart_combus"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

int uart_open(struct combus *combus, const void *port, void *params)
{
    struct termios tty;
    combus->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (combus->fd == -1)
    {
        LOG_E("serial open failed port %s!\n", port);
        return -1;
    }
    LOG_I("serial open successful port %s!\n", port);
    int baudrate = (int)params;

    tcgetattr(combus->fd, &tty);
    tty.c_cflag &= ~PARENB; // 校验位
    tty.c_cflag &= ~CSTOPB; // 停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 数据位
    tty.c_cflag &= ~CRTSCTS; // 流控
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~ICANON; // 禁用规范模式
    tty.c_lflag &= ~ECHO;   // Disable echo
    tty.c_lflag &= ~ECHOE;  // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG;   // Disable interpretation of INTR, QUIT and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                                      // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST;                                                       // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR;                                                       // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 0; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    cfsetspeed(&tty, baudrate);
    if (tcsetattr(combus->fd, TCSANOW, &tty) != 0)
    {
        LOG_E("tcsetattr failed!\n");
        close(combus->fd);
        return -1;
    }

    ioctl(combus->fd, TCFLSH, 2);
    combus->drv_data = NULL;
    return 0;
}

void uart_close(struct combus *combus)
{
    if (close(combus->fd) == -1)
    {
        LOG_E("serial close failed!\n");
    }
}

ssize_t uart_write(struct combus *combus, const void *buf, size_t counts)
{
}

ssize_t uart_read(struct combus *combus, void *buf, size_t counts)
{
}