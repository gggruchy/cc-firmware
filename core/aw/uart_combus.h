#ifndef UART_BUS_H
#define UART_BUS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "combus.h"

    int uart_open(struct combus *combus, const void *port, void *params);
    void uart_close(struct combus *combus);
    ssize_t uart_write(struct combus *combus, const void *buf, size_t counts);
    ssize_t uart_read(struct combus *combus, void *buf, size_t counts);

    static const combus_ops_t uart_combus_ops = {
        .open = uart_open,
        .close = uart_close,
        .write = uart_write,
        .read = uart_read,
    };
#ifdef __cplusplus
}
#endif
#endif