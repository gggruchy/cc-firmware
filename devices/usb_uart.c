/**
 * @file usb_uart.c
 * @author 
 * @brief 
 * @version 0.1
 * @date 2023-12-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "usb_uart.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
 
#include <termios.h>
#include <string.h>
#include <pthread.h>

// #include "data_global.h"
 
int usb_fd = 0;

static int set_com_config(int fd, int baud_rate, int data_bits, char parity, int stop_bits)
{
	struct termios new_cfg, old_cfg;
	int speed;
	/*保存原有串口配置*/
	if (tcgetattr(fd, &old_cfg) != 0){
		perror("tcgetattr");
		return -1;
	}
 
	new_cfg =old_cfg;
 
	/*配置为原始模�?*/
	cfmakeraw(&new_cfg);
	new_cfg.c_cflag &= ~CSIZE;
 
	/*设置波特�?*/
	switch (baud_rate)
	{
		case 2400:{
					  speed = B2400;
					  break; 
				  }
		case 4800:{
					  speed = B4800;
					  break;
				  }
		case 9600:{
					  speed = B9600;
					  break;
				  }
		case 19200:{
					   speed = B19200;
					   break;
				   }
		case 38400:{
					   speed = B38400;
					   break;
				   }
 
		default:
		case 115200:{
						speed = B115200;
						break;
					}
	}
 
	cfsetispeed(&new_cfg, speed);
	cfsetospeed(&new_cfg, speed);
 
	/*设置数据�?*/
	switch (data_bits)
	{
		case 7:{
				   new_cfg.c_cflag |= CS7;
				   break;
			   }   
		default:	
		case 8:{
				   new_cfg.c_cflag |= CS8;
				   break;
			   }
	}
 
	/*设置奇偶校验�?*/
	switch (parity)
	{
		default:
		case 'n':
		case 'N':{
					 new_cfg.c_cflag &= ~PARENB;
					 new_cfg.c_iflag &= ~INPCK;
					 break;
				 }
		case 'o':
		case 'O':{
					 new_cfg.c_cflag |= (PARODD |PARENB);
					 new_cfg.c_iflag |= INPCK;
					 break;
				 }
		case 'e':
		case 'E':{
					 new_cfg.c_cflag |= PARENB;
					 new_cfg.c_cflag &= ~PARODD;
					 new_cfg.c_iflag |= INPCK;
					 break;
				 }
		case 's':
		case 'S':{
					 new_cfg.c_cflag &= ~PARENB;
					 new_cfg.c_cflag &= ~CSTOPB;
					 break;
				 }
	}
 
	/*设置停止�?*/
	switch (stop_bits)
	{
		default:
		case 1:{
				   new_cfg.c_cflag &= ~CSTOPB;
				   break;
			   }   	
		case 2:{
				   new_cfg.c_cflag |= CSTOPB;
				   break;
			   }
	}
 
	/*设置等待时间和最小接收字�?*/
	new_cfg.c_cc[VTIME] = 0;
	new_cfg.c_cc[VMIN] = 1;
	tcflush(fd, TCIFLUSH);
	if ((tcsetattr(fd, TCSANOW, &new_cfg)) != 0)
	{
		perror("tcsetattr");
		return -1;
	}
 
	return 0;
}
 
static int open_port(char *com_port)
{
	int fd;
 
	/*打开串口*/
	fd = open(com_port, O_RDWR|O_NOCTTY|O_NDELAY);
	if (fd < 0){
		perror("open serial port");
		return -1;
	}
 
	/*恢复串口阻塞状态*/
	if (fcntl(fd, F_SETFL, 0) < 0){
		perror("fcntl F_SETFL\n");
	}
 
	/*判断是否为终端设备*/
	if (isatty(fd) == 0){
		perror("This is not a terminal device");
	}
 
	return fd;
}
 
 
/*--------------------CH340Ƥ׃---------------------------*/
void usb_uart_config(char* path, int baud_rate)
{
	usb_fd = open_port(path);
	if(usb_fd < 0){
		printf("open %s failed\n",path);
		return ;
	}
	if (set_com_config(usb_fd, baud_rate, 8, 'N', 1) < 0)
	{
		perror("set_com_config");
		return ;
	}
	// close(usb_fd);
	return ;
}
 
void usb_uart_recvdats(uint8_t *pBuf,int datalen)
{   
	read(usb_fd,pBuf, datalen);
}

void usb_uart_buf_clear(void)
{
	tcflush(usb_fd, TCIFLUSH);
}


