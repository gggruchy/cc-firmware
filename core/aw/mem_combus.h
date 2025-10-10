#ifndef MEM_BUS_H
#define MEM_BUS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "combus.h"

    int mem_open(struct combus *combus, const void *port, void *params);
    void mem_close(struct combus *combus);
    ssize_t mem_write(struct combus *combus, const void *buf, size_t counts);
    ssize_t mem_read(struct combus *combus, void *buf, size_t counts);

    static const combus_ops_t mem_combus_ops = {
        .open = mem_open,
        .close = mem_close,
        .write = mem_write,
        .read = mem_read,
    };
#ifdef __cplusplus
}
#endif
#endif