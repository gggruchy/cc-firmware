#ifndef TRDISPATCH_H
#define TRDISPATCH_H

#include <pthread.h> // pthread_mutex_lock
#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memset
#include "compiler.h" // ARRAY_SIZE
#include "list.h" // list_add_tail
#include "pollreactor.h" // PR_NEVER
#include "pyhelper.h" // report_errno
#include "serialqueue.h" // serialqueue_add_fastreader

struct trdispatch {
    struct list_head tdm_list;

    pthread_mutex_t lock; // protects variables below
    uint32_t is_active, can_trigger, dispatch_reason;
};

struct trdispatch_mcu {
    struct fastreader fr;
    struct trdispatch *td;
    struct list_node node;
    struct serialqueue *sq;
    struct command_queue *cq;
    uint32_t trsync_oid, set_timeout_msgtag, trigger_msgtag;

    // Remaining fields protected by trdispatch lock
    uint64_t last_status_clock, expire_clock;
    uint64_t expire_ticks, min_extend_ticks;
    struct clock_estimate ce;
};

static void send_trsync_trigger(struct trdispatch_mcu *tdm);

static void send_trsync_set_timeout(struct trdispatch_mcu *tdm);

static void handle_trsync_state(struct fastreader *fr, uint8_t *data, int len);

void trdispatch_start(struct trdispatch *td, uint32_t dispatch_reason);


void trdispatch_stop(struct trdispatch *td);

struct trdispatch * trdispatch_alloc(void);

struct trdispatch_mcu * trdispatch_mcu_alloc(struct trdispatch *td, struct serialqueue *sq
                     , struct command_queue *cq, uint32_t trsync_oid
                     , uint32_t set_timeout_msgtag, uint32_t trigger_msgtag
                     , uint32_t state_msgtag);

void trdispatch_mcu_setup(struct trdispatch_mcu *tdm
                     , uint64_t last_status_clock, uint64_t expire_clock
                     , uint64_t expire_ticks, uint64_t min_extend_ticks);
#endif