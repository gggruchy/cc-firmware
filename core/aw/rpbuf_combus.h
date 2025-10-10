#ifndef RPBUF_BUS_H
#define RPBUF_BUS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "combus.h"

    int rpbuf_open(struct combus *combus, const void *port, void *params);
    void rpbuf_close(struct combus *combus);
    ssize_t rpbuf_write(struct combus *combus, const void *buf, size_t counts);
    ssize_t rpbuf_read(struct combus *combus, void *buf, size_t counts);

    static const combus_ops_t rpbuf_combus_ops = {
        .open = rpbuf_open,
        .close = rpbuf_close,
        .write = rpbuf_write,
        .read = rpbuf_read,
    };
    
#ifdef __cplusplus
}
#endif
#endif