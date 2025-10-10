#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include "msgbox.h"
#include "Sharespace.h"
/*
 * Some default value
 */
#define RPMSG_CTRL_DEV "/dev/rpmsg_ctrl0"
#define ENDPOINT_NAME "msgbox_demo"    /* should be less than 32 bytes */
#define ENDPOINT_SRC_ADDR 0x3
#define ENDPOINT_DST_ADDR 0xffffffff



/**
 * Get the interface "/dev/rpmsgXX" that matches the endpoint name.
 *
 * This function reads all the "/sys/class/rpmsg/rpmsgXX/name" and finds the one
 * that is the same as the endpoint name.
 *
 * @param ept_name: endpoint name (The string length should be less than 32 bytes)
 * @param[out] ept_interface: the interface got (Its size should be large enough
 *                            to store the string "/dev/rpmsgXX")
 * @return: 0 on success, otherwise a negative number
 */



static void msgbox_signal(int sig)
{
	printf("signal got %d\n", sig );
	switch (sig) {
		case SIGINT:
		case SIGQUIT:
		case SIGHUP:
		case SIGTTIN:

			break;
		default:
			break;
	}
}

static int get_ept_interface_by_name(const char *ept_name, char *ept_interface)
{
	static const char *class_dir = "/sys/class/rpmsg";
	int ret = 0;
	DIR *dir = NULL;
	struct dirent *dir_entry = NULL;
	char ept_name_buf[32];
	int ept_name_len = strlen(ept_name);
	int is_found = 0;

	dir = opendir(class_dir);
	if (!dir) {
		printf("Failed to open directory \"%s\"\n", class_dir);
		ret = -1;
		goto out;
	}

	while ((dir_entry = readdir(dir))) {
		if (0 != strncmp(dir_entry->d_name, "rpmsg", 5)) {
			continue;
		}
		char name_path[256] = "/sys/class/rpmsg/";
		strcat(name_path, dir_entry->d_name);
		strcat(name_path, "/name");
		FILE *fp = fopen(name_path, "rb");
		if (!fp) {
			continue;
		}
		ret = fread(ept_name_buf, 1, ept_name_len, fp);
		if (ret != ept_name_len) {
			fclose(fp);
			continue;
		}
		ept_name_buf[ept_name_len] = '\0';
		if (0 == strcmp(ept_name, ept_name_buf)) {
			strcpy(ept_interface, "/dev/");
			strcat(ept_interface, dir_entry->d_name);
			is_found = 1;
			fclose(fp);
			break;
		}
		fclose(fp);
	}

	ret = is_found ? 0 : -1;

	closedir(dir);
out:
	return ret;
}

uint16_t msgbox_new_msg[2];
uint16_t msgbox_send_msg[2];
int msgbox_fd_ctrl = -1;
int msgbox_fd_ept = -1;
int msgbox_rpmsg_init()    //--1-msgbox-G-G-2022-07-08-----
{
    int ret = 0;
	struct rpmsg_endpoint_info ept_info = {
		.name = ENDPOINT_NAME,
		.src = ENDPOINT_SRC_ADDR,
		.dst = ENDPOINT_DST_ADDR,
	};

	char *ctrl_dev = RPMSG_CTRL_DEV;
	
	char ept_interface[16];
	msgbox_new_msg[0]=0;
	msgbox_new_msg[1]=0;

	// signal(SIGINT, msgbox_signal);
	// signal(SIGQUIT, msgbox_signal);
	// signal(SIGHUP, msgbox_signal);
	// signal(SIGTTIN, msgbox_signal);

	// printf("ctrl device: %s, src addr: 0x%.8x, dts addr: 0x%.8x\n",ctrl_dev, ept_info.src, ept_info.dst);

	msgbox_fd_ctrl = open(ctrl_dev, O_RDWR);
	// printf("fd_ctrl %d\n", msgbox_fd_ctrl);
	if (msgbox_fd_ctrl < 0) {
		printf("Failed to open \"%s\" (ret: %d)\n", ctrl_dev, msgbox_fd_ctrl);
		return -1;
	}

	ret = ioctl(msgbox_fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &ept_info);
	if (ret < 0) {
		printf("Failed to create endpoint (ret: %d)\n", ret);
        return -1;
	}

	ret = get_ept_interface_by_name(ENDPOINT_NAME, ept_interface);
	if (ret < 0) {
		printf("The endpoint interface named \"%s\" not found\n", ENDPOINT_NAME);
        close(msgbox_fd_ctrl);
        return -1;
	}

	// printf("ept_interface %s\n", ept_interface);
	msgbox_fd_ept = open(ept_interface, O_RDWR);
	// printf("msgbox_fd_ept %d\n", msgbox_fd_ept);
	if (msgbox_fd_ept < 0) {
		printf("Failed to open \"%s\" (ret: %d)\n", ept_interface, msgbox_fd_ept);
		close(msgbox_fd_ctrl);
        return -1;
	}

	return msgbox_fd_ept;
}
#if MSGBOX_ENABLE
#endif    

int fd_msgbox=-1;
void msgbox_send_signal(uint32_t read_write , uint16_t value)    //--4-msgbox-G-G-2022-07-08-----
{
	uint32_t data_send = 0;
	msgbox_send_msg[0] = sharespace_arm_addr[SHARESPACE_READ]; 
	msgbox_send_msg[1] = sharespace_arm_addr[SHARESPACE_WRITE]; 
    msgbox_send_msg[read_write] = value;

	data_send= (msgbox_send_msg[1]<<16) | msgbox_send_msg[0];

	// GAM_DEBUG_printf("send msgbox : %d %x\n", fd_msgbox,data_send );
	if (fd_msgbox <= 0)
	{
		printf("send msgbox fail! %d\n", fd_msgbox );
	}
	int ret = write(fd_msgbox, &data_send, sizeof(uint32_t));
	if (ret <= 0)
	{
		printf("send erroe: %x  %d fd_msgbox : %d\n",data_send ,ret, fd_msgbox);
		perror("write error");
		// printf("error : %s\n", strerror(errno));
	}
}
uint32_t msgbox_read_signal(uint32_t read_write)    //--4-msgbox-G-G-2022-07-08-----
{
	uint32_t data_recv = 0;
	int ret = read(fd_msgbox, &data_recv, sizeof(uint32_t));
	if(ret)
	{
		msgbox_new_msg[0] =  (uint16_t)data_recv;
		msgbox_new_msg[1] = (uint16_t)(data_recv >> 16);
	}
	if(msgbox_new_msg[read_write] >= 5000)
	{
		return 0;
	}
	if(msgbox_new_msg[read_write] == sharespace_arm_addr[read_write])
	{
		return 0;
	}
	// printf("msgbox_read_signal %d %d:\n",msgbox_new_msg[read_write], sharespace_arm_addr[read_write] );
	return 1;
}


 void msgbox_poll_read( )     //-----gggg-2022-0408-3-------
{
    {
      	struct pollfd poll_fds;
		poll_fds.fd = fd_msgbox;
		poll_fds.events = POLLHUP | POLLIN;
        int ret = poll(&poll_fds,1, -1);
        if (ret > 0) {
			
            msgbox_read_signal(MSGBOX_ALLOW_WRITE);
        }
    }
}











