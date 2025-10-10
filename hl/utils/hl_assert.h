#ifndef HL_ASSERT_H
#define HL_ASSERT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define HL_ASSERT(expr)                                     \
    do                                                      \
    {                                                       \
        if (!(expr))                                        \
        {                                                   \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
            fflush(stderr);                                 \
            abort();                                        \
        }                                                   \
    } while (0)

#define HL_ASS_LOG                                                            \
    do                                                                        \
    {                                                                         \
        fprintf(stdout, "\033[31m[LINE:%d]\033[0m:%s\n", __LINE__, __FILE__); \
        fflush(stdout);                                                       \
    } while (0);

#ifdef __cplusplus
}
#endif

#endif