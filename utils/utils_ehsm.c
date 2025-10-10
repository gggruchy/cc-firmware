#define LOG_TAG "ehsm"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_W

#include "utils_ehsm.h"
#include "log.h"

static int root_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event);

utils_ehsm_state_t root_state = {NULL, root_state_handler, "root", 0};
utils_ehsm_event_t entry_event = {UTILS_EHSM_ENTRY_EVENT, NULL, 0};
utils_ehsm_event_t exit_event = {UTILS_EHSM_EXIT_EVENT, NULL, 0};
utils_ehsm_event_t init_event = {UTILS_EHSM_INIT_EVENT, NULL, 0};

#define UTILS_EHSM_DEBUG_ENABLE 0
#if UTILS_EHSM_DEBUG_ENABLE
// #define EHSM_PRINT(x, ...) LOG_D(x, __VA_ARGS__)
#define EHSM_PRINT(x, ...) printf(x, __VA_ARGS__)
#else
#define EHSM_PRINT(x, ...) ;
#endif

int utils_ehsm_create_state(utils_ehsm_state_t *state, utils_ehsm_state_t *parent, utils_ehsm_handler_t handler, const char *name)
{
    if (parent == NULL)
    {
        parent = &root_state;
    }
    state->parent = parent;
    state->handler = handler;
    state->name = name;
    state->depth = parent->depth + 1;
    if (UTILS_CHECK(state->depth < UTILS_EHSM_MAX_DEPTH))
    {
        return -1;
    }
    return 0;
}

int utils_ehsm_create(utils_ehsm_t *ehsm, const char *name, utils_ehsm_state_t *initial_state, utils_ehsm_handler_t root_handler, uint32_t event_queue_size, void *user_data)
{
    if (UTILS_CHECK(ehsm != NULL))
        return -1;
    if (UTILS_CHECK(initial_state != NULL))
        return -2;
    if (UTILS_CHECK(event_queue_size > 0))
        return -3;
    ehsm->root_handler = root_handler;
    ehsm->name = name;
    ehsm->event_queue = ts_queue_create(sizeof(utils_ehsm_event_t), event_queue_size);
    if (UTILS_CHECK(ehsm->event_queue != NULL))
        return -4;
    ehsm->last_state = NULL;
    ehsm->current_state = initial_state;
    ehsm->init = false;
    ehsm->user_data = user_data;
    return 0;
}

int utils_ehsm_destroy(utils_ehsm_t *ehsm)
{
    if (UTILS_CHECK(ehsm != NULL))
        return -1;
    ts_queue_destroy(ehsm->event_queue);
    return 0;
}

int utils_ehsm_event(utils_ehsm_t *ehsm, int event, void *data, size_t size)
{
    if (UTILS_CHECK(ehsm != NULL))
        return -1;
    utils_ehsm_event_t _event = {0};
    _event.event = event;

    //拷贝参数
    if (size > 0)
    {
        _event.data = (uint8_t *)malloc(size);
        if(_event.data == NULL)
        {
            printf("ehsm alloc for event data failed\n");
        }
        memcpy(_event.data, data, size);
        _event.size = size;
    }
    else
    {
        _event.data = NULL;
        _event.size = 0;
    }

    //入队
    if (ts_queue_enqueue(ehsm->event_queue, &_event, 1) == 0)
    {
        free(_event.data);
        return -1;
    }
    return 0;
}

int utils_ehsm_run(utils_ehsm_t *ehsm)
{
    utils_ehsm_state_t *state;
    utils_ehsm_event_t event;

    if (UTILS_CHECK(ehsm != NULL))
        return -1;
    if (UTILS_CHECK(ehsm->current_state != NULL))
        return -2;

    //获取当前状态
    state = ehsm->current_state;

    if (!ehsm->init)
    {
        utils_ehsm_tran(ehsm, ehsm->current_state);
        ehsm->init = true;
    }

    //队列为空发送空闲事件
    if (ts_queue_is_empty(ehsm->event_queue))
    {
        event.event = UTILS_EHSM_IDLE_EVENT;
        event.data = NULL;
        event.size = 0;
        while (state->handler(ehsm, &event))
        {
            EHSM_PRINT("%s: %d ignore to %s from %s\n", ehsm->name, event.event, state->parent->name, state->name);
            state = state->parent;
        }
    }
    else
    {
        ts_queue_dequeue(ehsm->event_queue, &event, 1);
        //当返回非0零则继续将事件发送给父状态
        while (state->handler(ehsm, &event))
        {
            EHSM_PRINT("%s: %d ignore to %s from %s\n", ehsm->name, event.event, state->parent->name, state->name);
            state = state->parent;
        }
        //释放事件参数内存
        free(event.data);
    }
    return 0;
}

int utils_ehsm_tran(utils_ehsm_t *ehsm, utils_ehsm_state_t *next_state)
{
    utils_ehsm_state_t *src = ehsm->current_state;
    utils_ehsm_state_t *dst = next_state;
    utils_ehsm_state_t *list_exit[UTILS_EHSM_MAX_DEPTH];
    utils_ehsm_state_t *list_entry[UTILS_EHSM_MAX_DEPTH];

    uint8_t cnt_exit = 0;
    uint8_t cnt_entry = 0;
    //状态图迁移算法
    while (src->depth != dst->depth)
    {
        if (src->depth > dst->depth)
        {
            list_exit[cnt_exit++] = src;
            src = src->parent;
        }
        else
        {
            list_entry[cnt_entry++] = dst;
            dst = dst->parent;
        }
    }

    while (src != dst)
    {
        list_exit[cnt_exit++] = src;
        src = src->parent;
        list_entry[cnt_entry++] = dst;
        dst = dst->parent;
    }

    for (int i = 0; i < cnt_exit; i++)
    {
        src = list_exit[i];
        EHSM_PRINT("%s: %s EXIT\n", ehsm->name, src->name);
        src->handler(ehsm, &exit_event);
    }

    for (int i = 0; i < cnt_entry; i++)
    {
        dst = list_entry[cnt_entry - i - 1];
        EHSM_PRINT("%s: %s ENTRY\n", ehsm->name, dst->name);
        dst->handler(ehsm, &entry_event);
    }

    EHSM_PRINT("%s: %s --> %s\n", ehsm->name, ehsm->current_state->name, next_state->name);

    ehsm->last_state = ehsm->current_state;
    ehsm->current_state = next_state;
    ehsm->current_state->handler(ehsm, &init_event);
    EHSM_PRINT("%s: %s INIT\n", ehsm->name, dst->name);

    return 0;
}

utils_ehsm_state_t *utils_ehsm_get_state(utils_ehsm_t *ehsm)
{
    return ehsm->current_state;
}

utils_ehsm_state_t *utils_ehsm_get_last_state(utils_ehsm_t *ehsm)
{
    return ehsm->last_state;
}

static int root_state_handler(utils_ehsm_t *ehsm, utils_ehsm_event_t *event)
{
    EHSM_PRINT("%s: root handle: %d\n", ehsm->name, event->event);
    if (ehsm->root_handler)
        ehsm->root_handler(ehsm, event);
    return 0;
}
