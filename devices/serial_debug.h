#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG

#include <stdio.h>/*标准输入输出定义*/
#include <stdlib.h>/*标准函数库定义*/
#include <unistd.h>/*Unix标准函数定义*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>/*文件控制定义*/
#include <termios.h>/*PPSIX终端控制定义*/
#include <errno.h>/*错误号定义*/
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <string>
#include <vector>
#define use_serial_debug 0

#define DEBUG_SERIAL "/dev/ttyS0"
#define MAX_SERIAL_MSG_LENGTH 1024

 int serial_log(std::string logs);
 int serial_error(std::string error_msg);
 int serial_info(std::string info);
 int serial_debug(std::string debug_msg);
 void* debug_read_serial(void *arg);
 void debug_serial_init();
#endif