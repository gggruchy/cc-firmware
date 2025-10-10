#ifndef RPMSG_BUS_H
#define RPMSG_BUS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "combus.h"

    int rpmsg_open(struct combus *combus, const void *port, void *params);
    void rpmsg_close(struct combus *combus);
    ssize_t rpmsg_write(struct combus *combus, const void *buf, size_t counts);
    ssize_t rpmsg_read(struct combus *combus, void *buf, size_t counts);

    static const combus_ops_t rpmsg_combus_ops = {
        .open = rpmsg_open,
        .close = rpmsg_close,
        .write = rpmsg_write,
        .read = rpmsg_read,
    };
    
    
#ifdef __cplusplus
}
#endif
#endif