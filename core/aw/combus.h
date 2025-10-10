#ifndef COMBUS_H
#define COMBUS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <unistd.h>

    typedef struct combus combus_t;
    typedef struct combus_ops combus_ops_t;

    struct combus_ops
    {
        int (*open)(struct combus *combus, const void *port, void *params);
        void (*close)(struct combus *combus);
        ssize_t (*write)(struct combus *combus, const void *buf, size_t counts);
        ssize_t (*read)(struct combus *combus, void *buf, size_t counts);
    };

    struct combus
    {
        void *drv_data;
        const combus_ops_t *ops;
        int fd;
    };

    static inline void combus_attach_drvier(combus_t *combus, const combus_ops_t *ops)
    {
        combus->ops = ops;
    }

    static inline int combus_open(combus_t *combus, const char *port, void *params)
    {
        if (combus->ops->open)
            return combus->ops->open(combus, port, params);
        return -1;
    }

    static inline void combus_close(combus_t *combus)
    {
        if (combus->ops->close)
            combus->ops->close(combus);
    }

    static inline ssize_t combus_write(combus_t *combus, const void *buf, size_t counts)
    {
        if (combus->ops->write)
            return combus->ops->write(combus, buf, counts);
        return -1;
    }

    static inline ssize_t combus_read(combus_t *combus, void *buf, size_t counts)
    {
        if (combus->ops->read)
            return combus->ops->read(combus, buf, counts);
        return -1;
    }

#ifdef __cplusplus
}
#endif
#endif