#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "librpmsg.h"
#include "rpmsg.h"

int rpmsg_alloc_ept(const char *ctrl_name, const char *name)
{
	struct rpmsg_ept_info info;
	size_t name_len;
	char ctrl_dev_path[128];
	char ept_dev_path[RPMSG_NAME_SIZE];
	int ctrl_fd;
	int ret;

	name_len = strlen(name);
	if (name_len == 0 || name_len >= RPMSG_NAME_SIZE) {
		fprintf(stderr, "invalid ept name \"%s\", its length should be "
				"in range [1, %d)\n", name, RPMSG_NAME_SIZE);
		goto err_out;
	}

	strncpy(info.name, name, RPMSG_NAME_SIZE);
	info.id = -1;

	snprintf(ctrl_dev_path, sizeof(ctrl_dev_path), "/dev/rpmsg_ctrl-%s", ctrl_name);
	snprintf(ept_dev_path, sizeof(ept_dev_path), "/dev/rpmsg-%s", name);

	ctrl_fd = open(ctrl_dev_path, O_RDWR);
	if (ctrl_fd < 0) {
		fprintf(stderr, "failed to open %s (%s)\n", ctrl_dev_path, strerror(errno));
		goto err_out;
	}

	ret = ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &info);
	if (ret < 0) {
		fprintf(stderr, "ept %s: ioctl CREATE_BUF failed (%s)\n",
				name, strerror(errno));
		goto err_close_ctl_fd;
	}
	close(ctrl_fd);

	return info.id;

err_close_ctl_fd:
	close(ctrl_fd);
err_out:
	return -ENODEV;
}

int rpmsg_free_ept(const char *ctrl_name, int ept_id)
{
	struct rpmsg_ept_info info;
	char ctrl_dev_path[128];
	int ctrl_fd;
	int ret;

	snprintf(ctrl_dev_path, sizeof(ctrl_dev_path), "/dev/rpmsg_ctrl-%s", ctrl_name);

	info.id = ept_id;

	ctrl_fd = open(ctrl_dev_path, O_RDWR);
	if (ctrl_fd < 0) {
		fprintf(stderr, "failed to open %s (%s)\n", ctrl_dev_path, strerror(errno));
		return -EINVAL;
	}

	ret = ioctl(ctrl_fd, RPMSG_DESTROY_EPT_IOCTL, &info);
	if (ret < 0) {
		fprintf(stderr, "ept %s: ioctl DESTROY_EPT failed (%s)\n",
				info.name, strerror(errno));
		return -ENODEV;
	}

	close(ctrl_fd);

	return 0;
}
