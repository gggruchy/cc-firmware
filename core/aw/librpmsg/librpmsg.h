#ifndef __LIBrpmsg_H__
#define __LIBrpmsg_H__

#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RPMSG_NAME_SIZE					(32)
#define RPMSG_DATA_MAX_LEN				(512 - 16)

/**
 * rpmsg_alloc_ept - Allocate a rpmsg endpoint
 * @ctrl_name: specify which /dev/rpmsg-ctrl to use to allocate buffer
 * @name: endpoint name
 *
 * This function will block until the buffer is available , or a few seconds
 * timeout (normally 15 seconds).
 *
 * if successfully, it will return endpoint id. we can open /dev/rpmsg%d
 * to send and read data.
 *
 * Return pointer to rpmsg buffer on success, or negative number on failure.
 */
int rpmsg_alloc_ept(const char *ctrl_name, const char *name);

/**
 * rpmsg_free_buffer - Free a rpmsg endpoint
 * @ctrl_name: specify which /dev/rpmsg-ctrl to use to allocate buffer
 * @ept_id: rpmsg_alloc_ept function return value.
 *
 * Return pointer to rpmsg buffer on success, or negative number on failure.
 */
int rpmsg_free_ept(const char *ctrl_name, int ept_id);

#ifdef __cplusplus
}
#endif

#endif /* ifndef __LIBrpmsg_H__ */
