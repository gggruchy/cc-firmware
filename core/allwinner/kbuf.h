#ifndef __KBUF_H__
#define __KBUF_H__

#define KBUF_NAME_SIZE 32
struct kbuf_buf_data_t {
	char name[KBUF_NAME_SIZE];
	unsigned int len;
	unsigned int type;
	int minor;
	/*
	 * va: virtual address that program can directly use.
	 * pa: physical address.
	 * da: device address. This address is set by MASTER, and transfer to
	 *     SLAVE to use. What it means is defined by the SLAVE
	 *     translate_to_va() implementation.
	 */

	unsigned long va;
	unsigned long pa;
	//u64 da;
};

#define KBUF_TYPE_NONCACHE                      (1)

#define IOCTL_OPT_CREATE_MAGIC                  (0x100)
#define IOCTL_OPT_DESTROY_MAGIC                 (0x200)
#define IOCTL_OPT_GET_MAGIC                     (0x300)
#define IOCTL_OPT_MASK                          (0xFF00)
#define IOCTL_TYPE_NONCACHE_BUF_MAGIC           (0x1)
#define IOCTL_TYPE_MASK                         (0xFF)
#define KBUF_MGR_DEV_IOCTL_CREATE_NONCACHE_BUF  (IOCTL_TYPE_NONCACHE_BUF_MAGIC | IOCTL_OPT_CREATE_MAGIC)
#define KBUF_MGR_DEV_IOCTL_DESTROY_NONCACHE_BUF (IOCTL_TYPE_NONCACHE_BUF_MAGIC | IOCTL_OPT_DESTROY_MAGIC)
#define KBUF_MGR_DEV_IOCTL_GET_NONCACHE_BUF     (IOCTL_TYPE_NONCACHE_BUF_MAGIC | IOCTL_OPT_GET_MAGIC)
#define KBUF_MGR_DEV_IOCTL_CREATE_BUF           (IOCTL_OPT_CREATE_MAGIC)
#define KBUF_MGR_DEV_IOCTL_DESTROY_BUF          (IOCTL_OPT_DESTROY_MAGIC)
#define KBUF_MGR_DEV_IOCTL_GET_BUF              (IOCTL_OPT_GET_MAGIC)

#ifndef KBUF_DEBUG
#define kbuf_show_buf_data(buf_data)	do{}while(0)
#else
#define kbuf_show_buf_data(buf_data)	__kbuf_show_buf_data__((buf_data), (__func__), (__LINE__))
#ifdef __KERNEL__
#include <linux/printk.h>
#define __kbuf_print__ printk
#else
#include <stdio.h>
#define __kbuf_print__ printf
#endif /* __KERNEL__ */

static inline void __kbuf_show_buf_data__(struct kbuf_buf_data_t *buf_data, const char *func, int line)
{
	__kbuf_print__("<%s:%d>buf_data(%p):\n", func, line, buf_data);
	if(buf_data){
		__kbuf_print__("\tname\t:%s\n", buf_data->name);
		__kbuf_print__("\tlen\t:%u\n", buf_data->len);
		__kbuf_print__("\ttype\t:%u\n", buf_data->type);
		__kbuf_print__("\tminor\t:%d\n", buf_data->minor);
		__kbuf_print__("\tva\t:%lx\n", buf_data->va);
		__kbuf_print__("\tpa\t:%lx\n", buf_data->pa);
#ifdef __KERNEL__
		if(buf_data->va)
			__kbuf_print__("\tpa(va)\t:%lx\n", __pa(buf_data->va));
		if(buf_data->pa)
			__kbuf_print__("\tva(pa)\t:%lx\n", __va(buf_data->pa));
#endif /* __KERNEL__ */
	}
}
#endif /* KBUF_DEBUG */

#endif /* __KBUF_H__ */