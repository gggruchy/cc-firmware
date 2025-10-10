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

#include "librpbuf.h"

#include "local.h"

rpbuf_buffer_t *rpbuf_alloc_buffer(int ctrl_id, const char *name, size_t len)
{
	rpbuf_buffer_t *buffer;
	size_t name_len;
	char ctrl_dev_path[128];
	char buf_dev_path[128];
	int ctrl_fd;
	int buf_fd;
	void *addr;
	int ret;

	name_len = strlen(name);
	if (name_len == 0 || name_len >= RPBUF_NAME_SIZE) {
		fprintf(stderr, "invalid buffer name \"%s\", its length should be "
				"in range [1, %d)\n", name, RPBUF_NAME_SIZE);
		goto err_out;
	}

	buffer = calloc(1, sizeof(rpbuf_buffer_t));
	if (!buffer) {
		fprintf(stderr, "%s: calloc for rpbuf_buffer_t failed\n", name);
		goto err_out;
	}
	strncpy(buffer->info.name, name, RPBUF_NAME_SIZE);
	buffer->info.len = len;

	snprintf(ctrl_dev_path, sizeof(ctrl_dev_path), "/dev/rpbuf_ctrl%d", ctrl_id);
	snprintf(buf_dev_path, sizeof(buf_dev_path), "/dev/rpbuf-%s", name);

	// 打开控制器
	ctrl_fd = open(ctrl_dev_path, O_RDWR);
	if (ctrl_fd < 0) {
		fprintf(stderr, "failed to open %s (%s)\n", ctrl_dev_path, strerror(errno));
		goto err_free_buffer;
	}
	buffer->ctrl_fd = ctrl_fd;

	//创建BUFFER
	ret = ioctl(ctrl_fd, RPBUF_CTRL_DEV_IOCTL_CREATE_BUF, &buffer->info);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: ioctl CREATE_BUF failed (%s)\n",
				name, strerror(errno));
		goto err_close_ctl_fd;
	}

	//打开BUFFER接口
	buf_fd = open(buf_dev_path, O_RDWR);
	if (buf_fd < 0) {
		fprintf(stderr, "failed to open %s (%s)\n", buf_dev_path, strerror(errno));
		goto err_ioctl_destroy_buf;
	}
	buffer->buf_fd = buf_fd;
	
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "buffer %s: mmap failed (%s)\n", name, strerror(errno));
		goto err_close_buf_fd;
	}
	buffer->addr = addr;

	return buffer;

err_close_buf_fd:
	close(buf_fd);
err_ioctl_destroy_buf:
	ioctl(ctrl_fd, RPBUF_CTRL_DEV_IOCTL_DESTROY_BUF, &buffer->info);
err_close_ctl_fd:
	close(ctrl_fd);
err_free_buffer:
	free(buffer);
err_out:
	return NULL;
}

int rpbuf_free_buffer(rpbuf_buffer_t *buffer)
{
	int ret;

	munmap(buffer->addr, buffer->info.len);

	close(buffer->buf_fd);

	ret = ioctl(buffer->ctrl_fd, RPBUF_CTRL_DEV_IOCTL_DESTROY_BUF, &buffer->info);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: ioctl DESTROY_BUF failed (%s)\n",
				buffer->info.name, strerror(errno));
		/* Go on releasing other resources rather than return immediately. */
	}

	close(buffer->ctrl_fd);

	free(buffer);

	return 0;
}

void *rpbuf_buffer_addr(rpbuf_buffer_t *buffer)
{
	return buffer->addr;
}

const char *rpbuf_buffer_name(rpbuf_buffer_t *buffer)
{
	return buffer->info.name;
}

size_t rpbuf_buffer_len(rpbuf_buffer_t *buffer)
{
	return buffer->info.len;
}

int rpbuf_buffer_is_available(rpbuf_buffer_t *buffer)
{
	int ret;
	uint8_t is_available;

	ret = ioctl(buffer->buf_fd, RPBUF_BUF_DEV_IOCTL_CHECK_AVAIL, &is_available);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: ioctl CHECK_AVAIL failed (%s)\n",
				buffer->info.name, strerror(errno));
		/* Return 0 means not available */
		return 0;
	}

	return is_available;
}

int rpbuf_transmit_buffer(rpbuf_buffer_t *buffer, uint32_t offset, uint32_t data_len)
{
	struct rpbuf_buffer_xfer xfer;
	int ret;

	xfer.offset = offset;
	xfer.data_len = data_len;

	ret = ioctl(buffer->buf_fd, RPBUF_BUF_DEV_IOCTL_TRANSMIT_BUF, &xfer);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: ioctl TRANSMIT_BUF failed (%s)\n",
				buffer->info.name, strerror(errno));
		return ret;
	}

	return 0;
}

int rpbuf_receive_buffer(rpbuf_buffer_t *buffer, uint32_t *offset,
		uint32_t *data_len, int timeout_ms)
{
	struct rpbuf_buffer_xfer xfer;
	int ret;

	xfer.timeout_ms = timeout_ms;

	ret = ioctl(buffer->buf_fd, RPBUF_BUF_DEV_IOCTL_RECEIVE_BUF, &xfer);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: ioctl RECEIVE_BUF failed (%s)\n",
				buffer->info.name, strerror(errno));
		return ret;
	}

	*offset = xfer.offset;
	*data_len = xfer.data_len;

	return timeout_ms < 0 ? 0 : xfer.timeout_ms;
}

int rpbuf_install_rx_signal(rpbuf_buffer_t *buffer, sighandler_t handler)
{
	int flags, ret;
	sighandler_t old;

	ret = fcntl(buffer->buf_fd, F_SETOWN, getpid());
	if (ret < 0) {
		fprintf(stderr, "buffer %s: fcntl F_SETOWN failed (%s)\n",
				buffer->info.name, strerror(errno));
		return ret;
	}

	flags = fcntl(buffer->buf_fd, F_GETFL);
	if (flags < 0) {
		fprintf(stderr, "buffer %s: fcntl F_GETFL failed (%s)\n",
				buffer->info.name, strerror(errno));
		return errno;
	}

	ret = fcntl(buffer->buf_fd, F_SETFL, flags | FASYNC);
	if (ret < 0) {
		fprintf(stderr, "buffer %s: fcntl F_SETFL failed (%s)\n",
				buffer->info.name, strerror(errno));
		return ret;
	}

	old = signal(SIGIO, handler);
	if (old == SIG_ERR) {
		fprintf(stderr, "buffer %s: signal failed (%s)\n",
				buffer->info.name, strerror(errno));
		return errno;
	}

	return 0;
}

int rpbuf_remove_rx_signal(rpbuf_buffer_t *buffer)
{
	sighandler_t ret;

	ret = signal(SIGIO, SIG_DFL);
	if (ret == SIG_ERR) {
		fprintf(stderr, "buffer %s: signal failed (%s)\n",
				buffer->info.name, strerror(errno));
		return errno;
	}

	return 0;
}

int rpbuf_set_sync_buffer(rpbuf_buffer_t *buffer, uint32_t is_sync)
{
	int ret = 0;

	ret = ioctl(buffer->buf_fd, RPBUF_BUF_DEV_IOCTL_SET_SYNC_BUF, &is_sync);
	if (ret < 0)
		fprintf(stderr, "buffer %s: ioctl SET_SYNC_BUF failed (%s)\n",
				buffer->info.name, strerror(errno));

	return ret;
}
