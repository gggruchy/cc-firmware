#include "serial.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define LOG_TAG "serial"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

int serial_open(const char *devpath, int parity, int stop_bit, int data_bit, int baudrate)
{
    int fd;
    struct termios tty;

    if ((fd = open(devpath, O_RDWR)) < 0)
    {
        LOG_E("serial open failed: %s %s\n", devpath, strerror(errno));
        return -1;
    }

    if (tcgetattr(fd, &tty) != 0)
    {
        LOG_E("serial tcgetattr failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

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
    cfsetspeed(&tty, 115200);

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        LOG_E("serial tcsetattr failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, TCFLSH, 2) != 0)
    {
        LOG_E("serial tcflush failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int serial_write(int fd, const uint8_t *data, uint32_t length)
{
    int len = write(fd, data, length);
#if 0
    if (len > 0)
    {
        printf("serial write length: %d\n", len);
        for (int i = 0; i < len; i++)
        {
            printf("%x ", data[i]);
        }
        printf("\n");
    }
    else
    {
        // LOG_I("write error\n");
    }
#endif
    return len;
}

int serial_read(int fd, uint8_t *data, uint32_t length)
{
    int len = read(fd, data, length);
#if 0
    if (len > 0)
    {
        printf("serial read length: %d\n", len);
        for (int i = 0; i < len; i++)
        {
            printf("%x ", data[i]);
        }
        printf("\n");
    }
    else
    {
        // LOG_I("read error\n");
    }
#endif
    return len;
}

int serial_tcflush(int fd)
{
    if (ioctl(fd, TCFLSH, 2) != 0)
    {
        LOG_E("serial tcflush failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
}