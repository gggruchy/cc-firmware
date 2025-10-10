#ifndef __LIBRPBUF_LOCAL_H__
#define __LIBRPBUF_LOCAL_H__

#include "rpbuf.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct __rpbuf_buffer {
	struct rpbuf_buffer_info info;
	int ctrl_fd;
	int buf_fd;
	void *addr;
};

#ifdef __cplusplus
}
#endif

#endif /* ifndef __LIBRPBUF_LOCAL_H__ */
